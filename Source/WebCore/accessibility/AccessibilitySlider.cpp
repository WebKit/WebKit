/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AccessibilitySlider.h"

#include "AccessibilityObjectInlines.h"
#include "AXLoggerBase.h"
#include "AXObjectCache.h"
#include "ContainerNodeInlines.h"
#include "FrameDestructionObserverInlines.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "RenderSlider.h"
#include "RenderStyle+GettersInlines.h"
#include "SliderThumbElement.h"
#include "StyleAppearance.h"
#include <wtf/Scope.h>

namespace WebCore {

using namespace HTMLNames;

AccessibilitySlider::AccessibilitySlider(AXID axID, RenderObject& renderer, AXObjectCache& cache)
    : AccessibilityRenderObject(axID, renderer, cache)
{
}

Ref<AccessibilitySlider> AccessibilitySlider::create(AXID axID, RenderObject& renderer, AXObjectCache& cache)
{
    return adoptRef(*new AccessibilitySlider(axID, renderer, cache));
}

std::optional<AccessibilityOrientation> AccessibilitySlider::explicitOrientation() const
{
    if (std::optional orientation = orientationFromARIA())
        return orientation;

    CheckedPtr style = this->style();
    // Default to horizontal in the unknown case.
    if (!style)
        return AccessibilityOrientation::Horizontal;

    auto styleAppearance = style->usedAppearance();
    switch (styleAppearance) {
    case StyleAppearance::SliderThumbHorizontal:
    case StyleAppearance::SliderHorizontal:
        return AccessibilityOrientation::Horizontal;

    case StyleAppearance::SliderThumbVertical:
    case StyleAppearance::SliderVertical:
        return AccessibilityOrientation::Vertical;

    default:
        return AccessibilityOrientation::Horizontal;
    }
}

void AccessibilitySlider::addChildren()
{
    AX_ASSERT(!m_childrenInitialized);
    m_childrenInitialized = true;
    auto clearDirtySubtree = makeScopeExit([&] {
        m_subtreeDirty = false;
    });

    CheckedPtr cache = axObjectCache();
    if (!cache)
        return;

    Ref thumb = downcast<AccessibilitySliderThumb>(*cache->create(AccessibilityRole::SliderThumb));
    thumb->setParent(this);

    // Before actually adding the value indicator to the hierarchy,
    // allow the platform to make a final decision about it.
    if (thumb->isIgnored())
        cache->remove(thumb->objectID());
    else
        addChild(thumb.get());

#ifndef NDEBUG
    verifyChildrenIndexInParent();
#endif
}

RefPtr<AccessibilityObject> AccessibilitySlider::elementAccessibilityHitTest(const IntPoint& point) const
{
    if (m_children.size()) {
        AX_ASSERT(m_children.size() == 1);
        if (Ref { m_children[0] }->elementRect().contains(point))
            return downcast<AccessibilityObject>(m_children[0].get());
    }

    return checkedAxObjectCache()->getOrCreate(checkedRenderer().get());
}

float AccessibilitySlider::valueForRange() const
{
    if (RefPtr input = inputElement())
        return input->value()->toFloat();
    return 0;
}

float AccessibilitySlider::maxValueForRange() const
{
    if (RefPtr input = inputElement())
        return static_cast<float>(input->maximum());
    return 0;
}

float AccessibilitySlider::minValueForRange() const
{
    if (RefPtr input = inputElement())
        return static_cast<float>(input->minimum());
    return 0;
}

bool AccessibilitySlider::setValue(const String& value)
{
    RefPtr input = inputElement();
    if (!input)
        return false;

    if (input->value() != value)
        input->setValue(value, DispatchInputAndChangeEvent);
    return true;
}

HTMLInputElement* AccessibilitySlider::inputElement() const
{
    return dynamicDowncast<HTMLInputElement>(node());
}


AccessibilitySliderThumb::AccessibilitySliderThumb(AXID axID, AXObjectCache& cache)
    : AccessibilityMockObject(axID, cache)
{
}

Ref<AccessibilitySliderThumb> AccessibilitySliderThumb::create(AXID axID, AXObjectCache& cache)
{
    return adoptRef(*new AccessibilitySliderThumb(axID, cache));
}

LayoutRect AccessibilitySliderThumb::elementRect() const
{
    if (!m_parent)
        return LayoutRect();

    auto* sliderRenderer = dynamicDowncast<RenderSlider>(m_parent->renderer());
    if (!sliderRenderer)
        return LayoutRect();
    if (CheckedPtr thumbRenderer = protect(sliderRenderer->element())->sliderThumbElement()->renderer())
        return thumbRenderer->absoluteBoundingBoxRect();
    return LayoutRect();
}

bool AccessibilitySliderThumb::computeIsIgnored() const
{
    return isIgnoredByDefault();
}

} // namespace WebCore
