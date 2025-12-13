// OnlineHUDInterface.cpp: implementation of the OnlineHUDInterface class.
// Displays online players and chat for multiplayer mode
//
//////////////////////////////////////////////////////////////////////

#ifdef WIN32
	#include <windows.h>
#endif

#include <GL/gl.h>
#include <GL/glu.h>

#include "eclipse.h"
#include "redshirt.h"
#include "soundgarden.h"

#include "app/app.h"
#include "app/globals.h"
#include "app/miscutils.h"
#include "app/opengl_interface.h"

#include "game/game.h"

#include "options/options.h"

#include "interface/interface.h"
#include "interface/localinterface/localinterface.h"
#include "interface/localinterface/online_hud_interface.h"

#include "network/network.h"
#include "network/networkclient.h"

//////////////////////////////////////////////////////////////////////
// Static members
//////////////////////////////////////////////////////////////////////

int OnlineHUDInterface::scrollOffset = 0;
int OnlineHUDInterface::playerScrollOffset = 0;

//////////////////////////////////////////////////////////////////////
// Callbacks
//////////////////////////////////////////////////////////////////////

void OnlineHUDInterface::TitleClick(Button* button)
{
	game->GetInterface()->GetLocalInterface()->RunScreen(SCREEN_NONE);
}

void OnlineHUDInterface::TitleDraw(Button* button, bool highlighted, bool clicked)
{
	SetColour("TitleText");
	GciDrawText(button->x + 10, button->y + 10, button->caption, HELVETICA_18);
}

void OnlineHUDInterface::PlayerListDraw(Button* button, bool highlighted, bool clicked)
{
	// Background
	glBegin(GL_QUADS);
	SetColour("PanelBackground");
	glVertex2i(button->x, button->y);
	glVertex2i(button->x + button->width, button->y);
	glVertex2i(button->x + button->width, button->y + button->height);
	glVertex2i(button->x, button->y + button->height);
	glEnd();

	// Border
	SetColour("PanelBorder");
	border_draw(button);

#if ENABLE_NETWORK
	// Draw player list
	NetworkClient* client = app->GetNetwork()->GetClient();
	if (client && client->IsConnected()) {
		const std::vector<Net::PlayerListEntry>& players = client->GetOnlinePlayers();

		SetColour("DefaultText");
		int y = button->y + 5;
		int maxVisible = (button->height - 10) / 15;

		for (size_t i = playerScrollOffset; i < players.size() && (int)(i - playerScrollOffset) < maxVisible;
			 ++i) {
			char line[64];
			UplinkSnprintf(line, sizeof(line), "%s [%d]", players[i].handle, players[i].rating);
			GciDrawText(button->x + 5, y + 12, line, HELVETICA_10);
			y += 15;
		}

		if (players.empty()) {
			GciDrawText(button->x + 5, button->y + 20, "No players online", HELVETICA_10);
		}
	} else {
		SetColour("DefaultText");
		GciDrawText(button->x + 5, button->y + 20, "Not connected", HELVETICA_10);
	}
#else
	SetColour("DefaultText");
	GciDrawText(button->x + 5, button->y + 20, "Network disabled", HELVETICA_10);
#endif
}

void OnlineHUDInterface::ChatAreaDraw(Button* button, bool highlighted, bool clicked)
{
	// Background
	glBegin(GL_QUADS);
	SetColour("PanelBackground");
	glVertex2i(button->x, button->y);
	glVertex2i(button->x + button->width, button->y);
	glVertex2i(button->x + button->width, button->y + button->height);
	glVertex2i(button->x, button->y + button->height);
	glEnd();

	// Border
	SetColour("PanelBorder");
	border_draw(button);

#if ENABLE_NETWORK
	// Draw chat messages
	NetworkClient* client = app->GetNetwork()->GetClient();
	if (client && client->IsConnected()) {
		const std::vector<ChatDisplayMessage>& chat = client->GetChatHistory();

		int y = button->y + button->height - 15;
		int maxVisible = (button->height - 10) / 12;
		int startIdx = (int)chat.size() - 1 - scrollOffset;

		SetColour("DefaultText");
		int count = 0;
		for (int i = startIdx; i >= 0 && count < maxVisible; --i, ++count) {
			char line[320];
			UplinkSnprintf(line, sizeof(line), "[%s] %s", chat[i].sender.c_str(), chat[i].message.c_str());

			// Truncate if too long
			if (strlen(line) > 40) {
				line[37] = '.';
				line[38] = '.';
				line[39] = '.';
				line[40] = '\0';
			}

			GciDrawText(button->x + 5, y, line, HELVETICA_10);
			y -= 12;
		}

		if (chat.empty()) {
			GciDrawText(button->x + 5, button->y + 20, "No messages yet", HELVETICA_10);
		}
	}
#endif
}

