#pragma once

/*
 * Cybrelink Delta Encoder
 * Efficient delta compression for world state synchronization
 */

#include <cstdint>
#include <cstring>
#include <vector>
#include <string>

namespace Net {

// ============================================================================
// Variable-length Integer Encoding (Varint)
// ============================================================================

// Encode a uint32 as varint, returns bytes written
inline size_t EncodeVarint(uint8_t* buffer, uint32_t value) {
    size_t i = 0;
    while (value >= 0x80) {
        buffer[i++] = static_cast<uint8_t>((value & 0x7F) | 0x80);
        value >>= 7;
    }
    buffer[i++] = static_cast<uint8_t>(value);
    return i;
}

// Decode a varint, returns bytes consumed (0 on error)
inline size_t DecodeVarint(const uint8_t* buffer, size_t maxLen, uint32_t& outValue) {
    outValue = 0;
    size_t i = 0;
    int shift = 0;
    
    while (i < maxLen && i < 5) {
        uint8_t byte = buffer[i++];
        outValue |= (static_cast<uint32_t>(byte & 0x7F) << shift);
        if ((byte & 0x80) == 0) {
            return i;
        }
        shift += 7;
    }
    return 0; // Error: incomplete or too long
}

// ============================================================================
// String Encoding
// ============================================================================

// Encode a string (length-prefixed)
inline size_t EncodeString(uint8_t* buffer, const std::string& str) {
    size_t offset = EncodeVarint(buffer, static_cast<uint32_t>(str.length()));
    memcpy(buffer + offset, str.data(), str.length());
    return offset + str.length();
}

// Decode a string
inline size_t DecodeString(const uint8_t* buffer, size_t maxLen, std::string& outStr) {
    uint32_t len;
    size_t offset = DecodeVarint(buffer, maxLen, len);
    if (offset == 0 || offset + len > maxLen) return 0;
    
    outStr.assign(reinterpret_cast<const char*>(buffer + offset), len);
    return offset + len;
}

// ============================================================================
// Delta Buffer
// ============================================================================

class DeltaBuffer {
public:
    DeltaBuffer(size_t initialCapacity = 1024) {
        m_buffer.reserve(initialCapacity);
    }
    
    void Clear() { m_buffer.clear(); }
    
    const uint8_t* Data() const { return m_buffer.data(); }
    size_t Size() const { return m_buffer.size(); }
    
    // ---- Writing ----
    
    void WriteU8(uint8_t value) {
        m_buffer.push_back(value);
    }
    
    void WriteU16(uint16_t value) {
        m_buffer.push_back(static_cast<uint8_t>(value & 0xFF));
        m_buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    }
    
    void WriteU32(uint32_t value) {
        m_buffer.push_back(static_cast<uint8_t>(value & 0xFF));
        m_buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
        m_buffer.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
        m_buffer.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    }
    
    void WriteVarint(uint32_t value) {
        uint8_t temp[5];
        size_t len = EncodeVarint(temp, value);
        m_buffer.insert(m_buffer.end(), temp, temp + len);
    }
    
    void WriteString(const std::string& str) {
        WriteVarint(static_cast<uint32_t>(str.length()));
        m_buffer.insert(m_buffer.end(), str.begin(), str.end());
    }
    
    void WriteRaw(const void* data, size_t len) {
        const uint8_t* bytes = static_cast<const uint8_t*>(data);
        m_buffer.insert(m_buffer.end(), bytes, bytes + len);
    }
    
    // ---- Field Markers ----
    
    // Write a field marker (field_id + type)
    void WriteFieldStart(uint8_t fieldId, uint8_t fieldType) {
        WriteU8((fieldId << 3) | (fieldType & 0x07));
    }
    
private:
    std::vector<uint8_t> m_buffer;
};

// ============================================================================
// Delta Reader
// ============================================================================

class DeltaReader {
public:
    DeltaReader(const uint8_t* data, size_t size)
        : m_data(data), m_size(size), m_pos(0) {}
    
    bool HasMore() const { return m_pos < m_size; }
    size_t Remaining() const { return m_size - m_pos; }
    size_t Position() const { return m_pos; }
    
    // ---- Reading ----
    
    bool ReadU8(uint8_t& out) {
        if (m_pos >= m_size) return false;
        out = m_data[m_pos++];
        return true;
    }
    
    bool ReadU16(uint16_t& out) {
        if (m_pos + 2 > m_size) return false;
        out = m_data[m_pos] | (static_cast<uint16_t>(m_data[m_pos + 1]) << 8);
        m_pos += 2;
        return true;
    }
    
    bool ReadU32(uint32_t& out) {
        if (m_pos + 4 > m_size) return false;
        out = m_data[m_pos]
            | (static_cast<uint32_t>(m_data[m_pos + 1]) << 8)
            | (static_cast<uint32_t>(m_data[m_pos + 2]) << 16)
            | (static_cast<uint32_t>(m_data[m_pos + 3]) << 24);
        m_pos += 4;
        return true;
    }
    
    bool ReadVarint(uint32_t& out) {
        size_t consumed = DecodeVarint(m_data + m_pos, m_size - m_pos, out);
        if (consumed == 0) return false;
        m_pos += consumed;
        return true;
    }
    
    bool ReadString(std::string& out) {
        uint32_t len;
        if (!ReadVarint(len)) return false;
        if (m_pos + len > m_size) return false;
        
        out.assign(reinterpret_cast<const char*>(m_data + m_pos), len);
        m_pos += len;
        return true;
    }
    
    bool ReadRaw(void* buffer, size_t len) {
        if (m_pos + len > m_size) return false;
        memcpy(buffer, m_data + m_pos, len);
        m_pos += len;
        return true;
    }
    
    // ---- Field Markers ----
    
    bool ReadFieldStart(uint8_t& fieldId, uint8_t& fieldType) {
        uint8_t marker;
        if (!ReadU8(marker)) return false;
        fieldId = marker >> 3;
        fieldType = marker & 0x07;
        return true;
    }
    
    // Skip bytes
    bool Skip(size_t len) {
        if (m_pos + len > m_size) return false;
        m_pos += len;
        return true;
    }
    
private:
    const uint8_t* m_data;
    size_t m_size;
    size_t m_pos;
};

// ============================================================================
// Field Types (for delta encoding)
// ============================================================================

enum FieldType : uint8_t {
    FIELD_VARINT = 0,   // Variable-length integer
    FIELD_FIXED32 = 1,  // 4 bytes
    FIELD_FIXED64 = 2,  // 8 bytes
    FIELD_STRING = 3,   // Length-prefixed string
    FIELD_BYTES = 4,    // Length-prefixed raw bytes
    FIELD_END = 7,      // End of object marker
};

} // namespace Net
