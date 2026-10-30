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

	eOSState state = cMenuSetupPage::ProcessKey(key);

	if (key != kNone && oldEnable != m_enable)
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

	m_pClient->Configure(*m_pConfig);
}
