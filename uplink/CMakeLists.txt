add_executable(uplink-game "")
configure_uplink_component(uplink-game)

if(UPLINK_TARGET_TYPE STREQUAL "test")
  target_compile_definitions(uplink-game PRIVATE TESTGAME)
elseif(UPLINK_TARGET_TYPE STREQUAL "demo")
  target_compile_definitions(uplink-game PRIVATE DEMOGAME)
else()
  target_compile_definitions(uplink-game PRIVATE FULLGAME)
endif()

target_compile_definitions(
  uplink-game
  PRIVATE SDL_MAIN_HANDLED # we provide our own entry point
          _SILENCE_CXX17_STRSTREAM_DEPRECATION_WARNING # still need to remove
                                                       # uses of <strstream>
)

uplink_glob_files(
  UPLINK_GAME_SOURCES
  ${CMAKE_CURRENT_LIST_DIR}/src/**/**/**/*.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/**/**/*.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/**/*.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/*.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/resources.rc)

uplink_glob_files(
  UPLINK_GAME_HEADERS ${CMAKE_CURRENT_LIST_DIR}/src/**/**/**/*.h
  ${CMAKE_CURRENT_LIST_DIR}/src/**/**/*.h ${CMAKE_CURRENT_LIST_DIR}/src/**/*.h
  ${CMAKE_CURRENT_LIST_DIR}/src/*.h)

target_sources(
  uplink-game
  PRIVATE ${UPLINK_GAME_SOURCES}
  PUBLIC ${UPLINK_GAME_HEADERS})
target_include_directories(uplink-game PUBLIC ${CMAKE_CURRENT_LIST_DIR}/src)

find_package(SDL2 CONFIG REQUIRED)
find_package(SDL2_net CONFIG REQUIRED)
find_package(zstd CONFIG REQUIRED)
find_package(cpr CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(cpptrace CONFIG REQUIRED)
find_package(cppfs CONFIG REQUIRED)

target_compile_definitions(uplink-game PRIVATE ENABLE_NETWORK)

target_link_libraries(
  uplink-game
  PRIVATE tosser
          gucci
          bungle
          redshirt
          soundgarden
          vanbakel
          eclipse
          irclib
          $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
          $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
          $<IF:$<TARGET_EXISTS:SDL2_net::SDL2_net>,SDL2_net::SDL2_net,SDL2_net::SDL2_net-static>
          $<IF:$<TARGET_EXISTS:zstd::libzstd_shared>,zstd::libzstd_shared,zstd::libzstd_static>
          cpr::cpr
          nlohmann_json::nlohmann_json
          cpptrace::cpptrace
          cppfs::cppfs)

if(WIN32)
	set_target_properties(uplink-game PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE /SAFESEH:NO")
endif()