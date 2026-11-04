// SPDX-License-Identifier: AGPL-3.0-or-later

/**
 * @file setup.cpp
 * Setup Menu
 *
 * @copyright 2026 by Andreas Baierl. All Rights Reserved.
 *
 * @license{AGPL-3.0-or-later}
 */

#include <algorithm>
#include <ctime>

#include <vdr/config.h>
#include <vdr/i18n.h>
#include <vdr/skins.h>
#include <vdr/tools.h>

#include "client.h"
#include "setup.h"

/**
 * Describe the state of the ambient light
 */
static cString StateText(const sHyperionStats &stats)
{
	switch (stats.state) {
		case sHyperionStats::STATE_NO_OUTPUT: return tr("no output plugin provides the video");
		case sHyperionStats::STATE_DETACHED:  return tr("output device detached");
		case sHyperionStats::STATE_OFF:       return tr("switched off");
		case sHyperionStats::STATE_VIDEO:     return cString::sprintf(tr("video, %d images/s"), stats.imagesPerSecond);
		case sHyperionStats::STATE_PAUSED:    return tr("video paused");
		case sHyperionStats::STATE_KEPT:      return tr("no video, the last image is kept");
		case sHyperionStats::STATE_RELEASED:  return tr("no video, the leds are given back to Hyperion");
		case sHyperionStats::STATE_EFFECT:    return tr("no video, effect");
		case sHyperionStats::STATE_DARK:      return tr("no video, dark");
		default:                              return tr("disabled");
	}
}

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
	m_pauseBrightness = m_pConfig->pauseBrightness;

	m_offActions[sHyperionConfig::OFF_LEDDEVICE] = tr("leds off");
	m_offActions[sHyperionConfig::OFF_VIDEO] = tr("stop video");
	m_idleModes[sHyperionConfig::IDLE_RELEASE] = tr("Hyperion");
	m_idleModes[sHyperionConfig::IDLE_EFFECT] = tr("effect");
	m_idleModes[sHyperionConfig::IDLE_BLACK] = tr("dark");

	Create();
}

/**
 * Close the page (a running token request is cancelled)
 */
cMenuSetupHyperion::~cMenuSetupHyperion()
{
	std::string token, code, error;
	int seconds;
	if (m_pClient->GetTokenRequest(&token, &code, &seconds, &error) != cHyperionClient::TOKEN_IDLE)
		m_pClient->CancelTokenRequest();
}

/**
 * Create the items of the page
 */
