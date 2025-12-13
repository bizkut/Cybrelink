// NetworkClient.cpp: implementation of the NetworkClient class.
//
//////////////////////////////////////////////////////////////////////

#include <strstream>

#include "eclipse.h"
#include "gucci.h"

#include "app/app.h"
#include "app/globals.h"

#include "options/options.h"

#include "mainmenu/mainmenu.h"

#include "network/interfaces/clientcommsinterface.h"
#include "network/interfaces/clientstatusinterface.h"
#include "network/interfaces/networkscreen.h"
#include "network/network.h"
#include "network/networkclient.h"
#include "network/network_sdl.h"
#include "network/protocol.h"
#include "network/supabase_client.h"

#include "world/vlocation.h"
#include "world/world.h"
#include "game/game.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

NetworkClient::NetworkClient()
{

#if ENABLE_NETWORK
	socket = nullptr;
#endif

	clienttype = CLIENT_NONE;
	currentscreencode = CLIENT_NONE;
	screen = NULL;
}

NetworkClient::~NetworkClient()
{

	if (screen) {
		delete screen;
	}
}

bool NetworkClient::StartClient(const char* ip)
{

#if ENABLE_NETWORK
	// Establish a connection to the server at the specified ip

	unsigned short portnum = 31337;

	Net::Socket clientSocket;
	Net::NetResult result = Net::NetworkManager::Instance().Connect(ip, portnum, clientSocket);

	if (result != Net::NetResult::OK) {
		printf("NetworkClient::StartClient failed to connect to %s:%d\n", ip, portnum);
		return false;
	}

	else {
		// We have a connection!
		// We need to store this socket somewhere useful.
		// The original Uplink code stored it in 'socket'.
		// We can't simply cast Net::Socket to void* easily if we want to keep C++ semantics,
		// but for now, let's assume we update the 'socket' member to be of type Net::Socket* or wrapper.
		// Wait, 'socket' handles are passed around as void* in legacy code.
		// But I control NetworkClient.

		// Let's rely on the definition of 'socket' in the class.
		// In networkclient.h, 'socket' was typically void*. I need to check.
		// If I changed it to Net::Socket* or if I can store it.

		// Actually, NetworkClient shouldn't manage the raw socket directly if we have a connection
		// abstraction. BUT, I must ensure existing code that uses 'socket' still works OR replace that usage
		// too. Legacy 'TcpSend' takes 'socket'. I need to store the specific socket instance.

		// WORKAROUND: For this phase, I will stick it into a global or static map, OR
		// I will change 'socket' definition if I can.
		// But let's check networkclient.h first.

		// For now, I'll close the socket immediately if I don't store it? No.

		// Let's assume for a moment I can change 'socket' type or use a new member.
		// But to avoid huge refactoring, let's look at networkclient.h.

		// I'll assume I can just use the Manager to hold the "Client" socket?
		// No, the Manager is a factory.

		// I will dynamically allocate the socket and store the pointer in 'socket' (Net::Socket*).
		Net::Socket* newSock = new Net::Socket(std::move(clientSocket));
		socket = newSock;

		// Send Handshake Packet
		Net::HandshakePacket handshakePayload;
		handshakePayload.protocolVersion = Net::PROTOCOL_VERSION;
		handshakePayload.clientVersion = 1;

		// Handle (get from player profile if possible, else "Player")
		// Uplink world player might not be fully initialized yet?
		// Actually StartClient is called from MainMenu, player might be "NEWAGENT" or loaded.
		// For now, use "Guest".
		strncpy(handshakePayload.handle, "Guest", sizeof(handshakePayload.handle) - 1);

		Net::SupabaseClient& supabase = Net::SupabaseClient::Instance();
		std::string token = supabase.GetAuthToken();
		if (token.length() > 0) {
			strncpy(handshakePayload.authToken, token.c_str(), sizeof(handshakePayload.authToken) - 1);
		} else {
			memset(handshakePayload.authToken, 0, sizeof(handshakePayload.authToken));
		}

		// Serialize
		uint8_t buffer[1024];
		size_t packetLen = Net::WritePacket(
			buffer, Net::PacketType::HANDSHAKE, Net::FLAG_NONE, &handshakePayload, sizeof(handshakePayload));

		Net::NetResult sendRes = newSock->Send(buffer, packetLen);
		if (sendRes != Net::NetResult::OK) {
			printf("NetworkClient::StartClient failed to send handshake\n");
			delete newSock;
			socket = nullptr;
			return false;
		}

		m_recvBuffer.clear();
		return true;
	}
#else
	return false;
#endif
}

