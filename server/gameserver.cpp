/*
 * Cybrelink Dedicated Game Server Implementation
 */

#include "gameserver.h"
#include "network/supabase_client.h"

#include <cstdio>
#include <ctime>
#include <thread>
#include <algorithm>

// #include "world/world.h"
// #include "world/agent.h"

namespace Server {

// Helper to get formatted timestamp for logging
static const char* GetTimestamp()
{
	static char buffer[32];
	time_t now = time(nullptr);
	struct tm* timeinfo = localtime(&now);
	strftime(buffer, sizeof(buffer), "%H:%M:%S", timeinfo);
	return buffer;
}

// ============================================================================
// GameServer Implementation
// ============================================================================

GameServer::GameServer() :
	m_running(false),
	m_gameTickInterval(1000.0 / 60.0) // 60Hz
	,
	m_networkTickInterval(1000.0 / 20.0) // 20Hz
	,
	m_nextPlayerId(1),
	m_date(), // Initialize Date object directly
	m_tickNumber(0)
{
}

GameServer::~GameServer() { Shutdown(); }

bool GameServer::Init(const ServerConfig& config)
{
	m_config = config;

	// Calculate tick intervals
	m_gameTickInterval = 1000.0 / config.tickRateHz;
	m_networkTickInterval = 1000.0 / config.networkTickRateHz;

	printf("[Server] Initializing on port %d (max %d players)\n", config.port, config.maxPlayers);

	// Initialize Supabase
	if (!config.supabaseUrl.empty()) {
		printf("[Server] Connecting to Supabase at %s\n", config.supabaseUrl.c_str());
		Net::SupabaseClient::Instance().Init(config.supabaseUrl, config.supabaseKey);
	} else {
		printf("[Server] WARNING: Supabase URL not configured, persistence disabled\n");
	}

	// Initialize networking
	Net::NetResult result = Net::NetInit();
	if (result != Net::NetResult::OK) {
		printf("[Server] ERROR: Failed to initialize networking\n");
		return false;
	}

	// Start listening
	result = Net::NetworkManager::Instance().Listen(config.port);
	if (result != Net::NetResult::OK) {
		printf("[Server] ERROR: Failed to listen on port %d\n", config.port);
		return false;
	}

	printf("[Server] Listening on port %d\n", config.port);

	// Reserve space for players
	m_players.reserve(config.maxPlayers);

	// Create the world (now just initializes date)
	CreateWorld();

	m_running = true;
	m_lastGameTick = std::chrono::steady_clock::now();
	m_lastNetworkTick = std::chrono::steady_clock::now();

	printf("[Server] Initialization complete\n");
	return true;
}

void GameServer::Run()
{
	printf("[Server] Starting main loop\n");

	while (m_running) {
		auto now = std::chrono::steady_clock::now();

		// Game tick (60Hz)
		auto gameDelta = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastGameTick).count();
		if (gameDelta >= m_gameTickInterval) {
			GameTick();
			m_lastGameTick = now;
		}

		// Network tick (20Hz)
		auto networkDelta =
			std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastNetworkTick).count();
		if (networkDelta >= m_networkTickInterval) {
			NetworkTick();
			m_lastNetworkTick = now;
		}

		// Small sleep to prevent spinning
		std::this_thread::sleep_for(std::chrono::microseconds(500));
	}

	printf("[Server] Main loop ended\n");
}

void GameServer::Shutdown()
{
	if (!m_running) {
		return;
	}

	m_running = false;

	printf("[Server] Shutting down...\n");

	// Disconnect all players
	for (auto& player : m_players) {
		DisconnectPlayer(player, "Server shutting down");
	}
	m_players.clear();

	// Stop listening
	Net::NetworkManager::Instance().StopListening();

	// Cleanup networking
	Net::NetShutdown();

	// Cleanup world
	/*
	if (m_world) {
		delete m_world;
		m_world = nullptr;
	}
	*/

	printf("[Server] Shutdown complete\n");
}

bool GameServer::IsRunning() const { return m_running; }

int GameServer::GetPlayerCount() const { return static_cast<int>(m_players.size()); }

void GameServer::GameTick()
{
	// Update world simulation

	// We only want to update the date for now to avoid side effects of full world update
	// without full App context if not fully verified
	m_date.Update();

	// Update NPC agents
	UpdateNPCs();

	// Process mission completions
	ProcessMissions();

	// Periodic auto-save to Supabase (every 30 seconds)
	SaveDirtyStateToSupabase();

	m_tickNumber++;
}

