// SPDX-License-Identifier: AGPL-3.0-or-later

/**
 * @file setup.h
 * Setup Menu Header File
 *
 * @copyright 2026 by Andreas Baierl. All Rights Reserved.
 *
 * @license{AGPL-3.0-or-later}
 */

#ifndef __HYPERION_SETUP_H
#define __HYPERION_SETUP_H

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <vdr/menuitems.h>
#include <vdr/osd.h>
#include <vdr/tools.h>

#include "config.h"
#include "discovery.h"
#include "hyperionsettings.h"

class cHyperionClient;

/**
 * Menu of the settings, which are stored in Hyperion
 *
 * The settings are read, when the menu is opened, and written with OK.
 * Reading and writing is done by the client thread, the menu only shows
 * the state.
 */
class cMenuHyperionSettings : public cOsdMenu {
public:
	cMenuHyperionSettings(cHyperionClient *, const sHyperionConfig &);
	virtual eOSState ProcessKey(eKeys);

private:
	cHyperionClient *m_pClient;     ///< reads and writes the settings
	sHyperionConfig m_server;       ///< host, JSON port and token of the server
	int m_state;                    ///< state of the settings (cHyperionClient::eSettingsState)
	std::string m_error;            ///< error of the last request
	sHyperionSettings m_loaded;     ///< the read settings
	const char *m_borderModes[sHyperionSettings::BORDER_MODES]; ///< texts of the black border modes

	// local copies of the settings
	int m_smoothing;
	int m_smoothingTimeMs;
	int m_smoothingDelay;
	int m_blackBorder;
	int m_blackBorderThreshold;
	int m_blackBorderMode;
	int m_brightness;
	char m_delayTitle[64];          ///< title of the delay (contains the duration of a frame)

	void Create(void);
	sHyperionSettings Edited(void) const;
};

/**
 * Menu of the test patterns
 *
 * A pattern is shown on the whole screen (an own OSD above the menu) and sent
 * to Hyperion at the same moment, so the leds can be compared with the TV.
 * With the black and white pattern, the delay of the leds can be changed
 * (smoothing of Hyperion), until the leds change together with the screen.
 */
class cMenuHyperionTest : public cOsdMenu {
public:
	cMenuHyperionTest(cHyperionClient *, const sHyperionConfig &);
	virtual ~cMenuHyperionTest();
	virtual eOSState ProcessKey(eKeys);

private:
	cHyperionClient *m_pClient;     ///< sends the pattern and changes the delay
	sHyperionConfig m_server;       ///< host, JSON port and token of the server
	cOsd *m_pOsd = nullptr;         ///< shows the pattern (nullptr: no pattern)
	int m_mode = 0;                 ///< the shown pattern (cTestPattern::eMode)
	int m_step = 0;                 ///< step of the pattern
	cTimeMs m_stepTimer;            ///< time of the next step

	// delay of the leds (MODE_DELAY)
	int m_settingsState = -1;       ///< state of the settings (cHyperionClient::eSettingsState, -1: not requested)
	sHyperionSettings m_settings;   ///< the read settings (with the changed delay)
	std::string m_error;            ///< error of the settings

	void Start(int);
	void Stop(void);
	void ShowPattern(bool);
	void ChangeDelay(int);
	cString DelayText(void) const;
};

/**
 * Menu of the Hyperion servers found in the network
 *
 * The search (SSDP) runs in an own thread. The selected server is written
 * into the fields of the setup page.
 */
class cMenuHyperionDiscovery : public cOsdMenu {
public:
	cMenuHyperionDiscovery(char *host, size_t hostSize, int *port, int *jsonPort, char *token);
	virtual ~cMenuHyperionDiscovery();
	virtual eOSState ProcessKey(eKeys);

private:
	static constexpr int SEARCH_MS = 2000;  ///< time to wait for the answers

	char *m_host;                   ///< host field of the setup page
	size_t m_hostSize;              ///< size of the host field
	int *m_port;                    ///< flatbuffers port field of the setup page
	int *m_jsonPort;                ///< JSON port field of the setup page
	char *m_token;                  ///< token field of the setup page (cleared for another server)

	std::thread m_thread;           ///< runs the search
	std::atomic<bool> m_cancel = false; ///< stops the search
	std::mutex m_mutex;             ///< protects the members below
	bool m_searching = false;       ///< the search is running
	std::vector<sHyperionServer> m_servers; ///< the found servers (after the search)
	bool m_shown = false;           ///< the result is shown

	void StartSearch(void);
	void Create(void);
};

/**
 * Setup page of the plugin
 */
class cMenuSetupHyperion : public cMenuSetupPage {
public:
	cMenuSetupHyperion(sHyperionConfig *, cHyperionClient *);
	virtual ~cMenuSetupHyperion();
	virtual eOSState ProcessKey(eKeys);

protected:
	virtual void Store(void);

private:
	sHyperionConfig *m_pConfig;     ///< the configuration of the plugin
	cHyperionClient *m_pClient;     ///< the client, which gets the changed configuration

	// local copies of the settings
	int m_enable;
	char m_host[64];
	int m_port;
	int m_priority;
	int m_fps;
	int m_width;
	int m_jsonPort;
	char m_token[64];
	int m_offAction;
	int m_ledsFollowAttach;
	int m_idleMode;
	char m_idleEffect[64];
	int m_hideMainMenuEntry;
	int m_pauseBrightness;
	cOsdItem *m_settingsItem = nullptr; ///< opens the menu of the settings of Hyperion
	cOsdItem *m_tokenItem = nullptr;    ///< requests a token
	cOsdItem *m_searchItem = nullptr;   ///< searches Hyperion servers
	cOsdItem *m_testItem = nullptr;     ///< shows test patterns
	const char *m_offActions[2];    ///< texts of the switch off actions
	const char *m_idleModes[3];     ///< texts of the modes without video

	/// an item, whose text is updated, while the menu is shown
	struct sLiveItem {
		cOsdItem *item;
		std::function<cString(void)> text;
	};
	static constexpr int LIVE_UPDATE_INTERVAL_MS = 1000; ///< update interval of the state
	std::vector<sLiveItem> m_liveItems;                  ///< state items of the shown menu
	cTimeMs m_liveUpdateTimer;                           ///< timer for the next update of the state

	void Create(void);
	void AddLiveItem(std::function<cString(void)>, bool selectable = false);
	void UpdateLiveItems(void);
	sHyperionConfig ShownServer(void) const;
	cString TokenText(void);
	void CheckToken(void);
};

#endif
