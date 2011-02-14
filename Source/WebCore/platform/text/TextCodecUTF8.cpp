/*
 * Copyright (C) 2004, 2006, 2008, 2011 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "TextCodecUTF8.h"

#include <wtf/text/CString.h>
#include <wtf/text/StringBuffer.h>
#include <wtf/unicode/UTF8.h>

using namespace WTF::Unicode;
using namespace std;

namespace WebCore {

// Assuming that a pointer is the size of a "machine word", then
// uintptr_t is an integer type that is also a machine word.
typedef uintptr_t MachineWord;

// This constant has type uintptr_t since we will use it to align
// pointers. Not because MachineWord is uintptr_t.
const uintptr_t machineWordAlignmentMask = sizeof(MachineWord) - 1;

template<size_t size> struct NonASCIIMask;
template<> struct NonASCIIMask<4> {
    static unsigned value() { return 0x80808080U; }
};
template<> struct NonASCIIMask<8> {
    static unsigned long long value() { return 0x8080808080808080ULL; }
};

template<size_t size> struct UCharByteFiller;
template<> struct UCharByteFiller<4> {
    static void copy(UChar* destination, const uint8_t* source)
    {
        destination[0] = source[0];
        destination[1] = source[1];
        destination[2] = source[2];
        destination[3] = source[3];
    }
};
template<> struct UCharByteFiller<8> {
    static void copy(UChar* destination, const uint8_t* source)
    {
        destination[0] = source[0];
        destination[1] = source[1];
        destination[2] = source[2];
        destination[3] = source[3];
        destination[4] = source[4];
        destination[5] = source[5];
        destination[6] = source[6];
        destination[7] = source[7];
    }
};

static inline bool isAlignedToMachineWord(const void* pointer)
{
    return !(reinterpret_cast<uintptr_t>(pointer) & machineWordAlignmentMask);
}

template<typename T> static inline T* alignToMachineWord(T* pointer)
{
    return reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(pointer) & ~machineWordAlignmentMask);
}

PassOwnPtr<TextCodec> TextCodecUTF8::create(const TextEncoding&, const void*)
{
    return adoptPtr(new TextCodecUTF8);
}

void TextCodecUTF8::registerEncodingNames(EncodingNameRegistrar registrar)
{
    registrar("UTF-8", "UTF-8");
}

void TextCodecUTF8::registerCodecs(TextCodecRegistrar registrar)
{
    registrar("UTF-8", create, 0);
}

static inline int nonASCIISequenceLength(unsigned char firstByte)
{
    ASSERT(!isASCII(firstByte));
    switch (firstByte >> 4) {
    case 0xF:
        return 4;
    case 0xE:
        return 3;
    }
    return 2;
}

static inline int decodeNonASCIISequence(const unsigned char* sequence, unsigned length)
{
    ASSERT(!isASCII(sequence[0]));
    if (length == 2) {
        ASSERT(sequence[0] <= 0xDF);
        if (sequence[0] < 0xC2)
            return -1;
        if (sequence[1] < 0x80 || sequence[1] > 0xBF)
            return -1;
        return ((sequence[0] << 6) + sequence[1]) - 0x00003080;
    }
    if (length == 3) {
        ASSERT(sequence[0] >= 0xE0 && sequence[0] <= 0xEF);
        switch (sequence[0]) {
        case 0xE0:
            if (sequence[1] < 0xA0 || sequence[1] > 0xBF)
                return -1;
            break;
        case 0xED:
            if (sequence[1] < 0x80 || sequence[1] > 0x9F)
                return -1;
            break;
        default:
            if (sequence[1] < 0x80 || sequence[1] > 0xBF)
                return -1;
        }
        if (sequence[2] < 0x80 || sequence[2] > 0xBF)
            return -1;
        return ((sequence[0] << 12) + (sequence[1] << 6) + sequence[2]) - 0x000E2080;
    }
    ASSERT(length == 4);
    ASSERT(sequence[0] >= 0xF0 && sequence[0] <= 0xF4);
    switch (sequence[0]) {
    case 0xF0:
        if (sequence[1] < 0x90 || sequence[1] > 0xBF)
            return -1;
        break;
    case 0xF4:
        if (sequence[1] < 0x80 || sequence[1] > 0x8F)
            return -1;
        break;
    default:
        if (sequence[1] < 0x80 || sequence[1] > 0xBF)
            return -1;
    }
    if (sequence[2] < 0x80 || sequence[2] > 0xBF)
        return -1;
    if (sequence[3] < 0x80 || sequence[3] > 0xBF)
        return -1;
    return ((sequence[0] << 18) + (sequence[1] << 12) + (sequence[2] << 6) + sequence[3]) - 0x03C82080;
}

String TextCodecUTF8::decode(const char* bytes, size_t length, bool flush, bool stopOnError, bool& sawError)
{
    StringBuffer buffer(length);

    const uint8_t* source = reinterpret_cast<const uint8_t*>(bytes);
    const uint8_t* end = source + length;
    const uint8_t* alignedEnd = alignToMachineWord(end);
    UChar* destination = buffer.characters();

    int count;
    int character;

    if (m_partialSequenceSize) {
        count = nonASCIISequenceLength(m_partialSequence[0]);
        ASSERT(count > m_partialSequenceSize);
        if (count - m_partialSequenceSize > end - source) {
            memcpy(m_partialSequence + m_partialSequenceSize, source, end - source);
            m_partialSequenceSize += end - source;
            source = end;
        } else {
            uint8_t completeSequence[U8_MAX_LENGTH];
            memcpy(completeSequence, m_partialSequence, m_partialSequenceSize);
            memcpy(completeSequence + m_partialSequenceSize, source, count - m_partialSequenceSize);
            source += count - m_partialSequenceSize;
            m_partialSequenceSize = 0;
            character = decodeNonASCIISequence(completeSequence, count);
            goto decodedNonASCII;
        }
    }

    while (source < end) {
        if (isASCII(*source)) {
            // Fast path for ASCII. Most UTF-8 text will be ASCII.
            if (isAlignedToMachineWord(source)) {
                while (source < alignedEnd) {
                    MachineWord chunk = *reinterpret_cast_ptr<const MachineWord*>(source);
                    if (chunk & NonASCIIMask<sizeof(MachineWord)>::value()) {
                        if (isASCII(*source))
                            break;
                        goto nonASCII;
                    }
                    UCharByteFiller<sizeof(MachineWord)>::copy(destination, source);
                    source += sizeof(MachineWord);
                    destination += sizeof(MachineWord);
                }
                if (source == end)
                    break;
            }
            *destination++ = *source++;
        } else {
nonASCII:
            count = nonASCIISequenceLength(*source);
            ASSERT(count >= 2);
            ASSERT(count <= 4);
            if (count > end - source) {
                ASSERT(end - source <= static_cast<ptrdiff_t>(sizeof(m_partialSequence)));
                ASSERT(!m_partialSequenceSize);
                m_partialSequenceSize = end - source;
                memcpy(m_partialSequence, source, m_partialSequenceSize);
                break;
            }
            character = decodeNonASCIISequence(source, count);
            source += count;
decodedNonASCII:
            if (character < 0) {
                if (stopOnError) {
                    sawError = true;
                    break;
                }
            } else {
                ASSERT(!U_IS_SURROGATE(character));
                if (U_IS_BMP(character))
                    *destination++ = character;
                else {
                    *destination++ = U16_LEAD(character);
                    *destination++ = U16_TRAIL(character);
                }
            }
        }
    }

    buffer.shrink(destination - buffer.characters());

    if (flush && m_partialSequenceSize)
        sawError = true;

    return String::adopt(buffer);
}

CString TextCodecUTF8::encode(const UChar* characters, size_t length, UnencodableHandling)
{
    // The maximum number of UTF-8 bytes needed per UTF-16 code unit is 3.
    // BMP characters take only one UTF-16 code unit and can take up to 3 bytes (3x).
    // Non-BMP characters take two UTF-16 code units and can take up to 4 bytes (2x).
    if (length > numeric_limits<size_t>::max() / 3)
        CRASH();
    Vector<uint8_t> bytes(length * 3);

    size_t i = 0;
    size_t bytesWritten = 0;
    while (i < length) {
        UChar32 character;
        U16_NEXT(characters, i, length, character);
        U8_APPEND_UNSAFE(bytes.data(), bytesWritten, character);
    }

    return CString(reinterpret_cast<char*>(bytes.data()), bytesWritten);
}

} // namespace WebCore