void GameServer::NetworkTick()
{
	// Accept new connections
	AcceptConnections();

	// Process incoming data from all players
	for (auto& player : m_players) {
		ProcessIncoming(player);
	}

	// Send world state deltas to all players
	for (auto& player : m_players) {
		if (player.authenticated) {
			SendWorldDelta(player);
		}
	}

	// Broadcast online player list (every network tick = 20Hz)
	BroadcastPlayerList();

	// Check for timeouts
	CheckTimeouts();
}

void GameServer::AcceptConnections()
{
	if (static_cast<int>(m_players.size()) >= m_config.maxPlayers) {
		return; // Full
	}

	Net::Socket* newSocket = Net::NetworkManager::Instance().Accept();
	if (!newSocket) {
		return; // No pending connection
	}

	// Create new player connection
	PlayerConnection player;
	player.playerId = m_nextPlayerId++;
	player.socket = std::move(*newSocket);
	delete newSocket;

	printf("[%s] CONNECT: Player #%u from %s (total: %zu/%d)\n",
		   GetTimestamp(),
		   player.playerId,
		   player.socket.GetRemoteIP().c_str(),
		   m_players.size() + 1,
		   m_config.maxPlayers);

	m_players.push_back(std::move(player));
}

void GameServer::ProcessIncoming(PlayerConnection& player)
{
	if (!player.socket.IsValid()) {
		return;
	}

	uint8_t buffer[4096];
	int received = player.socket.Recv(buffer, sizeof(buffer), 0);

	if (received < 0) {
		// Error or disconnect
		DisconnectPlayer(player, "Connection lost");
		return;
	}

	if (received == 0) {
		return; // No data
	}

	player.lastActivity = std::chrono::steady_clock::now();

	// Parse packet header
	if (received < static_cast<int>(sizeof(Net::PacketHeader))) {
		return; // Incomplete header
	}

	const Net::PacketHeader* header = reinterpret_cast<const Net::PacketHeader*>(buffer);
	const uint8_t* payload = buffer + sizeof(Net::PacketHeader);
	size_t payloadLen = received - sizeof(Net::PacketHeader);

	// Route to appropriate handler
	switch (static_cast<Net::PacketType>(header->type)) {
	case Net::PacketType::HANDSHAKE:
		HandleHandshake(player, payload, payloadLen);
		break;
	case Net::PacketType::PLAYER_ACTION:
		HandlePlayerAction(player, payload, payloadLen);
		break;
	case Net::PacketType::PLAYER_CHAT:
		HandleChat(player, payload, payloadLen);
		break;
	case Net::PacketType::KEEPALIVE:
		// Just update lastActivity, already done above
		break;
	default:
		printf("[Server] Unknown packet type 0x%02X from player %u\n", header->type, player.playerId);
		break;
	}
}

void GameServer::SendWorldDelta(PlayerConnection& player)
{
	// Send TimeSync packet
	Net::TimeSyncPacket packet;
	packet.second = m_date.GetSecond();
	packet.minute = m_date.GetMinute();
	packet.hour = m_date.GetHour();
	packet.day = m_date.GetDay();
	packet.month = m_date.GetMonth();
	packet.year = m_date.GetYear();
	packet.paused = false; // TODO: Support pausing
	packet.gameSpeed = 1.0f;

	uint8_t buffer[128];
	size_t len =
		Net::WritePacket(buffer, Net::PacketType::TIME_SYNC, Net::FLAG_NONE, &packet, sizeof(packet));

	player.socket.Send(buffer, len);
}

void GameServer::BroadcastMessage(const void* data, size_t length)
{
	for (auto& player : m_players) {
		if (player.authenticated && player.socket.IsValid()) {
			player.socket.Send(data, length);
		}
	}
}

void GameServer::BroadcastPlayerList()
{
	Net::PlayerListPacket packet;
	memset(&packet, 0, sizeof(packet));

	uint8_t count = 0;
	for (const auto& player : m_players) {
		if (player.authenticated && count < 32) {
			packet.players[count].playerId = player.playerId;
			strncpy(packet.players[count].handle,
					player.handle.c_str(),
					sizeof(packet.players[count].handle) - 1);
			packet.players[count].rating = player.uplinkRating;
			count++;
		}
	}
	packet.playerCount = count;

	// Calculate actual packet size (header + count + entries)
	size_t dataSize = sizeof(uint8_t) + (count * sizeof(Net::PlayerListEntry));

	uint8_t buffer[2048];
	size_t len = Net::WritePacket(buffer, Net::PacketType::PLAYER_LIST, Net::FLAG_NONE, &packet, dataSize);

	BroadcastMessage(buffer, len);
}