void cMenuSetupHyperion::Create(void)
{
	int current = Current();
	Clear();
	m_liveItems.clear();
	m_settingsItem = nullptr;
	m_tokenItem = nullptr;

	Add(new cMenuEditBoolItem(tr("Send video to Hyperion"), &m_enable, trVDR("no"), trVDR("yes")));
	if (m_enable) {
		Add(new cMenuEditStrItem(tr(" Host"), m_host, sizeof(m_host), "abcdefghijklmnopqrstuvwxyz0123456789.-:"));
		Add(new cMenuEditIntItem(tr(" Port (flatbuffers server)"), &m_port, 1, 65535));
		Add(new cMenuEditIntItem(tr(" Priority"), &m_priority, 100, 199));
		Add(new cMenuEditIntItem(tr(" Images per second"), &m_fps, 1, 50));
		Add(new cMenuEditIntItem(tr(" Image width (pixels)"), &m_width, 16, 320));
		Add(new cMenuEditIntItem(tr(" Port (JSON server)"), &m_jsonPort, 1, 65535));
		Add(new cMenuEditStrItem(tr(" Token (JSON server)"), m_token, sizeof(m_token), "abcdefghijklmnopqrstuvwxyz0123456789-"));
		AddLiveItem([this]() { return TokenText(); }, true);
		m_tokenItem = m_liveItems.back().item;
		Add(new cMenuEditStraItem(tr(" Switch off"), &m_offAction, 2, m_offActions));
		Add(new cMenuEditBoolItem(tr(" Leds off when detached"), &m_ledsFollowAttach, trVDR("no"), trVDR("yes")));
		Add(new cMenuEditStraItem(tr(" Without video"), &m_idleMode, 3, m_idleModes));
		if (m_idleMode == sHyperionConfig::IDLE_EFFECT)
			Add(new cMenuEditStrItem(tr("  Effect"), m_idleEffect, sizeof(m_idleEffect)));
		Add(new cMenuEditIntItem(tr(" Dim on pause to (%)"), &m_pauseBrightness, 0, 100, NULL, trVDR("no")));
		Add(new cMenuEditBoolItem(tr(" Hide main menu entry"), &m_hideMainMenuEntry, trVDR("no"), trVDR("yes")));
		m_settingsItem = new cOsdItem(tr(" Settings in Hyperion..."));
		Add(m_settingsItem);

		// state
		Add(new cOsdItem(" ", osUnknown, false));
		Add(new cOsdItem(cString::sprintf("%s:", tr("State")), osUnknown, false));
		AddLiveItem([this]() {
			sHyperionStats stats;
			m_pClient->GetStats(&stats);
			return cString::sprintf(tr("Ambient light: %s"), *StateText(stats));
		});
		AddLiveItem([this]() {
			sHyperionStats stats;
			m_pClient->GetStats(&stats);
			return cString::sprintf(tr("Output plugin: %s"), stats.output.empty() ? "-" : stats.output.c_str());
		});
		AddLiveItem([this]() {
			sHyperionStats stats;
			m_pClient->GetStats(&stats);
			cString leds = stats.ledDevice < 0 ? cString("") : cString::sprintf(", %s", stats.ledDevice ? tr("leds on") : tr("leds off"));
			return cString::sprintf(tr("Hyperion server: %s, %s, priority %d%s"), stats.server.c_str(),
				stats.connected ? tr("connected") : tr("not connected"), stats.priority, *leds);
		});
		AddLiveItem([this]() {
			sHyperionStats stats;
			m_pClient->GetStats(&stats);
			if (stats.format.empty())
				return cString(tr("Image: -"));
			return cString::sprintf(tr("Image: %s -> %dx%d, %d.%02d ms (max. %d.%02d ms)"), stats.format.c_str(), stats.width, stats.height,
				stats.sampleUsAvg / 1000, stats.sampleUsAvg % 1000 / 10, stats.sampleUsMax / 1000, stats.sampleUsMax % 1000 / 10);
		});
		AddLiveItem([this]() {
			sHyperionStats stats;
			m_pClient->GetStats(&stats);
			if (stats.lastError.empty())
				return cString(tr("Error: -"));
			struct tm tm;
			char t[16];
			strftime(t, sizeof(t), "%H:%M:%S", localtime_r(&stats.lastErrorTime, &tm));
			return cString::sprintf(tr("Error: %s (%s)"), stats.lastError.c_str(), t);
		});
		m_liveUpdateTimer.Set(LIVE_UPDATE_INTERVAL_MS);
	}

	SetCurrent(Get(current));
	Display();
}

/**
 * Add an item, whose text is updated, while the menu is shown
 */
void cMenuSetupHyperion::AddLiveItem(std::function<cString(void)> text, bool selectable)
{
	cOsdItem *item = new cOsdItem(text(), osUnknown, selectable);
	Add(item);
	m_liveItems.push_back({ item, std::move(text) });
}

/**
 * Update the texts of the state items
 */
void cMenuSetupHyperion::UpdateLiveItems(void)
{
	bool changed = false;

	for (auto &live : m_liveItems) {
		cString text = live.text();
		if (strcmp(text, live.item->Text())) {
			live.item->SetText(text);
			changed = true;
		}
	}

	if (changed)
		Display();
}

/**
 * Handle a key
 */
