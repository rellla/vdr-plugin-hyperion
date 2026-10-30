// SPDX-License-Identifier: AGPL-3.0-or-later

/**
 * @file protocol.h
 * Hyperion Protocol Header File
 *
 * @copyright 2026 by Andreas Baierl. All Rights Reserved.
 *
 * @license{AGPL-3.0-or-later}
 */

#ifndef __HYPERION_PROTOCOL_H
#define __HYPERION_PROTOCOL_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

/**
 * Hyperion flatbuffers protocol
 *
 * Encodes the requests of the Hyperion flatbuffers server (hyperion_request.fbs)
 * and decodes its replies (hyperion_reply.fbs) without a dependency on the
 * flatbuffers library. Every encoded message is prefixed by its size
 * (4 bytes, big endian), as the server expects it.
 */
class cHyperionProtocol {
public:
	static constexpr int PRIORITY_MIN = 100;  ///< lowest priority, the server accepts from flatbuffers clients
	static constexpr int PRIORITY_MAX = 199;  ///< highest priority, the server accepts from flatbuffers clients

	static std::vector<uint8_t> EncodeRegister(const char *origin, int priority);
	static std::vector<uint8_t> EncodeImage(const uint8_t *rgb, int width, int height, int durationMs = -1);
	static std::vector<uint8_t> EncodeClear(int priority);
	static std::vector<uint8_t> EncodeColor(uint32_t rgb, int durationMs = -1);
	static bool DecodeReply(const uint8_t *data, size_t size, std::string *error, int *video, int *registered);
};

/**
 * Hyperion JSON-RPC protocol
 *
 * Creates the requests of the JSON server of Hyperion (default port 19444)
 * and reads its replies. Every message is one line of JSON, terminated by
 * a newline. Only the few commands, which are needed to control Hyperion,
 * are supported, so no JSON library is needed.
 */
class cHyperionJson {
public:
	static std::string Login(const std::string &token, int tan);
	static std::string ComponentState(const char *component, bool state, int tan);
	static std::string Effect(const std::string &name, int priority, const char *origin, int tan);
	static std::string Clear(int priority, int tan);
	static bool ParseReply(const std::string &line, int *tan, bool *success, std::string *error);
	static std::string Escape(const std::string &);
};

#endif
