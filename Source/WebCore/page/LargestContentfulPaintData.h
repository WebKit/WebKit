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

#include "FloatRect.h"
#include <wtf/FastMalloc.h>
#include <wtf/Markable.h>
#include <wtf/MonotonicTime.h>
#include <wtf/WeakHashMap.h>
#include <wtf/WeakHashSet.h>

namespace WebCore {

class CachedImage;
class Element;
class HTMLImageElement;
class LargestContentfulPaint;
class Node;
class RenderBlockFlow;
class Text;
class WeakPtrImplWithEventTargetData;

using DOMHighResTimeStamp = double;

struct PerElementImageData {
    WeakPtr<CachedImage> image;
    FloatRect rect;
    Markable<MonotonicTime> loadTime;
    bool inContentSet { false };
};

struct ElementLargestContentfulPaintData {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(ElementLargestContentfulPaintData);
    FloatRect accumulatedTextRect;
    Vector<PerElementImageData> imageData;
};

class LargestContentfulPaintData {
    WTF_MAKE_TZONE_ALLOCATED(LargestContentfulPaintData);
public:
    LargestContentfulPaintData();
    ~LargestContentfulPaintData();

    void didLoadImage(Element&, CachedImage*);
    void didPaintImage(Element&, CachedImage*, FloatRect localRect);
    void didPaintText(const RenderBlockFlow& formattingContextRoot, FloatRect localRect, bool isOnlyTextBoxForElement);

    RefPtr<LargestContentfulPaint> generateLargestContentfulPaintEntry(DOMHighResTimeStamp);

    static bool isExposedForPaintTiming(const Element&);

private:

    static std::optional<float> effectiveVisualArea(const Element&, CachedImage*, FloatRect imageLocalRect, FloatRect intersectionRect, FloatSize viewportSize);

    static FloatRect computeViewportIntersectionRect(Element&, FloatRect localRect);
    static FloatRect computeViewportIntersectionRectForTextContainer(Element&, const WeakHashSet<Text, WeakPtrImplWithEventTargetData>&);

    static bool NODELETE isEligibleForLargestContentfulPaint(const Element&, float effectiveVisualArea);
    static bool canCompareWithLargestPaintArea(const Element&);

    void potentiallyAddLargestContentfulPaintEntry(Element&, CachedImage*, FloatRect imageLocalRect, FloatRect intersectionRect, MonotonicTime loadTime, DOMHighResTimeStamp paintTimestamp, std::optional<FloatSize>& viewportSize);

    void scheduleRenderingUpdateIfNecessary(Element&);

    float m_largestPaintArea { 0 };

    WeakHashSet<Element, WeakPtrImplWithEventTargetData> m_paintedTextRecords;
    WeakHashMap<Element, Vector<WeakPtr<CachedImage>>, WeakPtrImplWithEventTargetData> m_pendingImageRecords;

    RefPtr<LargestContentfulPaint> m_pendingEntry;
    bool m_haveNewCandidate { false };
};

} // namespace WebCore
