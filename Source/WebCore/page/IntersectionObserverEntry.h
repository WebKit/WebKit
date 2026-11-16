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

#include "DOMRectReadOnly.h"
#include "Element.h"
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

class Element;

class IntersectionObserverEntry : public RefCounted<IntersectionObserverEntry> {
    WTF_MAKE_TZONE_ALLOCATED(IntersectionObserverEntry);
public:

    struct Init {
        double time;
        std::optional<DOMRectInit> rootBounds;
        DOMRectInit boundingClientRect;
        DOMRectInit intersectionRect;
        bool isIntersecting;
        double intersectionRatio;
        Ref<Element> target;
    };

    static Ref<IntersectionObserverEntry> create(Init&& init)
    {
        return adoptRef(*new IntersectionObserverEntry(WTF::move(init)));
    }

    double time() const { return m_time; }
    DOMRectReadOnly* rootBounds() const { return m_rootBounds.get(); }
    DOMRectReadOnly& boundingClientRect() const { return m_boundingClientRect; }
    DOMRectReadOnly& intersectionRect() const { return m_intersectionRect; }
    Element& target() const { return m_target; }

    bool isIntersecting() const { return m_isIntersecting; }
    double intersectionRatio() const { return m_intersectionRatio; }

private:
    explicit IntersectionObserverEntry(Init&&);

    double m_time { 0 };
    const RefPtr<DOMRectReadOnly> m_rootBounds;
    const Ref<DOMRectReadOnly> m_boundingClientRect;
    const Ref<DOMRectReadOnly> m_intersectionRect;
    double m_intersectionRatio { 0 };
    const Ref<Element> m_target;
    bool m_isIntersecting { false };
};

TextStream& operator<<(TextStream&, const IntersectionObserverEntry&);

} // namespace WebCore