eOSState cMenuSetupHyperion::ProcessKey(eKeys key)
{
	// open the settings of Hyperion with the server, as it is shown
	if (key == kOk && !HasSubMenu() && m_settingsItem && Get(Current()) == m_settingsItem)
		return AddSubMenu(new cMenuHyperionSettings(m_pClient, ShownServer()));

	// request a token or cancel the request
	if (key == kOk && !HasSubMenu() && m_tokenItem && Get(Current()) == m_tokenItem) {
		std::string token, code, error;
		int seconds;
		if (m_pClient->GetTokenRequest(&token, &code, &seconds, &error) == cHyperionClient::TOKEN_WAITING)
			m_pClient->CancelTokenRequest();
		else
			m_pClient->RequestToken(ShownServer());
		UpdateLiveItems();
		return osContinue;
	}

	int oldEnable = m_enable;
	int oldIdleMode = m_idleMode;

	eOSState state = cMenuSetupPage::ProcessKey(key);

	if (key != kNone && (oldEnable != m_enable || oldIdleMode != m_idleMode))
		Create();

	// update the state, while the menu is shown
	if (key == kNone && !HasSubMenu() && !m_liveItems.empty() && m_liveUpdateTimer.TimedOut()) {
		CheckToken();
		UpdateLiveItems();
		m_liveUpdateTimer.Set(LIVE_UPDATE_INTERVAL_MS);
	}

	return state;
}

/**
 * The server, as it is shown (maybe not stored yet)
 */
sHyperionConfig cMenuSetupHyperion::ShownServer(void) const
{
	sHyperionConfig server;
	char host[sizeof(m_host)];
	char token[sizeof(m_token)];
	strn0cpy(host, m_host, sizeof(host));
	strn0cpy(token, m_token, sizeof(token));
	stripspace(host);
	stripspace(token);
	server.host = *host ? host : "127.0.0.1";
	server.jsonPort = m_jsonPort;
	server.token = token;
	return server;
}

/**
 * Text of the item, which requests a token
 */
cString cMenuSetupHyperion::TokenText(void)
{
	std::string token, code, error;
	int seconds;

	switch (m_pClient->GetTokenRequest(&token, &code, &seconds, &error)) {
		case cHyperionClient::TOKEN_WAITING:
			return cString::sprintf(tr(" Accept the code %s in the web interface of Hyperion (%d s)"), code.c_str(), seconds);
		case cHyperionClient::TOKEN_ERROR:
			return cString::sprintf(tr(" Token request failed: %s"), error.c_str());
		default:
			return tr(" Request token...");
	}
}

/**
 * Store a received token at once (it can't be shown again)
 */
void cMenuSetupHyperion::CheckToken(void)
{
	std::string token, code, error;
	int seconds;
	if (m_pClient->GetTokenRequest(&token, &code, &seconds, &error) != cHyperionClient::TOKEN_RECEIVED)
		return;

	m_pClient->CancelTokenRequest();     // back to TOKEN_IDLE

	strn0cpy(m_token, token.c_str(), sizeof(m_token));
	SetupStore("Token", m_token);
	m_pConfig->token = m_token;
	Setup.Save();
	m_pClient->Configure(*m_pConfig);

	Create();
	Skins.Message(mtInfo, tr("Token received and stored"));
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
	SetupStore("PauseBrightness", m_pConfig->pauseBrightness = m_pauseBrightness);

	m_pClient->Configure(*m_pConfig);
}

/*****************************************************************************
 * Settings in Hyperion
 ****************************************************************************/

/**
 * Create the menu and read the settings
 */
cMenuHyperionSettings::cMenuHyperionSettings(cHyperionClient *client, const sHyperionConfig &server)
	: cOsdMenu(tr("Settings in Hyperion"), 33),
	  m_pClient(client),
	  m_server(server),
	  m_state(cHyperionClient::SETTINGS_LOADING)
{
	m_borderModes[sHyperionSettings::BORDER_DEFAULT] = tr("default");
	m_borderModes[sHyperionSettings::BORDER_CLASSIC] = tr("classic");
	m_borderModes[sHyperionSettings::BORDER_OSD] = tr("osd");
	m_borderModes[sHyperionSettings::BORDER_LETTERBOX] = tr("letterbox");
	m_delayTitle[0] = 0;

	SetMenuCategory(mcPluginSetup);
	m_pClient->LoadSettings(m_server);
	Create();
}

/**
 * Create the items (depending on the state)
 */
