/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CorpseByteParserTest.h"

#if OS(MACOS) || USE(APPLE_INTERNAL_SDK)

#include "LibJSCToolsTestUtilities.h"

#include <JavaScriptCore/CorpseByteParser.h>
#include <limits>

namespace JSCToolsTest {

using JSC::Corpse::ByteParser;

void testByteParser()
{
    if (!beginSuite("ByteParser"))
        return;

    {
        Vector<uint8_t> empty;
        ByteParser parser(empty.span());
        TEST_ASSERT(!parser.consumeByte(), "consumeByte on an empty buffer yields nothing");
        TEST_ASSERT(!parser.consumeULEB128(), "consumeULEB128 on an empty buffer yields nothing");
        TEST_ASSERT(!parser.consumeCString(), "consumeCString on an empty buffer yields nothing");
        TEST_ASSERT_EQ(parser.position(), static_cast<size_t>(0), "a failed read does not advance");
    }
    {
        Vector<uint8_t> data { 0x11, 0x22 };
        ByteParser parser(data.span());
        auto first = parser.consumeByte();
        TEST_ASSERT(first && *first == 0x11, "consumeByte yields the first byte");
        TEST_ASSERT_EQ(parser.position(), static_cast<size_t>(1), "consumeByte advances by one");
        auto second = parser.consumeByte();
        TEST_ASSERT(second && *second == 0x22, "consumeByte yields the next byte");
        TEST_ASSERT_EQ(parser.position(), static_cast<size_t>(2), "consumeByte advances past the last byte");
        TEST_ASSERT(!parser.consumeByte(), "consumeByte stops at the end");
    }
    {
        // A parser may start part way in, which is how a node is read out of a trie.
        Vector<uint8_t> data { 0x11, 0x22, 0x33 };
        ByteParser parser(data.span(), 2);
        auto value = parser.consumeByte();
        TEST_ASSERT(value && *value == 0x33, "a parser starts at the position it is given");
        TEST_ASSERT_EQ(parser.position(), static_cast<size_t>(3), "a read advances from the given position");
    }
    {
        // A trie node's fields are ULEB128s read through a parser bounded to the whole
        // trie but starting at the node, so decoding has to begin at that position and
        // leave the cursor on the field that follows.
        Vector<uint8_t> data { 0x11, 0x22, 0xe5, 0x8e, 0x26, 0x33 };
        ByteParser parser(data.span(), 2);
        auto value = parser.consumeULEB128();
        TEST_ASSERT(value && *value == 624485, "consumeULEB128 decodes from the position it is given");
        TEST_ASSERT_EQ(parser.position(), static_cast<size_t>(5), "consumeULEB128 stops after the value it decoded");
        auto next = parser.consumeByte();
        TEST_ASSERT(next && *next == 0x33, "the byte after a decoded value is left for the next read");
    }
    {
        // A failed read rewinds to where the cursor was, which is not necessarily the
        // start of the buffer.
        Vector<uint8_t> data { 0x11, 0x22, 0x80 };
        ByteParser parser(data.span(), 2);
        TEST_ASSERT(!parser.consumeULEB128(), "a truncated ULEB128 is rejected wherever it starts");
        TEST_ASSERT_EQ(parser.position(), static_cast<size_t>(2), "a failed read rewinds to the given position");
    }

    struct ULEBCase {
        Vector<uint8_t> bytes;
        bool decodes;
        uint64_t value;
        const char* description;
    };
    Vector<ULEBCase> ulebCases;
    ulebCases.append({ { 0x00 }, true, 0, "zero" });
    ulebCases.append({ { 0x01 }, true, 1, "one" });
    ulebCases.append({ { 0x7f }, true, 127, "the largest one-byte value" });
    ulebCases.append({ { 0x80, 0x01 }, true, 128, "the smallest two-byte value" });
    ulebCases.append({ { 0xe5, 0x8e, 0x26 }, true, 624485, "a three-byte value" });
    ulebCases.append({ { 0x80, 0x80, 0x80, 0x00 }, true, 0, "zero padded with continuations" });
    ulebCases.append({ { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01 }, true,
        std::numeric_limits<uint64_t>::max(), "the largest 64-bit value" });
    // Rejected: the top group carries bits that do not fit in 64.
    ulebCases.append({ { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x02 }, false, 0,
        "a value one bit too wide for 64 bits" });
    // Rejected: the shift would reach 64, which is undefined rather than merely large.
    ulebCases.append({ { 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x01 }, false, 0,
        "an encoding longer than 64 bits can hold" });
    ulebCases.append({ { 0x80 }, false, 0, "a value whose continuation runs off the end" });
    ulebCases.append({ { 0xff, 0xff }, false, 0, "a truncated multi-byte value" });

    for (const ULEBCase& testCase : ulebCases) {
        ByteParser parser(testCase.bytes.span());
        auto decoded = parser.consumeULEB128();
        if (!testCase.decodes) {
            TEST_ASSERT(!decoded, testCase.description);
            TEST_ASSERT_EQ(parser.position(), static_cast<size_t>(0), testCase.description);
            continue;
        }
        if (!decoded) {
            TEST_ASSERT(decoded, testCase.description);
            continue;
        }
        TEST_ASSERT_HEX_EQ(*decoded, testCase.value, testCase.description);
        TEST_ASSERT_EQ(parser.position(), testCase.bytes.size(), testCase.description);
    }

    {
        Vector<uint8_t> data { 'a', 'b', 0, 'c', 0 };
        ByteParser parser(data.span());
        auto first = parser.consumeCString();
        TEST_ASSERT(first && *first == "ab", "consumeCString yields the string");
        TEST_ASSERT_EQ(parser.position(), static_cast<size_t>(3), "consumeCString consumes the terminator");
        auto second = parser.consumeCString();
        TEST_ASSERT(second && *second == "c", "consumeCString yields the following string");
        TEST_ASSERT_EQ(parser.position(), static_cast<size_t>(5), "consumeCString consumes the second terminator");
        TEST_ASSERT(!parser.consumeCString(), "consumeCString stops at the end");
    }
    {
        Vector<uint8_t> data { 0 };
        ByteParser parser(data.span());
        auto empty = parser.consumeCString();
        TEST_ASSERT(empty && empty->empty(), "an empty string is a string");
        TEST_ASSERT_EQ(parser.position(), static_cast<size_t>(1), "an empty string still consumes its terminator");
    }
    {
        // A read that fails consumes nothing.
        Vector<uint8_t> unterminated { 'a', 'b' };
        ByteParser cstring(unterminated.span());
        TEST_ASSERT(!cstring.consumeCString(), "an unterminated string is rejected");
        TEST_ASSERT_EQ(cstring.position(), static_cast<size_t>(0),
            "a failed string read leaves the cursor where it was");

        Vector<uint8_t> truncated { 0x80, 0x80 };
        ByteParser uleb(truncated.span());
        TEST_ASSERT(!uleb.consumeULEB128(), "a truncated ULEB128 is rejected");
        TEST_ASSERT_EQ(uleb.position(), static_cast<size_t>(0),
            "a failed ULEB128 read leaves the cursor where it was");

        // The failed read should not advance the cursor. Therefore, the 2nd read should succeed.
        Vector<uint8_t> lone { 0x80 };
        ByteParser retry(lone.span());
        TEST_ASSERT(!retry.consumeULEB128(), "a lone continuation byte is not a value");
        auto byte = retry.consumeByte();
        TEST_ASSERT(byte && *byte == 0x80, "a failed read leaves its bytes for another reader");
    }
    {
        Vector<uint8_t> data { 0x11, 'a', 0 };
        for (size_t position : { data.size(), data.size() + 1, std::numeric_limits<size_t>::max() }) {
            ByteParser parser(data.span(), position);
            TEST_ASSERT(!parser.consumeByte(), "consumeByte from beyond the end yields nothing");
            TEST_ASSERT(!parser.consumeULEB128(), "consumeULEB128 from beyond the end yields nothing");
            TEST_ASSERT(!parser.consumeCString(), "consumeCString from beyond the end yields nothing");
            TEST_ASSERT_EQ(parser.position(), position, "a failed read beyond the end does not move the cursor");
        }
    }
}

} // namespace JSCToolsTest

#endif // OS(MACOS) || USE(APPLE_INTERNAL_SDK)
