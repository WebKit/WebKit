/*
 * Copyright (C) 2023 Igalia S.L. All rights reserved.
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

#include "IntersectionObserver.h"
#include <wtf/WeakHashMap.h>

namespace WebCore {

class Document;
class Element;

enum class DidUpdateAnyContentRelevancy : bool { No, Yes };
enum class IsSkippedContent : bool { No, Yes };
// https://drafts.csswg.org/css-contain/#proximity-to-the-viewport
enum class ViewportProximity : bool { Far, Near };

enum class HadInitialVisibleContentVisibilityDetermination : bool { No, Yes };

class ContentVisibilityDocumentState {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static void observe(Element&);
    static void unobserve(Element&);

    void updateContentRelevancyForScrollIfNeeded(const Element& scrollAnchor);

    bool hasObservationTargets() const { return m_observer && m_observer->hasObservationTargets(); }

    DidUpdateAnyContentRelevancy updateRelevancyOfContentVisibilityElements(OptionSet<ContentRelevancy>) const;
    HadInitialVisibleContentVisibilityDetermination determineInitialVisibleContentVisibility() const;

    void updateViewportProximity(const Element&, ViewportProximity);

    static void updateAnimations(const Element&, IsSkippedContent wasSkipped, IsSkippedContent becomesSkipped);

private:
    bool checkRelevancyOfContentVisibilityElement(Element&, OptionSet<ContentRelevancy>) const;

    void removeViewportProximity(const Element&);

    IntersectionObserver* intersectionObserver(Document&);

    RefPtr<IntersectionObserver> m_observer;

    WeakHashMap<Element, ViewportProximity, WeakPtrImplWithEventTargetData> m_elementViewportProximities;
};

} // namespace
