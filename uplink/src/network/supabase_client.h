#pragma once

#include <string>
#include <functional>
#include <cstdint>
#include <vector>
#include <optional>

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

// World persistence structures
struct Computer {
	int32_t id;
	int64_t ip;
	std::string name;
	int32_t company_id;
	int16_t computer_type;
	int16_t security_level;
	bool is_running;
	// state_data is bytea, handled separately if needed
};

struct Mission {
	int32_t id;
	int16_t mission_type;
	int64_t target_ip;
	int32_t employer_id;
	std::string description;
	int32_t payment;
	int32_t max_payment;
	int16_t difficulty;
	int16_t min_rating;
	int32_t claimed_by; // player_id or 0 if unclaimed
	bool completed;
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

	// World Persistence - Computers
	std::vector<Computer> GetAllComputers();
	bool UpdateComputer(const Computer& computer);

	// World Persistence - Missions
	std::vector<Mission> GetAllMissions();
	std::vector<Mission> GetUnclaimedMissions();
	bool UpdateMission(const Mission& mission);
	bool ClaimMission(int32_t missionId, int32_t playerId);

	// Set the auth token for subsequent requests
	void SetAuthToken(const std::string& token) { m_authToken = token; }
	const std::string& GetAuthToken() const { return m_authToken; }

	// Verify a JWT token and return the user's auth_id (UUID) if valid
	// Returns empty string if token is invalid/expired
	std::string VerifyToken(const std::string& token);

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
