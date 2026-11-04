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

#include <functional>
#include <string>
#include <vector>

#include <vdr/menuitems.h>
#include <vdr/tools.h>

#include "config.h"
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
 * Setup page of the plugin
 */
class cMenuSetupHyperion : public cMenuSetupPage {
public:
	cMenuSetupHyperion(sHyperionConfig *, cHyperionClient *);
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
	void AddLiveItem(std::function<cString(void)>);
	void UpdateLiveItems(void);
};

#endif
