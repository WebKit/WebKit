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
#include "MIMESniffer.h"

#include <array>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace WebCore {

namespace MIMESniffer {

template<std::size_t N>
constexpr auto span8(const char(&p)[N])
{
    return std::span<const uint8_t, N - 1>(byteCast<uint8_t>(&p[0]), N - 1);
}

static bool hasSignatureForMP4(std::span<const uint8_t> sequence)
{
    // To determine whether a byte sequence matches the signature for MP4, use the following steps:
    // Let sequence be the byte sequence to be matched, where sequence[s] is byte s in sequence and sequence[0] is the first byte in sequence.
    // Let length be the number of bytes in sequence.
    size_t length = sequence.size();
    // If length is less than 12, return false.
    if (length < 12)
        return false;
    // Let box-size be the four bytes from sequence[0] to sequence[3], interpreted as a 32-bit unsigned big-endian integer.
    uint32_t boxSize = sequence[0] << 24 | sequence[1] << 16 | sequence[2] << 8 | sequence[3] << 0;

    // If length is less than box-size or if box-size modulo 4 is not equal to 0, return false.
    if (length < boxSize || (boxSize % 4))
        return false;
    // If the four bytes from sequence[4] to sequence[7] are not equal to 0x66 0x74 0x79 0x70 ("ftyp"), return false.
    if (sequence[4] != 0x66 || sequence[5] != 0x74 || sequence[6] != 0x79 || sequence[7] != 0x70)
        return false;
    // If the three bytes from sequence[8] to sequence[10] are equal to 0x6D 0x70 0x34 ("mp4"), return true.
    if (sequence[8] == 0x6d && sequence[9] == 0x70 && sequence[10] == 0x34)
        return true;

    // Let bytes-read be 16.
    uint32_t bytesRead = 16;

    // This ignores the four bytes that correspond to the version number of the "major brand".
    // While bytes-read is less than box-size, continuously loop through these steps:
    while (bytesRead < boxSize) {
        // If the three bytes from sequence[bytes-read] to sequence[bytes-read + 2] are equal to 0x6D 0x70 0x34 ("mp4"), return true.
        if (sequence[bytesRead] == 0x6d && sequence[bytesRead + 1] == 0x70 && sequence[bytesRead + 2] == 0x34)
            return true;
        // Increment bytes-read by 4.
        bytesRead += 4;
    }
    // Return false.
    return false;
}

static std::optional<std::pair<uint8_t, int64_t>> parseWebMVint(std::span<const uint8_t> sequence)
{
    size_t index = 0;
    // Let mask be 128.
    uint8_t mask = 128;
    // Let max vint length be 8.
    const uint8_t maxVintLength = 8;
    // Let number size be 1.
    uint8_t numberSize = 1;
    // While number size is less than max vint length, and less than length, continuously loop through these steps:
    while (numberSize < maxVintLength && numberSize < sequence.size()) {
        // If the sequence[index] & mask is not zero, abort these steps.
        if (sequence[index] & mask)
            break;
        // Let mask be the value of mask >> 1.
        mask >>= 1;
        // Increment number size by one.
        numberSize++;
    }
    // Let index be 0.
    index = 0;
    // Let parsed number be sequence[index] & ~mask.
    uint64_t parsedNumber = sequence[index] & ~mask;
    // Increment index by one.
    index++;
    // Let bytes remaining be the value of number size.
    // TODO: spec is wrong, we've already read the first digit, should be value of number size - 1.
    uint8_t bytesRemaining = numberSize - 1;
    // While bytes remaining is not zero, execute there steps:
    while (bytesRemaining) {
        // Let parsed number be parsed number << 8.
        parsedNumber <<= 8;
        // Let parsed number be parsed number | sequence[index].
        parsedNumber |= sequence[index];
        // Increment index by one.
        index++;
        // If index is greater or equal than length, abort these steps.
        if (index >= sequence.size())
            return { };
        // Decrement bytes remaining by one.
        bytesRemaining--;
    }
    // Return parsed number and number size
    return std::make_pair(numberSize, parsedNumber);
}

static bool hasSignatureForWebM(std::span<const uint8_t> sequence)
{
    // To determine whether a byte sequence matches the signature for WebM, use the following steps:
    //   1. Let sequence be the byte sequence to be matched, where sequence[s] is byte s in sequence and sequence[0] is the first byte in sequence.
    //   2. Let length be the number of bytes in sequence.
    auto length = sequence.size();
    //   3. If length is less than 4, return false.
    if (length < 4)
        return false;
    //   4. If the four bytes from sequence[0] to sequence[3], are not equal to 0x1A 0x45 0xDF 0xA3, return false.
    if (sequence[0] != 0x1a || sequence[1] != 0x45 || sequence[2] != 0xdf || sequence[3] != 0xa3)
        return false;
    //   5. Let iter be 4.
    size_t iter = 4;
    //   6. While iter is less than length and iter is less than 38, continuously loop through these steps:
    while (iter < length && iter < 38) {
        //  1. If the two bytes from sequence[iter] to sequence[iter + 1] are equal to 0x42 0x82,
        if (sequence[iter] == 0x42 && sequence[iter + 1] == 0x82) {
            //  1. Increment iter by 2.
            iter += 2;
            //  2. If iter is greater or equal than length, abort these steps.
            if (iter >= length)
                break;
            //  3. Let number size be the result of parsing a vint starting at sequence[iter].
            auto result = parseWebMVint(sequence.subspan(iter));
            if (!result)
                return false;
            auto numberSize = result->first;
            //  4. Increment iter by number size.
            iter += numberSize;
            //  5. If iter is less than length - 4, abort these steps.
            // TODO: this is a spec error. It will always be less than length - 4 unless an error occurred.
            //  6. Let matched be the result of matching a padded sequence 0x77 0x65 0x62 0x6D ("webm") on sequence at offset iter.
            while (!sequence[iter] && iter < length)
                iter++;
            if (iter >= length - 4)
                break;
            //  7. If matched is true, abort these steps and return true.
            if (sequence[iter] == 0x77 && sequence[iter + 1] == 0x65 && sequence[iter + 2] == 0x62 && sequence[iter + 3] == 0x6d)
                return true;
        }
        //  2. Increment iter by 1.
        iter++;
    }
    //   7. Return false.
    return false;
}

static bool matchMP3Header(std::span<const uint8_t> sequence)
{
    // To match an mp3 header, using a byte sequence sequence of length length at offset s execute these steps:

    // If length is less than 4, return false.
    if (sequence.size() < 4)
        return false;

    // If sequence[s] is not equal to 0xff and sequence[s + 1] & 0xe0 is not equal to 0xe0, return false.
    if (sequence[0] != 0xff || sequence[1] != 0xe0)
        return false;
    // Let layer be the result of sequence[s + 1] & 0x06 >> 1.
    auto layer = sequence[1] & 0x06 >> 1;
    // If layer is 0, return false.
    if (!layer)
        return false;
    // Let bit-rate be sequence[s + 2] & 0xf0 >> 4.
    auto bitRate = sequence[2] & 0xf0 >> 4;
    // If bit-rate is 15, return false.
    if (bitRate == 15)
        return false;
    // Let sample-rate be sequence[s + 2] & 0x0c >> 2.
    auto sampleRate = sequence[2] & 0x0c >> 2;
    // If sample-rate is 3, return false.
    if (sampleRate == 3)
        return false;
    // Let freq be the value given by sample-rate in the table sample-rate.
    // const uint32_t sampleRateTable[] = { 44100, 48000, 32000 };
    // auto freq = sampleRateTable[sampleRate];
    // Let final-layer be the result of 4 - (sequence[s + 1]).
    auto finalLayer = 4 - sequence[1];
    // If final-layer & 0x06 >> 1 is not 3, return false.
    if ((finalLayer & 0x06 >> 1) != 3)
        return false;
    // Return true.
    return true;
}

struct MP3Frame {
    uint8_t version;
    uint32_t bitRate;
    uint32_t sampleRate;
    uint8_t pad;
};

static uint32_t mp3FrameSize(const MP3Frame& frame)
{
    // To compute an mp3 frame size, execute these steps:
    // If version is 1, let scale be 72, else, let scale be 144.
    auto scale = frame.version == 1 ? 72u : 144u;
    // Let size be bitrate * scale / freq.
    uint32_t size = frame.bitRate * scale / frame.sampleRate;
    // If pad is not zero, increment size by 1.
    if (frame.pad)
        size += 1;
    // Return size.
    return size;
}

static MP3Frame parseMP3Frame(std::span<const uint8_t> sequence)
{
    // Let version be sequence[s + 1] & 0x18 >> 3.
    uint8_t version = sequence[1] & 0x18 >> 3;
    // Let bitrate-index be sequence[s + 2] & 0xf0 >> 4.
    auto bitrateIndex = sequence[2] & 0xf0 >> 4;
    // If the version & 0x01 is non-zero, let bitrate be the value given by bitrate-index in the table mp2.5-rates
    constexpr std::array mp25ratesTable { 0u, 8000u, 16000u, 24000u, 32000u, 40000u, 48000u, 56000u, 64000u, 80000u, 96000u, 112000u, 128000u, 144000u, 160000u };
    // If version & 0x01 is zero, let bitrate be the value given by bitrate-index in the table mp3-rates
    constexpr std::array mp3ratesTable { 0u, 32000u, 40000u, 48000u, 56000u, 64000u, 80000u, 96000u, 112000u, 128000u, 160000u, 192000u, 224000u, 256000u, 320000u };
    auto bitRate = (version & 0x1) ? mp25ratesTable[bitrateIndex] : mp3ratesTable[bitrateIndex];
    // Let samplerate-index be sequence[s + 2] & 0x0c >> 2.
    auto sampleRateIndex = sequence[2] & 0x0c >> 2;
    // Let samplerate be the value given by samplerate-index in the sample-rate table.
    constexpr std::array sampleRateTable { 44100u, 48000u, 32000u };
    auto sampleRate = sampleRateTable[sampleRateIndex];
    // Let pad be sequence[s + 2] & 0x02 >> 1.
    uint8_t pad = sequence[2] & 0x02 >> 1;
    return { version, bitRate, sampleRate, pad };
}

static bool hasSignatureForMP3WithoutID3(std::span<const uint8_t> sequence)
{
    // To determine whether a byte sequence matches the signature for MP3 without ID3, use the following steps:

    // Let sequence be the byte sequence to be matched, where sequence[s] is byte s in sequence and sequence[0] is the first byte in sequence.
    //  Let length be the number of bytes in sequence.
    auto length = sequence.size();
    // Initialize s to 0.
    size_t s = 0;
    // If the result of the operation match mp3 header is false, return false.
    if (!matchMP3Header(sequence))
        return false;
    // Parse an mp3 frame on sequence at offset s
    auto frame = parseMP3Frame(sequence);
    // Let skipped-bytes the return value of the execution of mp3 framesize computation
    auto skippedBytes = mp3FrameSize(frame);
    //    If skipped-bytes is less than 4, or skipped-bytes is greater than s - length, return false.
    if (skippedBytes < 4 || skippedBytes > length)
        return false;
    // Increment s by skipped-bytes.
    s += skippedBytes;
    // If the result of the operation match mp3 header operation is false, return false, else, return true.
    return matchMP3Header(sequence.subspan(s));
}

static String mimeTypeFromSnifferEntries(std::span<const uint8_t> sequence)
{
    struct MIMESniffEntry {
        const std::span<const uint8_t> pattern;
        const std::span<const uint8_t> mask;
        const ASCIILiteral contentType;
    };

    static const MIMESniffEntry sSnifferEntries[] = {
        // The string "FORM" followed by four bytes followed by the string "AIFF", the AIFF signature.
        { span8("\x46\x4F\x52\x4D\x00\x00\x00\x00\x41\x49\x46\x46"), span8("\xFF\xFF\xFF\xFF\x00\x00\x00\x00\xFF\xFF\xFF\xFF"), "audio/aiff"_s },
        // The string "ID3", the ID3v2-tagged MP3 signature.
        { span8("\x49\x44\x33"), span8("\xFF\xFF\xFF"), "audio/mpeg"_s },
        // The string "OggS" followed by NUL, the Ogg container signature.
        { span8("\x4F\x67\x67\x53\x00"), span8("\xFF\xFF\xFF\xFF\xFF"), "application/ogg"_s },
        // The string "MThd" followed by four bytes representing the number 6 in 32 bits (big-endian), the MIDI signature.
        { span8("\0x4D\x54\x68\x64\x00\x00\x00\x06"), span8("\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"), "audio/midi"_s },
        // The string "RIFF" followed by four bytes followed by the string "AVI ", the AVI signature.
        { span8("\0x52\x49\x46\x46\x00\x00\x00\x00\x41\x56\x49\x20"), span8("\xFF\xFF\xFF\xFF\x00\x00\x00\x00\xFF\xFF\xFF\xFF"), "video/avi"_s },
        // The string "RIFF" followed by four bytes followed by the string "WAVE", the WAVE signature.
        { span8("\x52\x49\x46\x46\x00\x00\x00\x00\x57\x41\x56\x45"), span8("\xFF\xFF\xFF\xFF\x00\x00\x00\x00\xFF\xFF\xFF\xFF"), "audio/wave"_s },
    };

    // 1. Execute the following steps for each row row in the following table:
    //   1. Let patternMatched be the result of the pattern matching algorithm given input,
    //      the value in the first column of row, the value in the second column of row, and the value in the third column of row.
    //   2. If patternMatched is true, return the value in the fourth column of row.
    for (const auto& currentEntry : sSnifferEntries) {
        if (sequence.size() < currentEntry.mask.size() || currentEntry.mask.size() != currentEntry.pattern.size())
            continue;
        bool matched = true;
        for (uint32_t i = 0; i < currentEntry.mask.size(); i++) {
            if (currentEntry.pattern[i] != (currentEntry.mask[i] & sequence[i])) {
                matched = false;
                break;
            }
        }
        if (matched)
            return currentEntry.contentType;
    }
    return nullString();
}

String getMIMETypeFromContent(std::span<const uint8_t> sequence)
{
    // 6.2. Matching an audio or video type pattern
    // To determine which audio or video MIME type byte pattern a byte sequence input matches, if any, use the following audio or video type pattern matching algorithm:

    if (auto mimeType = mimeTypeFromSnifferEntries(sequence); !mimeType.isNull())
        return mimeType;

    // 2. If input matches the signature for MP4, return "video/mp4".
    if (hasSignatureForMP4(sequence))
        return "video/mp4"_s;

    // 3. If input matches the signature for WebM, return "video/webm".
    if (hasSignatureForWebM(sequence))
        return "video/webm"_s;

    // 4. If input matches the signature for MP3 without ID3, return "audio/mpeg".
    if (hasSignatureForMP3WithoutID3(sequence))
        return "audio/mpeg"_s;

    // 5. Return undefined.
    return emptyString();
}

} // namespace MIMESniffer

} // namespace WebCore

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
