/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "AccessibilityLabel.h"

#include "AXObjectCache.h"
#include "HTMLNames.h"

namespace WebCore {
    
using namespace HTMLNames;

AccessibilityLabel::AccessibilityLabel(RenderObject* renderer)
    : AccessibilityRenderObject(renderer)
{
}

AccessibilityLabel::~AccessibilityLabel() = default;

Ref<AccessibilityLabel> AccessibilityLabel::create(RenderObject* renderer)
{
    return adoptRef(*new AccessibilityLabel(renderer));
}

bool AccessibilityLabel::computeAccessibilityIsIgnored() const
{
    return accessibilityIsIgnoredByDefault();
}

String AccessibilityLabel::stringValue() const
{
    if (containsOnlyStaticText())
        return textUnderElement();
    return AccessibilityNodeObject::stringValue();
}

static bool childrenContainOnlyStaticText(const AccessibilityObject::AccessibilityChildrenVector& children)
{
    if (!children.size())
        return false;
    for (const auto& child : children) {
        if (child->roleValue() == AccessibilityRole::StaticText)
            continue;
        if (child->roleValue() == AccessibilityRole::Group) {
            if (!childrenContainOnlyStaticText(child->children()))
                return false;
        } else
            return false;
    }
    return true;
}

bool AccessibilityLabel::containsOnlyStaticText() const
{
    if (m_containsOnlyStaticTextDirty)
        return childrenContainOnlyStaticText(m_children);
    return m_containsOnlyStaticText;
}

static bool childrenContainUnrelatedControls(const AccessibilityObject::AccessibilityChildrenVector& children, AccessibilityObject* controlForLabel)
{
    if (!children.size())
        return false;

    for (const auto& child : children) {
        if (child->isControl()) {
            if (child == controlForLabel)
                continue;
            return true;
        }

        if (childrenContainUnrelatedControls(child->children(), controlForLabel))
            return true;
    }

    return false;
}

bool AccessibilityLabel::containsUnrelatedControls() const
{
    if (containsOnlyStaticText())
        return false;

    return childrenContainUnrelatedControls(m_children, correspondingControlForLabelElement());
}

void AccessibilityLabel::updateChildrenIfNecessary()
{
    AccessibilityRenderObject::updateChildrenIfNecessary();
    if (m_containsOnlyStaticTextDirty)
        m_containsOnlyStaticText = childrenContainOnlyStaticText(m_children);
    m_containsOnlyStaticTextDirty = false;
}

void AccessibilityLabel::clearChildren()
{
    AccessibilityRenderObject::clearChildren();
    m_containsOnlyStaticText = false;
    m_containsOnlyStaticTextDirty = false;
}

void AccessibilityLabel::insertChild(AccessibilityObject* object, unsigned index)
{
    AccessibilityRenderObject::insertChild(object, index);
    m_containsOnlyStaticTextDirty = true;
}

} // namespace WebCore
