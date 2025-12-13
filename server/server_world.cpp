/*
 * ServerWorld implementation
 * Authoritative game state management
 */

#include "server_world.h"
#include "network/supabase_client.h"
#include <cstdio>
#include <algorithm>

namespace Server {

// ============================================================================
// Construction
// ============================================================================

ServerWorld::ServerWorld() :
	m_dirty(false),
	m_nextAgentId(1000) // NPCs start at 1000 to differentiate from player IDs
{
}

ServerWorld::~ServerWorld()
{
	if (m_dirty) {
		SaveDirtyState();
	}
}

// ============================================================================
// Data Loading/Saving
// ============================================================================

void ServerWorld::LoadFromSupabase()
{
	printf("[ServerWorld] Loading world state from Supabase...\n");

	// Load computers
	auto computers = Net::SupabaseClient::Instance().GetAllComputers();
	m_computers.clear();
	m_computerByIp.clear();

	for (const auto& c : computers) {
		ServerComputer sc;
		sc.id = c.id;
		sc.ipString = c.ip;
		sc.ip = 0; // TODO: Parse IP string to int
		sc.name = c.name;
		sc.securityLevel = c.security_level;
		sc.running = c.is_running;
		sc.type = 0;
		sc.proxyBypassed = false;
		sc.firewallBypassed = false;
		sc.monitorDisabled = false;

		m_computerByIp[sc.ip] = m_computers.size();
		m_computers.push_back(sc);
	}

	printf("[ServerWorld] Loaded %zu computers\n", m_computers.size());

	// Load missions
	auto missions = Net::SupabaseClient::Instance().GetAllMissions();
	m_missions.clear();

	for (const auto& m : missions) {
		ServerMission sm;
		sm.id = m.id;
		sm.type = m.mission_type;
		sm.targetIp = 0; // TODO: Parse
		sm.payment = m.payment;
		sm.difficulty = m.difficulty;
		sm.claimedBy = m.claimed_by;
		sm.completed = m.completed;

		m_missions.push_back(sm);
	}

	printf("[ServerWorld] Loaded %zu missions\n", m_missions.size());

	m_dirty = false;
}

void ServerWorld::SaveDirtyState()
{
	if (!m_dirty) {
		return;
	}

	printf("[ServerWorld] Saving dirty state...\n");

	// TODO: Iterate and save only changed entities

	m_dirty = false;
}

// ============================================================================
// Computer Management
// ============================================================================

ServerComputer* ServerWorld::FindComputerByIP(int64_t ip)
{
	auto it = m_computerByIp.find(ip);
	if (it != m_computerByIp.end() && it->second < m_computers.size()) {
		return &m_computers[it->second];
	}
	return nullptr;
}

ServerComputer* ServerWorld::FindComputerByIPString(const char* ipString)
{
	for (auto& c : m_computers) {
		if (c.ipString == ipString) {
			return &c;
		}
	}
	return nullptr;
}

bool ServerWorld::PlayerConnect(uint32_t playerId, int64_t targetIp)
{
	ServerComputer* computer = FindComputerByIP(targetIp);
	if (!computer) {
		printf("[ServerWorld] REJECT: Player %u tried to connect to unknown IP\n", playerId);
		return false;
	}

	if (!computer->running) {
		printf("[ServerWorld] REJECT: Player %u tried to connect to offline computer %s\n",
			   playerId,
			   computer->name.c_str());
		return false;
	}

	// Add player to connected list if not already
	auto& connected = computer->connectedPlayers;
	if (std::find(connected.begin(), connected.end(), playerId) == connected.end()) {
		connected.push_back(playerId);
	}

	printf("[ServerWorld] Player %u connected to %s\n", playerId, computer->name.c_str());
	m_dirty = true;
	return true;
}

void ServerWorld::PlayerDisconnect(uint32_t playerId, int64_t fromIp)
{
	ServerComputer* computer = FindComputerByIP(fromIp);
	if (!computer) {
		return;
	}

	auto& connected = computer->connectedPlayers;
	connected.erase(std::remove(connected.begin(), connected.end(), playerId), connected.end());

	// Reset bypass states for this player
	// Note: In full implementation, track per-player bypass state

	printf("[ServerWorld] Player %u disconnected from %s\n", playerId, computer->name.c_str());
}

// ============================================================================
// Security Bypass
// ============================================================================

bool ServerWorld::TryBypassProxy(uint32_t playerId, int64_t targetIp, int16_t playerRating)
{
	ServerComputer* computer = FindComputerByIP(targetIp);
	if (!computer) {
		return false;
	}

	// Simple check: player rating must be >= security level
	if (playerRating >= computer->securityLevel) {
		computer->proxyBypassed = true;
		m_dirty = true;
		printf("[ServerWorld] Player %u bypassed proxy on %s\n", playerId, computer->name.c_str());
		return true;
	}

	printf("[ServerWorld] REJECT: Player %u failed to bypass proxy (rating %d < security %d)\n",
		   playerId,
		   playerRating,
		   computer->securityLevel);
	return false;
}

bool ServerWorld::TryBypassFirewall(uint32_t playerId, int64_t targetIp, int16_t playerRating)
{
	ServerComputer* computer = FindComputerByIP(targetIp);
	if (!computer) {
		return false;
	}

	if (playerRating >= computer->securityLevel) {
		computer->firewallBypassed = true;
		m_dirty = true;
		printf("[ServerWorld] Player %u bypassed firewall on %s\n", playerId, computer->name.c_str());
		return true;
	}

	return false;
}

bool ServerWorld::TryDisableMonitor(uint32_t playerId, int64_t targetIp, int16_t playerRating)
{
	ServerComputer* computer = FindComputerByIP(targetIp);
	if (!computer) {
		return false;
	}

	if (playerRating >= computer->securityLevel) {
		computer->monitorDisabled = true;
		m_dirty = true;
		printf("[ServerWorld] Player %u disabled monitor on %s\n", playerId, computer->name.c_str());
		return true;
	}

	return false;
}

// ============================================================================
// Banking
// ============================================================================

ServerBankAccount* ServerWorld::FindAccount(int64_t bankIp, const std::string& accountNumber)
{
	for (auto& acc : m_bankAccounts) {
		if (acc.bankIp == bankIp && acc.accountNumber == accountNumber) {
			return &acc;
		}
	}
	return nullptr;
}

bool ServerWorld::TransferMoney(int64_t srcBankIp,
								const std::string& srcAccount,
								int64_t dstBankIp,
								const std::string& dstAccount,
								int32_t amount)
{
	if (amount <= 0) {
		return false;
	}

	ServerBankAccount* src = FindAccount(srcBankIp, srcAccount);
	ServerBankAccount* dst = FindAccount(dstBankIp, dstAccount);

	if (!src || !dst) {
		printf("[ServerWorld] REJECT: Transfer failed - account not found\n");
		return false;
	}

	if (src->balance < amount) {
		printf(
			"[ServerWorld] REJECT: Transfer failed - insufficient funds (%d < %d)\n", src->balance, amount);
		return false;
	}

	src->balance -= amount;
	dst->balance += amount;
	m_dirty = true;

	printf(
		"[ServerWorld] Transferred %d credits: %s -> %s\n", amount, srcAccount.c_str(), dstAccount.c_str());

	return true;
}

// ============================================================================
// Missions
// ============================================================================

ServerMission* ServerWorld::FindMission(int32_t missionId)
{
	for (auto& m : m_missions) {
		if (m.id == missionId) {
			return &m;
		}
	}
	return nullptr;
}

bool ServerWorld::ClaimMission(int32_t missionId, uint32_t playerId)
{
	ServerMission* mission = FindMission(missionId);
	if (!mission) {
		return false;
	}

	if (mission->claimedBy != 0) {
		printf(
			"[ServerWorld] REJECT: Mission %d already claimed by player %d\n", missionId, mission->claimedBy);
		return false;
	}

	mission->claimedBy = playerId;
	m_dirty = true;

	printf("[ServerWorld] Player %u claimed mission %d\n", playerId, missionId);
	return true;
}

bool ServerWorld::CompleteMission(int32_t missionId, uint32_t playerId)
{
	ServerMission* mission = FindMission(missionId);
	if (!mission) {
		return false;
	}

	if (mission->claimedBy != static_cast<int32_t>(playerId)) {
		printf(
			"[ServerWorld] REJECT: Player %u tried to complete mission %d not theirs\n", playerId, missionId);
		return false;
	}

	mission->completed = true;
	m_dirty = true;

	printf("[ServerWorld] Player %u completed mission %d (payment: %d)\n",
		   playerId,
		   missionId,
		   mission->payment);

	return true;
}

// ============================================================================
// Access Logging
// ============================================================================

void ServerWorld::LogAccess(int32_t computerId, int64_t accessorIp, const std::string& action)
{
	ServerAccessLog log;
	log.computerId = computerId;
	log.accessorIp = accessorIp;
	log.action = action;
	log.timestamp = 0; // TODO: Get current time

	m_accessLogs.push_back(log);
	m_dirty = true;
}

// ============================================================================
// NPC Management
// ============================================================================

void ServerWorld::SpawnNPCs(int count)
{
	printf("[ServerWorld] Spawning %d NPCs...\n", count);

	static const char* npcNames[] = { "Scarab", "Serpent", "Phoenix", "Raven",	 "Falcon",
									  "Shadow", "Ghost",   "Phantom", "Specter", "Wraith" };

	for (int i = 0; i < count; i++) {
		ServerAgent npc;
		npc.id = m_nextAgentId++;
		npc.handle = npcNames[i % 10];
		npc.isNPC = true;
		npc.playerId = 0;
		npc.uplinkRating = 1 + (i % 5); // Rating 1-5
		npc.neuromancerRating = 0;
		npc.credits = 1000 + (i * 500);
		npc.connectedToIp = 0;
		npc.currentMissionId = 0;
		npc.aiThinkTimer = 5.0f + (i * 2.0f); // Stagger AI ticks

		m_agents.push_back(npc);
		printf("[ServerWorld] Created NPC: %s (rating %d)\n", npc.handle.c_str(), npc.uplinkRating);
	}
}

void ServerWorld::Update(float deltaTime)
{
	// Update all NPC agents
	for (auto& agent : m_agents) {
		if (agent.isNPC) {
			UpdateNPCAgent(agent, deltaTime);
		}
	}
}

void ServerWorld::UpdateNPCAgent(ServerAgent& npc, float deltaTime)
{
	npc.aiThinkTimer -= deltaTime;

	if (npc.aiThinkTimer > 0.0f) {
		return; // Not time to think yet
	}

	// Reset timer (NPCs think every 10-30 seconds)
	npc.aiThinkTimer = 10.0f + (rand() % 20);

	// Simple AI: if not on a mission, try to claim one
	if (npc.currentMissionId == 0) {
		// Find unclaimed mission
		for (auto& m : m_missions) {
			if (m.claimedBy == 0 && !m.completed && m.difficulty <= npc.uplinkRating) {
				npc.currentMissionId = m.id;
				m.claimedBy = npc.id; // Use agent ID for NPCs
				m_dirty = true;
				printf("[NPC AI] %s claimed mission %d\n", npc.handle.c_str(), m.id);
				break;
			}
		}
	} else {
		// Already on mission - attempt to complete it
		NPCAttemptMission(npc);
	}
}

void ServerWorld::NPCAttemptMission(ServerAgent& npc)
{
	ServerMission* mission = FindMission(npc.currentMissionId);
	if (!mission) {
		npc.currentMissionId = 0;
		return;
	}

	// Simple success chance based on rating vs difficulty
	int successChance = 50 + ((npc.uplinkRating - mission->difficulty) * 10);
	successChance = std::max(10, std::min(90, successChance));

	if ((rand() % 100) < successChance) {
		// Success!
		mission->completed = true;
		npc.credits += mission->payment;
		m_dirty = true;

		printf("[NPC AI] %s COMPLETED mission %d, earned %d credits\n",
			   npc.handle.c_str(),
			   mission->id,
			   mission->payment);

		// Increase rating occasionally
		if (rand() % 3 == 0) {
			npc.uplinkRating++;
			printf("[NPC AI] %s rating increased to %d\n", npc.handle.c_str(), npc.uplinkRating);
		}

		npc.currentMissionId = 0;
	} else {
		// Failed - may get traced
		printf("[NPC AI] %s failed mission %d attempt\n", npc.handle.c_str(), mission->id);

		// 10% chance of getting caught
		if (rand() % 10 == 0) {
			npc.uplinkRating = std::max(0, npc.uplinkRating - 1);
			printf("[NPC AI] %s TRACED! Rating dropped to %d\n", npc.handle.c_str(), npc.uplinkRating);
		}
	}
}

} // namespace Server
