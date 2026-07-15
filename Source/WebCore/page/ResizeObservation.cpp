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
#include "LayoutPoint.h"
#include "Logging.h"
#include "RenderBoxInlines.h"
#include "RenderElementStyleInlines.h"
#include "SVGElement.h"
#include "Settings.h"
#include "StyleZoomPrimitivesInlines.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

Ref<ResizeObservation> ResizeObservation::create(Element& target, ResizeObserverBoxOptions observedBox)
{
    return adoptRef(*new ResizeObservation(target, observedBox));
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(ResizeObservation);

ResizeObservation::ResizeObservation(Element& element, ResizeObserverBoxOptions observedBox)
    : m_target { element }
    , m_lastObservationSizes { LayoutSize(-1, -1), LayoutSize(-1, -1), LayoutSize(-1, -1), LayoutSize(-1, -1) }
    , m_observedBox { observedBox }
{
}

ResizeObservation::~ResizeObservation() = default;

void ResizeObservation::updateObservationSize(const BoxSizes& boxSizes)
{
    m_lastObservationSizes = boxSizes;
}

void ResizeObservation::resetObservationSize()
{
    m_lastObservationSizes = { LayoutSize(-1, -1), LayoutSize(-1, -1), LayoutSize(-1, -1), LayoutSize(-1, -1) };
}

// Computing the device-pixel-content-box is comparatively expensive: for a RenderBox it needs
// the element's absolute position (localToAbsolute) to snap to the device pixel grid. So it is
// only computed when actually needed: when the device-pixel-content-box is the observed box (so
// per-frame change detection in computeObservedSizes() can compare it), and when building the
// entry to deliver (which always exposes devicePixelContentBoxSize, regardless of observed box).
LayoutSize ResizeObservation::computeDevicePixelContentBoxLogicalSize() const
{
    RefPtr target = m_target;
    if (!target)
        return { };

    Ref document = target->document();
    if (!document->settings().resizeObserverDevicePixelContentBoxEnabled())
        return { };

    auto deviceScaleFactor = document->deviceScaleFactor();

    if (RefPtr svg = dynamicDowncast<SVGElement>(target.get())) {
        if (!svg->hasAssociatedSVGLayoutBox())
            return { };
        LayoutSize size;
        if (auto svgRect = svg->getBoundingBox()) {
            size.setWidth(svgRect->width());
            size.setHeight(svgRect->height());
        }
        float zoom = svg->computedStyle() ? svg->computedStyle()->usedZoom() : 1.0f;
        return LayoutSize(roundf(size.width() * zoom * deviceScaleFactor), roundf(size.height() * zoom * deviceScaleFactor));
    }

    CheckedPtr box = target->renderBox();
    if (!box || box->isSkippedContent())
        return { };

    auto absoluteContentBoxOrigin = box->localToAbsolute(FloatPoint(box->contentBoxLocation()));
    LayoutPoint snappingOrigin(absoluteContentBoxOrigin.x(), absoluteContentBoxOrigin.y());
    auto snappedPhysicalSize = snapSizeToDevicePixel(box->contentBoxSize(), snappingOrigin, deviceScaleFactor);
    FloatSize snappedLogicalSize = box->isHorizontalWritingMode() ? snappedPhysicalSize : snappedPhysicalSize.transposedSize();
    return LayoutSize(snappedLogicalSize.width() * deviceScaleFactor, snappedLogicalSize.height() * deviceScaleFactor);
}

auto ResizeObservation::computeObservedSizes() const -> std::optional<BoxSizes>
{
    // Only compute the (expensive) device-pixel-content-box here when it is the observed box, so
    // that per-frame change detection can compare it. The delivered entry computes it lazily.
    LayoutSize devicePixelContentBoxLogicalSize;
    if (m_observedBox == ResizeObserverBoxOptions::DevicePixelContentBox)
        devicePixelContentBoxLogicalSize = computeDevicePixelContentBoxLogicalSize();

    if (RefPtr svg = dynamicDowncast<SVGElement>(target())) {
        if (svg->hasAssociatedSVGLayoutBox()) {
            LayoutSize size;
            if (auto svgRect = svg->getBoundingBox()) {
                size.setWidth(svgRect->width());
                size.setHeight(svgRect->height());
            }
            return { { size, size, size, devicePixelContentBoxLogicalSize } };
        }
    }

    if (RefPtr target = m_target) {
        if (CheckedPtr box = target->renderBox()) {
            if (box->isSkippedContent())
                return std::nullopt;
            return { {
                Style::adjustLayoutSizeForAbsoluteZoom(box->contentBoxSize(), *box),
                Style::adjustLayoutSizeForAbsoluteZoom(box->contentBoxLogicalSize(), *box),
                Style::adjustLayoutSizeForAbsoluteZoom(box->logicalSize(), *box),
                devicePixelContentBoxLogicalSize
            } };
        }
    }

    return BoxSizes { };
}

LayoutPoint ResizeObservation::computeTargetLocation() const
{
    if (RefPtr target = m_target; target && !target->isSVGElement()) {
        if (CheckedPtr box = target->renderBox()) {
            auto zoom = box->style().usedZoom();
            return LayoutPoint(box->paddingLeft() / zoom, box->paddingTop() / zoom);
        }
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

FloatSize ResizeObservation::devicePixelContentBoxSize() const
{
    // The delivered entry always exposes devicePixelContentBoxSize regardless of the observed box,
    // so compute it lazily here (at delivery) rather than storing it for every observation.
    return computeDevicePixelContentBoxLogicalSize();
}

std::optional<ResizeObservation::BoxSizes> ResizeObservation::elementSizeChanged() const
{
    auto currentSizes = computeObservedSizes();
    if (!currentSizes)
        return std::nullopt;

    switch (m_observedBox) {
    case ResizeObserverBoxOptions::BorderBox:
        if (m_lastObservationSizes.borderBoxLogicalSize != currentSizes->borderBoxLogicalSize) {
            LOG_WITH_STREAM(ResizeObserver, stream << "ResizeObservation " << *this << " elementSizeChanged - border box size changed from " << m_lastObservationSizes.borderBoxLogicalSize << " to " << currentSizes->borderBoxLogicalSize);
            return currentSizes;
        }
        break;
    case ResizeObserverBoxOptions::ContentBox:
        if (m_lastObservationSizes.contentBoxLogicalSize != currentSizes->contentBoxLogicalSize) {
            LOG_WITH_STREAM(ResizeObserver, stream << "ResizeObservation " << *this << " elementSizeChanged - content box size changed from " << m_lastObservationSizes.contentBoxLogicalSize << " to " << currentSizes->contentBoxLogicalSize);
            return currentSizes;
        }
        break;
    case ResizeObserverBoxOptions::DevicePixelContentBox:
        if (m_lastObservationSizes.devicePixelContentBoxLogicalSize != currentSizes->devicePixelContentBoxLogicalSize) {
            LOG_WITH_STREAM(ResizeObserver, stream << "ResizeObservation " << *this << " elementSizeChanged - device pixel content box size changed from " << m_lastObservationSizes.devicePixelContentBoxLogicalSize << " to " << currentSizes->devicePixelContentBoxLogicalSize);
            return currentSizes;
        }
        break;
    }

    return { };
}

// https://drafts.csswg.org/resize-observer/#calculate-depth-for-node
size_t ResizeObservation::targetElementDepth() const
{
    unsigned depth = 0;
    for (auto* ownerElement = m_target.get(); ownerElement; ownerElement = ownerElement->document().ownerElement()) {
        for (auto* parent = ownerElement; parent; parent = parent->parentElementInComposedTree())
            ++depth;
    }

    return depth;
}

TextStream& operator<<(TextStream& ts, const ResizeObservation& observation)
{
    ts.dumpProperty("target"_s, ValueOrNull(observation.target()));

    if (CheckedPtr box = observation.target()->renderBox())
        ts.dumpProperty("target box"_s, box);

    ts.dumpProperty("border box"_s, observation.borderBoxSize());
    ts.dumpProperty("content box"_s, observation.contentBoxSize());
    ts.dumpProperty("snapped content box"_s, observation.snappedContentBoxSize());
    ts.dumpProperty("device pixel content box"_s, observation.devicePixelContentBoxSize());
    return ts;
}

} // namespace WebCore
