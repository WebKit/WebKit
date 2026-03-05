/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "AccessibilitySpinButton.h"

#include "AXLoggerBase.h"
#include "AXObjectCache.h"
#include "AXUtilities.h"
#include "AccessibilityObjectInlines.h"
#include "ContainerNodeInlines.h"
#include "RenderElement.h"

namespace WebCore {

AccessibilitySpinButton::AccessibilitySpinButton(AXID axID, SpinButtonElement& spinButtonElement, AXObjectCache& cache)
    : AccessibilityMockObject(axID, cache)
    , m_spinButtonElement(spinButtonElement)
{
    // Eagerly initialize our role because it influences the result of the is-ignored
    // computation for us and our child spin-button-parts, which are also created as
    // part of this constructor (thus not allowing us to wait for the normal AccessibilityObject::init()).
    m_role = determineAccessibilityRole();

    lazyInitialize(m_incrementor, Ref { downcast<AccessibilitySpinButtonPart>(*cache.create(AccessibilityRole::SpinButtonPart)) });
    m_incrementor->setIsIncrementor(true);
    lazyInitialize(m_decrementor, Ref { downcast<AccessibilitySpinButtonPart>(*cache.create(AccessibilityRole::SpinButtonPart)) });
    m_decrementor->setIsIncrementor(false);
}

Ref<AccessibilitySpinButton> AccessibilitySpinButton::create(AXID axID, SpinButtonElement& spinButtonElement, AXObjectCache& cache)
{
    Ref spinButton = adoptRef(*new AccessibilitySpinButton(axID, spinButtonElement, cache));
    // We have to do this setup here and not in the constructor to avoid an
    // adoptionIsRequired ASSERT in RefCounted.h.
    spinButton->m_incrementor->setParent(spinButton.ptr());
    spinButton->m_decrementor->setParent(spinButton.ptr());
    spinButton->addChild(spinButton->m_incrementor.get());
    spinButton->addChild(spinButton->m_decrementor.get());
    spinButton->m_childrenInitialized = true;

    return spinButton;
}

AccessibilitySpinButton::~AccessibilitySpinButton() = default;

LayoutRect AccessibilitySpinButton::elementRect() const
{
    AX_ASSERT(m_spinButtonElement);

    CheckedPtr renderer = m_spinButtonElement ? m_spinButtonElement->renderer() : nullptr;
    if (!renderer)
        return { };

    Vector<FloatQuad> quads;
    renderer->absoluteFocusRingQuads(quads);
    return boundingBoxForQuads(renderer.get(), quads);
}

void AccessibilitySpinButton::addChildren()
{
    // This class sets its children once in the create function, and should never
    // have dirty or uninitialized children afterwards.
    AX_ASSERT(m_childrenInitialized);
    AX_ASSERT(!m_subtreeDirty);
    AX_ASSERT(!m_childrenDirty);
}

void AccessibilitySpinButton::step(int amount)
{
    AX_ASSERT(m_spinButtonElement);
    if (RefPtr element = m_spinButtonElement.get())
        element->step(amount);
}

bool AccessibilitySpinButton::computeIsIgnored() const
{
    if (isIgnoredByDefault())
        return true;
    // If the spin button element has no renderer, or is render-hidden (e.g.,
    // inside a collapsed <details> element with content-visibility: hidden),
    // the spin button is not visible.
    CheckedPtr renderer = m_spinButtonElement ? m_spinButtonElement->renderer() : nullptr;
    return !renderer || WebCore::isRenderHidden(protect(renderer->style()).get());
}

} // namespace WebCore
