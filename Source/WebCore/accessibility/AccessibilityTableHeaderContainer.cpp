/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
#include "AccessibilityTableHeaderContainer.h"

#include "AXObjectCache.h"
#include "AccessibilityTable.h"

namespace WebCore {

AccessibilityTableHeaderContainer::AccessibilityTableHeaderContainer(AXID axID)
    : AccessibilityMockObject(axID)
{
}

AccessibilityTableHeaderContainer::~AccessibilityTableHeaderContainer() = default;

Ref<AccessibilityTableHeaderContainer> AccessibilityTableHeaderContainer::create(AXID axID)
{
    return adoptRef(*new AccessibilityTableHeaderContainer(axID));
}
    
LayoutRect AccessibilityTableHeaderContainer::elementRect() const
{
    // this will be filled in when addChildren is called
    return m_headerRect;
}

bool AccessibilityTableHeaderContainer::computeIsIgnored() const
{
#if PLATFORM(IOS_FAMILY) || USE(ATSPI)
    return true;
#endif
    return !m_parent || m_parent->isIgnored();
}

void AccessibilityTableHeaderContainer::addChildren()
{
    ASSERT(!m_childrenInitialized); 
    
    m_childrenInitialized = true;
    RefPtr parentTable = dynamicDowncast<AccessibilityTable>(m_parent.get());
    if (!parentTable || !parentTable->isExposable())
        return;

    for (auto& columnHeader : parentTable->columnHeaders())
        addChild(downcast<AccessibilityObject>(columnHeader.get()));

    for (const auto& child : m_children)
        m_headerRect.unite(child->elementRect());
}

} // namespace WebCore
