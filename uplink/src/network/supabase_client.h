#pragma once

#include <string>
#include <functional>
#include <cstdint>
#include <vector>
#include <optional>

// Forward declarations
// Forward declarations
// nlohmann::json is a typedef, do not forward declare as class

namespace Net {

struct PlayerProfile {
	int32_t id;
	std::string auth_id;
	std::string handle;
	int32_t credits;
	int16_t uplink_rating;
	int16_t neuromancer_rating;
	// gateway_type, created_at, last_seen omitted for now
};

class SupabaseClient {
public:
	static SupabaseClient& Instance();

	// Initialize with project URL and Anon Key
	void Init(const std::string& url, const std::string& anonKey);

	// Authentication
	// Returns auth_token on success, or empty string on failure
	std::string Login(const std::string& email, const std::string& password);

	// Returns auth_id (UUID) on success, or empty string on failure
	std::string SignUp(const std::string& email, const std::string& password, const std::string& handle);

	// Player Metadata
	std::optional<PlayerProfile> GetPlayerProfile(const std::string& authId);
	bool CreatePlayerProfile(const std::string& authId, const std::string& handle);
	bool UpdatePlayerProfile(const PlayerProfile& profile);

	// Set the auth token for subsequent requests
	void SetAuthToken(const std::string& token) { m_authToken = token; }
	const std::string& GetAuthToken() const { return m_authToken; }

	// Get last error message for debugging
	const std::string& GetLastError() const { return m_lastError; }

private:
	SupabaseClient() = default;

	std::string m_url;
	std::string m_anonKey;
	std::string m_authToken;
	std::string m_lastError;
};

} // namespace Net
