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

#include <strings.h>

#include <vdr/i18n.h>
#include <vdr/plugin.h>
#include <vdr/skins.h>

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
	virtual const char *MainMenuEntry(void);
	virtual cOsdObject *MainMenuAction(void);
	virtual cMenuSetupPage *SetupMenu(void);
	virtual bool SetupParse(const char *, const char *);
	virtual const char **SVDRPHelpPages(void);
	virtual cString SVDRPCommand(const char *, const char *, int &);

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
 * Main menu entry: switches the ambient light on or off
 */
const char *cPluginHyperion::MainMenuEntry(void)
{
	if (m_config.hideMainMenuEntry || !m_config.enable || !m_pClient)
		return nullptr;

	return m_pClient->IsOn() ? tr("Switch ambient light off") : tr("Switch ambient light on");
}

/**
 * Switch the ambient light on or off
 *
 * Called with the main menu entry or a key macro (@hyperion), no menu is opened.
 */
cOsdObject *cPluginHyperion::MainMenuAction(void)
{
	if (!m_pClient || !m_config.enable) {
		Skins.QueueMessage(mtInfo, tr("Ambient light: disabled in the setup"));
		return nullptr;
	}

	m_pClient->SetOn(!m_pClient->IsOn());
	Skins.QueueMessage(mtInfo, m_pClient->IsOn() ? tr("Ambient light: on") : tr("Ambient light: off"));

	return nullptr;
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

/**
 * Help pages of the SVDRP commands
 */
const char **cPluginHyperion::SVDRPHelpPages(void)
{
	static const char *HelpPages[] = {
		"AMBI [on | off | toggle]\n"
		"    Switch the ambient light on or off (not stored).\n"
		"    Without an argument the ambient light state is returned.",
		nullptr
	};

	return HelpPages;
}

/**
 * Handle the SVDRP commands
 */
cString cPluginHyperion::SVDRPCommand(const char *command, const char *option, int &replyCode)
{
	if (strcasecmp(command, "AMBI"))
		return nullptr;

	if (!m_pClient || !m_config.enable) {
		replyCode = 550;
		return "Ambient light: disabled in the setup";
	}

	if (option && !strcasecmp(option, "on")) {
		m_pClient->SetOn(true);
	} else if (option && !strcasecmp(option, "off")) {
		m_pClient->SetOn(false);
	} else if (option && !strcasecmp(option, "toggle")) {
		m_pClient->SetOn(!m_pClient->IsOn());
	} else if (option && *option) {
		replyCode = 501;
		return cString::sprintf("Invalid argument '%s'", option);
	}

	return m_pClient->IsOn() ? "Ambient light: on" : "Ambient light: off";
}

VDRPLUGINCREATOR(cPluginHyperion); // Don't touch this!