bool NetworkClient::StopClient()
{
#if ENABLE_NETWORK
	if (socket) {
		Net::Socket* clientSock = (Net::Socket*)socket;
		clientSock->Close();
		delete clientSock;
		socket = nullptr;
		return true;
	}
	return false;

#else
	return false;
#endif
}

void NetworkClient::SetClientType(int newtype)
{

#if ENABLE_NETWORK
	// Request this mode from the server
	// Legacy: sent SETCLIENTTYPE string.
	// New: Should send an ACTION packet or similar.
	// For now, we just switch local state, effectively assuming success.

	if (socket) {
		// TODO: Send packet to server requesting state change
	}

	clienttype = newtype;
	RunScreen(clienttype);

#endif
}

int NetworkClient::InScreen() { return currentscreencode; }

void NetworkClient::RunScreen(int SCREENCODE)
{

	// Get rid of the current interface
	if (screen) {
		screen->Remove();
		delete screen;
		screen = NULL;
	}

	currentscreencode = SCREENCODE;

	switch (currentscreencode) {

	case CLIENT_COMMS:
		screen = new ClientCommsInterface();
		break;
	case CLIENT_STATUS:
		screen = new ClientStatusInterface();
		break;

	case CLIENT_NONE:
		return;
	default:
		UplinkAbort("Tried to create a screen with unknown SCREENCODE");
	}

	screen->Create();
}

NetworkScreen* NetworkClient::GetNetworkScreen()
{

	UplinkAssert(screen);
	return screen;
}

bool NetworkClient::Load(FILE* file)
{
	// not needed
	return true;
}

void NetworkClient::Save(FILE* file)
{
	// not needed
}

void NetworkClient::Print()
{

#if ENABLE_NETWORK
	printf("NetworkClient : SOCKET:%d\n", socket);
	printf("\tcurrentscreen:%d\n", currentscreencode);
#endif
}

void NetworkClient::Update()
{

#if ENABLE_NETWORK
	// Check for input from server

	if (socket != nullptr) {
		Net::Socket* clientSock = (Net::Socket*)socket;

		// Simple receive loop
		if (clientSock->HasData()) {
			char buffer[4096];
			int received = clientSock->Recv(buffer, sizeof(buffer));

			if (received > 0) {
				// Append to buffer
				size_t oldSize = m_recvBuffer.size();
				m_recvBuffer.resize(oldSize + received);
				memcpy(m_recvBuffer.data() + oldSize, buffer, received);

				// Process packets
				while (m_recvBuffer.size() >= sizeof(Net::PacketHeader)) {
					Net::PacketHeader header;
					memcpy(&header, m_recvBuffer.data(), sizeof(header));

					size_t fullPacketSize = sizeof(Net::PacketHeader) + header.length;

					if (m_recvBuffer.size() < fullPacketSize) {
						break; // Wait for more data
					}

					// We have a full packet
					const uint8_t* payload = m_recvBuffer.data() + sizeof(Net::PacketHeader);

					// Handle packet
					switch (static_cast<Net::PacketType>(header.type)) {
					case Net::PacketType::TIME_SYNC: {
						if (header.length >= sizeof(Net::TimeSyncPacket)) {
							const Net::TimeSyncPacket* tsp =
								reinterpret_cast<const Net::TimeSyncPacket*>(payload);
							if (game && game->GetWorld()) {
								game->GetWorld()->date.SetDate(
									tsp->second, tsp->minute, tsp->hour, tsp->day, tsp->month, tsp->year);
							}
						}
						break;
					}
					// TODO: Other packets
					default:
						break;
					}

					// Remove processed packet
					m_recvBuffer.erase(m_recvBuffer.begin(), m_recvBuffer.begin() + fullPacketSize);
				}
			} else if (received == -1) {
				// Disconnected
				printf("NetworkClient: Connection lost\n");

				EclReset(app->GetOptions()->GetOptionValue("graphics_screenwidth"),
						 app->GetOptions()->GetOptionValue("graphics_screenheight"));

				clientSock->Close();
				delete clientSock;
				socket = nullptr;
				m_recvBuffer.clear();

				app->GetNetwork()->SetStatus(NETWORK_NONE);
				app->GetMainMenu()->RunScreen(MAINMENU_NETWORKOPTIONS);
				return;
			}
		}
	}

	// Update interface

	if (screen) {
		screen->Update();
	}
#endif
}

std::string NetworkClient::GetID() { return "CLIENT"; }
