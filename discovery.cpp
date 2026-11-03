// SPDX-License-Identifier: AGPL-3.0-or-later

/**
 * @file discovery.cpp
 * Hyperion Discovery
 *
 * @copyright 2026 by Andreas Baierl. All Rights Reserved.
 *
 * @license{AGPL-3.0-or-later}
 */

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "discovery.h"

namespace {

constexpr const char *SSDP_ADDRESS = "239.255.255.250";
constexpr int SSDP_PORT = 1900;

std::string Lower(std::string s)
{
	for (char &c : s)
		c = (char)tolower((unsigned char)c);
	return s;
}

std::string Trim(const std::string &s)
{
	size_t b = s.find_first_not_of(" \t");
	size_t e = s.find_last_not_of(" \t\r");
	return b == std::string::npos ? "" : s.substr(b, e - b + 1);
}

int Port(const std::string &s)
{
	char *end;
	long port = strtol(s.c_str(), &end, 10);
	return s.empty() || *end || port < 1 || port > 65535 ? 0 : (int)port;
}

uint64_t NowMs(void)
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

} // namespace

/**
 * The search request (M-SEARCH), sent to the SSDP multicast address
 *
 * Hyperion answers within MX seconds.
 */
std::string cHyperionDiscovery::SearchRequest(void)
{
	return std::string("M-SEARCH * HTTP/1.1\r\n"
	                   "HOST: 239.255.255.250:1900\r\n"
	                   "MAN: \"ssdp:discover\"\r\n"
	                   "MX: 1\r\n"
	                   "ST: ") + SEARCH_TARGET + "\r\n\r\n";
}

/**
 * Read the answer of a Hyperion server
 *
 * @param response      the answer
 * @param sender        address of the sender (the server is reached there)
 * @param[out] server   the server
 * @return              false, if it isn't the answer of a Hyperion server
 */
bool cHyperionDiscovery::ParseResponse(const std::string &response, const std::string &sender, sHyperionServer *server)
{
	size_t eol = response.find('\n');
	if (Lower(Trim(response.substr(0, eol))).rfind("http/1.1 200", 0) != 0)
		return false;

	sHyperionServer s;
	std::string st;
	bool hasJsonPort = false;

	size_t pos = eol;
	while (pos != std::string::npos && pos < response.size()) {
		size_t next = response.find('\n', pos + 1);
		std::string line = response.substr(pos + 1, next == std::string::npos ? std::string::npos : next - pos - 1);
		pos = next;

		size_t colon = line.find(':');
		if (colon == std::string::npos)
			continue;
		std::string key = Lower(Trim(line.substr(0, colon)));
		std::string value = Trim(line.substr(colon + 1));

		if (key == "st")
			st = value;
		else if (key == "usn")
			s.uuid = value.rfind("uuid:", 0) == 0 ? value.substr(5) : value;
		else if (key == "hyperion-name")
			s.name = value;
		else if (key == "hyperion-jss-port") {
			s.jsonPort = Port(value);
			hasJsonPort = true;
		} else if (key == "hyperion-fbs-port")
			s.flatbufPort = Port(value);
	}

	if (st != SEARCH_TARGET || !hasJsonPort || !s.jsonPort || sender.empty())
		return false;

	s.host = sender;
	if (s.name.empty())
		s.name = "Hyperion";
	*server = s;
	return true;
}

/**
 * Search Hyperion servers
 *
 * Blocks for timeoutMs (or until cancel is set). Every server is returned
 * once, sorted by name and address.
 *
 * @param timeoutMs     time to wait for answers
 * @param cancel        stops the search, if set (optional)
 * @return              the found servers
 */
std::vector<sHyperionServer> cHyperionDiscovery::Search(int timeoutMs, const std::atomic<bool> *cancel)
{
	std::vector<sHyperionServer> servers;

	int fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
	if (fd < 0)
		return servers;

	unsigned char ttl = 2;
	setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));

	struct sockaddr_in addr = {};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(SSDP_PORT);
	inet_pton(AF_INET, SSDP_ADDRESS, &addr.sin_addr);

	std::string request = SearchRequest();
	uint64_t start = NowMs();
	uint64_t deadline = start + timeoutMs;
	int sent = 0;

	while (!(cancel && *cancel)) {
		uint64_t now = NowMs();
		if (now >= deadline)
			break;

		// UDP may get lost: the request is sent twice
		if (sent < 2 && now >= start + sent * 300) {
			sendto(fd, request.data(), request.size(), 0, (struct sockaddr *)&addr, sizeof(addr));
			sent++;
		}

		struct pollfd pfd = { fd, POLLIN, 0 };
		if (poll(&pfd, 1, (int)std::min<uint64_t>(100, deadline - now)) <= 0)
			continue;

		char buf[2048];
		struct sockaddr_in from = {};
		socklen_t fromLen = sizeof(from);
		ssize_t n = recvfrom(fd, buf, sizeof(buf), MSG_DONTWAIT, (struct sockaddr *)&from, &fromLen);
		if (n <= 0)
			continue;

		char host[INET_ADDRSTRLEN] = "";
		inet_ntop(AF_INET, &from.sin_addr, host, sizeof(host));

		sHyperionServer server;
		if (!ParseResponse(std::string(buf, n), host, &server))
			continue;

		auto same = [&server](const sHyperionServer &s) {
			return server.uuid.empty() ? s.host == server.host : s.uuid == server.uuid;
		};
		if (std::find_if(servers.begin(), servers.end(), same) == servers.end())
			servers.push_back(server);
	}
	close(fd);

	std::sort(servers.begin(), servers.end(), [](const sHyperionServer &a, const sHyperionServer &b) {
		return a.name != b.name ? a.name < b.name : a.host < b.host;
	});
	return servers;
}
