/*
 * Cybrelink Dedicated Game Server Implementation
 */

#include "gameserver.h"
#include "network/supabase_client.h"

#include <cstdio>
#include <thread>
#include <algorithm>

// TODO: Include Uplink world headers when integrating
// #include "world/world.h"
// #include "world/agent.h"

namespace Server {

// ============================================================================
// GameServer Implementation
// ============================================================================

GameServer::GameServer()
    : m_running(false)
    , m_gameTickInterval(1000.0 / 60.0)  // 60Hz
    , m_networkTickInterval(1000.0 / 20.0)  // 20Hz
    , m_nextPlayerId(1)
    , m_world(nullptr)
    , m_tickNumber(0)
{
}

GameServer::~GameServer() {
    Shutdown();
}

bool GameServer::Init(const ServerConfig& config) {
    m_config = config;
    
    // Calculate tick intervals
    m_gameTickInterval = 1000.0 / config.tickRateHz;
    m_networkTickInterval = 1000.0 / config.networkTickRateHz;
    
    printf("[Server] Initializing on port %d (max %d players)\n", 
           config.port, config.maxPlayers);

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
    
    // Create the world
    CreateWorld();
    
    m_running = true;
    m_lastGameTick = std::chrono::steady_clock::now();
    m_lastNetworkTick = std::chrono::steady_clock::now();
    
    printf("[Server] Initialization complete\n");
    return true;
}

void GameServer::Run() {
    printf("[Server] Starting main loop\n");
    
    while (m_running) {
        auto now = std::chrono::steady_clock::now();
        
        // Game tick (60Hz)
        auto gameDelta = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - m_lastGameTick).count();
        if (gameDelta >= m_gameTickInterval) {
            GameTick();
            m_lastGameTick = now;
        }
        
        // Network tick (20Hz)
        auto networkDelta = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - m_lastNetworkTick).count();
        if (networkDelta >= m_networkTickInterval) {
            NetworkTick();
            m_lastNetworkTick = now;
        }
        
        // Small sleep to prevent spinning
        std::this_thread::sleep_for(std::chrono::microseconds(500));
    }
    
    printf("[Server] Main loop ended\n");
}

void GameServer::Shutdown() {
    if (!m_running) return;
    
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
    if (m_world) {
        // TODO: delete m_world
        m_world = nullptr;
    }
    
    printf("[Server] Shutdown complete\n");
}

bool GameServer::IsRunning() const {
    return m_running;
}

int GameServer::GetPlayerCount() const {
    return static_cast<int>(m_players.size());
}

void GameServer::GameTick() {
    // Update world simulation
    // TODO: m_world->Update();
    
    // Update NPC agents
    UpdateNPCs();
    
    // Process mission completions
    ProcessMissions();
    
    m_tickNumber++;
}

void GameServer::NetworkTick() {
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
    
    // Check for timeouts
    CheckTimeouts();
}

void GameServer::AcceptConnections() {
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
    
    printf("[Server] New connection: Player %u from %s\n", 
           player.playerId, player.socket.GetRemoteIP().c_str());
    
    m_players.push_back(std::move(player));
}

void GameServer::ProcessIncoming(PlayerConnection& player) {
    if (!player.socket.IsValid()) return;
    
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
            printf("[Server] Unknown packet type 0x%02X from player %u\n", 
                   header->type, player.playerId);
            break;
    }
}

void GameServer::SendWorldDelta(PlayerConnection& player) {
    // TODO: Build delta packet with changed world state
    // For now, send keepalive
    
    Net::PacketHeader header;
    header.type = static_cast<uint8_t>(Net::PacketType::KEEPALIVE);
    header.flags = 0;
    header.length = 0;
    
    player.socket.Send(&header, sizeof(header));
}

void GameServer::BroadcastMessage(const void* data, size_t length) {
    for (auto& player : m_players) {
        if (player.authenticated && player.socket.IsValid()) {
            player.socket.Send(data, length);
        }
    }
}

