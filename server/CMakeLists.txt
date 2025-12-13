# Cybrelink Dedicated Server
# Headless server executable - no graphics, console only

add_executable(uplink-server)

# Server source files
target_sources(uplink-server PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/gameserver.cpp
    ${CMAKE_CURRENT_LIST_DIR}/gameserver.h
    ${CMAKE_CURRENT_LIST_DIR}/server_date.cpp
    ${CMAKE_CURRENT_LIST_DIR}/server_date.h
    ${CMAKE_CURRENT_LIST_DIR}/server_world.cpp
    ${CMAKE_CURRENT_LIST_DIR}/server_world.h
    # Network layer (shared with client)
    ${CMAKE_SOURCE_DIR}/uplink/src/network/network_sdl.cpp
    ${CMAKE_SOURCE_DIR}/uplink/src/network/network_sdl.h
    ${CMAKE_SOURCE_DIR}/uplink/src/network/protocol.h
    ${CMAKE_SOURCE_DIR}/uplink/src/network/supabase_client.cpp
    ${CMAKE_SOURCE_DIR}/uplink/src/network/supabase_client.h
)

# Include paths - need access to uplink source for network/protocol headers
target_include_directories(uplink-server PRIVATE
    ${CMAKE_SOURCE_DIR}/uplink/src
    ${CMAKE_SOURCE_DIR}/lib/tosser/include
)

target_link_libraries(uplink-server PRIVATE tosser)

# Server-specific definitions
target_compile_definitions(uplink-server PRIVATE
    UPLINK_SERVER
    ENABLE_NETWORK
)

# Find required packages
find_package(SDL2 CONFIG REQUIRED)
find_package(SDL2_net CONFIG REQUIRED)
find_package(zstd CONFIG REQUIRED)
find_package(cpr CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# Link libraries - SDL2_net requires SDL2 for memory functions
target_link_libraries(uplink-server PRIVATE
    $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
    $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
    $<IF:$<TARGET_EXISTS:SDL2_net::SDL2_net>,SDL2_net::SDL2_net,SDL2_net::SDL2_net-static>
    $<IF:$<TARGET_EXISTS:zstd::libzstd_shared>,zstd::libzstd_shared,zstd::libzstd_static>
    cpr::cpr
    nlohmann_json::nlohmann_json
)

# Platform-specific settings
if(WIN32)
    # Console application on Windows
    set_target_properties(uplink-server PROPERTIES
        LINK_FLAGS "/SUBSYSTEM:CONSOLE"
    )
    target_link_libraries(uplink-server PRIVATE ws2_32 iphlpapi)
endif()

# Output to same bin directory as game
set_target_properties(uplink-server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/uplink/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/uplink/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/uplink/bin"
)

# ============================================================================
# Supabase Test Executable
# ============================================================================

add_executable(test-supabase)

target_sources(test-supabase PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/test_supabase.cpp
    ${CMAKE_SOURCE_DIR}/uplink/src/network/supabase_client.cpp
)

target_include_directories(test-supabase PRIVATE
    ${CMAKE_SOURCE_DIR}/uplink/src
)

target_link_libraries(test-supabase PRIVATE
    cpr::cpr
    nlohmann_json::nlohmann_json
)

set_target_properties(test-supabase PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/uplink/bin"
)
