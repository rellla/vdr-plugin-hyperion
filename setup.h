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

#include <vdr/menuitems.h>

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
	const char *m_offActions[2];    ///< texts of the switch off actions
	const char *m_idleModes[3];     ///< texts of the modes without video

	void Create(void);
};

#endif
