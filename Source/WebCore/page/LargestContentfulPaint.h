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

#include "DOMHighResTimeStamp.h"
#include "PerformanceEntry.h"
#include <wtf/WeakPtr.h>

namespace WebCore {

class Element;
class WeakPtrImplWithEventTargetData;

class LargestContentfulPaint final : public PerformanceEntry {
public:
    static Ref<LargestContentfulPaint> create(DOMHighResTimeStamp timeStamp)
    {
        return adoptRef(*new LargestContentfulPaint(timeStamp));
    }

    ~LargestContentfulPaint();

    // PaintTimingMixin
    DOMHighResTimeStamp paintTime() const;
    std::optional<DOMHighResTimeStamp> presentationTime() const;

    // LargestContentfulPaint
    DOMHighResTimeStamp loadTime() const;
    void setLoadTime(DOMHighResTimeStamp);

    DOMHighResTimeStamp renderTime() const;
    void setRenderTime(DOMHighResTimeStamp);

    DOMHighResTimeStamp startTime() const final;

    unsigned size() const;
    void setSize(unsigned);

    String id() const;
    void setID(const String&);

    String url() const;
    void setURLString(const String&);

    Element* element() const;
    void setElement(Element*);

    ASCIILiteral entryType() const final { return "largest-contentful-paint"_s; }

protected:
    explicit LargestContentfulPaint(DOMHighResTimeStamp);

private:
    Type performanceEntryType() const final { return Type::LargestContentfulPaint; }

    WeakPtr<Element, WeakPtrImplWithEventTargetData> m_element;
    DOMHighResTimeStamp m_loadTime { 0 };
    DOMHighResTimeStamp m_renderTime { 0 };
    String m_urlString;
    String m_id;
    unsigned m_pixelArea { 0 };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_PERFORMANCE_ENTRY(LargestContentfulPaint, LargestContentfulPaint);
