# Cybrelink Multiplayer - Agent Handover & Context

> **Last Updated:** 2025-12-13
> **Current Phase:** Phase 4 (PVP Mechanics) & Phase 5 (Lobby/UI)

## 1. Project Overview
We are adding multiplayer capabilities to the legacy game **Uplink**.
- **Repo**: `Cybrelink`
- **Architecture**: Client (OpenGL/SDL) <-> Dedicated Server (Headless) <-> Supabase (Persistence).
- **Protocol**: Custom binary protocol over TCP (`uplink/src/network/protocol.h`).

## 2. Completed Architecture (Verified)
- **Shared Core**: Refactored logic into `uplink-core` static lib used by both Client and Server.
- **Dedicated Server**: `uplink-server.exe` runs headless, accepts TCP connections.
- **Network Layer**:
    - `NetworkClient` (Client-side) handles state replication.
    - `GameServer` (Server-side) runs authoritative simulation (60Hz tick).
    - Delta Encoding (`deltaencoder.h`) implemented for bandwidth efficiency.
- **Authentication**:
    - **Supabase** via `CPR` (C++ Requests).
    - Real email/password registration implemented (`Script33`).
    - Auto-login via `.auth` file (locally stored credentials).

## 3. Recent Bug Fixes (Phase 6 Polish)
- **Tutorial**: Fixed `EclSuperUnHighlight` bug where it deleted buttons instead of borders.
- **Quit Confirmation**: Added "Quit Game?" dialog for **ESC** and **F12** keys.
    - *Note*: ESC logic cancels text edits -> closes windows -> prompts quit.
- **Stability**: Reduced Supabase timeout to 5s and disabled SSL verify to prevent freezing.
- **Time Sync**: Implemented server->client time synchronization.
    - Server uses standalone `ServerDate` class (`server/server_date.h`) to track game time.
    - `TIME_SYNC` packet (`0x43`) sent at 20Hz to all authenticated clients.
    - Client's `NetworkClient::Update()` receives and applies time to `game->GetWorld()->date`.
- **Server Auth Verification**: Handshake now validates JWT tokens against Supabase.
    - `SupabaseClient::VerifyToken()` calls `/auth/v1/user` endpoint.
    - Invalid/expired tokens result in disconnection.
    - Players without tokens connect as "guest" (can be made stricter later).
- **Welcome Email**: Upon registration, player receives personalized email from "Uplink public access system".
    - Contains email, handle, and bank account number (NO password).
    - Bank uses separate random password (not the real player password).

## 4. Critical Developer Notes (READ THIS)
### ⚠️ File Encoding Warning
**`uplink/src/app/opengl.cpp`** has standard encoding issues that confuse some tools.
- **DO NOT** use standard text search/replace tools on this file if they fail.
- **USE** Python scripts to read/patch this file safely (binary read -> decode -> patch -> binary write).

### ⚠️ Supabase Configuration
- SSL Verification is **DISABLED** (`verifySsl = false`) in `supabase_client.cpp` for stability.
- Timeout is strict (**5 seconds**) to avoid blocking the main thread (UI freeze).

### ⚠️ Password Security
The player password is stored in **exactly two places**:
1. **Supabase `auth.users` table** - hashed by Supabase (we don't manage this)
2. **Local `.auth` file** - plaintext for auto-login convenience

Password is **NOT** stored in:
- Save game files (`Player` struct has no password field)
- Any Supabase table we create (players, etc.)
- Welcome emails or any in-game messages

## 5. Next Steps (Immediate Priorities)

### Phase 4: PVP Mechanics
1. ~~**Server Auth**: Verify the `auth_token` sent in the Handshake packet against Supabase.~~ ✅ DONE
2. ~~**Player State**: Load `PlayerProfile` (rating, balance, hardware) from Supabase upon connection.~~ ✅ DONE
3. **Gameplay Packets**: Implement `PKT_PLAYER_ACTION` handlers in `GameServer`. ⏳ IN PROGRESS
    - Action routing framework complete (switch on ActionType)
    - Stub handlers for: AddBounce, ConnectTarget, RunSoftware, BypassSecurity,
      DownloadFile, DeleteFile, DeleteLog, TransferMoney, FramePlayer, PlaceBounty
    - **TODO**: Connect handlers to existing Agent/World simulation code

### ⚠️ Architecture Note: Player-NPC Parity
Existing NPCs (`Agent` class) already behave like players - they hack, take missions, get traced.
**Real players should use the SAME Agent class and World simulation**, not parallel systems.
- Each connected player controls an `Agent*` in the server's `World`
- `PLAYER_ACTION` packets should call the same Agent methods NPCs use
- Server runs the same world simulation as client (headless)

### Phase 5: UI & Lobby
1. **Lobby Interface**: Create a new screen (`LobbyInterface`) to list active servers/players.
2. **Matchmaking**: Simple matchmaking screen to find opponents.

## 6. Build Commands
```powershell
# Build Game and Server
cmake --build out/build --config Release --target uplink-game uplink-server

# Run Server
./out/build/uplink/bin/uplink-server.exe

# Run Client
./out/build/uplink/uplink-game.exe
```
