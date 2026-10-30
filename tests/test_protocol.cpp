/**
 * @file test_protocol.cpp
 * Unit tests for cHyperionProtocol
 *
 * The expected messages were checked with the verifier, which flatc generates
 * from hyperion_request.fbs (the same check the Hyperion server does), and the
 * replies were created with the flatbuffers builder from hyperion_reply.fbs,
 * as Hyperion creates them.
 */

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstring>
#include <random>
#include <vector>

#include "protocol.h"

namespace {

// Register("VDR", 150)
const uint8_t kRegister[] = {
	0x00, 0x00, 0x00, 0x34, 0x0c, 0x00, 0x00, 0x00, 0x08, 0x00, 0x09, 0x00, 0x08, 0x00, 0x04, 0x00,
	0x08, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x04, 0x00, 0x08, 0x00, 0x0c, 0x00, 0x04, 0x00,
	0x08, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x96, 0x00, 0x00, 0x00,
	0x03, 0x00, 0x00, 0x00, 0x56, 0x44, 0x52, 0x00};

// Clear(150)
const uint8_t kClear[] = {
	0x00, 0x00, 0x00, 0x24, 0x0c, 0x00, 0x00, 0x00, 0x08, 0x00, 0x09, 0x00, 0x08, 0x00, 0x04, 0x00,
	0x08, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x03, 0x00, 0x06, 0x00, 0x08, 0x00, 0x04, 0x00,
	0x06, 0x00, 0x00, 0x00, 0x96, 0x00, 0x00, 0x00};

// Image(RawImage({1, 2, 3, 4, 5, 6}, 2, 1), -1)
const uint8_t kImage[] = {
	0x00, 0x00, 0x00, 0x54, 0x0c, 0x00, 0x00, 0x00, 0x08, 0x00, 0x09, 0x00, 0x08, 0x00, 0x04, 0x00,
	0x08, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x02, 0x00, 0x0a, 0x00, 0x0d, 0x00, 0x0c, 0x00,
	0x04, 0x00, 0x08, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
	0x01, 0x00, 0x0a, 0x00, 0x10, 0x00, 0x04, 0x00, 0x08, 0x00, 0x0c, 0x00, 0x0a, 0x00, 0x00, 0x00,
	0x0c, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00,
	0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x00, 0x00};

// Reply(registered = 150), as sent after a Register
const uint8_t kReplyRegistered[] = {
	0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00,
	0x0a, 0x00, 0x00, 0x00, 0x96, 0x00, 0x00, 0x00};

// Reply(error = "bad priority")
const uint8_t kReplyError[] = {
	0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x08, 0x00, 0x04, 0x00, 0x06, 0x00, 0x00, 0x00,
	0x04, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x62, 0x61, 0x64, 0x20, 0x70, 0x72, 0x69, 0x6f,
	0x72, 0x69, 0x74, 0x79, 0x00, 0x00, 0x00, 0x00};

template <size_t N>
std::vector<uint8_t> Vec(const uint8_t (&a)[N]) { return std::vector<uint8_t>(a, a + N); }

uint32_t SizePrefix(const std::vector<uint8_t> &m)
{
	return ((uint32_t)m[0] << 24) | (m[1] << 16) | (m[2] << 8) | m[3];
}

} // namespace

TEST_CASE("HyperionProtocol: encoded requests", "[hyperion]")
{
	SECTION("Register") {
		CHECK(cHyperionProtocol::EncodeRegister("VDR", 150) == Vec(kRegister));
	}

	SECTION("Clear") {
		CHECK(cHyperionProtocol::EncodeClear(150) == Vec(kClear));
	}

	SECTION("Image") {
		const uint8_t rgb[6] = {1, 2, 3, 4, 5, 6};
		CHECK(cHyperionProtocol::EncodeImage(rgb, 2, 1) == Vec(kImage));
	}

	SECTION("size prefix and alignment of all messages") {
		std::vector<uint8_t> rgb(81 * 46 * 3, 7);
		for (const auto &m : { cHyperionProtocol::EncodeRegister("VDR drmdevice", 199),
		                       cHyperionProtocol::EncodeClear(-1),
		                       cHyperionProtocol::EncodeColor(0x123456, 500),
		                       cHyperionProtocol::EncodeImage(rgb.data(), 81, 46, 1000) }) {
			REQUIRE(m.size() > 8);
			CHECK(SizePrefix(m) == m.size() - 4);
			CHECK((m.size() - 4) % 4 == 0);
		}
	}

	SECTION("the image data is copied unchanged") {
		std::vector<uint8_t> rgb(80 * 45 * 3);
		for (size_t i = 0; i < rgb.size(); i++)
			rgb[i] = i * 7;
		auto m = cHyperionProtocol::EncodeImage(rgb.data(), 80, 45);
		// the vector is the last object of the message (followed by padding)
		auto it = std::search(m.begin(), m.end(), rgb.begin(), rgb.end());
		REQUIRE(it != m.end());
		uint32_t len;
		memcpy(&len, &*(it - 4), 4);
		CHECK(len == rgb.size());
	}
}

