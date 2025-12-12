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
				  cpr::Body { payload.dump() });

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
				  cpr::Timeout { 10000 } // 10 second timeout
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

} // namespace Net
