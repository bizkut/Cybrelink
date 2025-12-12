/*
 * Supabase Client Test
 * Tests SignUp functionality from C++
 */

#include "network/supabase_client.h"
#include <cstdio>
#include <cstdlib>
#include <ctime>

int main() {
    printf("Cybrelink Supabase Client Test\n");
    printf("==============================\n\n");
    
    // Initialize with Cybrelink project credentials
    Net::SupabaseClient::Instance().Init(
        "https://lszlgjxdygugmvylkxta.supabase.co",
        "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6ImxzemxnanhkeWd1Z212eWxreHRhIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NjU1MDkwNDAsImV4cCI6MjA4MTA4NTA0MH0.oV0AiRm3vn_IkclBiHOcVUXAFD84st9fCS0cuASesd8"
    );
    
    // Generate a unique test email
    srand(static_cast<unsigned>(time(nullptr)));
    char email[128];
    snprintf(email, sizeof(email), "cybrelink_cpp_test_%d@gmail.com", rand() % 100000);
    
    printf("Testing SignUp with: %s\n", email);
    printf("Password: TestPass123!\n\n");
    
    // Attempt signup
    std::string authId = Net::SupabaseClient::Instance().SignUp(
        email,
        "TestPass123!",
        "CPPTestAgent"
    );
    
    if (!authId.empty()) {
        printf("\n*** SUCCESS ***\n");
        printf("User created with auth_id: %s\n", authId.c_str());
        return 0;
    } else {
        printf("\n*** FAILED ***\n");
        printf("SignUp returned empty auth_id\n");
        return 1;
    }
}
