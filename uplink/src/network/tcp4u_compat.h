#pragma once

/*
 * Cybrelink TCP4U Compatibility Layer
 * Stubs for legacy tcp4u/smtp4u APIs until full migration to SDL_net
 * 
 * The old Status Monitor feature used tcp4u. This header provides
 * stub implementations so the code compiles. Will be removed when
 * multiplayer implementation replaces this legacy code.
 */

#include <cstdint>

// Result codes
#define TCP4U_SUCCESS       0
#define TCP4U_ERROR        -1
#define TCP4U_SOCKETCLOSED -2
#define TCP4U_OVERFLOW     -3
#define TCP4U_TIMEOUT      -4

// HFILE_ERROR placeholder (int, not pointer)
#ifndef HFILE_ERROR
#define HFILE_ERROR (-1)
#endif

// Stub function implementations
inline int TcpSend(void* socket, const char* data, unsigned int len, bool flag, int hfile) {
    (void)socket;
    (void)data;
    (void)len;
    (void)flag;
    (void)hfile;
    // Status monitor not implemented in new network layer
    return TCP4U_SUCCESS;
}

inline int TcpRecv(void* socket, char* buffer, unsigned int* len, int timeout, int hfile) {
    (void)socket;
    (void)buffer;
    (void)timeout;
    (void)hfile;
    if (len) *len = 0;
    return TCP4U_ERROR; // No data
}

inline int TcpRecvUntilStr(void* socket, char* buffer, unsigned int* len, 
                           const char* terminator, int maxRead, bool flag, int timeout, int hfile) {
    (void)socket;
    (void)buffer;
    (void)terminator;
    (void)maxRead;
    (void)flag;
    (void)timeout;
    (void)hfile;
    if (len) *len = 0;
    return TCP4U_ERROR;
}

// TcpConnect - connects to a remote host
template<typename SockT>
inline int TcpConnect(SockT* outSocket, const char* host, const char* service, unsigned short* port) {
    (void)host;
    (void)service;
    (void)port;
    if (outSocket) *outSocket = nullptr;
    return TCP4U_ERROR; // Legacy connect not implemented
}

// TcpGetListenSocket - creates a listening socket
// Old signature: int TcpGetListenSocket(SOCKET*, char*, unsigned short*, int)
template<typename SockT>
inline int TcpGetListenSocket(SockT* outSocket, const char* host, unsigned short* port, int backlog) {
    (void)host;
    (void)port;
    (void)backlog;
    if (outSocket) *outSocket = nullptr;
    return TCP4U_ERROR; // Legacy listen not implemented
}

// TcpAccept - accepts an incoming connection
template<typename SockT, typename ListenT>
inline int TcpAccept(SockT* outSocket, ListenT listenSocket, int timeout) {
    (void)listenSocket;
    (void)timeout;
    if (outSocket) *outSocket = nullptr;
    return TCP4U_ERROR; // No connection
}

// TcpClose - closes a socket
template<typename SockT>
inline int TcpClose(SockT* socket) {
    if (socket) *socket = nullptr;
    return TCP4U_SUCCESS;
}

inline int Tcp4uInit() {
    return TCP4U_SUCCESS;
}

inline void Tcp4uCleanup() {
}

inline int TcpGetLocalID(char* host, int hostLen, uint32_t* ip) {
    (void)hostLen;
    if (host) host[0] = '\0';
    if (ip) *ip = 0x7F000001; // 127.0.0.1
    return TCP4U_SUCCESS;
}

// TcpGetRemoteID - gets remote host info
inline int TcpGetRemoteID(void* socket, char* host, int hostLen, unsigned long* ip) {
    (void)socket;
    (void)hostLen;
    if (host) host[0] = '\0';
    if (ip) *ip = 0;
    return TCP4U_ERROR;
}
