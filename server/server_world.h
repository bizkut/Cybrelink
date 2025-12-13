#pragma once

/*
 * ServerWorld - Authoritative world state for dedicated server
 * Manages all computers, banks, missions, access logs
 * Clients are "dumb terminals" - server owns all game logic
 */

#include <vector>
#include <string>
#include <unordered_map>
#include <cstdint>

namespace Server {

// ============================================================================
// Server-side Computer State
// ============================================================================

struct ServerComputer {
	int32_t id;
	int64_t ip; // IP as integer for fast lookup
	std::string ipString; // "123.456.789.012" format
	std::string name;
	int16_t type;
	int16_t securityLevel;
	bool running;

	// Security state
	bool proxyBypassed;
	bool firewallBypassed;
	bool monitorDisabled;

	// Connected players (by playerId)
	std::vector<uint32_t> connectedPlayers;
};

// ============================================================================
// Server-side Bank Account
// ============================================================================

struct ServerBankAccount {
	int32_t id;
	int64_t bankIp;
	std::string accountNumber;
	std::string accountName;
	int32_t balance;
	int32_t ownerPlayerId; // 0 = NPC/system
};

// ============================================================================
// Server-side Mission
// ============================================================================

struct ServerMission {
	int32_t id;
	int16_t type;
	int64_t targetIp;
	std::string description;
	int32_t payment;
	int16_t difficulty;
	int32_t claimedBy; // player ID or 0
	bool completed;
};

// ============================================================================
// Server-side Access Log Entry
// ============================================================================

struct ServerAccessLog {
	int32_t computerId;
	int64_t accessorIp;
	std::string action;
	int64_t timestamp;
};

// ============================================================================
// Server-side Agent (NPC or Player)
// ============================================================================

struct ServerAgent {
	int32_t id;
	std::string handle;
	bool isNPC; // true = AI-controlled, false = player
	uint32_t playerId; // 0 if NPC

	// Stats
	int16_t uplinkRating;
	int16_t neuromancerRating;
	int32_t credits;

	// Current state
	int64_t connectedToIp; // 0 = not connected
	std::vector<int64_t> bouncePath;

	// AI state (for NPCs)
	int32_t currentMissionId;
	float aiThinkTimer;
};

// ============================================================================
// ServerWorld - The authoritative game state
// ============================================================================

class ServerWorld {
public:
	ServerWorld();
	~ServerWorld();

	// Initialization
	void LoadFromSupabase();
	void SaveDirtyState();

	// Computer management
	ServerComputer* FindComputerByIP(int64_t ip);
	ServerComputer* FindComputerByIPString(const char* ipString);
	bool PlayerConnect(uint32_t playerId, int64_t targetIp);
	void PlayerDisconnect(uint32_t playerId, int64_t fromIp);

	// Security
	bool TryBypassProxy(uint32_t playerId, int64_t targetIp, int16_t playerRating);
	bool TryBypassFirewall(uint32_t playerId, int64_t targetIp, int16_t playerRating);
	bool TryDisableMonitor(uint32_t playerId, int64_t targetIp, int16_t playerRating);

	// Banking
	ServerBankAccount* FindAccount(int64_t bankIp, const std::string& accountNumber);
	bool TransferMoney(int64_t srcBankIp,
					   const std::string& srcAccount,
					   int64_t dstBankIp,
					   const std::string& dstAccount,
					   int32_t amount);

	// Missions
	ServerMission* FindMission(int32_t missionId);
	bool ClaimMission(int32_t missionId, uint32_t playerId);
	bool CompleteMission(int32_t missionId, uint32_t playerId);

	// Access logging
	void LogAccess(int32_t computerId, int64_t accessorIp, const std::string& action);

	// Getters
	const std::vector<ServerComputer>& GetComputers() const { return m_computers; }
	const std::vector<ServerMission>& GetMissions() const { return m_missions; }
	const std::vector<ServerAgent>& GetAgents() const { return m_agents; }

	// NPC Management
	void SpawnNPCs(int count);
	void Update(float deltaTime); // Called every game tick - runs NPC AI

private:
	void UpdateNPCAgent(ServerAgent& npc, float deltaTime);
	void NPCAttemptMission(ServerAgent& npc);

	std::vector<ServerComputer> m_computers;
	std::vector<ServerBankAccount> m_bankAccounts;
	std::vector<ServerMission> m_missions;
	std::vector<ServerAccessLog> m_accessLogs;
	std::vector<ServerAgent> m_agents; // NPCs and player agents

	// Fast lookup maps
	std::unordered_map<int64_t, size_t> m_computerByIp; // ip -> index

	bool m_dirty; // needs saving
	int32_t m_nextAgentId;
};

} // namespace Server
