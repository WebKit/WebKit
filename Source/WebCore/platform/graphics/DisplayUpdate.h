/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "AnimationFrameRate.h"

namespace WTF {
class TextStream;
}

namespace WebCore {

// Used to represent a given update. An value of { 3, 60 } indicates that this is the third update in a 1-second interval
// on a 60fps cadence. updateIndex will reset to zero every second, so { 59, 60 } is followed by { 0, 60 }.
struct DisplayUpdate {
    unsigned updateIndex { 0 };
    FramesPerSecond updatesPerSecond { 0 };
    
    DisplayUpdate nextUpdate() const
    {
        return { (updateIndex + 1) % updatesPerSecond, updatesPerSecond };
    }
    
    WEBCORE_EXPORT bool relevantForUpdateFrequency(FramesPerSecond) const;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<DisplayUpdate> decode(Decoder&);
};

template<class Encoder>
void DisplayUpdate::encode(Encoder& encoder) const
{
    encoder << updateIndex;
    encoder << updatesPerSecond;
}

template<class Decoder>
std::optional<DisplayUpdate> DisplayUpdate::decode(Decoder& decoder)
{
    std::optional<unsigned> updateIndex;
    decoder >> updateIndex;
    if (!updateIndex)
        return std::nullopt;

    std::optional<FramesPerSecond> updatesPerSecond;
    decoder >> updatesPerSecond;
    if (!updatesPerSecond)
        return std::nullopt;

    return {{ *updateIndex, *updatesPerSecond }};
}

WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const DisplayUpdate&);

} // namespace WebCore
