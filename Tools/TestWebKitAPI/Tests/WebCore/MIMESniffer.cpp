/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include <WebCore/MIMESniffer.h>

namespace TestWebKitAPI {

using namespace WebCore;

TEST(MIMESniffer, MIMETypes)
{
    constexpr std::array<const uint8_t, 11> oggData = { 'O', 'g', 'g', 'S', 0, 'm', 'e', 'e', 'e', 'h', '.' };
    ASSERT_EQ(MIMESniffer::getMIMETypeFromContent({ oggData.data(), oggData.size()}), String("application/ogg"_s));

    constexpr std::array<const uint8_t, 41> webmData = { 0x1A, 0x45, 0xDF, 0xA3, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x42, 0x86, 0x81, 0x01, 0x42, 0xF7, 0x81, 0x01, 0x42, 0xF2, 0x81, 0x04, 0x42, 0xF3, 0x81, 0x08, 0x42, 0x82, 0x84, 0x77, 0x65, 0x62, 0x6D, 0x42, 0x87, 0x81, 0x04, 0x42, 0x85 };
    ASSERT_EQ(MIMESniffer::getMIMETypeFromContent({ webmData.data(), webmData.size() }), String("video/webm"_s));

    constexpr std::array<const uint8_t, 29> mp4data = { 0x00, 0x00, 0x00, 0x1C, 0x66, 0x74, 0x79, 0x70, 0x6D, 0x70, 0x34, 0x32, 0x00, 0x00, 0x00, 0x01, 0x69, 0x73, 0x6F, 0x6D, 0x6D, 0x70, 0x34, 0x31, 0x6D, 0x70, 0x34, 0x32, 0x00 };
    ASSERT_EQ(MIMESniffer::getMIMETypeFromContent({ mp4data.data(), mp4data.size() }), String("video/mp4"_s));

    constexpr std::array<const uint8_t, 32> mp3id3data = { 0x49, 0x44, 0x33, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x22, 0x54, 0x53, 0x53, 0x45, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x03, 0x4C, 0x61, 0x76, 0x66, 0x36, 0x30, 0x2E, 0x35, 0x2E, 0x31, 0x30 };
    ASSERT_EQ(MIMESniffer::getMIMETypeFromContent({ mp3id3data.data(), mp3id3data.size() }), String("audio/mpeg"_s));

    static std::array<const uint8_t, 32> wavedata = { 0x52, 0x49, 0x46, 0x46, 0x30, 0x13, 0x00, 0x00, 0x57, 0x41, 0x56, 0x45, 0x66, 0x6D, 0x74, 0x20, 0x10, 0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x00, 0x44, 0xAC, 0x00, 0x00, 0x10, 0xB1, 0x02, 0x00 };
    ASSERT_EQ(MIMESniffer::getMIMETypeFromContent({ wavedata.data(), wavedata.size() }), String("audio/wave"_s));
}

} // namespace TestWebKitAPI
