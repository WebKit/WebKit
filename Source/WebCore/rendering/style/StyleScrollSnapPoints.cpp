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
#include "StyleScrollSnapPoints.h"

namespace WebCore {

WTF::TextStream& operator<<(WTF::TextStream& ts, ScrollSnapAlign align)
{
    auto populateTextStreamForAlignType = [&](ScrollSnapAxisAlignType type) {
        switch (type) {
        case ScrollSnapAxisAlignType::None: ts << "none"; break;
        case ScrollSnapAxisAlignType::Start: ts << "start"; break;
        case ScrollSnapAxisAlignType::Center: ts << "center"; break;
        case ScrollSnapAxisAlignType::End: ts << "end"; break;
        }
    };
    populateTextStreamForAlignType(align.blockAlign);
    if (align.blockAlign != align.inlineAlign) {
        ts << ' ';
        populateTextStreamForAlignType(align.inlineAlign);
    }
    return ts;
}

WTF::TextStream& operator<<(WTF::TextStream& ts, ScrollSnapType type)
{
    if (type.strictness != ScrollSnapStrictness::None) {
        switch (type.axis) {
        case ScrollSnapAxis::XAxis: ts << "x"; break;
        case ScrollSnapAxis::YAxis: ts << "y"; break;
        case ScrollSnapAxis::Block: ts << "block"; break;
        case ScrollSnapAxis::Inline: ts << "inline"; break;
        case ScrollSnapAxis::Both: ts << "both"; break;
        }
        ts << ' ';
    }
    switch (type.strictness) {
    case ScrollSnapStrictness::None: ts << "none"; break;
    case ScrollSnapStrictness::Proximity: ts << "proximity"; break;
    case ScrollSnapStrictness::Mandatory: ts << "mandatory"; break;
    }
    return ts;
}

} // namespace WebCore