void GameServer::HandleHandshake(PlayerConnection& player, const uint8_t* data, size_t length)
{
	if (length < sizeof(Net::HandshakePacket)) {
		DisconnectPlayer(player, "Invalid handshake");
		return;
	}

	const Net::HandshakePacket* handshake = reinterpret_cast<const Net::HandshakePacket*>(data);

	// Check protocol version
	if (handshake->protocolVersion != Net::PROTOCOL_VERSION) {
		printf("[Server] Player %u has wrong protocol version (%u vs %u)\n",
			   player.playerId,
			   handshake->protocolVersion,
			   Net::PROTOCOL_VERSION);
		DisconnectPlayer(player, "Protocol version mismatch");
		return;
	}

	// Store player handle
	player.handle = std::string(handshake->handle, strnlen(handshake->handle, sizeof(handshake->handle)));

	// Extract auth token
	std::string authToken(handshake->authToken, strnlen(handshake->authToken, sizeof(handshake->authToken)));

	// Verify token with Supabase (if Supabase is configured)
	if (!m_config.supabaseUrl.empty() && !authToken.empty()) {
		std::string authId = Net::SupabaseClient::Instance().VerifyToken(authToken);

		if (authId.empty()) {
			printf("[%s] AUTH FAIL: Player #%u - invalid token\n", GetTimestamp(), player.playerId);
			DisconnectPlayer(player, "Invalid or expired auth token");
			return;
		}

		player.authId = authId;
		printf("[%s] AUTH OK: Player #%u '%s' verified (id: %.8s...)\n",
			   GetTimestamp(),
			   player.playerId,
			   player.handle.c_str(),
			   authId.c_str());
	} else if (authToken.empty()) {
		// No token provided - allow as guest for now (can be made stricter)
		printf("[%s] AUTH GUEST: Player #%u '%s' (no token)\n",
			   GetTimestamp(),
			   player.playerId,
			   player.handle.c_str());
		player.authId = "";
	} else {
		// Supabase not configured - trust the handle
		printf("[%s] AUTH SKIP: Player #%u '%s' (Supabase disabled)\n",
			   GetTimestamp(),
			   player.playerId,
			   player.handle.c_str());
		player.authId = "";
	}

	player.authenticated = true;

	// Load player profile from Supabase if authId is set
	if (!player.authId.empty()) {
		// Need to set auth token for profile queries
		Net::SupabaseClient::Instance().SetAuthToken(authToken);

		auto profile = Net::SupabaseClient::Instance().GetPlayerProfile(player.authId);
		if (profile.has_value()) {
			player.credits = profile->credits;
			player.uplinkRating = profile->uplink_rating;
			player.neuromancerRating = profile->neuromancer_rating;
			printf("[Server] Loaded profile for %s: credits=%d rating=%d\n",
				   player.handle.c_str(),
				   player.credits,
				   player.uplinkRating);
		} else {
			// Profile doesn't exist yet - create default
			printf("[Server] No profile found for %s, using defaults\n", player.handle.c_str());
			player.credits = 3000; // PLAYER_START_BALANCE
			player.uplinkRating = 1;
			player.neuromancerRating = 0;

			// Attempt to create profile in database
			Net::SupabaseClient::Instance().CreatePlayerProfile(player.authId, player.handle);
		}
	} else {
		// Guest player - use defaults
		player.credits = 3000;
		player.uplinkRating = 1;
		player.neuromancerRating = 0;
	}

	// TODO: Create or load agent for this player
	// TODO: Send handshake response with player ID
	// TODO: Send initial world state
}

