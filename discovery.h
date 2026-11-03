// SPDX-License-Identifier: AGPL-3.0-or-later

/**
 * @file discovery.h
 * Hyperion Discovery Header File
 *
 * @copyright 2026 by Andreas Baierl. All Rights Reserved.
 *
 * @license{AGPL-3.0-or-later}
 */

#ifndef __HYPERION_DISCOVERY_H
#define __HYPERION_DISCOVERY_H

#include <atomic>
#include <string>
#include <vector>

/**
 * A Hyperion server found in the network
 */
struct sHyperionServer {
	std::string name;                   ///< name of the server (HYPERION-NAME)
	std::string host;                   ///< address of the server
	int jsonPort = 0;                   ///< port of the JSON server
	int flatbufPort = 0;                ///< port of the flatbuffers server (0: disabled)
	std::string uuid;                   ///< unique id of the server
};

/**
 * Finds Hyperion servers in the local network with SSDP
 *
 * Hyperion answers a search for "urn:hyperion-project.org:device:basic:1"
 * with its name and the ports of its servers (SSDPHandler of Hyperion).
 */
class cHyperionDiscovery {
public:
	static constexpr const char *SEARCH_TARGET = "urn:hyperion-project.org:device:basic:1";

	static std::string SearchRequest(void);
	static bool ParseResponse(const std::string &response, const std::string &sender, sHyperionServer *server);
	static std::vector<sHyperionServer> Search(int timeoutMs, const std::atomic<bool> *cancel);
};

#endif
