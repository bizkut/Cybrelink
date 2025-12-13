#include "supabase_client.h"

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <iostream>

using json = nlohmann::json;

namespace Net {

SupabaseClient& SupabaseClient::Instance()
{
	static SupabaseClient instance;
	return instance;
}

void SupabaseClient::Init(const std::string& url, const std::string& anonKey)
{
	m_url = url;
	m_anonKey = anonKey;
}

std::string SupabaseClient::Login(const std::string& email, const std::string& password)
{
	std::string endpoint = m_url + "/auth/v1/token?grant_type=password";

	json payload = { { "email", email }, { "password", password } };

	cpr::Response r =
		cpr::Post(cpr::Url { endpoint },
				  cpr::Header { { "apikey", m_anonKey }, { "Content-Type", "application/json" } },
				  cpr::Body { payload.dump() },
				  cpr::VerifySsl { false },
				  cpr::Timeout { 5000 });

	if (r.status_code == 200) {
		try {
			auto j = json::parse(r.text);
			if (j.contains("access_token")) {
				std::string token = j["access_token"];
				SetAuthToken(token);
				return token;
			}
		} catch (const std::exception& e) {
			std::cerr << "[Supabase] Login JSON parse error: " << e.what() << std::endl;
		}
	} else {
		std::cerr << "[Supabase] Login failed: " << r.status_code << " " << r.text << std::endl;
	}

	return "";
}

std::string
SupabaseClient::SignUp(const std::string& email, const std::string& password, const std::string& handle)
{
	std::string endpoint = m_url + "/auth/v1/signup";

	json payload = { { "email", email }, { "password", password }, { "data", { { "handle", handle } } } };

	printf("[Supabase] SignUp: email=%s handle=%s url=%s\n", email.c_str(), handle.c_str(), m_url.c_str());

	cpr::Response r =
		cpr::Post(cpr::Url { endpoint },
				  cpr::Header { { "apikey", m_anonKey }, { "Content-Type", "application/json" } },
				  cpr::Body { payload.dump() },
				  cpr::VerifySsl { false }, // Disable SSL verification for testing
				  cpr::Timeout { 5000 } // 5 second timeout to prevent app freeze
		);

	if (r.status_code == 200) {
		try {
			auto j = json::parse(r.text);
			if (j.contains("id")) {
				std::string authId = j["id"];

				// Note: Triggers should handle creating the player row,
				// but if not, we might need to do it manually.
				// For now, assume we handle it or call CreatePlayerProfile next.

				return authId;
			}
			// Sometimes it's inside "user" object
			if (j.contains("user") && j["user"].contains("id")) {
				return j["user"]["id"];
			}
		} catch (const std::exception& e) {
			std::cerr << "[Supabase] SignUp JSON parse error: " << e.what() << std::endl;
		}
	} else {
		m_lastError = "Status: " + std::to_string(r.status_code) + " - " + r.text;
		std::cerr << "[Supabase] SignUp failed: " << r.status_code << " " << r.text << std::endl;
	}

	return "";
}

std::optional<PlayerProfile> SupabaseClient::GetPlayerProfile(const std::string& authId)
{
	if (m_authToken.empty()) {
		std::cerr << "[Supabase] Cannot GetPlayerProfile: Not logged in" << std::endl;
		return std::nullopt;
	}

	std::string endpoint = m_url + "/rest/v1/players?auth_id=eq." + authId + "&select=*";

	cpr::Response r = cpr::Get(cpr::Url { endpoint },
							   cpr::Header { { "apikey", m_anonKey },
											 { "Authorization", "Bearer " + m_authToken },
											 { "Content-Type", "application/json" } });

	if (r.status_code == 200) {
		try {
			auto j = json::parse(r.text);
			if (j.is_array() && !j.empty()) {
				auto& p = j[0];
				PlayerProfile profile;
				profile.id = p.value("id", 0);
				profile.auth_id = p.value("auth_id", "");
				profile.handle = p.value("handle", "");
				profile.credits = p.value("credits", 0);
				profile.uplink_rating = p.value("uplink_rating", 0);
				profile.neuromancer_rating = p.value("neuromancer_rating", 0);
				return profile;
			}
		} catch (const std::exception& e) {
			std::cerr << "[Supabase] GetPlayerProfile JSON parse error: " << e.what() << std::endl;
		}
	} else {
		std::cerr << "[Supabase] GetPlayerProfile failed: " << r.status_code << " " << r.text << std::endl;
	}

	return std::nullopt;
}

bool SupabaseClient::CreatePlayerProfile(const std::string& authId, const std::string& handle)
{
	// Should be called if the trigger doesn't exist
	std::string endpoint = m_url + "/rest/v1/players";

	json payload = { { "auth_id", authId },
					 { "handle", handle },
					 { "credits", 3000 },
					 { "uplink_rating", 1 },
					 { "neuromancer_rating", 0 } };

	// Authorization header needed if RLS is enabled and allows insert for authenticated users
	cpr::Header headers = { { "apikey", m_anonKey },
							{ "Content-Type", "application/json" },
							{ "Prefer", "return=minimal" } };

	if (!m_authToken.empty()) {
		headers.insert({ "Authorization", "Bearer " + m_authToken });
	}

	cpr::Response r = cpr::Post(cpr::Url { endpoint }, headers, cpr::Body { payload.dump() });

	if (r.status_code == 201) {
		return true;
	} else {
		std::cerr << "[Supabase] CreatePlayerProfile failed: " << r.status_code << " " << r.text << std::endl;
		return false;
	}
}

bool SupabaseClient::UpdatePlayerProfile(const PlayerProfile& profile)
{
	if (m_authToken.empty() || profile.id == 0) {
		return false;
	}

	std::string endpoint = m_url + "/rest/v1/players?id=eq." + std::to_string(profile.id);

	json payload = { { "credits", profile.credits },
					 { "uplink_rating", profile.uplink_rating },
					 { "neuromancer_rating", profile.neuromancer_rating } };

	cpr::Response r = cpr::Patch(cpr::Url { endpoint },
								 cpr::Header { { "apikey", m_anonKey },
											   { "Authorization", "Bearer " + m_authToken },
											   { "Content-Type", "application/json" } },
								 cpr::Body { payload.dump() });

	if (r.status_code == 200 || r.status_code == 204) {
		return true;
	} else {
		std::cerr << "[Supabase] UpdatePlayerProfile failed: " << r.status_code << " " << r.text << std::endl;
		return false;
	}
}

std::string SupabaseClient::VerifyToken(const std::string& token)
{
	if (token.empty() || m_url.empty()) {
		return "";
	}

	// Use Supabase's /auth/v1/user endpoint to validate the token
	// This endpoint returns user info if the token is valid
	std::string endpoint = m_url + "/auth/v1/user";

	cpr::Response r = cpr::Get(cpr::Url { endpoint },
							   cpr::Header { { "apikey", m_anonKey },
											 { "Authorization", "Bearer " + token },
											 { "Content-Type", "application/json" } },
							   cpr::VerifySsl { false },
							   cpr::Timeout { 5000 });

	if (r.status_code == 200) {
		try {
			auto j = json::parse(r.text);
			if (j.contains("id")) {
				std::string authId = j["id"];
				printf("[Supabase] Token verified for user: %s\n", authId.c_str());
				return authId;
			}
		} catch (const std::exception& e) {
			std::cerr << "[Supabase] VerifyToken JSON parse error: " << e.what() << std::endl;
		}
	} else {
		std::cerr << "[Supabase] VerifyToken failed: " << r.status_code << " " << r.text << std::endl;
	}

	return "";
}

// ============================================================================
// World Persistence - Computers
// ============================================================================

std::vector<Computer> SupabaseClient::GetAllComputers()
{
	std::vector<Computer> computers;
	if (m_url.empty()) {
		return computers;
	}

	std::string endpoint = m_url + "/rest/v1/computers?select=*";

	cpr::Response r = cpr::Get(
		cpr::Url { endpoint },
		cpr::Header { { "apikey", m_anonKey },
					  { "Authorization", "Bearer " + (m_authToken.empty() ? m_anonKey : m_authToken) } },
		cpr::VerifySsl { false },
		cpr::Timeout { 5000 });

	if (r.status_code == 200) {
		try {
			auto j = json::parse(r.text);
			for (const auto& item : j) {
				Computer c;
				c.id = item.value("id", 0);
				c.ip = item.value("ip", 0LL);
				c.name = item.value("name", "");
				c.company_id = item.value("company_id", 0);
				c.computer_type = item.value("computer_type", 0);
				c.security_level = item.value("security_level", 0);
				c.is_running = item.value("is_running", true);
				computers.push_back(c);
			}
			printf("[Supabase] Loaded %zu computers\n", computers.size());
		} catch (const std::exception& e) {
			std::cerr << "[Supabase] GetAllComputers parse error: " << e.what() << std::endl;
		}
	} else {
		std::cerr << "[Supabase] GetAllComputers failed: " << r.status_code << std::endl;
	}

	return computers;
}

bool SupabaseClient::UpdateComputer(const Computer& computer)
{
	if (m_url.empty()) {
		return false;
	}

	std::string endpoint = m_url + "/rest/v1/computers?id=eq." + std::to_string(computer.id);

	json payload = { { "security_level", computer.security_level }, { "is_running", computer.is_running } };

	cpr::Response r = cpr::Patch(cpr::Url { endpoint },
								 cpr::Header { { "apikey", m_anonKey },
											   { "Authorization", "Bearer " + m_authToken },
											   { "Content-Type", "application/json" },
											   { "Prefer", "return=minimal" } },
								 cpr::Body { payload.dump() },
								 cpr::VerifySsl { false },
								 cpr::Timeout { 5000 });

	return r.status_code == 200 || r.status_code == 204;
}

// ============================================================================
// World Persistence - Missions
// ============================================================================

std::vector<Mission> SupabaseClient::GetAllMissions()
{
	std::vector<Mission> missions;
	if (m_url.empty()) {
		return missions;
	}

	std::string endpoint = m_url + "/rest/v1/missions?select=*";

	cpr::Response r = cpr::Get(
		cpr::Url { endpoint },
		cpr::Header { { "apikey", m_anonKey },
					  { "Authorization", "Bearer " + (m_authToken.empty() ? m_anonKey : m_authToken) } },
		cpr::VerifySsl { false },
		cpr::Timeout { 5000 });

	if (r.status_code == 200) {
		try {
			auto j = json::parse(r.text);
			for (const auto& item : j) {
				Mission m;
				m.id = item.value("id", 0);
				m.mission_type = item.value("mission_type", 0);
				m.target_ip = item.value("target_ip", 0LL);
				m.employer_id = item.value("employer_id", 0);
				m.description = item.value("description", "");
				m.payment = item.value("payment", 0);
				m.max_payment = item.value("max_payment", 0);
				m.difficulty = item.value("difficulty", 0);
				m.min_rating = item.value("min_rating", 0);
				m.claimed_by = item.value("claimed_by", 0);
				m.completed = item.value("completed", false);
				missions.push_back(m);
			}
			printf("[Supabase] Loaded %zu missions\n", missions.size());
		} catch (const std::exception& e) {
			std::cerr << "[Supabase] GetAllMissions parse error: " << e.what() << std::endl;
		}
	}

	return missions;
}

std::vector<Mission> SupabaseClient::GetUnclaimedMissions()
{
	std::vector<Mission> missions;
	if (m_url.empty()) {
		return missions;
	}

	std::string endpoint = m_url + "/rest/v1/missions?claimed_by=is.null&completed=eq.false";

	cpr::Response r = cpr::Get(
		cpr::Url { endpoint },
		cpr::Header { { "apikey", m_anonKey },
					  { "Authorization", "Bearer " + (m_authToken.empty() ? m_anonKey : m_authToken) } },
		cpr::VerifySsl { false },
		cpr::Timeout { 5000 });

	if (r.status_code == 200) {
		try {
			auto j = json::parse(r.text);
			for (const auto& item : j) {
				Mission m;
				m.id = item.value("id", 0);
				m.mission_type = item.value("mission_type", 0);
				m.target_ip = item.value("target_ip", 0LL);
				m.description = item.value("description", "");
				m.payment = item.value("payment", 0);
				m.difficulty = item.value("difficulty", 0);
				m.min_rating = item.value("min_rating", 0);
				m.claimed_by = 0;
				m.completed = false;
				missions.push_back(m);
			}
		} catch (...) {
		}
	}

	return missions;
}

bool SupabaseClient::UpdateMission(const Mission& mission)
{
	if (m_url.empty()) {
		return false;
	}

	std::string endpoint = m_url + "/rest/v1/missions?id=eq." + std::to_string(mission.id);

	json payload = { { "completed", mission.completed } };
	if (mission.claimed_by > 0) {
		payload["claimed_by"] = mission.claimed_by;
	}

	cpr::Response r = cpr::Patch(cpr::Url { endpoint },
								 cpr::Header { { "apikey", m_anonKey },
											   { "Authorization", "Bearer " + m_authToken },
											   { "Content-Type", "application/json" },
											   { "Prefer", "return=minimal" } },
								 cpr::Body { payload.dump() },
								 cpr::VerifySsl { false },
								 cpr::Timeout { 5000 });

	return r.status_code == 200 || r.status_code == 204;
}

bool SupabaseClient::ClaimMission(int32_t missionId, int32_t playerId)
{
	if (m_url.empty()) {
		return false;
	}

	std::string endpoint = m_url + "/rest/v1/missions?id=eq." + std::to_string(missionId);

	json payload = { { "claimed_by", playerId } };

	cpr::Response r = cpr::Patch(cpr::Url { endpoint },
								 cpr::Header { { "apikey", m_anonKey },
											   { "Authorization", "Bearer " + m_authToken },
											   { "Content-Type", "application/json" },
											   { "Prefer", "return=minimal" } },
								 cpr::Body { payload.dump() },
								 cpr::VerifySsl { false },
								 cpr::Timeout { 5000 });

	return r.status_code == 200 || r.status_code == 204;
}

} // namespace Net