void GameServer::HandlePlayerAction(PlayerConnection& player, const uint8_t* data, size_t length)
{
	if (!player.authenticated) {
		return;
	}

	if (length < sizeof(Net::ActionPacket)) {
		return;
	}

	const Net::ActionPacket* action = reinterpret_cast<const Net::ActionPacket*>(data);

	printf("[Server] Player %u action: type=0x%02X target=%u param1=%u param2=%u\n",
		   player.playerId,
		   static_cast<uint8_t>(action->actionType),
		   action->targetId,
		   action->param1,
		   action->param2);

	// Route action to appropriate handler
	switch (action->actionType) {
	// Connection actions
	case Net::ActionType::ADD_BOUNCE:
		HandleAction_AddBounce(player, action);
		break;
	case Net::ActionType::CONNECT_TARGET:
		HandleAction_ConnectTarget(player, action);
		break;
	case Net::ActionType::DISCONNECT_ALL:
		HandleAction_Disconnect(player, action);
		break;

	// Hacking actions
	case Net::ActionType::RUN_SOFTWARE:
		HandleAction_RunSoftware(player, action);
		break;
	case Net::ActionType::BYPASS_SECURITY:
		HandleAction_BypassSecurity(player, action);
		break;

	// File actions
	case Net::ActionType::DOWNLOAD_FILE:
		HandleAction_DownloadFile(player, action);
		break;
	case Net::ActionType::DELETE_FILE:
		HandleAction_DeleteFile(player, action);
		break;

	// Log actions
	case Net::ActionType::DELETE_LOG:
		HandleAction_DeleteLog(player, action);
		break;

	// Bank actions
	case Net::ActionType::TRANSFER_MONEY:
		HandleAction_TransferMoney(player, action);
		break;

	// PVP actions
	case Net::ActionType::FRAME_PLAYER:
		HandleAction_FramePlayer(player, action);
		break;
	case Net::ActionType::PLACE_BOUNTY:
		HandleAction_PlaceBounty(player, action);
		break;

	default:
		printf("[Server] Unknown action type 0x%02X from player %u\n",
			   static_cast<uint8_t>(action->actionType),
			   player.playerId);
		break;
	}
}

void GameServer::HandleChat(PlayerConnection& player, const uint8_t* data, size_t length)
{
	if (!player.authenticated) {
		return;
	}

	if (length < sizeof(Net::PacketHeader) + sizeof(Net::ChatPacket)) {
		printf("[%s] CHAT: Invalid packet size from %u\n", GetTimestamp(), player.playerId);
		return;
	}

	const Net::ChatPacket* incoming =
		reinterpret_cast<const Net::ChatPacket*>(data + sizeof(Net::PacketHeader));

	// Sanitize message (ensure null termination)
	char message[256];
	strncpy(message, incoming->message, sizeof(message) - 1);
	message[sizeof(message) - 1] = '\0';

	// Log the chat
	printf("[%s] CHAT: [%s] %s: %s\n", GetTimestamp(), incoming->channel, player.handle.c_str(), message);

	// Create outgoing chat packet with server-verified sender
	Net::ChatPacket outgoing;
	strncpy(outgoing.sender, player.handle.c_str(), sizeof(outgoing.sender) - 1);
	outgoing.sender[sizeof(outgoing.sender) - 1] = '\0';
	strncpy(outgoing.channel, incoming->channel, sizeof(outgoing.channel) - 1);
	outgoing.channel[sizeof(outgoing.channel) - 1] = '\0';
	strncpy(outgoing.message, message, sizeof(outgoing.message) - 1);
	outgoing.message[sizeof(outgoing.message) - 1] = '\0';

	// Serialize and broadcast to all authenticated players
	uint8_t buffer[512];
	size_t packetLen =
		Net::WritePacket(buffer, Net::PacketType::PLAYER_CHAT, Net::FLAG_NONE, &outgoing, sizeof(outgoing));

	for (auto& p : m_players) {
		if (p.authenticated && p.socket.IsValid()) {
			p.socket.Send(buffer, packetLen);
		}
	}
}

PlayerConnection* GameServer::FindPlayer(uint32_t playerId)
{
	for (auto& player : m_players) {
		if (player.playerId == playerId) {
			return &player;
		}
	}
	return nullptr;
}

