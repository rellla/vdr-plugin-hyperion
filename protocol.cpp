// SPDX-License-Identifier: AGPL-3.0-or-later

/**
 * @file protocol.cpp
 * Hyperion Flatbuffers Protocol
 *
 * Encodes the requests of the Hyperion flatbuffers server and decodes its
 * replies. The schemas are hyperion_request.fbs and hyperion_reply.fbs of
 * hyperion.ng (libsrc/flatbufserver). To avoid a dependency on the flatbuffers
 * library for a handful of tiny messages, the buffers are written by hand.
 *
 * @copyright 2026 by Andreas Baierl. All Rights Reserved.
 *
 * @license{AGPL-3.0-or-later}
 */

#include <algorithm>
#include <cstring>

#include "protocol.h"

/*****************************************************************************
 * Flatbuffers encoding
 ****************************************************************************/

namespace {

/// union type ids of hyperion_request.fbs
enum eHyperionCommand : uint8_t {
	COMMAND_COLOR = 1,
	COMMAND_IMAGE = 2,
	COMMAND_CLEAR = 3,
	COMMAND_REGISTER = 4,
};

enum eHyperionImageType : uint8_t {
	IMAGE_RAW = 1,
};

/**
 * Minimal flatbuffer writer
 *
 * Writes the buffer from front to back: a table is followed by the objects it
 * refers to, so all offsets point forward, as required by the format. The
 * vtable of a table is written directly before it.
 */
class cFlatBufferWriter {
public:
	/** A scalar field or an offset to an object, which is written later */
	struct sField {
		int id;             ///< field id (slot in the vtable)
		int size;           ///< 1 or 4 bytes
		uint32_t value;     ///< value (ignored for offsets)
	};

	cFlatBufferWriter(void)
	{
		// room for the size prefix of the message and the root offset
		m_buf.resize(8);
	}

	/**
	 * Write a table
	 *
	 * The fields are written in the given order, so the 4 byte fields have
	 * to be given first. The positions of the fields are returned in pos
	 * (indexed by the order of the given fields).
	 */
	size_t Table(const std::vector<sField> &fields, std::vector<size_t> *pos)
	{
		int slots = 0;
		uint16_t tableSize = 4;     // soffset to the vtable
		for (const auto &f : fields) {
			slots = std::max(slots, f.id + 1);
			tableSize += f.size;
		}

		std::vector<uint16_t> vtable(slots, 0);
		uint16_t fieldOffset = 4;
		for (const auto &f : fields) {
			vtable[f.id] = fieldOffset;
			fieldOffset += f.size;
		}

		Align(2);
		size_t vtablePos = m_buf.size();
		Put16(4 + 2 * slots);
		Put16(tableSize);
		for (uint16_t o : vtable)
			Put16(o);

		Align(4);
		size_t tablePos = m_buf.size();
		Put32(tablePos - vtablePos);    // soffset: vtable = table - soffset

		pos->clear();
		for (const auto &f : fields) {
			pos->push_back(m_buf.size());
			if (f.size == 1)
				m_buf.push_back((uint8_t)f.value);
			else
				Put32(f.value);
		}

		return tablePos;
	}

	/** Write a vector of bytes and return its position */
	size_t Bytes(const uint8_t *data, size_t size)
	{
		Align(4);
		size_t pos = m_buf.size();
		Put32(size);
		m_buf.insert(m_buf.end(), data, data + size);
		return pos;
	}

	/** Write a string and return its position */
	size_t String(const char *str)
	{
		size_t len = strlen(str);
		size_t pos = Bytes((const uint8_t *)str, len);
		m_buf.push_back(0);
		return pos;
	}

	/** Let the offset field at fieldPos point to the object at objPos */
	void Link(size_t fieldPos, size_t objPos)
	{
		Patch32(fieldPos, objPos - fieldPos);
	}

	/** Set the root table, add the size prefix and return the message */
	std::vector<uint8_t> Finish(size_t rootPos)
	{
		Align(4);
		Link(4, rootPos);
		uint32_t size = m_buf.size() - 4;
		m_buf[0] = size >> 24;
		m_buf[1] = size >> 16;
		m_buf[2] = size >> 8;
		m_buf[3] = size;
		return std::move(m_buf);
	}

private:
	std::vector<uint8_t> m_buf;

	// the flatbuffer starts behind the size prefix, align relative to it
	void Align(size_t align) { while ((m_buf.size() - 4) % align) m_buf.push_back(0); }
	void Put16(uint32_t v) { m_buf.push_back(v); m_buf.push_back(v >> 8); }
	void Put32(uint32_t v) { m_buf.push_back(v); m_buf.push_back(v >> 8); m_buf.push_back(v >> 16); m_buf.push_back(v >> 24); }
	void Patch32(size_t pos, uint32_t v) { m_buf[pos] = v; m_buf[pos + 1] = v >> 8; m_buf[pos + 2] = v >> 16; m_buf[pos + 3] = v >> 24; }
};

/** Write the Request table with the command and return the position of the command offset */
size_t WriteRequest(cFlatBufferWriter &w, eHyperionCommand command, size_t *requestPos)
{
	std::vector<size_t> pos;
	// Request { command_type (id 0), command (id 1) }
	*requestPos = w.Table({ {1, 4, 0}, {0, 1, command} }, &pos);
	return pos[0];
}

/** Bounds checked reader of a received flatbuffer */
class cFlatBufferReader {
public:
	cFlatBufferReader(const uint8_t *data, size_t size) : m_data(data), m_size(size) {}

	bool Root(size_t *table) const
	{
		uint32_t off = 0;
		return Get32(0, &off) && Table(off, table);
	}

