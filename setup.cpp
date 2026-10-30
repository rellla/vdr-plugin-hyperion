// SPDX-License-Identifier: AGPL-3.0-or-later

/**
 * @file setup.cpp
 * Setup Menu
 *
 * @copyright 2026 by Andreas Baierl. All Rights Reserved.
 *
 * @license{AGPL-3.0-or-later}
 */

#include <vdr/i18n.h>
#include <vdr/tools.h>

#include "client.h"
#include "setup.h"

/**
 * Create the setup page
 */
cMenuSetupHyperion::cMenuSetupHyperion(sHyperionConfig *config, cHyperionClient *client)
	: m_pConfig(config),
	  m_pClient(client)
{
	m_enable = m_pConfig->enable;
	strn0cpy(m_host, m_pConfig->host.c_str(), sizeof(m_host));
	m_port = m_pConfig->port;
	m_priority = m_pConfig->priority;
	m_fps = m_pConfig->fps;
	m_width = m_pConfig->width;
	m_jsonPort = m_pConfig->jsonPort;
	strn0cpy(m_token, m_pConfig->token.c_str(), sizeof(m_token));
	m_offAction = m_pConfig->offAction;
	m_ledsFollowAttach = m_pConfig->ledsFollowAttach;
	m_idleMode = m_pConfig->idleMode;
	strn0cpy(m_idleEffect, m_pConfig->idleEffect.c_str(), sizeof(m_idleEffect));
	m_hideMainMenuEntry = m_pConfig->hideMainMenuEntry;

	m_offActions[sHyperionConfig::OFF_LEDDEVICE] = tr("leds off");
	m_offActions[sHyperionConfig::OFF_VIDEO] = tr("stop video");
	m_idleModes[sHyperionConfig::IDLE_RELEASE] = tr("Hyperion");
	m_idleModes[sHyperionConfig::IDLE_EFFECT] = tr("effect");
	m_idleModes[sHyperionConfig::IDLE_BLACK] = tr("dark");

	Create();
}

/**
 * Create the items of the page
 */
void cMenuSetupHyperion::Create(void)
{
	int current = Current();
	Clear();

	Add(new cMenuEditBoolItem(tr("Send video to Hyperion"), &m_enable, trVDR("no"), trVDR("yes")));
	if (m_enable) {
		Add(new cMenuEditStrItem(tr(" Host"), m_host, sizeof(m_host), "abcdefghijklmnopqrstuvwxyz0123456789.-:"));
		Add(new cMenuEditIntItem(tr(" Port (flatbuffers server)"), &m_port, 1, 65535));
		Add(new cMenuEditIntItem(tr(" Priority"), &m_priority, 100, 199));
		Add(new cMenuEditIntItem(tr(" Images per second"), &m_fps, 1, 50));
		Add(new cMenuEditIntItem(tr(" Image width (pixels)"), &m_width, 16, 320));
		Add(new cMenuEditIntItem(tr(" Port (JSON server)"), &m_jsonPort, 1, 65535));
		Add(new cMenuEditStrItem(tr(" Token (JSON server)"), m_token, sizeof(m_token), "abcdefghijklmnopqrstuvwxyz0123456789-"));
		Add(new cMenuEditStraItem(tr(" Switch off"), &m_offAction, 2, m_offActions));
		Add(new cMenuEditBoolItem(tr(" Leds off when detached"), &m_ledsFollowAttach, trVDR("no"), trVDR("yes")));
		Add(new cMenuEditStraItem(tr(" Without video"), &m_idleMode, 3, m_idleModes));
		if (m_idleMode == sHyperionConfig::IDLE_EFFECT)
			Add(new cMenuEditStrItem(tr("  Effect"), m_idleEffect, sizeof(m_idleEffect)));
		Add(new cMenuEditBoolItem(tr(" Hide main menu entry"), &m_hideMainMenuEntry, trVDR("no"), trVDR("yes")));
	}

	SetCurrent(Get(current));
	Display();
}

/**
 * Handle a key
 */
eOSState cMenuSetupHyperion::ProcessKey(eKeys key)
{
	int oldEnable = m_enable;
	int oldIdleMode = m_idleMode;

	eOSState state = cMenuSetupPage::ProcessKey(key);

	if (key != kNone && (oldEnable != m_enable || oldIdleMode != m_idleMode))
		Create();

	return state;
}

/**
 * Store the settings and apply them
 */
void cMenuSetupHyperion::Store(void)
{
	// cMenuEditStrItem pads with blanks
	stripspace(m_host);
	if (!*m_host)
		strn0cpy(m_host, "127.0.0.1", sizeof(m_host));

	SetupStore("Enable", m_pConfig->enable = m_enable);
	SetupStore("Host", m_host);
	m_pConfig->host = m_host;
	SetupStore("Port", m_pConfig->port = m_port);
	SetupStore("Priority", m_pConfig->priority = m_priority);
	SetupStore("Fps", m_pConfig->fps = m_fps);
	SetupStore("Width", m_pConfig->width = m_width);
	SetupStore("JsonPort", m_pConfig->jsonPort = m_jsonPort);
	stripspace(m_token);
	SetupStore("Token", m_token);
	m_pConfig->token = m_token;
	SetupStore("OffAction", m_pConfig->offAction = m_offAction);
	SetupStore("LedsFollowAttach", m_pConfig->ledsFollowAttach = m_ledsFollowAttach);
	SetupStore("IdleMode", m_pConfig->idleMode = m_idleMode);
	stripspace(m_idleEffect);
	if (!*m_idleEffect)
		strn0cpy(m_idleEffect, "Rainbow swirl", sizeof(m_idleEffect));
	SetupStore("IdleEffect", m_idleEffect);
	m_pConfig->idleEffect = m_idleEffect;
	SetupStore("HideMainMenuEntry", m_pConfig->hideMainMenuEntry = m_hideMainMenuEntry);

	m_pClient->Configure(*m_pConfig);
}
