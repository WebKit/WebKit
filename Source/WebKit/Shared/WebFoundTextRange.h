/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include <wtf/HashTraits.h>
#include <wtf/text/WTFString.h>

namespace WTF {
class TextStream;
}

namespace WebKit {

struct WebFoundTextRange {
    struct DOMData {
        uint64_t location { 0 };
        uint64_t length { 0 };

        bool operator==(const DOMData& other) const = default;
    };

    struct PDFData {
        uint64_t startPage { 0 };
        uint64_t startOffset { 0 };
        uint64_t endPage { 0 };
        uint64_t endOffset { 0 };

        bool operator==(const PDFData& other) const = default;
        unsigned hash() const;
    };

    Variant<DOMData, PDFData> data { DOMData { } };
    AtomString frameIdentifier;
    uint64_t order { 0 };

    unsigned hash() const;

    bool operator==(const WebFoundTextRange& other) const;
};

TextStream& operator<<(TextStream&, const WebKit::WebFoundTextRange::PDFData&);

} // namespace WebKit

namespace WTF {

struct WebFoundTextRangePDFDataHash {
    static unsigned hash(const WebKit::WebFoundTextRange::PDFData& data) { return data.hash(); }
    static bool equal(const WebKit::WebFoundTextRange::PDFData& a, const WebKit::WebFoundTextRange::PDFData& b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = true;
};

template<> struct HashTraits<WebKit::WebFoundTextRange::PDFData> : GenericHashTraits<WebKit::WebFoundTextRange::PDFData> {
    static constexpr bool emptyValueIsZero = false;
    static constexpr bool needsDeletedValue = true;

private:
    static constexpr auto sentinelUnit { std::numeric_limits<uint64_t>::max() };
    static constexpr WebKit::WebFoundTextRange::PDFData sentinel { sentinelUnit, sentinelUnit, sentinelUnit, sentinelUnit };

    static constexpr auto deletedSentinelUnit { sentinelUnit - 1 };
    static constexpr WebKit::WebFoundTextRange::PDFData deletedSentinel { deletedSentinelUnit, deletedSentinelUnit, deletedSentinelUnit, deletedSentinelUnit };

public:
    static WebKit::WebFoundTextRange::PDFData emptyValue()
    {
        return sentinel;
    }
    static bool isEmptyValue(const WebKit::WebFoundTextRange::PDFData& data)
    {
        return data == sentinel;
    }
    static void constructDeletedValue(WebKit::WebFoundTextRange::PDFData& slot)
    {
        slot = deletedSentinel;
    }
    static bool isDeletedValue(const WebKit::WebFoundTextRange::PDFData& data)
    {
        return data == deletedSentinel;
    }
};

struct WebFoundTextRangeHash {
    static unsigned hash(const WebKit::WebFoundTextRange& range) { return range.hash(); }
    static bool equal(const WebKit::WebFoundTextRange& a, const WebKit::WebFoundTextRange& b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = true;
};

template<> struct HashTraits<WebKit::WebFoundTextRange> : GenericHashTraits<WebKit::WebFoundTextRange> {
    static WebKit::WebFoundTextRange emptyValue() { return { }; }

    static void constructDeletedValue(WebKit::WebFoundTextRange& slot) { new (NotNull, &slot.frameIdentifier) AtomString { HashTableDeletedValue }; }
    static bool isDeletedValue(const WebKit::WebFoundTextRange& range) { return range.frameIdentifier.isHashTableDeletedValue(); }
};

template<> struct DefaultHash<WebKit::WebFoundTextRange::PDFData> : WebFoundTextRangePDFDataHash { };
template<> struct DefaultHash<WebKit::WebFoundTextRange> : WebFoundTextRangeHash { };

}