	/** Position of a field in a table (0, if the field is not set) */
	size_t Field(size_t table, int id) const
	{
		uint32_t soff = 0;
		if (!Get32(table, &soff))
			return 0;

		int64_t vtable = (int64_t)table - (int32_t)soff;
		uint16_t vtableSize = 0, fieldOffset = 0;
		if (vtable < 0 || !Get16(vtable, &vtableSize) || 4 + 2 * id + 2 > vtableSize || !Get16(vtable + 4 + 2 * id, &fieldOffset) || !fieldOffset)
			return 0;

		return table + fieldOffset;
	}

	bool Int(size_t table, int id, int *value) const
	{
		size_t pos = Field(table, id);
		uint32_t v = 0;
		if (!pos || !Get32(pos, &v))
			return false;
		*value = (int32_t)v;
		return true;
	}

	bool String(size_t table, int id, std::string *str) const
	{
		size_t pos = Field(table, id);
		uint32_t off = 0, len = 0;
		if (!pos || !Get32(pos, &off) || !Get32(pos + off, &len) || pos + off + 4 + len > m_size)
			return false;
		str->assign((const char *)m_data + pos + off + 4, len);
		return true;
	}

private:
	const uint8_t *m_data;
	size_t m_size;

	bool Table(uint32_t pos, size_t *table) const { *table = pos; return pos + 4 <= m_size; }
	bool Get16(size_t pos, uint16_t *v) const
	{
		if (pos + 2 > m_size)
			return false;
		*v = m_data[pos] | (m_data[pos + 1] << 8);
		return true;
	}
	bool Get32(size_t pos, uint32_t *v) const
	{
		if (pos + 4 > m_size)
			return false;
		*v = m_data[pos] | (m_data[pos + 1] << 8) | (m_data[pos + 2] << 16) | ((uint32_t)m_data[pos + 3] << 24);
		return true;
	}
};

} // namespace

/**
 * Encode a Register request
 *
 * @param origin      name of the input, shown in Hyperion
 * @param priority    priority of the input (100 - 199)
 */
std::vector<uint8_t> cHyperionProtocol::EncodeRegister(const char *origin, int priority)
{
	cFlatBufferWriter w;
	size_t request;
	size_t commandField = WriteRequest(w, COMMAND_REGISTER, &request);

	std::vector<size_t> pos;
	// Register { origin (id 0), priority (id 1) }
	size_t reg = w.Table({ {0, 4, 0}, {1, 4, (uint32_t)priority} }, &pos);
	w.Link(commandField, reg);
	w.Link(pos[0], w.String(origin));

	return w.Finish(request);
}

/**
 * Encode an Image request with a RawImage
 *
 * @param rgb         RGB24 data, width * height * 3 bytes
 * @param width       image width
 * @param height      image height
 * @param durationMs  duration in Hyperion (-1: until it is replaced or cleared)
 */
std::vector<uint8_t> cHyperionProtocol::EncodeImage(const uint8_t *rgb, int width, int height, int durationMs)
{
	cFlatBufferWriter w;
	size_t request;
	size_t commandField = WriteRequest(w, COMMAND_IMAGE, &request);

	std::vector<size_t> pos;
	// Image { data_type (id 0), data (id 1), duration (id 2) }
	size_t image = w.Table({ {1, 4, 0}, {2, 4, (uint32_t)durationMs}, {0, 1, IMAGE_RAW} }, &pos);
	w.Link(commandField, image);
	size_t dataField = pos[0];

	// RawImage { data (id 0), width (id 1), height (id 2) }
	size_t raw = w.Table({ {0, 4, 0}, {1, 4, (uint32_t)width}, {2, 4, (uint32_t)height} }, &pos);
	w.Link(dataField, raw);
	w.Link(pos[0], w.Bytes(rgb, (size_t)width * height * 3));

	return w.Finish(request);
}

/**
 * Encode a Clear request
 *
 * @param priority    priority to clear (-1 clears all priorities)
 */
std::vector<uint8_t> cHyperionProtocol::EncodeClear(int priority)
{
	cFlatBufferWriter w;
	size_t request;
	size_t commandField = WriteRequest(w, COMMAND_CLEAR, &request);

	std::vector<size_t> pos;
	// Clear { priority (id 0) }
	w.Link(commandField, w.Table({ {0, 4, (uint32_t)priority} }, &pos));

	return w.Finish(request);
}

/**
 * Encode a Color request
 *
 * @param rgb         color as 0x00RRGGBB
 * @param durationMs  duration in Hyperion (-1: until it is replaced or cleared)
 */
std::vector<uint8_t> cHyperionProtocol::EncodeColor(uint32_t rgb, int durationMs)
{
	cFlatBufferWriter w;
	size_t request;
	size_t commandField = WriteRequest(w, COMMAND_COLOR, &request);

	std::vector<size_t> pos;
	// Color { data (id 0), duration (id 1) }
	w.Link(commandField, w.Table({ {0, 4, rgb & 0xffffff}, {1, 4, (uint32_t)durationMs} }, &pos));

	return w.Finish(request);
}

/**
 * Decode a Reply (without size prefix)
 *
 * @param[out] error        error message (empty, if there is none)
 * @param[out] video        video mode (-1, if not set)
 * @param[out] registered   registered priority (-1, if not set)
 * @return                  false, if the reply is malformed
 */
bool cHyperionProtocol::DecodeReply(const uint8_t *data, size_t size, std::string *error, int *video, int *registered)
{
	cFlatBufferReader r(data, size);
	size_t root;

	error->clear();
	*video = -1;
	*registered = -1;

	if (!r.Root(&root))
		return false;

	// Reply { error (id 0), video (id 1), registered (id 2) }
	r.String(root, 0, error);
	r.Int(root, 1, video);
	r.Int(root, 2, registered);

	return true;
}

