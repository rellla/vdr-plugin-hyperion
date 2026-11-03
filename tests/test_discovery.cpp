/**
 * @file test_discovery.cpp
 * Unit tests for cHyperionDiscovery
 *
 * The answers follow SSDPServer::sendMSearchResponse() and
 * SSDPHandler::getInfo() of Hyperion.
 */

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <string>

#include "discovery.h"

namespace {

const char *kResponse =
	"HTTP/1.1 200 OK\r\n"
	"CACHE-CONTROL: max-age = 1800\r\n"
	"DATE: Tue, 03 Nov 2026 21:40:00 GMT\r\n"
	"EXT: \r\n"
	"LOCATION: http://192.168.1.20:8090/description.xml\r\n"
	"SERVER: Linux/6.12 UPnP/1.0 Hyperion/2.1.1\r\n"
	"ST: urn:hyperion-project.org:device:basic:1\r\n"
	"USN: uuid:c9a5e2a1-1234-4bcd-9f00-0123456789ab\r\n"
	"HYPERION-NAME: Wohnzimmer\r\n"
	"HYPERION-JSS-PORT: 19444\r\n"
	"HYPERION-FBS-PORT: 19400\r\n"
	"HYPERION-PBS-PORT: 19445\r\n"
	"\r\n";

} // namespace

TEST_CASE("HyperionDiscovery: search request", "[hyperion]")
{
	CHECK(cHyperionDiscovery::SearchRequest() ==
		"M-SEARCH * HTTP/1.1\r\n"
		"HOST: 239.255.255.250:1900\r\n"
		"MAN: \"ssdp:discover\"\r\n"
		"MX: 1\r\n"
		"ST: urn:hyperion-project.org:device:basic:1\r\n\r\n");
}

TEST_CASE("HyperionDiscovery: answers", "[hyperion]")
{
	sHyperionServer server;

	SECTION("answer of Hyperion")
	{
		REQUIRE(cHyperionDiscovery::ParseResponse(kResponse, "192.168.1.20", &server));
		CHECK(server.name == "Wohnzimmer");
		CHECK(server.host == "192.168.1.20");
		CHECK(server.jsonPort == 19444);
		CHECK(server.flatbufPort == 19400);
		CHECK(server.uuid == "c9a5e2a1-1234-4bcd-9f00-0123456789ab");
	}

	SECTION("without flatbuffers server and name")
	{
		std::string r = kResponse;
		r.erase(r.find("HYPERION-FBS-PORT"), strlen("HYPERION-FBS-PORT: 19400\r\n"));
		r.erase(r.find("HYPERION-NAME"), strlen("HYPERION-NAME: Wohnzimmer\r\n"));
		REQUIRE(cHyperionDiscovery::ParseResponse(r, "10.0.0.2", &server));
		CHECK(server.flatbufPort == 0);
		CHECK(server.name == "Hyperion");
	}

	SECTION("lower case keys and line feeds only")
	{
		REQUIRE(cHyperionDiscovery::ParseResponse("http/1.1 200 OK\nst: urn:hyperion-project.org:device:basic:1\nhyperion-jss-port: 19555\n", "10.0.0.3", &server));
		CHECK(server.jsonPort == 19555);
	}

	SECTION("other devices and invalid answers")
	{
		std::string other = kResponse;
		other.replace(other.find("urn:hyperion-project.org:device:basic:1"), strlen("urn:hyperion-project.org:device:basic:1"), "upnp:rootdevice");
		CHECK_FALSE(cHyperionDiscovery::ParseResponse(other, "192.168.1.20", &server));

		std::string notify = kResponse;
		notify.replace(0, strlen("HTTP/1.1 200 OK"), "NOTIFY * HTTP/1.1");
		CHECK_FALSE(cHyperionDiscovery::ParseResponse(notify, "192.168.1.20", &server));

		std::string badPort = kResponse;
		badPort.replace(badPort.find("19444"), 5, "99999");
		CHECK_FALSE(cHyperionDiscovery::ParseResponse(badPort, "192.168.1.20", &server));

		CHECK_FALSE(cHyperionDiscovery::ParseResponse("", "192.168.1.20", &server));
		CHECK_FALSE(cHyperionDiscovery::ParseResponse(kResponse, "", &server));
	}
}
