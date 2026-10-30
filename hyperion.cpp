// SPDX-License-Identifier: AGPL-3.0-or-later

/**
 * @file hyperion.cpp
 * VDR Plugin 'hyperion'
 *
 * Sends the displayed video to Hyperion (ambient lighting). The image of the
 * video comes from the output plugin (service AmbientImage-v1.0, provided by
 * drmdevice).
 *
 * @copyright 2026 by Andreas Baierl. All Rights Reserved.
 *
 * @license{AGPL-3.0-or-later}
 */

#include <vdr/i18n.h>
#include <vdr/plugin.h>

#include "client.h"
#include "config.h"
#include "setup.h"

static const char *VERSION        = "0.1.0";
static const char *DESCRIPTION    = trNOOP("Ambient light with Hyperion");

/**
 * The plugin
 */
class cPluginHyperion : public cPlugin {
public:
	cPluginHyperion(void) = default;
	virtual ~cPluginHyperion(void);
	virtual const char *Version(void) { return VERSION; }
	virtual const char *Description(void) { return tr(DESCRIPTION); }
	virtual bool Start(void);
	virtual void Stop(void);
	virtual cMenuSetupPage *SetupMenu(void);
	virtual bool SetupParse(const char *, const char *);

private:
	sHyperionConfig m_config;               ///< the settings
	cHyperionClient *m_pClient = nullptr;   ///< sends the video to Hyperion
};

cPluginHyperion::~cPluginHyperion(void)
{
	delete m_pClient;
}

/**
 * Start the client thread
 */
bool cPluginHyperion::Start(void)
{
	m_pClient = new cHyperionClient();
	m_pClient->Configure(m_config);
	m_pClient->Start();

	return true;
}

/**
 * Stop the client thread (gives the leds back)
 */
void cPluginHyperion::Stop(void)
{
	if (m_pClient)
		m_pClient->Stop();
}

/**
 * Create the setup page
 */
cMenuSetupPage *cPluginHyperion::SetupMenu(void)
{
	return m_pClient ? new cMenuSetupHyperion(&m_config, m_pClient) : nullptr;
}

/**
 * Parse a setup.conf parameter
 */
bool cPluginHyperion::SetupParse(const char *name, const char *value)
{
	return m_config.Parse(name, value);
}

VDRPLUGINCREATOR(cPluginHyperion); // Don't touch this!