void GameServer::DisconnectPlayer(PlayerConnection& player, const char* reason)
{
	printf("[%s] DISCONNECT: Player #%u '%s' - %s (remaining: %zu)\n",
		   GetTimestamp(),
		   player.playerId,
		   player.handle.empty() ? "(unknown)" : player.handle.c_str(),
		   reason,
		   m_players.size() - 1);

	// Save player state to Supabase
	if (player.authenticated) {
		// TODO: Get actual stats from agent
		Net::PlayerProfile profile;
		profile.handle = player.handle;
		// profile.credits = player.agent->GetBalance(); // Stub

		// Net::SupabaseClient::Instance().UpdatePlayerProfile(profile);
		printf("[%s] SAVE: Saving state for '%s'\n", GetTimestamp(), player.handle.c_str());
	}

	player.socket.Close();
	player.authenticated = false;

	// Remove from list
	m_players.erase(
		std::remove_if(m_players.begin(),
					   m_players.end(),
					   [&player](const PlayerConnection& p) { return p.playerId == player.playerId; }),
		m_players.end());
}

void GameServer::CheckTimeouts()
{
	auto now = std::chrono::steady_clock::now();

	for (auto& player : m_players) {
		auto elapsed =
			std::chrono::duration_cast<std::chrono::milliseconds>(now - player.lastActivity).count();

		if (elapsed > m_config.connectionTimeoutMs) {
			DisconnectPlayer(player, "Connection timeout");
		}
	}
}

void GameServer::CreateWorld()
{
	printf("[%s] Creating world...\n", GetTimestamp());

	// Set start date: 14:00, 14th April 2010 (Uplink default)
	m_date.SetDate(0, 0, 14, 14, 4, 3010);
	m_date.Activate(); // Ensure it updates

	// Load world state from Supabase
	LoadWorldFromSupabase();

	m_lastSaveTime = std::chrono::steady_clock::now();
	printf("[%s] World created at %s\n", GetTimestamp(), m_date.GetLongString());
}

void GameServer::LoadWorldFromSupabase()
{
	if (m_config.supabaseUrl.empty()) {
		printf("[%s] WORLD: Supabase not configured, using empty world\n", GetTimestamp());
		return;
	}

	printf("[%s] WORLD: Loading from Supabase via ServerWorld...\n", GetTimestamp());
	m_world.LoadFromSupabase();

	// Spawn NPCs that run independently of players
	m_world.SpawnNPCs(5);

	printf("[%s] WORLD: Load complete\n", GetTimestamp());
}

void GameServer::SaveDirtyStateToSupabase()
{
	if (m_config.supabaseUrl.empty()) {
		return;
	}

	// Only save every 30 seconds
	auto now = std::chrono::steady_clock::now();
	auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_lastSaveTime).count();
	if (elapsed < 30) {
		return;
	}

	m_lastSaveTime = now;
	printf("[%s] WORLD: Auto-saving state via ServerWorld...\n", GetTimestamp());
	m_world.SaveDirtyState();
}

void GameServer::UpdateNPCs()
{
	// Run NPC AI through ServerWorld
	// deltaTime = 1/60 second at 60Hz tick
	m_world.Update(1.0f / 60.0f);
}

void GameServer::ProcessMissions()
{
	// TODO: Check for mission completions, update ratings, etc.
}

// ============================================================================
// Action Handlers
// ============================================================================

void GameServer::HandleAction_AddBounce(PlayerConnection& player, const Net::ActionPacket* action)
{
	// param1 = IP address as uint32
	// data = IP string
	printf("[Server] Player %u adding bounce: %s\n", player.playerId, action->data);
	// TODO: Add to player's bounce path, validate IP exists
}

void GameServer::HandleAction_ConnectTarget(PlayerConnection& player, const Net::ActionPacket* action)
{
	// data = target IP string
	printf("[Server] Player %u connecting to: %s\n", player.playerId, action->data);
	// TODO: Initiate connection, start trace timer, etc.
}

void GameServer::HandleAction_Disconnect(PlayerConnection& player, const Net::ActionPacket* action)
{
	(void)action;
	printf("[Server] Player %u disconnecting from current target\n", player.playerId);
	// TODO: Clear connection, stop trace, clean up
}

void GameServer::HandleAction_RunSoftware(PlayerConnection& player, const Net::ActionPacket* action)
{
	// param1 = software type
	// param2 = software version
	printf("[Server] Player %u running software type=%u ver=%u\n",
		   player.playerId,
		   action->param1,
		   action->param2);
	// TODO: Validate player owns software, execute effect
}

void GameServer::HandleAction_BypassSecurity(PlayerConnection& player, const Net::ActionPacket* action)
{
	// param1 = security type (proxy, firewall, monitor, etc.)
	printf("[Server] Player %u bypassing security type=%u\n", player.playerId, action->param1);
	// TODO: Check if player has right tools, grant access
}

