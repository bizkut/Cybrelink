// Network.cpp: implementation of the Network class.
//
//////////////////////////////////////////////////////////////////////

#include "gucci.h"

#include "app/app.h"
#include "app/globals.h"
#include "app/serialise.h"

#include "network/network.h"

#if ENABLE_NETWORK
#include "network/network_sdl.h"
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Network::Network()
{
	STATUS = NETWORK_NONE;

#if ENABLE_NETWORK
	Net::NetResult result = Net::NetInit();
	if (result != Net::NetResult::OK) {
		printf("Network error : failed to initialise networking\n");
		return;
	}
#endif
}

Network::~Network()
{
#if ENABLE_NETWORK
	Net::NetShutdown();
#endif
}

NetworkServer* Network::GetServer()
{
	UplinkAssert(STATUS == NETWORK_SERVER);
	return &server;
}

NetworkClient* Network::GetClient()
{
	UplinkAssert(STATUS == NETWORK_CLIENT);
	return &client;
}

void Network::SetStatus(int newSTATUS) { STATUS = newSTATUS; }

char* Network::GetLocalHost()
{
#if ENABLE_NETWORK
	static char host[128] = "localhost";
	return host;
#else
	return NULL;
#endif
}

char* Network::GetLocalIP()
{
#if ENABLE_NETWORK
	static char ip[64];
	std::string localIp = Net::NetworkManager::Instance().GetLocalIP();
	UplinkStrncpy(ip, localIp.c_str(), sizeof(ip));
	return ip;
#else
	return NULL;
#endif
}

void Network::StartServer()
{
	if (STATUS == NETWORK_NONE) {
		int result = server.StartServer();
		if (result) {
			STATUS = NETWORK_SERVER;
		} else {
			printf("Network::StartServer, failed to start server\n");
		}
	} else if (STATUS == NETWORK_CLIENT) {
		printf("Network::StartServer, Cannot start server when running as a client\n");
	} else if (STATUS == NETWORK_SERVER) {
		printf("Network::StartServer, Cannot start server when server is already running\n");
	}
}

void Network::StopServer()
{
	if (STATUS == NETWORK_SERVER) {
		server.StopServer();
		STATUS = NETWORK_NONE;
	}
}

void Network::StartClient(const char* ip)
{
	if (STATUS == NETWORK_NONE) {
		int result = client.StartClient(ip);
		if (result) {
			STATUS = NETWORK_CLIENT;
		} else {
			printf("Network::StartClient, failed to start client\n");
		}
	} else if (STATUS == NETWORK_CLIENT) {
		printf("Network::StartClient, Cannot start client when running as a client\n");
	} else if (STATUS == NETWORK_SERVER) {
		printf("Network::StartClient, Cannot start client when server is already running\n");
	}
}

void Network::StopClient()
{
	if (STATUS == NETWORK_CLIENT) {
		int result = GetClient()->StopClient();
		if (!result) {
			printf("Network::StopClient, failed to stop client\n");
		} else {
			STATUS = NETWORK_NONE;
		}
	}
}

bool Network::IsActive() { return (STATUS != NETWORK_NONE); }

bool Network::Load(FILE* file)
{
	// not needed
	return true;
}

void Network::Save(FILE* file)
{
	// not needed
}

void Network::Print()
{
	printf("============== N E T W O R K ===============================\n");
	printf("Status:%d\n", STATUS);

	if (STATUS == NETWORK_SERVER) {
		GetServer()->Print();
	} else if (STATUS == NETWORK_CLIENT) {
		GetClient()->Print();
	}

	printf("============== E N D  O F  N E T W O R K ===================\n");
}

void Network::Update()
{
	if (STATUS == NETWORK_SERVER) {
		GetServer()->Update();
	} else if (STATUS == NETWORK_CLIENT) {
		GetClient()->Update();
	}
}

std::string Network::GetID() { return "NETWORK"; }

