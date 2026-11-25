/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "TextExtractionToStringConversion.h"
#include <wtf/HashMap.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

class TextExtractionCache {
public:
    static constexpr auto maxEntries = 20;
    static constexpr auto contextLineCount = 3;
    static constexpr auto maxRemapCandidates = 4;

    enum class NodeResolution : uint8_t {
        Current,
        Remapped,
        Stale,
        Ambiguous,
        Unknown,
    };

    struct ResolvedNode {
        Vector<RefPtr<StringImpl>> identifiers;
        NodeResolution resolution { NodeResolution::Unknown };
    };

    void add(StringImpl* url, Vector<TextExtractionLineContent>&&);
    ResolvedNode resolve(StringImpl* identifier) const;
    void clear() { m_entries.clear(); }

private:
    struct Entry {
        RefPtr<StringImpl> url;
        Vector<TextExtractionLineContent> lines;
        HashMap<RefPtr<StringImpl>, unsigned> lineIndexForUID;
    };

    static Vector<RefPtr<StringImpl>> contextWindowBefore(const Vector<TextExtractionLineContent>&, unsigned targetIndex);
    static Vector<RefPtr<StringImpl>> contextWindowAfter(const Vector<TextExtractionLineContent>&, unsigned targetIndex);

    Vector<Entry> m_entries;
};

struct StaleNodeResolutionState {
    RefPtr<StringImpl> requestedIdentifier;
    RefPtr<StringImpl> noteForCurrentAttempt;

    bool didRemap() const { return !isEmpty(noteForCurrentAttempt); }
};

} // namespace WebKit
