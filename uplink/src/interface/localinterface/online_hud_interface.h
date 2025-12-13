// OnlineHUDInterface.h: interface for the OnlineHUDInterface class.
// Displays online players and chat for multiplayer mode
//
//////////////////////////////////////////////////////////////////////

#ifndef _included_onlinehudinterface_h
#define _included_onlinehudinterface_h

#include "interface/localinterface/localinterfacescreen.h"

// Forward declarations - actual types in networkclient.h
struct ChatDisplayMessage;
namespace Net {
struct PlayerListEntry;
}

class OnlineHUDInterface : public LocalInterfaceScreen {

protected:
	static int scrollOffset; // For chat scrolling
	static int playerScrollOffset; // For player list scrolling

	static void TitleClick(Button* button);
	static void TitleDraw(Button* button, bool highlighted, bool clicked);
	static void PlayerListDraw(Button* button, bool highlighted, bool clicked);
	static void ChatAreaDraw(Button* button, bool highlighted, bool clicked);
	static void ChatInputDraw(Button* button, bool highlighted, bool clicked);
	static void SendButtonClick(Button* button);
	static void ScrollUpClick(Button* button);
	static void ScrollDownClick(Button* button);
	static void PlayerScrollUpClick(Button* button);
	static void PlayerScrollDownClick(Button* button);

public:
	void Create();
	void Remove();
	void Update();
	bool IsVisible();

	int ScreenID();
};

#endif