void GameServer::HandleHandshake(PlayerConnection& player, const uint8_t* data, size_t length) {
    if (length < sizeof(Net::HandshakePacket)) {
        DisconnectPlayer(player, "Invalid handshake");
        return;
    }
    
    const Net::HandshakePacket* handshake = 
        reinterpret_cast<const Net::HandshakePacket*>(data);
    
    // Check protocol version
    if (handshake->protocolVersion != Net::PROTOCOL_VERSION) {
        printf("[Server] Player %u has wrong protocol version (%u vs %u)\n",
               player.playerId, handshake->protocolVersion, Net::PROTOCOL_VERSION);
        DisconnectPlayer(player, "Protocol version mismatch");
        return;
    }
    
    // Store player handle
    player.handle = std::string(handshake->handle, 
        strnlen(handshake->handle, sizeof(handshake->handle)));
    
    printf("[Server] Player %u authenticated as '%s'\n", 
           player.playerId, player.handle.c_str());
    
    player.authenticated = true;
    
    // TODO: Create or load agent for this player
    // player.agent = CreatePlayerAgent(player.handle);
    
    // Send handshake response with player ID
    // TODO: Send initial world state
}

void GameServer::HandlePlayerAction(PlayerConnection& player, const uint8_t* data, size_t length) {
    if (!player.authenticated) {
        return;
    }
    
    if (length < sizeof(Net::ActionPacket)) {
        return;
    }
    
    const Net::ActionPacket* action = reinterpret_cast<const Net::ActionPacket*>(data);
    
    // TODO: Validate and apply action to world
    // This is where server-authoritative logic prevents cheating
    
    printf("[Server] Player %u action: type=%u target=%u\n",
           player.playerId, static_cast<uint8_t>(action->actionType), action->targetId);
}

void GameServer::HandleChat(PlayerConnection& player, const uint8_t* data, size_t length) {
    if (!player.authenticated) {
        return;
    }
    
    // TODO: Parse chat message and broadcast to all players
    printf("[Server] Chat from player %u\n", player.playerId);
}

PlayerConnection* GameServer::FindPlayer(uint32_t playerId) {
    for (auto& player : m_players) {
        if (player.playerId == playerId) {
            return &player;
        }
    }
    return nullptr;
}

void GameServer::DisconnectPlayer(PlayerConnection& player, const char* reason) {
    printf("[Server] Disconnecting player %u: %s\n", player.playerId, reason);
    
    // Save player state to Supabase
    if (player.authenticated) {
        // TODO: Get actual stats from agent
        Net::PlayerProfile profile;
        profile.handle = player.handle;
        // profile.credits = player.agent->GetBalance(); // Stub
        
        // Net::SupabaseClient::Instance().UpdatePlayerProfile(profile);
        printf("[Server] Would save player state for %s\n", player.handle.c_str());
    }

    player.socket.Close();
    player.authenticated = false;
    
    // Remove from list
    m_players.erase(
        std::remove_if(m_players.begin(), m_players.end(),
            [&player](const PlayerConnection& p) { 
                return p.playerId == player.playerId; 
            }),
        m_players.end());
}

void GameServer::CheckTimeouts() {
    auto now = std::chrono::steady_clock::now();
    
    for (auto& player : m_players) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - player.lastActivity).count();
        
        if (elapsed > m_config.connectionTimeoutMs) {
            DisconnectPlayer(player, "Connection timeout");
        }
    }
}

void GameServer::CreateWorld() {
    printf("[Server] Creating world...\n");
    
    // TODO: Use existing WorldGenerator
    // m_world = new World();
    // WorldGenerator::GenerateAll();
    
    printf("[Server] World created\n");
}

void GameServer::UpdateNPCs() {
    // TODO: Iterate through all NPC agents and run their AI
    // This uses the existing Agent::AttemptMission() logic
}

void GameServer::ProcessMissions() {
    // TODO: Check for mission completions, update ratings, etc.
}

// ============================================================================
// Server Entry Point
// ============================================================================

int ServerMain(int argc, char* argv[]) {
    printf("Cybrelink Dedicated Server\n");
    printf("==========================\n\n");
    
    ServerConfig config;
    
    // Default Supabase config (Cybrelink project)
    config.supabaseUrl = "https://lszlgjxdygugmvylkxta.supabase.co";
    config.supabaseKey = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6ImxzemxnanhkeWd1Z212eWxreHRhIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NjU1MDkwNDAsImV4cCI6MjA4MTA4NTA0MH0.oV0AiRm3vn_IkclBiHOcVUXAFD84st9fCS0cuASesd8";

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
int main(int argc, char* argv[]) {
    return Server::ServerMain(argc, argv);
}
