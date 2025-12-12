#pragma once

/*
 * Cybrelink Dedicated Game Server
 * Runs headless world simulation with multiple connected players
 */

#include <cstdint>
#include <string>
#include <vector>
#include <atomic>
#include <chrono>

#include "network/network_sdl.h"
#include "network/protocol.h"

// Forward declarations
class World;
class Agent;

namespace Server {

// ============================================================================
// Player Connection
// ============================================================================

struct PlayerConnection {
    uint32_t playerId;
    Net::Socket socket;
    std::string handle;
    Agent* agent;
    
    std::chrono::steady_clock::time_point lastActivity;
    std::chrono::steady_clock::time_point lastNetworkTick;
    
    // Send queue for batching
    std::vector<uint8_t> sendBuffer;
    
    bool authenticated;
    bool ready;
    
    PlayerConnection() 
        : playerId(0)
        , agent(nullptr)
        , authenticated(false)
        , ready(false) 
    {
        lastActivity = std::chrono::steady_clock::now();
        lastNetworkTick = std::chrono::steady_clock::now();
    }
};

// ============================================================================
// Server Configuration
// ============================================================================

struct ServerConfig {
    uint16_t port = Net::DEFAULT_PORT;
    int maxPlayers = 8;
    int tickRateHz = 60;
    int networkTickRateHz = 20;
    int connectionTimeoutMs = 15000;
    std::string worldSeed;
    
    // Supabase (optional)
    std::string supabaseUrl;
    std::string supabaseKey;
};

// ============================================================================
// Game Server
// ============================================================================

class GameServer {
public:
    GameServer();
    ~GameServer();
    
    // Lifecycle
    bool Init(const ServerConfig& config);
    void Run();
    void Shutdown();
    
    // State
    bool IsRunning() const;
    int GetPlayerCount() const;
    
private:
    // Main loops
    void GameTick();      // 60Hz game logic
    void NetworkTick();   // 20Hz network sync
    
    // Networking
    void AcceptConnections();
    void ProcessIncoming(PlayerConnection& player);
    void SendWorldDelta(PlayerConnection& player);
    void BroadcastMessage(const void* data, size_t length);
    
    // Packet handlers
    void HandleHandshake(PlayerConnection& player, const uint8_t* data, size_t length);
    void HandlePlayerAction(PlayerConnection& player, const uint8_t* data, size_t length);
    void HandleChat(PlayerConnection& player, const uint8_t* data, size_t length);
    
    // Player management
    PlayerConnection* FindPlayer(uint32_t playerId);
    void DisconnectPlayer(PlayerConnection& player, const char* reason);
    void CheckTimeouts();
    
    // World management
    void CreateWorld();
    void UpdateNPCs();
    void ProcessMissions();
    
private:
    ServerConfig m_config;
    std::atomic<bool> m_running;
    
    // Timing
    std::chrono::steady_clock::time_point m_lastGameTick;
    std::chrono::steady_clock::time_point m_lastNetworkTick;
    double m_gameTickInterval;
    double m_networkTickInterval;
    
    // Players
    std::vector<PlayerConnection> m_players;
    uint32_t m_nextPlayerId;
    
    // World (using existing Uplink world system)
    World* m_world;
    
    // Network tick counter for delta encoding
    uint32_t m_tickNumber;
};

// ============================================================================
// Server Entry Point
// ============================================================================

int ServerMain(int argc, char* argv[]);

} // namespace Server
