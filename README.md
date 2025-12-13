# Cybrelink

This is an Uplink mod I started to make small quality-of-life changes for things that got a bit grating after fifty hours of play. At this point, it's an aimless spare-time project to refactor as much of Uplink's source code as I can.

## Gameplay Changes

#### Hotkeys / Shortcuts
* Middle clicking on links (like on the main screen) will load your saved bounce path before connecting. If you're connecting to a central mainframe computer, it will connect through that company's internal services system if you've discovered it.
* Middle clicking on an account on the accounts view will fill in its details on the bank transfer page.
* Middle clicking on a password input box will run the password breaker automatically.
* Middle clicking on a decypher interface will run the decypher tool automatically.
* Middle clicking on a LAN system will run the LAN probe tool automatically.
* Middle clicking on the "Connect" button on a LAN system will connect directly back to it without hiding the LAN screen.
* Pressing F2 anywhere in-game will connect you directly to the Uplink Internal Services System.
* Pressing F3 anywhere in-game will connect you directly to InterNIC.

#### UI
* Added a mail view interface tab, which lists all emails in a simpler interface, and makes deleting a bunch of emails a lot easier. There is no scrollbar on this interface yet.
* The mouse wheel now scrolls the main links interface.
* The mouse wheel now zooms on the world map.
* If you are connected to a server that you have a mission for, the mission will be highlighted with a border around it.

#### Misc
* Added an auto bounce path button on the map interface. This will make a path through all found links, starting with InterNIC.
* Added an add all button on the top left of long lists of links (like the InterNIC browse page) that saves all of the links.
* Added an auto-bypass button on the connection analyser page.

## Multiplayer / Online Features (WIP)

Cybrelink adds experimental multiplayer capabilities:

### Architecture
* **Client-Server Model** - Dedicated server handles authoritative game state
* **Supabase Backend** - Cloud authentication and world persistence
* **Real-time Networking** - Async connections, game never freezes

### Online Registration
* **Email-based registration** - Create accounts with real email addresses
* **Password requirements** - Minimum 8 characters with uppercase, lowercase, and number
* **Auto-login** - Credentials stored locally for seamless re-login
* **JWT authentication** - Server validates tokens against Supabase

### Dedicated Server
* **Standalone server** (`uplink-server.exe`) for hosting multiplayer sessions
* **SDL_net networking** - Replaced legacy TCP/IP stack
* **Binary protocol** - Efficient packet-based communication
* **Timestamped logging** - `[HH:MM:SS] CONNECT/AUTH/DISCONNECT` events

### World Persistence
* **Supabase tables** - `computers`, `missions`, `bank_accounts`, `access_logs`
* **API methods** - `GetAllComputers()`, `GetAllMissions()`, `ClaimMission()`
* **Planned** - Server loads world on startup, periodic auto-saves

### Running the Server
```bash
# Start with default Supabase project
.\uplink-server.exe

# Or specify custom Supabase credentials
.\uplink-server.exe --url https://your-project.supabase.co --key your-anon-key
```

## Building

### Requirements
- **CMake 3.30+**
- **vcpkg** (set `VCPKG_ROOT` environment variable)
- **Visual Studio 2022 or 2026** with "Desktop development with C++" workload

### Build Instructions

The project uses CMake for building and vcpkg for dependencies. Run the following commands in the project's root directory:

```bash
# Configure (use appropriate generator for your VS version)
cmake -S . -B out/build -G "Visual Studio 17 2022" -A x64   # For VS 2022
cmake -S . -B out/build -G "Visual Studio 18 2026" -A x64   # For VS 2026

# Build Release (both game and server)
cmake --build out/build --config Release --target uplink-game uplink-server

# Build Debug
cmake --build out/build --config Debug
```

The executable will be written to `uplink/bin/Release/` (or `Debug/`). Copy the original Uplink game data files (`.dat` files from the `Installer/data` folder) to this directory to run the game.