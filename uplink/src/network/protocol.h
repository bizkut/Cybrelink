#pragma once

/*
 * Cybrelink Network Protocol
 * Binary protocol for efficient multiplayer communication
 */

#include <cstdint>
#include <cstring>

namespace Net {

// ============================================================================
// Packet Header (4 bytes)
// ============================================================================

#pragma pack(push, 1)
struct PacketHeader {
    uint8_t  type;      // PacketType enum
    uint8_t  flags;     // PacketFlags bitmask
    uint16_t length;    // Payload length (max 65535 bytes)
};
#pragma pack(pop)

static_assert(sizeof(PacketHeader) == 4, "PacketHeader must be 4 bytes");

// ============================================================================
// Packet Types
// ============================================================================

enum class PacketType : uint8_t {
    // Connection
    HANDSHAKE       = 0x01,
    HANDSHAKE_ACK   = 0x02,
    DISCONNECT      = 0x03,
    KEEPALIVE       = 0x04,
    
    // Authentication
    AUTH_REQUEST    = 0x10,
    AUTH_RESPONSE   = 0x11,
    
    // Player Actions (Client -> Server)
    PLAYER_CONNECT      = 0x20,  // Connect to an IP in-game
    PLAYER_DISCONNECT   = 0x21,  // Disconnect from current server
    PLAYER_ACTION       = 0x22,  // Generic action (hack, transfer, etc.)
    PLAYER_CHAT         = 0x23,
    
    // World State (Server -> Client)
    WORLD_FULL      = 0x30,  // Full world snapshot (on join)
    WORLD_DELTA     = 0x31,  // Delta update
    
    // Agent Updates
    AGENT_UPDATE    = 0x40,  // Agent state change
    TRACE_UPDATE    = 0x41,  // Trace progress
    MISSION_UPDATE  = 0x42,  // Mission taken/completed
    
    // Logging/Debug
    LOG_ENTRY       = 0xF0,
    NET_ERROR       = 0xFE,
    
    // Reserved
    MAX_TYPE        = 0xFF
};

// ============================================================================
// Packet Flags
// ============================================================================

enum PacketFlags : uint8_t {
    FLAG_NONE           = 0x00,
    FLAG_COMPRESSED     = 0x01,  // Payload is zstd compressed
    FLAG_RELIABLE       = 0x02,  // Requires acknowledgment
    FLAG_FRAGMENTED     = 0x04,  // Part of larger message
    FLAG_LAST_FRAGMENT  = 0x08,  // Last fragment of message
};

// ============================================================================
// Player Actions (for PLAYER_ACTION packet)
// ============================================================================

enum class ActionType : uint8_t {
    NONE            = 0x00,
    
    // Connection actions
    ADD_BOUNCE      = 0x10,  // Add IP to bounce path
    CLEAR_BOUNCES   = 0x11,
    CONNECT_TARGET  = 0x12,
    DISCONNECT_ALL  = 0x13,
    
    // Hacking actions
    RUN_SOFTWARE    = 0x20,
    BYPASS_SECURITY = 0x21,
    
    // File actions
    DOWNLOAD_FILE   = 0x30,
    UPLOAD_FILE     = 0x31,
    DELETE_FILE     = 0x32,
    COPY_FILE       = 0x33,
    
    // Log actions
    DELETE_LOG      = 0x40,
    MODIFY_LOG      = 0x41,
    
    // Bank actions
    TRANSFER_MONEY  = 0x50,
    
    // Admin actions
    SHUTDOWN_SYSTEM = 0x60,
    
    // PVP actions
    FRAME_PLAYER    = 0x70,
    PLACE_BOUNTY    = 0x71,
};

// ============================================================================
// Packet Builders
// ============================================================================

// Helper to write a packet to a buffer
// Returns total bytes written (header + payload)
inline size_t WritePacket(uint8_t* buffer, PacketType type, uint8_t flags, 
                          const void* payload, uint16_t payloadLen) {
    PacketHeader header;
    header.type = static_cast<uint8_t>(type);
    header.flags = flags;
    header.length = payloadLen;
    
    memcpy(buffer, &header, sizeof(header));
    if (payload && payloadLen > 0) {
        memcpy(buffer + sizeof(header), payload, payloadLen);
    }
    return sizeof(header) + payloadLen;
}

// Helper to read a packet header from buffer
inline bool ReadPacketHeader(const uint8_t* buffer, size_t bufferLen, PacketHeader& out) {
    if (bufferLen < sizeof(PacketHeader)) {
        return false;
    }
    memcpy(&out, buffer, sizeof(PacketHeader));
    return true;
}

// Get payload pointer from packet buffer
inline const uint8_t* GetPayload(const uint8_t* packet) {
    return packet + sizeof(PacketHeader);
}

// ============================================================================
// Common Packet Structures
// ============================================================================

#pragma pack(push, 1)

struct HandshakePacket {
    uint32_t protocolVersion;
    uint32_t clientVersion;
    char handle[32];
    char authToken[512]; // Supabase JWT can be long
};

struct ActionPacket {
    ActionType actionType;
    uint32_t targetId;
    uint32_t param1;
    uint32_t param2;
    char data[64];  // Variable data depending on action
};

#pragma pack(pop)

// Note: Network constants (DEFAULT_PORT, PROTOCOL_VERSION, TICK_RATE_HZ, etc.)
// are defined in network_sdl.h which should be included before this header

} // namespace Net