void GameServer::HandleAction_DownloadFile(PlayerConnection& player, const Net::ActionPacket* action)
{
	// targetId = file ID
	// data = filename
	printf("[Server] Player %u downloading file: %s\n", player.playerId, action->data);
	// TODO: Check access, start download, transfer data
}

void GameServer::HandleAction_DeleteFile(PlayerConnection& player, const Net::ActionPacket* action)
{
	// targetId = file ID
	printf("[Server] Player %u deleting file ID=%u\n", player.playerId, action->targetId);
	// TODO: Check permission, remove file, log action
}

void GameServer::HandleAction_DeleteLog(PlayerConnection& player, const Net::ActionPacket* action)
{
	// targetId = log entry ID
	printf("[Server] Player %u deleting log ID=%u\n", player.playerId, action->targetId);
	// TODO: Check if log is visible to player, remove it
}

void GameServer::HandleAction_TransferMoney(PlayerConnection& player, const Net::ActionPacket* action)
{
	// param1 = amount
	// param2 = source account ID
	// targetId = destination account ID
	printf("[Server] Player %u transferring %u credits from %u to %u\n",
		   player.playerId,
		   action->param1,
		   action->param2,
		   action->targetId);
	// TODO: Validate accounts, check balance, make transfer
}

void GameServer::HandleAction_FramePlayer(PlayerConnection& player, const Net::ActionPacket* action)
{
	// targetId = target player ID (to frame)
	// param1 = crime type
	printf("[Server] PVP: Player %u framing player %u for crime %u\n",
		   player.playerId,
		   action->targetId,
		   action->param1);
	// TODO: Plant evidence, modify logs to incriminate target player
}

void GameServer::HandleAction_PlaceBounty(PlayerConnection& player, const Net::ActionPacket* action)
{
	// targetId = target player ID
	// param1 = bounty amount
	printf("[Server] PVP: Player %u placing bounty of %u on player %u\n",
		   player.playerId,
		   action->param1,
		   action->targetId);

	// Validate player has funds
	if ((int32_t)action->param1 > player.credits) {
		printf("[Server] Player %u has insufficient funds for bounty\n", player.playerId);
		return;
	}

	// Deduct from player
	player.credits -= action->param1;

	// TODO: Add bounty to target player in database
	// TODO: Notify target player of bounty
}

// ============================================================================
// Server Entry Point
// ============================================================================

int ServerMain(int argc, char* argv[])
{
	printf("Cybrelink Dedicated Server\n");
	printf("==========================\n\n");

	ServerConfig config;

	// Default Supabase config (Cybrelink project)
	config.supabaseUrl = "https://lszlgjxdygugmvylkxta.supabase.co";
	config.supabaseKey =
		"eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9."
		"eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6ImxzemxnanhkeWd1Z212eWxreHRhIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NjU1MDkw"
		"NDAsImV4cCI6MjA4MTA4NTA0MH0.oV0AiRm3vn_IkclBiHOcVUXAFD84st9fCS0cuASesd8";

	// Parse command line args
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0) {
			if (i + 1 < argc) {
				config.port = static_cast<uint16_t>(atoi(argv[++i]));
			}
		} else if (strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--max-players") == 0) {
			if (i + 1 < argc) {
				config.maxPlayers = atoi(argv[++i]);
			}
		} else if (strcmp(argv[i], "--url") == 0) {
			if (i + 1 < argc) {
				config.supabaseUrl = argv[++i];
			}
		} else if (strcmp(argv[i], "--key") == 0) {
			if (i + 1 < argc) {
				config.supabaseKey = argv[++i];
			}
		} else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
			printf("Usage: uplink-server [options]\n");
			printf("  -p, --port <port>          Server port (default: %d)\n", Net::DEFAULT_PORT);
			printf("  -m, --max-players <num>    Max players (default: 8)\n");
			printf("  --url <url>                Supabase URL\n");
			printf("  --key <key>                Supabase Anon Key\n");
			printf("  -h, --help                 Show this help\n");
			return 0;
		}
	}

	GameServer server;

	if (!server.Init(config)) {
		printf("Failed to initialize server\n");
		return 1;
	}

	server.Run();

	return 0;
}

} // namespace Server

// Main entry point for server executable
int main(int argc, char* argv[]) { return Server::ServerMain(argc, argv); }