TEST_CASE("HyperionProtocol: decoded replies", "[hyperion]")
{
	std::string error;
	int video, registered;

	SECTION("registered") {
		REQUIRE(cHyperionProtocol::DecodeReply(kReplyRegistered, sizeof(kReplyRegistered), &error, &video, &registered));
		CHECK(error.empty());
		CHECK(video == -1);
		CHECK(registered == 150);
	}

	SECTION("error") {
		REQUIRE(cHyperionProtocol::DecodeReply(kReplyError, sizeof(kReplyError), &error, &video, &registered));
		CHECK(error == "bad priority");
		CHECK(registered == -1);
	}

	SECTION("truncated and random data doesn't crash") {
		for (size_t n = 0; n < sizeof(kReplyError); n++)
			cHyperionProtocol::DecodeReply(kReplyError, n, &error, &video, &registered);

		std::mt19937 rng(42);
		for (int i = 0; i < 20000; i++) {
			std::vector<uint8_t> g(rng() % 48);
			for (auto &b : g)
				b = rng();
			cHyperionProtocol::DecodeReply(g.data(), g.size(), &error, &video, &registered);
		}
		SUCCEED();
	}
}

TEST_CASE("HyperionJson: requests", "[hyperion]")
{
	CHECK(cHyperionJson::ComponentState("LEDDEVICE", false, 3) ==
	      "{\"command\":\"componentstate\",\"componentstate\":{\"component\":\"LEDDEVICE\",\"state\":false},\"tan\":3}\n");
	CHECK(cHyperionJson::ComponentState("LEDDEVICE", true, 4) ==
	      "{\"command\":\"componentstate\",\"componentstate\":{\"component\":\"LEDDEVICE\",\"state\":true},\"tan\":4}\n");
	CHECK(cHyperionJson::Effect("Rainbow swirl", 150, "VDR drmdevice", 5) ==
	      "{\"command\":\"effect\",\"effect\":{\"name\":\"Rainbow swirl\"},\"priority\":150,\"origin\":\"VDR drmdevice\",\"tan\":5}\n");
	CHECK(cHyperionJson::Clear(150, 6) == "{\"command\":\"clear\",\"priority\":150,\"tan\":6}\n");
	CHECK(cHyperionJson::Login("0123-abcd", 1) ==
	      "{\"command\":\"authorize\",\"subcommand\":\"login\",\"token\":\"0123-abcd\",\"tan\":1}\n");

	SECTION("strings are escaped, a request stays one line") {
		std::string r = cHyperionJson::Effect("a\"b\\c\nd\x01", 150, "o", 1);
		CHECK(r.find("\"name\":\"a\\\"b\\\\c\\nd\\u0001\"") != std::string::npos);
		CHECK(std::count(r.begin(), r.end(), '\n') == 1);
		CHECK(r.back() == '\n');
	}
}

TEST_CASE("HyperionJson: replies", "[hyperion]")
{
	int tan;
	bool success;
	std::string error;

	SECTION("success, as Hyperion sends it (compact)") {
		REQUIRE(cHyperionJson::ParseReply("{\"command\":\"componentstate\",\"success\":true,\"tan\":3}", &tan, &success, &error));
		CHECK(success);
		CHECK(tan == 3);
		CHECK(error.empty());
	}

	SECTION("error with blanks and escaped quotes") {
		REQUIRE(cHyperionJson::ParseReply("{ \"command\" : \"effect\", \"error\" : \"Effect \\\"x\\\" not found\", \"success\" : false, \"tan\" : 12 }", &tan, &success, &error));
		CHECK_FALSE(success);
		CHECK(tan == 12);
		CHECK(error == "Effect \"x\" not found");
	}

	SECTION("no authorization") {
		REQUIRE(cHyperionJson::ParseReply("{\"command\":\"clear\",\"error\":\"No Authorization\",\"success\":false,\"tan\":2}", &tan, &success, &error));
		CHECK_FALSE(success);
		CHECK(error == "No Authorization");
	}

	SECTION("no reply") {
		CHECK_FALSE(cHyperionJson::ParseReply("{\"command\":\"instance-update\",\"data\":[]}", &tan, &success, &error));
		CHECK_FALSE(cHyperionJson::ParseReply("", &tan, &success, &error));
		CHECK(tan == -1);
	}
}
