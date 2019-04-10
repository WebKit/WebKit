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

#include "config.h"

#if ENABLE(RESIZE_OBSERVER)
#include "ResizeObservation.h"

#include "HTMLFrameOwnerElement.h"
#include "RenderBox.h"
#include "SVGElement.h"

namespace WebCore {

Ref<ResizeObservation> ResizeObservation::create(Element* target)
{
    return adoptRef(*new ResizeObservation(target));
}

ResizeObservation::ResizeObservation(Element* element)
    : m_target(element)
{
}

ResizeObservation::~ResizeObservation()
{
}

void ResizeObservation::updateObservationSize(const LayoutSize& size)
{
    m_lastObservationSize = size;
}

LayoutSize ResizeObservation::computeObservedSize() const
{
    if (m_target->isSVGElement()) {
        FloatRect svgRect;
        if (downcast<SVGElement>(*m_target).getBoundingBox(svgRect))
            return LayoutSize(svgRect.width(), svgRect.height());
    }
    if (m_target->renderBox())
        return m_target->renderBox()->contentSize();
    return LayoutSize();
}

LayoutPoint ResizeObservation::computeTargetLocation() const
{
    if (!m_target->isSVGElement()) {
        if (auto box = m_target->renderBox())
            return LayoutPoint(box->paddingLeft(), box->paddingTop());
    }

    return { };
}

FloatRect ResizeObservation::computeContentRect() const
{
    return FloatRect(FloatPoint(computeTargetLocation()), FloatSize(m_lastObservationSize));
}

bool ResizeObservation::elementSizeChanged(LayoutSize& currentSize) const
{
    currentSize = computeObservedSize();
    return m_lastObservationSize != currentSize;
}

size_t ResizeObservation::targetElementDepth() const
{
    unsigned depth = 0;
    for (Element* ownerElement  = m_target; ownerElement; ownerElement = ownerElement->document().ownerElement()) {
        for (Element* parent = ownerElement; parent; parent = parent->parentElement())
            ++depth;
    }

    return depth;
}

} // namespace WebCore

#endif // ENABLE(RESIZE_OBSERVER)
