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

#include "AXObjectCache.h"
#include "RenderElement.h"

namespace WebCore {

Ref<AccessibilitySpinButton> AccessibilitySpinButton::create(AXID axID, AXObjectCache& cache)
{
    return adoptRef(*new AccessibilitySpinButton(axID, cache));
}
    
AccessibilitySpinButton::AccessibilitySpinButton(AXID axID, AXObjectCache& cache)
    : AccessibilityMockObject(axID)
    , m_spinButtonElement(nullptr)
    , m_incrementor(downcast<AccessibilitySpinButtonPart>(*cache.create(AccessibilityRole::SpinButtonPart)))
    , m_decrementor(downcast<AccessibilitySpinButtonPart>(*cache.create(AccessibilityRole::SpinButtonPart)))
{
    m_incrementor->setIsIncrementor(true);
    m_incrementor->setParent(this);

    m_decrementor->setIsIncrementor(false);
    m_decrementor->setParent(this);

    addChild(m_incrementor.ptr());
    addChild(m_decrementor.ptr());
    m_childrenInitialized = true;
}

AccessibilitySpinButton::~AccessibilitySpinButton() = default;
    
AXCoreObject* AccessibilitySpinButton::incrementButton()
{
    ASSERT(m_childrenInitialized);
    RELEASE_ASSERT(m_children.size() == 2);
    return m_children[0].get();
}
   
AXCoreObject* AccessibilitySpinButton::decrementButton()
{
    ASSERT(m_childrenInitialized);
    RELEASE_ASSERT(m_children.size() == 2);
    return m_children[1].get();
}
    
LayoutRect AccessibilitySpinButton::elementRect() const
{
    ASSERT(m_spinButtonElement);
    
    CheckedPtr renderer = m_spinButtonElement ? m_spinButtonElement->renderer() : nullptr;
    if (!renderer)
        return { };

    Vector<FloatQuad> quads;
    renderer->absoluteFocusRingQuads(quads);
    return boundingBoxForQuads(renderer.get(), quads);
}

void AccessibilitySpinButton::addChildren()
{
    // This class sets its children once in the constructor, and should never
    // have dirty or uninitialized children afterwards.
    ASSERT(m_childrenInitialized);
    ASSERT(!m_subtreeDirty);
    ASSERT(!m_childrenDirty);
}
    
void AccessibilitySpinButton::step(int amount)
{
    ASSERT(m_spinButtonElement);
    if (m_spinButtonElement)
        m_spinButtonElement->step(amount);
}

} // namespace WebCore
