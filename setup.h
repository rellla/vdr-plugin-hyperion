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
#include <vector>

#include <vdr/menuitems.h>
#include <vdr/tools.h>

#include "config.h"

class cHyperionClient;

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
