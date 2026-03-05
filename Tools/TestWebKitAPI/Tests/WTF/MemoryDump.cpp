/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include "Test.h"
#include <wtf/HexNumber.h>
#include <wtf/MemoryDump.h>
#include <wtf/StringPrintStream.h>
#include <wtf/text/StringBuilder.h>

namespace TestWebKitAPI {

TEST(WTF, MemoryDumpNullPointer)
{
    // Test null data pointer case
    MemoryDump nullDump(std::span(static_cast<std::byte*>(nullptr), 42));

    EXPECT_EQ(nullDump.span().data(), nullptr);
    EXPECT_EQ(nullDump.span().size(), 42u);
    EXPECT_EQ(nullDump.sizeLimit(), MemoryDump::DefaultSizeLimit);

    StringPrintStream stream;
    stream.print(nullDump);
    auto result = stream.tryToString();
    EXPECT_TRUE(result.has_value());

    String output = result.value();
    EXPECT_EQ(output, "\n00000000: (not dumping 42 bytes)"_s);
}

TEST(WTF, MemoryDumpEmptySize)
{
    // Test zero size case
    uint8_t data[] = { 0x41, 0x42, 0x43, 0x44 };
    MemoryDump emptyDump(std::span<uint8_t>(data, 0));

    EXPECT_EQ(emptyDump.span().data(), reinterpret_cast<const std::byte*>(data));
    EXPECT_EQ(emptyDump.span().size(), 0u);

    StringPrintStream stream;
    stream.print(emptyDump);
    auto result = stream.tryToString();
    EXPECT_TRUE(result.has_value());

    String output = result.value();
    EXPECT_TRUE(output.startsWith('\n'));
    EXPECT_TRUE(output.contains(": (span is empty)"_s));
}

TEST(WTF, MemoryDumpSingleByte)
{
    uint8_t data[] = { 0x42 }; // 'B'
    MemoryDump dump { std::span<uint8_t>(data) };

    StringPrintStream stream;
    stream.print(dump);
    auto result = stream.tryToString();
    EXPECT_TRUE(result.has_value());

    String output = result.value();
    EXPECT_TRUE(output.startsWith('\n'));

    // Should contain the address, hex representation, and ASCII
    EXPECT_TRUE(output.contains("42"_s));
    EXPECT_TRUE(output.contains("B"_s));
}

TEST(WTF, MemoryDumpExactly16Bytes)
{
    uint8_t data[16];
    for (int i = 0; i < 16; ++i)
        data[i] = static_cast<uint8_t>(0x41 + i); // A, B, C, ...

    MemoryDump dump { std::span<uint8_t>(data) };

    StringPrintStream stream;
    stream.print(dump);
    auto result = stream.tryToString();
    EXPECT_TRUE(result.has_value());

    String output = result.value();
    EXPECT_TRUE(output.startsWith('\n'));

    // Verify hex formatting: should have space after 8th byte
    Vector<String> parts = output.split("  "_s); // Split on double space before ASCII
    EXPECT_EQ(parts.size(), 2u);

    String hexPart = parts[0];
    String asciiPart = parts[1];

    // Verify ASCII part
    EXPECT_EQ(asciiPart, "ABCDEFGHIJKLMNOP"_s);

    // Verify hex part contains proper spacing
    EXPECT_TRUE(hexPart.contains("41 42 43 44 45 46 47 48 49 4a 4b 4c 4d 4e 4f 50"_s));
}

TEST(WTF, MemoryDumpMultipleLines)
{
    uint8_t data[33]; // More than 2 lines
    for (int i = 0; i < 33; ++i)
        data[i] = static_cast<uint8_t>(i);

    MemoryDump dump { std::span<uint8_t>(data) };

    StringPrintStream stream;
    stream.print(dump);
    auto result = stream.tryToString();
    EXPECT_TRUE(result.has_value());

    String output = result.value();
    auto lines = output.split('\n');
    EXPECT_EQ(lines.size(), 3u);

    // First line: 16 bytes (0x00-0x0f)
    String line1 = lines[0];
    EXPECT_TRUE(line1.contains("00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f"_s));

    // Second line: 16 bytes (0x10-0x1f)
    String line2 = lines[1];
    EXPECT_TRUE(line2.contains("10 11 12 13 14 15 16 17 18 19 1a 1b 1c 1d 1e 1f"_s));

    // Third line: 1 byte (0x20) - should be padded
    String line3 = lines[2];
    EXPECT_TRUE(line3.contains("20"_s));
    EXPECT_TRUE(line3.contains(" "_s)); // Should have padding spaces
}

TEST(WTF, MemoryDumpASCIIRepresentation)
{
    // Test various ASCII ranges
    uint8_t data[] = {
        31, 32, 65, 90, 97, 122, 126, 127, // Control, space, A, Z, a, z, ~, DEL
        0, 255, 128 // NUL, 0xFF, 0x80
    };

    MemoryDump dump { std::span<uint8_t>(data) };

    StringPrintStream stream;
    stream.print(dump);
    auto result = stream.tryToString();
    EXPECT_TRUE(result.has_value());

    String output = result.value();
    Vector<String> parts = output.split("  "_s);
    EXPECT_EQ(parts.size(), 2u);

    String asciiPart = parts[1];
    // Verify ASCII conversion:
    // 31->'.', 32->' ', 65->'A', 90->'Z', 97->'a', 122->'z', 126->'~', 127->'.', 0->'.', 255->'.', 128->'.'
    EXPECT_TRUE(asciiPart.contains(". AZaz~...."_s));
}

TEST(WTF, MemoryDumpSizeLimitTruncation)
{
    constexpr size_t dataSize = 100;
    constexpr size_t limitSize = 32; // Less than 2 full lines

    uint8_t data[dataSize];
    for (size_t i = 0; i < dataSize; ++i)
        data[i] = static_cast<uint8_t>(i & 0xFF);

    MemoryDump dump { std::span<uint8_t>(data), limitSize };

    StringPrintStream stream;
    stream.print(dump);
    auto result = stream.tryToString();
    EXPECT_TRUE(result.has_value());

    String output = result.value();

    auto lines = output.split("\n"_s);
    EXPECT_EQ(lines.size(), 3u); // two output lines and the truncation message

    auto truncation = lines[2];

    // Should contain truncation message
    EXPECT_TRUE(truncation.contains("... (remaining 68 bytes not dumped)"_s));
}

TEST(WTF, MemoryDumpAddressFormatting)
{
    uint8_t data[] = { 0x01, 0x02, 0x03, 0x04 };
    MemoryDump dump { std::span<uint8_t>(data) };

    StringPrintStream stream;
    stream.print(dump);
    auto result = stream.tryToString();
    EXPECT_TRUE(result.has_value());

    String output = result.value();
    auto lines = output.split('\n');
    EXPECT_EQ(lines.size(), 1u);
    String dataLine = lines[0];

    // Address should be 8 hex digits followed by ": "
    auto colonPos = dataLine.find(':');
    EXPECT_NE(colonPos, notFound);
    String addressStr = dataLine.left(colonPos);

    StringPrintStream expectedStream;
    expectedStream.print(hex(reinterpret_cast<uintptr_t>(data), Lowercase));
    auto expectedResult = expectedStream.tryToString();
    EXPECT_TRUE(expectedResult.has_value());
    String expected = expectedResult.value();

    EXPECT_EQ(addressStr, expected);
}

TEST(WTF, MemoryDumpBasic)
{
    uint8_t data[] = { 0x41, 0x42, 0x43, 0x44 }; // "ABCD"

    // Test span constructor
    std::span<uint8_t> dataSpan(data);
    MemoryDump dump(dataSpan);

    // Test that we can create a MemoryDump object
    EXPECT_EQ(dump.span().data(), reinterpret_cast<const std::byte*>(data));
    EXPECT_EQ(dump.span().size(), 4u);

    // Test StringBuilder integration via StringPrintStream
    StringPrintStream stream;
    stream.print(dump);
    auto expectedResult = stream.tryToString();
    EXPECT_TRUE(expectedResult.has_value());

    StringBuilder builder;
    builder.append(expectedResult.value());
    String result = builder.toString();

    // Should contain some output
    EXPECT_FALSE(result.isEmpty());

    // Test default size limit
    EXPECT_EQ(dump.sizeLimit(), MemoryDump::DefaultSizeLimit);

    // Test custom size limit with span constructor
    constexpr size_t customLimit = 512;
    MemoryDump spanDump(dataSpan, customLimit);
    EXPECT_EQ(spanDump.span().data(), reinterpret_cast<const std::byte*>(data));
    EXPECT_EQ(spanDump.span().size(), 4u);
    EXPECT_EQ(spanDump.sizeLimit(), customLimit);

    // Test output with custom limit (should still work for small data)
    StringPrintStream limitedStream;
    limitedStream.print(spanDump);
    auto limitedResult = limitedStream.tryToString();
    EXPECT_TRUE(limitedResult.has_value());
    EXPECT_FALSE(limitedResult.value().isEmpty());
}

TEST(WTF, MemoryDumpRange)
{
    uint8_t data[] = { 0x41, 0x42, 0x43, 0x44, 0x45 }; // "ABCDE"

    // Test range with pointers in correct order
    MemoryDump dump1 = MemoryDump(std::span { data });
    EXPECT_EQ(dump1.span().data(), reinterpret_cast<const std::byte*>(data));
    EXPECT_EQ(dump1.span().size(), 5u);
    EXPECT_EQ(dump1.sizeLimit(), MemoryDump::DefaultSizeLimit);

    // Test range with custom size limit
    constexpr size_t customLimit = 256;
    MemoryDump dump3(std::span { data }.first(3), customLimit);
    EXPECT_EQ(dump3.span().data(), reinterpret_cast<const std::byte*>(data));
    EXPECT_EQ(dump3.span().size(), 3u);
    EXPECT_EQ(dump3.sizeLimit(), customLimit);

    // Test outputs
    StringPrintStream stream1;
    stream1.print(dump1);
    auto result1 = stream1.tryToString();
    EXPECT_TRUE(result1.has_value());
    EXPECT_TRUE(result1.value().contains("ABCDE"_s));
}

} // namespace TestWebKitAPI
