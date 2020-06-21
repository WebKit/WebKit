/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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

#include "SVGPoint.h"
#include "SVGValuePropertyList.h"

namespace WebCore {

class SVGPointList : public SVGValuePropertyList<SVGPoint> {
    using Base = SVGValuePropertyList<SVGPoint>;
    using Base::Base;

public:
    static Ref<SVGPointList> create()
    {
        return adoptRef(*new SVGPointList());
    }

    static Ref<SVGPointList> create(SVGPropertyOwner* owner, SVGPropertyAccess access)
    {
        return adoptRef(*new SVGPointList(owner, access));
    }

    static Ref<SVGPointList> create(const SVGPointList& other, SVGPropertyAccess access)
    {
        return adoptRef(*new SVGPointList(other, access));
    }

    bool parse(const String& value)
    {
        clearItems();

        auto upconvertedCharacters = StringView(value).upconvertedCharacters();
        const UChar* cur = upconvertedCharacters;
        const UChar* end = cur + value.length();

        skipOptionalSVGSpaces(cur, end);

        bool delimParsed = false;
        while (cur < end) {
            delimParsed = false;

            auto xPos = parseNumber(cur, end);
            if (!xPos)
                return false;

            auto yPos = parseNumber(cur, end, SuffixSkippingPolicy::DontSkip);
            if (!yPos)
                return false;

            skipOptionalSVGSpaces(cur, end);

            if (cur < end && *cur == ',') {
                delimParsed = true;
                cur++;
            }
            skipOptionalSVGSpaces(cur, end);

            append(SVGPoint::create({ *xPos, *yPos }));
        }

        return !delimParsed;
    }

    String valueAsString() const override
    {
        StringBuilder builder;

        for (const auto& point : m_items) {
            if (builder.length())
                builder.append(' ');

            builder.append(point->x(), ' ', point->y());
        }

        return builder.toString();
    }
};

}
