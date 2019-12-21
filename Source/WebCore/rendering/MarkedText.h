/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class RenderedDocumentMarker;

struct MarkedText {
    // Sorted by paint order
    enum Type {
        Unmarked,
        GrammarError,
        Correction,
        SpellingError,
        TextMatch,
        DictationAlternatives,
        Highlight,
#if PLATFORM(IOS_FAMILY)
        // FIXME: See <rdar://problem/8933352>. Also, remove the PLATFORM(IOS_FAMILY)-guard.
        DictationPhraseWithAlternatives,
#endif
        Selection,
        DraggedContent,
    };
    unsigned startOffset;
    unsigned endOffset;
    Type type;
    const RenderedDocumentMarker* marker { nullptr };
    String highlightName { };

    bool isEmpty() const { return endOffset <= startOffset; }
    bool operator!=(const MarkedText& other) const { return !(*this == other); }
    bool operator==(const MarkedText& other) const
    {
        return startOffset == other.startOffset && endOffset == other.endOffset && type == other.type && marker == other.marker && highlightName == other.highlightName;
    }
};

enum class OverlapStrategy { None, Frontmost };
WEBCORE_EXPORT Vector<MarkedText> subdivide(const Vector<MarkedText>&, OverlapStrategy = OverlapStrategy::None);

}