void cMenuHyperionSettings::Create(void)
{
	int current = Current();
	Clear();

	switch (m_state) {
		case cHyperionClient::SETTINGS_LOADED:
		case cHyperionClient::SETTINGS_SAVED:
			snprintf(m_delayTitle, sizeof(m_delayTitle), tr(" Delay (frames of %d ms)"), 1000 / std::max(1, m_loaded.smoothingFrequency));
			Add(new cMenuEditBoolItem(tr("Smoothing"), &m_smoothing, trVDR("no"), trVDR("yes")));
			Add(new cMenuEditIntItem(tr(" Time (ms)"), &m_smoothingTimeMs, 1, 5000));
			Add(new cMenuEditIntItem(m_delayTitle, &m_smoothingDelay, 0, 2048));
			Add(new cMenuEditBoolItem(tr("Black border detection"), &m_blackBorder, trVDR("no"), trVDR("yes")));
			Add(new cMenuEditIntItem(tr(" Threshold (%)"), &m_blackBorderThreshold, 0, 100));
			Add(new cMenuEditStraItem(tr(" Mode"), &m_blackBorderMode, sHyperionSettings::BORDER_MODES, m_borderModes));
			Add(new cMenuEditIntItem(tr("Brightness (%)"), &m_brightness, 0, 100));
			Add(new cOsdItem(" ", osUnknown, false));
			Add(new cOsdItem(tr("OK writes the settings into all instances of Hyperion."), osUnknown, false));
			break;
		case cHyperionClient::SETTINGS_ERROR:
			Add(new cOsdItem(cString::sprintf(tr("Error: %s"), m_error.c_str()), osUnknown, false));
			Add(new cOsdItem(" ", osUnknown, false));
			Add(new cOsdItem(tr("OK reads the settings again."), osUnknown, false));
			break;
		case cHyperionClient::SETTINGS_SAVING:
			Add(new cOsdItem(tr("Writing the settings..."), osUnknown, false));
			break;
		default:
			Add(new cOsdItem(tr("Reading the settings..."), osUnknown, false));
			break;
	}

	SetCurrent(Get(current));
	Display();
}

/**
 * The settings, as they are shown
 */
sHyperionSettings cMenuHyperionSettings::Edited(void) const
{
	sHyperionSettings s = m_loaded;
	s.smoothing = m_smoothing;
	s.smoothingTimeMs = m_smoothingTimeMs;
	s.smoothingDelay = m_smoothingDelay;
	s.blackBorder = m_blackBorder;
	s.blackBorderThreshold = m_blackBorderThreshold;
	s.blackBorderMode = m_blackBorderMode;
	s.brightness = m_brightness;
	return s;
}

/**
 * Handle a key
 */
eOSState cMenuHyperionSettings::ProcessKey(eKeys key)
{
	// the client thread has finished a request
	if (m_state == cHyperionClient::SETTINGS_LOADING || m_state == cHyperionClient::SETTINGS_SAVING) {
		sHyperionSettings settings;
		std::string error;
		int state = m_pClient->GetSettings(&settings, &error);

		if (state == cHyperionClient::SETTINGS_SAVED && m_state == cHyperionClient::SETTINGS_SAVING) {
			Skins.Message(mtInfo, tr("Settings written into Hyperion"));
			return osBack;
		}
		if (state == cHyperionClient::SETTINGS_LOADED || state == cHyperionClient::SETTINGS_SAVED) {
			m_loaded = settings;
			m_smoothing = settings.smoothing;
			m_smoothingTimeMs = settings.smoothingTimeMs;
			m_smoothingDelay = settings.smoothingDelay;
			m_blackBorder = settings.blackBorder;
			m_blackBorderThreshold = settings.blackBorderThreshold;
			m_blackBorderMode = settings.blackBorderMode;
			m_brightness = settings.brightness;
			m_state = cHyperionClient::SETTINGS_LOADED;
			Create();
		} else if (state == cHyperionClient::SETTINGS_ERROR) {
			m_error = error;
			m_state = state;
			Create();
		}
	}

	eOSState osState = cOsdMenu::ProcessKey(key);
	if (osState != osUnknown || key != kOk)
		return osState;

	switch (m_state) {
		case cHyperionClient::SETTINGS_LOADED:
			if (Edited() == m_loaded)
				return osBack;
			m_pClient->SaveSettings(m_server, Edited());
			m_state = cHyperionClient::SETTINGS_SAVING;
			Create();
			return osContinue;
		case cHyperionClient::SETTINGS_ERROR:
			m_pClient->LoadSettings(m_server);
			m_state = cHyperionClient::SETTINGS_LOADING;
			Create();
			return osContinue;
		default:
			return osContinue;
	}
}