void OnlineHUDInterface::ChatInputDraw(Button* button, bool highlighted, bool clicked)
{
	// Input field background
	glBegin(GL_QUADS);
	if (highlighted) {
		SetColour("ButtonHighlighted");
	} else {
		SetColour("ButtonNormal");
	}
	glVertex2i(button->x, button->y);
	glVertex2i(button->x + button->width, button->y);
	glVertex2i(button->x + button->width, button->y + button->height);
	glVertex2i(button->x, button->y + button->height);
	glEnd();

	SetColour("PanelBorder");
	border_draw(button);

	// Draw text
	SetColour("DefaultText");
	if (!button->caption.empty()) {
		GciDrawText(button->x + 5, button->y + 14, button->caption, HELVETICA_10);
	} else {
		SetColour("DimmedText");
		GciDrawText(button->x + 5, button->y + 14, "Type message...", HELVETICA_10);
	}
}

void OnlineHUDInterface::SendButtonClick(Button* button)
{
#if ENABLE_NETWORK
	Button* inputBtn = EclGetButton("online_chatinput");
	if (inputBtn && !inputBtn->caption.empty()) {
		NetworkClient* client = app->GetNetwork()->GetClient();
		if (client && client->IsConnected()) {
			client->SendChat("global", inputBtn->caption.c_str());
			inputBtn->SetCaption("");
			EclDirtyButton("online_chatinput");
			SgPlaySound(RsArchiveFileOpen("sounds/done.wav"), "sounds/done.wav", false);
		}
	}
#endif
}

void OnlineHUDInterface::ScrollUpClick(Button* button)
{
#if ENABLE_NETWORK
	NetworkClient* client = app->GetNetwork()->GetClient();
	if (client) {
		const std::vector<ChatDisplayMessage>& chat = client->GetChatHistory();
		if (scrollOffset < (int)chat.size() - 5) {
			scrollOffset++;
			EclDirtyButton("online_chatarea");
		}
	}
#endif
}

void OnlineHUDInterface::ScrollDownClick(Button* button)
{
	if (scrollOffset > 0) {
		scrollOffset--;
		EclDirtyButton("online_chatarea");
	}
}

void OnlineHUDInterface::PlayerScrollUpClick(Button* button)
{
	if (playerScrollOffset > 0) {
		playerScrollOffset--;
		EclDirtyButton("online_playerlist");
	}
}

void OnlineHUDInterface::PlayerScrollDownClick(Button* button)
{
#if ENABLE_NETWORK
	NetworkClient* client = app->GetNetwork()->GetClient();
	if (client) {
		const std::vector<Net::PlayerListEntry>& players = client->GetOnlinePlayers();
		if (playerScrollOffset < (int)players.size() - 3) {
			playerScrollOffset++;
			EclDirtyButton("online_playerlist");
		}
	}
#endif
}

//////////////////////////////////////////////////////////////////////
// Interface methods
//////////////////////////////////////////////////////////////////////

