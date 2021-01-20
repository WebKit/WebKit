/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#if ENABLE(INTERSECTION_OBSERVER)

#include "DOMRectReadOnly.h"
#include "Element.h"
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class Element;

class IntersectionObserverEntry : public RefCounted<IntersectionObserverEntry> {
    WTF_MAKE_FAST_ALLOCATED;
public:

    struct Init {
        double time;
        Optional<DOMRectInit> rootBounds;
        DOMRectInit boundingClientRect;
        DOMRectInit intersectionRect;
        double intersectionRatio;
        RefPtr<Element> target;
        bool isIntersecting;
    };

    static Ref<IntersectionObserverEntry> create(const Init& init)
    {
        return WTF::adoptRef(*new IntersectionObserverEntry(init));
    }
    
    double time() const { return m_time; }
    DOMRectReadOnly* rootBounds() const { return m_rootBounds.get(); }
    DOMRectReadOnly* boundingClientRect() const { return m_boundingClientRect.get(); }
    DOMRectReadOnly* intersectionRect() const { return m_intersectionRect.get(); }
    Element* target() const { return m_target.get(); }

    bool isIntersecting() const { return m_isIntersecting; }
    double intersectionRatio() const { return m_intersectionRatio; }

private:
    IntersectionObserverEntry(const Init&);

    double m_time { 0 };
    RefPtr<DOMRectReadOnly> m_rootBounds;
    RefPtr<DOMRectReadOnly> m_boundingClientRect;
    RefPtr<DOMRectReadOnly> m_intersectionRect;
    double m_intersectionRatio { 0 };
    RefPtr<Element> m_target;
    bool m_isIntersecting { false };
};


} // namespace WebCore

#endif // ENABLE(INTERSECTION_OBSERVER)
