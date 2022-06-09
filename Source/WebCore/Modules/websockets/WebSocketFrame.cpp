/*
 * Copyright (C) 2011 Google Inc.  All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this program; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"

#include "WebSocketFrame.h"
#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/MathExtras.h>
#include <wtf/text/StringConcatenateNumbers.h>

namespace WebCore {

// Constants for hybi-10 frame format.
constexpr uint8_t finalBit = 0x80;
constexpr uint8_t compressBit = 0x40;
constexpr uint8_t reserved2Bit = 0x20;
constexpr uint8_t reserved3Bit = 0x10;
constexpr uint8_t opCodeMask = 0xF;
constexpr uint8_t maskBit = 0x80;
constexpr uint8_t payloadLengthMask = 0x7F;
constexpr size_t maxPayloadLengthWithoutExtendedLengthField = 125;
constexpr size_t payloadLengthWithTwoByteExtendedLengthField = 126;
constexpr size_t payloadLengthWithEightByteExtendedLengthField = 127;
constexpr size_t maskingKeyWidthInBytes = 4;

bool WebSocketFrame::needsExtendedLengthField(size_t payloadLength)
{
    return payloadLength > maxPayloadLengthWithoutExtendedLengthField;
}

WebSocketFrame::ParseFrameResult WebSocketFrame::parseFrame(uint8_t* data, size_t dataLength, WebSocketFrame& frame, const uint8_t*& frameEnd, String& errorString)
{
    auto p = data;
    const uint8_t* bufferEnd = data + dataLength;

    if (dataLength < 2)
        return FrameIncomplete;

    auto firstByte = *p++;
    auto secondByte = *p++;

    bool final = firstByte & finalBit;
    bool compress = firstByte & compressBit;
    bool reserved2 = firstByte & reserved2Bit;
    bool reserved3 = firstByte & reserved3Bit;
    auto opCode = firstByte & opCodeMask;

    bool masked = secondByte & maskBit;
    uint64_t payloadLength64 = secondByte & payloadLengthMask;
    if (payloadLength64 > maxPayloadLengthWithoutExtendedLengthField) {
        int extendedPayloadLengthSize;
        if (payloadLength64 == payloadLengthWithTwoByteExtendedLengthField)
            extendedPayloadLengthSize = 2;
        else {
            ASSERT(payloadLength64 == payloadLengthWithEightByteExtendedLengthField);
            extendedPayloadLengthSize = 8;
        }
        if (bufferEnd - p < extendedPayloadLengthSize)
            return FrameIncomplete;
        payloadLength64 = 0;
        for (int i = 0; i < extendedPayloadLengthSize; ++i) {
            payloadLength64 <<= 8;
            payloadLength64 |= static_cast<uint8_t>(*p++);
        }
        if (extendedPayloadLengthSize == 2 && payloadLength64 <= maxPayloadLengthWithoutExtendedLengthField) {
            errorString = "The minimal number of bytes MUST be used to encode the length";
            return FrameError;
        }
        if (extendedPayloadLengthSize == 8 && payloadLength64 <= 0xFFFF) {
            errorString = "The minimal number of bytes MUST be used to encode the length";
            return FrameError;
        }
    }

    constexpr uint64_t maxPayloadLength = UINT64_C(0x7FFFFFFFFFFFFFFF);
    size_t maskingKeyLength = masked ? maskingKeyWidthInBytes : 0;
    if (payloadLength64 > maxPayloadLength || payloadLength64 + maskingKeyLength > std::numeric_limits<size_t>::max()) {
        errorString = makeString("WebSocket frame length too large: ", payloadLength64, " bytes");
        return FrameError;
    }
    size_t payloadLength = static_cast<size_t>(payloadLength64);

    if (static_cast<size_t>(bufferEnd - p) < maskingKeyLength + payloadLength)
        return FrameIncomplete;

    if (masked) {
        auto maskingKey = p;
        auto payload = p + maskingKeyWidthInBytes;
        for (size_t i = 0; i < payloadLength; ++i)
            payload[i] ^= maskingKey[i % maskingKeyWidthInBytes]; // Unmask the payload.
    }

    frame.opCode = static_cast<WebSocketFrame::OpCode>(opCode);
    frame.final = final;
    frame.compress = compress;
    frame.reserved2 = reserved2;
    frame.reserved3 = reserved3;
    frame.masked = masked;
    frame.payload = p + maskingKeyLength;
    frame.payloadLength = payloadLength;
    frameEnd = p + maskingKeyLength + payloadLength;
    return FrameOK;
}

static void appendFramePayload(const WebSocketFrame& frame, Vector<uint8_t>& frameData)
{
    size_t maskingKeyStart = 0;
    if (frame.masked) {
        maskingKeyStart = frameData.size();
        frameData.grow(frameData.size() + maskingKeyWidthInBytes); // Add placeholder for masking key. Will be overwritten.
    }

    size_t payloadStart = frameData.size();
    frameData.append(frame.payload, frame.payloadLength);

    if (frame.masked) {
        cryptographicallyRandomValues(frameData.data() + maskingKeyStart, maskingKeyWidthInBytes);
        for (size_t i = 0; i < frame.payloadLength; ++i)
            frameData[payloadStart + i] ^= frameData[maskingKeyStart + i % maskingKeyWidthInBytes];
    }
}

void WebSocketFrame::makeFrameData(Vector<uint8_t>& frameData)
{
    ASSERT(!(opCode & ~opCodeMask)); // Checks whether "opCode" fits in the range of opCodes.

    frameData.resize(2);
    frameData.at(0) = (final ? finalBit : 0) | (compress ? compressBit : 0) | opCode;
    frameData.at(1) = masked ? maskBit : 0;

    if (payloadLength <= maxPayloadLengthWithoutExtendedLengthField)
        frameData.at(1) |= payloadLength;
    else if (payloadLength <= 0xFFFF) {
        frameData.at(1) |= payloadLengthWithTwoByteExtendedLengthField;
        frameData.append((payloadLength & 0xFF00) >> 8);
        frameData.append(payloadLength & 0xFF);
    } else {
        frameData.at(1) |= payloadLengthWithEightByteExtendedLengthField;
        uint8_t extendedPayloadLength[8];
        size_t remaining = payloadLength;
        // Fill the length into extendedPayloadLength in the network byte order.
        for (int i = 0; i < 8; ++i) {
            extendedPayloadLength[7 - i] = remaining & 0xFF;
            remaining >>= 8;
        }
        ASSERT(!remaining);
        frameData.append(extendedPayloadLength, 8);
    }

    appendFramePayload(*this, frameData);
}

WebSocketFrame::WebSocketFrame(OpCode opCode, bool final, bool compress, bool masked, const uint8_t* payload, size_t payloadLength)
    : opCode(opCode)
    , final(final)
    , compress(compress)
    , reserved2(false)
    , reserved3(false)
    , masked(masked)
    , payload(payload)
    , payloadLength(payloadLength)
{
}

} // namespace WebCore