void OnlineHUDInterface::Create()
{
	if (!IsVisible()) {

		LocalInterfaceScreen::Create();

		int screenw = app->GetOptions()->GetOptionValue("graphics_screenwidth");
		int screenh = app->GetOptions()->GetOptionValue("graphics_screenheight");
		int paneltop = (int)(100.0 * ((screenw * PANELSIZE) / 188.0) + 30);
		int panelwidth = (int)(screenw * PANELSIZE);

		// Title
		EclRegisterButton(screenw - panelwidth,
						  paneltop + 3,
						  panelwidth - 7,
						  15,
						  "ONLINE",
						  "Close online players panel",
						  "online_title");
		EclRegisterButtonCallbacks("online_title", TitleDraw, TitleClick, button_click, button_highlight);

		// Players section header
		EclRegisterButton(
			screenw - panelwidth + 5, paneltop + 25, 100, 15, "Players Online", "", "online_playerstitle");
		EclRegisterButtonCallbacks("online_playerstitle", TitleDraw, NULL, NULL, NULL);

		// Player list
		EclRegisterButton(
			screenw - panelwidth + 5, paneltop + 45, panelwidth - 30, 80, "", "", "online_playerlist");
		EclRegisterButtonCallbacks("online_playerlist", PlayerListDraw, NULL, NULL, NULL);

		// Player scroll buttons
		EclRegisterButton(screenw - 20, paneltop + 45, 15, 15, "^", "Scroll up", "online_playerup");
		EclRegisterButtonCallback("online_playerup", PlayerScrollUpClick);

		EclRegisterButton(screenw - 20, paneltop + 110, 15, 15, "v", "Scroll down", "online_playerdown");
		EclRegisterButtonCallback("online_playerdown", PlayerScrollDownClick);

		// Chat section header
		EclRegisterButton(screenw - panelwidth + 5, paneltop + 135, 100, 15, "Chat", "", "online_chattitle");
		EclRegisterButtonCallbacks("online_chattitle", TitleDraw, NULL, NULL, NULL);

		// Chat area
		int chatHeight = screenh - paneltop - 220;
		EclRegisterButton(
			screenw - panelwidth + 5, paneltop + 155, panelwidth - 30, chatHeight, "", "", "online_chatarea");
		EclRegisterButtonCallbacks("online_chatarea", ChatAreaDraw, NULL, NULL, NULL);

		// Chat scroll buttons
		EclRegisterButton(screenw - 20, paneltop + 155, 15, 15, "^", "Scroll up", "online_chatup");
		EclRegisterButtonCallback("online_chatup", ScrollUpClick);

		EclRegisterButton(
			screenw - 20, paneltop + 155 + chatHeight - 15, 15, 15, "v", "Scroll down", "online_chatdown");
		EclRegisterButtonCallback("online_chatdown", ScrollDownClick);

		// Chat input
		int inputY = paneltop + 160 + chatHeight;
		EclRegisterButton(screenw - panelwidth + 5,
						  inputY,
						  panelwidth - 50,
						  20,
						  "",
						  "Type your message",
						  "online_chatinput");
		EclRegisterButtonCallbacks("online_chatinput", ChatInputDraw, NULL, button_click, button_highlight);
		EclMakeButtonEditable("online_chatinput");

		// Send button
		EclRegisterButton(screenw - 40, inputY, 35, 20, "Send", "Send message", "online_chatsend");
		EclRegisterButtonCallback("online_chatsend", SendButtonClick);

		// Reset scroll positions
		scrollOffset = 0;
		playerScrollOffset = 0;
	}
}

void OnlineHUDInterface::Remove()
{
	if (IsVisible()) {

		LocalInterfaceScreen::Remove();

		EclRemoveButton("online_title");
		EclRemoveButton("online_playerstitle");
		EclRemoveButton("online_playerlist");
		EclRemoveButton("online_playerup");
		EclRemoveButton("online_playerdown");
		EclRemoveButton("online_chattitle");
		EclRemoveButton("online_chatarea");
		EclRemoveButton("online_chatup");
		EclRemoveButton("online_chatdown");
		EclRemoveButton("online_chatinput");
		EclRemoveButton("online_chatsend");
	}
}

void OnlineHUDInterface::Update()
{
	// Refresh displays periodically
	static int updateCounter = 0;
	updateCounter++;

	if (updateCounter % 30 == 0) { // Every ~0.5 seconds at 60fps
		EclDirtyButton("online_playerlist");
		EclDirtyButton("online_chatarea");
	}
}

bool OnlineHUDInterface::IsVisible() { return (EclGetButton("online_title") != NULL); }

int OnlineHUDInterface::ScreenID() { return SCREEN_ONLINE; }
