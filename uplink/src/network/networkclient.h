
/*

  Network client runs on a client computer
  Handles incoming data from the server
  Deals with the interface

  */

#ifndef _included_networkclient_h
#define _included_networkclient_h

#if ENABLE_NETWORK
	#include "network/network_sdl.h"
	#include "network/tcp4u_compat.h"
	#include <vector>
	#include <string>
	#include <thread>
	#include <atomic>
#endif

#include "app/uplinkobject.h"

class NetworkScreen;

// Types of client ============================================================

#define CLIENT_NONE 0
#define CLIENT_COMMS 1
#define CLIENT_STATUS 2

// Connection state for async connections
enum class ConnectionState { DISCONNECTED, CONNECTING, CONNECTED, FAILED };

// ============================================================================

class NetworkClient : public UplinkObject {

protected:
#if ENABLE_NETWORK
	Net::Socket* socket;
	std::vector<uint8_t> m_recvBuffer;

	// Async connection state
	std::atomic<ConnectionState> m_connectionState;
	std::string m_pendingHost;
	std::thread m_connectThread;
#endif

	int clienttype;

	int currentscreencode;
	NetworkScreen* screen;

protected:
	void Handle_ClientCommsData(char* buffer);
	void Handle_ClientStatusData(char* buffer);

public:
	NetworkClient();
	virtual ~NetworkClient();

	// Async connection - returns immediately, check IsConnected()/GetConnectionState()
	bool StartClientAsync(const char* ip);

	// Blocking connection (legacy)
	bool StartClient(const char* ip);
	bool StopClient();

	// Connection status
	ConnectionState GetConnectionState() const;
	bool IsConnected() const;
	bool IsConnecting() const;

	void SetClientType(int newtype);

	int InScreen(); // Returns id code of current screen
	void RunScreen(int SCREENCODE);
	NetworkScreen* GetNetworkScreen(); // Asserts screen

	// Common functions

	bool Load(FILE* file);
	void Save(FILE* file);
	void Print();
	void Update();
	std::string GetID();
};

#endif
