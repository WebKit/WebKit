/*
 * Copyright (C) 2019 Igalia S.L.
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
#include "LayoutSize.h"
#include "ResizeObserverBoxOptions.h"

#include <wtf/IsoMalloc.h>
#include <wtf/RefCounted.h>
#include <wtf/WeakPtr.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

class Element;
class WeakPtrImplWithEventTargetData;

class ResizeObservation : public RefCounted<ResizeObservation> {
    WTF_MAKE_ISO_ALLOCATED(ResizeObservation);
public:
    static Ref<ResizeObservation> create(Element& target, ResizeObserverBoxOptions);

    ~ResizeObservation();
    
    struct BoxSizes {
        LayoutSize contentBoxSize;
        LayoutSize contentBoxLogicalSize;
        LayoutSize borderBoxLogicalSize;
    };

    std::optional<BoxSizes> elementSizeChanged() const;
    void updateObservationSize(const BoxSizes&);
    void resetObservationSize();

    FloatRect computeContentRect() const;
    FloatSize borderBoxSize() const;
    FloatSize contentBoxSize() const;
    FloatSize snappedContentBoxSize() const;

    Element* target() const { return m_target.get(); }
    ResizeObserverBoxOptions observedBox() const { return m_observedBox; }
    size_t targetElementDepth() const;

private:
    ResizeObservation(Element&, ResizeObserverBoxOptions);

    std::optional<BoxSizes> computeObservedSizes() const;
    LayoutPoint computeTargetLocation() const;

    WeakPtr<Element, WeakPtrImplWithEventTargetData> m_target;
    BoxSizes m_lastObservationSizes;
    ResizeObserverBoxOptions m_observedBox;
};

WTF::TextStream& operator<<(WTF::TextStream&, const ResizeObservation&);

} // namespace WebCore
