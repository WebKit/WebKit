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
#include "ResizeObservation.h"

#include "ElementInlines.h"
#include "HTMLFrameOwnerElement.h"
#include "Logging.h"
#include "RenderBox.h"
#include "SVGElement.h"

namespace WebCore {

Ref<ResizeObservation> ResizeObservation::create(Element& target, ResizeObserverBoxOptions observedBox)
{
    return adoptRef(*new ResizeObservation(target, observedBox));
}

ResizeObservation::ResizeObservation(Element& element, ResizeObserverBoxOptions observedBox)
    : m_target { element }
    , m_observedBox { observedBox }
{
}

ResizeObservation::~ResizeObservation() = default;

void ResizeObservation::updateObservationSize(const BoxSizes& boxSizes)
{
    m_lastObservationSizes = boxSizes;
}

auto ResizeObservation::computeObservedSizes() const -> std::optional<BoxSizes>
{
    if (m_target->isSVGElement()) {
        if (auto svgRect = downcast<SVGElement>(*m_target).getBoundingBox()) {
            auto size = LayoutSize(svgRect->width(), svgRect->height());
            return { { size, size, size } };
        }
    }
    auto* box = m_target->renderBox();
    if (box) {
        if (box->isSkippedContent())
            return std::nullopt;
        return { {
            adjustLayoutSizeForAbsoluteZoom(box->contentSize(), *box),
            adjustLayoutSizeForAbsoluteZoom(box->contentLogicalSize(), *box),
            adjustLayoutSizeForAbsoluteZoom(box->borderBoxLogicalSize(), *box)
        } };
    }

    return BoxSizes { };
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
    return FloatRect(FloatPoint(computeTargetLocation()), FloatSize(m_lastObservationSizes.contentBoxSize));
}

FloatSize ResizeObservation::borderBoxSize() const
{
    return m_lastObservationSizes.borderBoxLogicalSize;
}

FloatSize ResizeObservation::contentBoxSize() const
{
    return m_lastObservationSizes.contentBoxLogicalSize;
}

FloatSize ResizeObservation::snappedContentBoxSize() const
{
    return m_lastObservationSizes.contentBoxLogicalSize; // FIXME: Need to pixel snap.
}

std::optional<ResizeObservation::BoxSizes> ResizeObservation::elementSizeChanged() const
{
    auto currentSizes = computeObservedSizes();
    if (!currentSizes)
        return std::nullopt;

    LOG_WITH_STREAM(ResizeObserver, stream << "ResizeObservation " << this << " elementSizeChanged - new content box " << currentSizes->contentBoxSize);

    switch (m_observedBox) {
    case ResizeObserverBoxOptions::BorderBox:
        if (m_lastObservationSizes.borderBoxLogicalSize != currentSizes->borderBoxLogicalSize)
            return currentSizes;
        break;
    case ResizeObserverBoxOptions::ContentBox:
        if (m_lastObservationSizes.contentBoxLogicalSize != currentSizes->contentBoxLogicalSize)
            return currentSizes;
        break;
    }

    return { };
}

size_t ResizeObservation::targetElementDepth() const
{
    unsigned depth = 0;
    for (Element* ownerElement = m_target.get(); ownerElement; ownerElement = ownerElement->document().ownerElement()) {
        for (Element* parent = ownerElement; parent; parent = parent->parentElement())
            ++depth;
    }

    return depth;
}

TextStream& operator<<(TextStream& ts, const ResizeObservation& observation)
{
    ts.dumpProperty("target", ValueOrNull(observation.target()));
    ts.dumpProperty("border box", observation.borderBoxSize());
    ts.dumpProperty("content box", observation.contentBoxSize());
    ts.dumpProperty("snapped content box", observation.snappedContentBoxSize());
    return ts;
}

} // namespace WebCore
