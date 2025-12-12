#pragma once

/*
 * Cybrelink Network SDL Wrapper
 * Cross-platform networking using SDL_net
 */

#include <cstdint>
#include <string>
#include <vector>

#include <SDL_net.h>

namespace Net {

// ============================================================================
// Constants
// ============================================================================

constexpr uint16_t DEFAULT_PORT = 31337;
constexpr size_t NET_MAX_PLAYERS = 32;
constexpr uint32_t PROTOCOL_VERSION = 1;

// Default server address - change this to your production server IP/hostname
constexpr const char* DEFAULT_SERVER_HOST = "localhost";

// Network tick rates
constexpr int TICK_RATE_HZ = 60; // Game logic tick rate
constexpr int NETWORK_TICK_HZ = 20; // Network send rate
constexpr int KEEPALIVE_INTERVAL_MS = 5000;
constexpr int CONNECTION_TIMEOUT_MS = 15000;

// ============================================================================
// Network Result Codes
// ============================================================================

enum class NetResult {
	OK = 0,
	ERR_INIT_FAILED,
	ERR_RESOLVE_FAILED,
	ERR_CONNECT_FAILED,
	ERR_BIND_FAILED,
	ERR_ACCEPT_FAILED,
	ERR_SEND_FAILED,
	ERR_RECV_FAILED,
	ERR_TIMEOUT,
	ERR_DISCONNECTED,
	ERR_WOULD_BLOCK,
};

// ============================================================================
// Socket Wrapper
// ============================================================================

class Socket {
public:
	Socket();
	~Socket();

	// No copy
	Socket(const Socket&) = delete;
	Socket& operator=(const Socket&) = delete;

	// Move
	Socket(Socket&& other) noexcept;
	Socket& operator=(Socket&& other) noexcept;

	// Check if socket is valid
	bool IsValid() const;

	// Close the socket
	void Close();

	// Get remote IP as string
	std::string GetRemoteIP() const;

	// Get remote port
	uint16_t GetRemotePort() const;

	// Send data (blocking)
	NetResult Send(const void* data, size_t length);

	// Receive data (non-blocking if timeout is 0)
	// Returns bytes received, or 0 if no data, or -1 on error
	int Recv(void* buffer, size_t maxLength, uint32_t timeoutMs = 0);

	// Check if data is available to read
	bool HasData(uint32_t timeoutMs = 0);

private:
	friend class NetworkManager;
	TCPsocket m_socket;
};

// ============================================================================
// Network Manager (Singleton)
// ============================================================================

class NetworkManager {
public:
	static NetworkManager& Instance();

	// Initialize SDL_net
	NetResult Init();

	// Shutdown SDL_net
	void Shutdown();

	// Check if initialized
	bool IsInitialized() const;

	// ---- Server Functions ----

	// Start listening on a port
	NetResult Listen(uint16_t port);

	// Stop listening
	void StopListening();

	// Accept a pending connection (non-blocking)
	// Returns nullptr if no pending connection
	Socket* Accept();

	// ---- Client Functions ----

	// Connect to a server
	NetResult Connect(const std::string& host, uint16_t port, Socket& outSocket);

	// ---- Utility ----

	// Get local IP address
	std::string GetLocalIP() const;

	// Resolve hostname to IP
	bool ResolveHost(const std::string& host, uint16_t port, IPaddress& outAddr);

private:
	NetworkManager();
	~NetworkManager();

	// No copy
	NetworkManager(const NetworkManager&) = delete;
	NetworkManager& operator=(const NetworkManager&) = delete;

	bool m_initialized;
	TCPsocket m_listenSocket;
	SDLNet_SocketSet m_socketSet;
};

// ============================================================================
// Convenience functions
// ============================================================================

inline NetResult NetInit() { return NetworkManager::Instance().Init(); }

inline void NetShutdown() { NetworkManager::Instance().Shutdown(); }

} // namespace Net
