/*
 * Cybrelink Network SDL Wrapper Implementation
 */

#include "network_sdl.h"

#include <cstring>

namespace Net {

// ============================================================================
// Socket Implementation
// ============================================================================

Socket::Socket() : m_socket(nullptr) {}

Socket::~Socket() {
    Close();
}

Socket::Socket(Socket&& other) noexcept : m_socket(other.m_socket) {
    other.m_socket = nullptr;
}

Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        Close();
        m_socket = other.m_socket;
        other.m_socket = nullptr;
    }
    return *this;
}

bool Socket::IsValid() const {
    return m_socket != nullptr;
}

void Socket::Close() {
    if (m_socket) {
        SDLNet_TCP_Close(m_socket);
        m_socket = nullptr;
    }
}

std::string Socket::GetRemoteIP() const {
    if (!m_socket) return "";
    
    IPaddress* addr = SDLNet_TCP_GetPeerAddress(m_socket);
    if (!addr) return "";
    
    uint32_t ip = SDL_SwapBE32(addr->host);
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%u.%u.%u.%u",
             (ip >> 24) & 0xFF,
             (ip >> 16) & 0xFF,
             (ip >> 8) & 0xFF,
             ip & 0xFF);
    return buffer;
}

uint16_t Socket::GetRemotePort() const {
    if (!m_socket) return 0;
    
    IPaddress* addr = SDLNet_TCP_GetPeerAddress(m_socket);
    if (!addr) return 0;
    
    return SDL_SwapBE16(addr->port);
}

NetResult Socket::Send(const void* data, size_t length) {
    if (!m_socket) return NetResult::ERR_DISCONNECTED;
    if (!data || length == 0) return NetResult::OK;
    
    int sent = SDLNet_TCP_Send(m_socket, data, static_cast<int>(length));
    if (sent < static_cast<int>(length)) {
        return NetResult::ERR_SEND_FAILED;
    }
    return NetResult::OK;
}

int Socket::Recv(void* buffer, size_t maxLength, uint32_t timeoutMs) {
    if (!m_socket) return -1;
    if (!buffer || maxLength == 0) return 0;
    
    // Check if data is available
    if (timeoutMs == 0) {
        SDLNet_SocketSet set = SDLNet_AllocSocketSet(1);
        if (!set) return -1;
        
        SDLNet_TCP_AddSocket(set, m_socket);
        int ready = SDLNet_CheckSockets(set, 0);
        bool hasData = (ready > 0) && SDLNet_SocketReady(m_socket);
        SDLNet_FreeSocketSet(set);
        
        if (!hasData) return 0;
    }
    
    int received = SDLNet_TCP_Recv(m_socket, buffer, static_cast<int>(maxLength));
    if (received <= 0) {
        return -1; // Error or disconnected
    }
    return received;
}

bool Socket::HasData(uint32_t timeoutMs) {
    if (!m_socket) return false;
    
    SDLNet_SocketSet set = SDLNet_AllocSocketSet(1);
    if (!set) return false;
    
    SDLNet_TCP_AddSocket(set, m_socket);
    int ready = SDLNet_CheckSockets(set, timeoutMs);
    bool hasData = (ready > 0) && SDLNet_SocketReady(m_socket);
    SDLNet_FreeSocketSet(set);
    
    return hasData;
}

// ============================================================================
// NetworkManager Implementation
// ============================================================================

NetworkManager::NetworkManager()
    : m_initialized(false)
    , m_listenSocket(nullptr)
    , m_socketSet(nullptr)
{
}

NetworkManager::~NetworkManager() {
    Shutdown();
}

NetworkManager& NetworkManager::Instance() {
    static NetworkManager instance;
    return instance;
}

NetResult NetworkManager::Init() {
    if (m_initialized) return NetResult::OK;
    
    if (SDLNet_Init() < 0) {
        return NetResult::ERR_INIT_FAILED;
    }
    
    m_socketSet = SDLNet_AllocSocketSet(NET_MAX_PLAYERS + 1);
    if (!m_socketSet) {
        SDLNet_Quit();
        return NetResult::ERR_INIT_FAILED;
    }
    
    m_initialized = true;
    return NetResult::OK;
}

void NetworkManager::Shutdown() {
    if (!m_initialized) return;
    
    StopListening();
    
    if (m_socketSet) {
        SDLNet_FreeSocketSet(m_socketSet);
        m_socketSet = nullptr;
    }
    
    SDLNet_Quit();
    m_initialized = false;
}

bool NetworkManager::IsInitialized() const {
    return m_initialized;
}

NetResult NetworkManager::Listen(uint16_t port) {
    if (!m_initialized) return NetResult::ERR_INIT_FAILED;
    if (m_listenSocket) return NetResult::OK; // Already listening
    
    IPaddress ip;
    if (SDLNet_ResolveHost(&ip, nullptr, port) < 0) {
        return NetResult::ERR_BIND_FAILED;
    }
    
    m_listenSocket = SDLNet_TCP_Open(&ip);
    if (!m_listenSocket) {
        return NetResult::ERR_BIND_FAILED;
    }
    
    SDLNet_TCP_AddSocket(m_socketSet, m_listenSocket);
    return NetResult::OK;
}

void NetworkManager::StopListening() {
    if (m_listenSocket) {
        if (m_socketSet) {
            SDLNet_TCP_DelSocket(m_socketSet, m_listenSocket);
        }
        SDLNet_TCP_Close(m_listenSocket);
        m_listenSocket = nullptr;
    }
}

Socket* NetworkManager::Accept() {
    if (!m_listenSocket) return nullptr;
    
    // Check if there's a pending connection
    int ready = SDLNet_CheckSockets(m_socketSet, 0);
    if (ready <= 0 || !SDLNet_SocketReady(m_listenSocket)) {
        return nullptr;
    }
    
    TCPsocket clientSocket = SDLNet_TCP_Accept(m_listenSocket);
    if (!clientSocket) {
        return nullptr;
    }
    
    Socket* socket = new Socket();
    socket->m_socket = clientSocket;
    return socket;
}

NetResult NetworkManager::Connect(const std::string& host, uint16_t port, Socket& outSocket) {
    if (!m_initialized) return NetResult::ERR_INIT_FAILED;
    
    IPaddress ip;
    if (SDLNet_ResolveHost(&ip, host.c_str(), port) < 0) {
        return NetResult::ERR_RESOLVE_FAILED;
    }
    
    TCPsocket sock = SDLNet_TCP_Open(&ip);
    if (!sock) {
        return NetResult::ERR_CONNECT_FAILED;
    }
    
    outSocket.Close();
    outSocket.m_socket = sock;
    return NetResult::OK;
}

std::string NetworkManager::GetLocalIP() const {
    // SDL_net doesn't provide a direct way to get local IP
    // Return placeholder - actual implementation would use platform APIs
    return "127.0.0.1";
}

bool NetworkManager::ResolveHost(const std::string& host, uint16_t port, IPaddress& outAddr) {
    return SDLNet_ResolveHost(&outAddr, host.c_str(), port) == 0;
}

} // namespace Net
