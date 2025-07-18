/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include "PropertyCascade.h"
#include "ResolvedStyle.h"
#include <wtf/WeakHashMap.h>

namespace WebCore {

class Element;
class WeakPtrImplWithEventTargetData;

namespace Style {

struct CachedMatchResult {
    UnadjustedStyle unadjustedStyle;
    PropertyCascade::IncludedProperties changedProperties;
    CheckedRef<RenderStyle> styleToUpdate;
};

class MatchResultCache {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(MatchResultCache);
public:
    MatchResultCache();
    ~MatchResultCache();

    const std::optional<CachedMatchResult> resultWithCurrentInlineStyle(const Element&);
    static void update(CachedMatchResult&, const RenderStyle&);
    void updateForFastPathInherit(const Element&, const RenderStyle& parentStyle);
    void set(const Element&, const UnadjustedStyle&);

private:
    struct Entry;
    static bool isUsableAfterInlineStyleChange(const MatchResultCache::Entry&, const StyleProperties& inlineStyle);
    static PropertyCascade::IncludedProperties computeAndUpdateChangedProperties(MatchResultCache::Entry&);

    WeakHashMap<const Element, UniqueRef<Entry>, WeakPtrImplWithEventTargetData> m_entries;
};

}
}
