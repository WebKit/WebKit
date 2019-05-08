/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#include "ArgumentCoders.h"
#include <wtf/EnumTraits.h>
#include <wtf/RefPtr.h>

namespace WebCore {
class Frame;
class Range;
}

namespace WebKit {

enum class EditingRangeIsRelativeTo : uint8_t {
    EditableRoot,
    Paragraph,
};

struct EditingRange {
    EditingRange()
        : location(notFound)
        , length(0)
    {
    }

    EditingRange(uint64_t location, uint64_t length)
        : location(location)
        , length(length)
    {
    }

    // (notFound, 0) is notably valid.
    bool isValid() const { return location + length >= location; }

    static RefPtr<WebCore::Range> toRange(WebCore::Frame&, const EditingRange&, EditingRangeIsRelativeTo = EditingRangeIsRelativeTo::EditableRoot);
    static EditingRange fromRange(WebCore::Frame&, const WebCore::Range*, EditingRangeIsRelativeTo = EditingRangeIsRelativeTo::EditableRoot);

#if defined(__OBJC__)
    EditingRange(NSRange range)
    {
        if (range.location != NSNotFound) {
            location = range.location;
            length = range.length;
        } else {
            location = notFound;
            length = 0;
        }
    }

    operator NSRange() const
    {
        if (location == notFound)
            return NSMakeRange(NSNotFound, 0);
        return NSMakeRange(location, length);
    }
#endif

    uint64_t location;
    uint64_t length;
};

}

namespace IPC {
template<> struct ArgumentCoder<WebKit::EditingRange> {
    static void encode(Encoder&, const WebKit::EditingRange&);
    static Optional<WebKit::EditingRange> decode(Decoder&);
};
}

namespace WTF {
template<> struct EnumTraits<WebKit::EditingRangeIsRelativeTo> {
    using values = EnumValues<WebKit::EditingRangeIsRelativeTo, WebKit::EditingRangeIsRelativeTo::EditableRoot, WebKit::EditingRangeIsRelativeTo::Paragraph>;
};
}
