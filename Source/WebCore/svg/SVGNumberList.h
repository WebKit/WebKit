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

#include "SVGNumber.h"
#include "SVGValuePropertyList.h"

namespace WebCore {

class SVGNumberList : public SVGValuePropertyList<SVGNumber> {
    using Base = SVGValuePropertyList<SVGNumber>;
    using Base::Base;

public:
    static Ref<SVGNumberList> create()
    {
        return adoptRef(*new SVGNumberList());
    }

    static Ref<SVGNumberList> create(SVGPropertyOwner* owner, SVGPropertyAccess access)
    {
        return adoptRef(*new SVGNumberList(owner, access));
    }

    static Ref<SVGNumberList> create(const SVGNumberList& other, SVGPropertyAccess access)
    {
        return adoptRef(*new SVGNumberList(other, access));
    }

    bool parse(const String& value)
    {
        clearItems();

        float number = 0;
        auto upconvertedCharacters = StringView(value).upconvertedCharacters();
        const UChar* ptr = upconvertedCharacters;
        const UChar* end = ptr + value.length();

        // The spec (section 4.1) strangely doesn't allow leading whitespace.
        // We might choose to violate that intentionally.
        while (ptr < end) {
            if (!parseNumber(ptr, end, number))
                break;
            append(SVGNumber::create(number));
        }

        return ptr == end;
    }

    String valueAsString() const override
    {
        StringBuilder builder;

        for (const auto& number : m_items) {
            if (builder.length())
                builder.append(' ');

            builder.appendFixedPrecisionNumber(number->value());
        }

        return builder.toString();
    }
};

} // namespace WebCore
