/*
 * Copyright (C) 2023 ChangSeok Oh <changseok@webkit.org>
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

#pragma once

#include <variant>
#include <wtf/Markable.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

struct FloatMarkableTraits {
    constexpr static bool isEmptyValue(float value)
    {
        return value != value;
    }

    constexpr static float emptyValue()
    {
        return std::numeric_limits<float>::quiet_NaN();
    }
};

struct FontSizeAdjust {
    bool operator==(const FontSizeAdjust& other) const
    {
        return metric == other.metric && value == other.value
            && isFromFont == other.isFromFont;
    }

    enum class Metric : uint8_t {
        ExHeight,
        CapHeight,
        ChWidth,
        IcWidth,
        IcHeight
    };
    Metric metric { Metric::ExHeight };
    bool isFromFont { false };
    Markable<float, FloatMarkableTraits> value { };
};

inline void add(Hasher& hasher, const FontSizeAdjust& fontSizeAdjust)
{
    add(hasher, fontSizeAdjust.metric, *fontSizeAdjust.value);
}

inline TextStream& operator<<(TextStream& ts, const FontSizeAdjust& fontSizeAdjust)
{
    switch (fontSizeAdjust.metric) {
    case FontSizeAdjust::Metric::CapHeight:
        ts << "cap-height";
        break;
    case FontSizeAdjust::Metric::ChWidth:
        ts << "ch-width";
        break;
    case FontSizeAdjust::Metric::IcWidth:
        ts << "ic-width";
        break;
    case FontSizeAdjust::Metric::IcHeight:
        ts << "ic-height";
        break;
    case FontSizeAdjust::Metric::ExHeight:
    default:
        if (fontSizeAdjust.isFromFont)
            return ts << "from-font";
        return ts << *fontSizeAdjust.value;
    }

    if (fontSizeAdjust.isFromFont)
        return ts << " " << "from-font";
    return ts << " " << fontSizeAdjust.value;
}

}
