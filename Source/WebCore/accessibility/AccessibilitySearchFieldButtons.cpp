/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "AccessibilitySearchFieldButtons.h"

#include "LocalizedStrings.h"
#include "RenderObject.h"

namespace WebCore {

PassRefPtr<AccessibilitySearchFieldCancelButton> AccessibilitySearchFieldCancelButton::create(RenderObject* renderer)
{
    return adoptRef(new AccessibilitySearchFieldCancelButton(renderer));
}

AccessibilitySearchFieldCancelButton::AccessibilitySearchFieldCancelButton(RenderObject* renderer)
    : AccessibilityRenderObject(renderer)
{
}

String AccessibilitySearchFieldCancelButton::accessibilityDescription() const
{
#if PLATFORM(IOS)
    return String();
#else
    return AXSearchFieldCancelButtonText();
#endif
}

void AccessibilitySearchFieldCancelButton::accessibilityText(Vector<AccessibilityText>& textOrder)
{
    textOrder.append(AccessibilityText(accessibilityDescription(), AlternativeText));
}

bool AccessibilitySearchFieldCancelButton::press() const
{
    Node* node = this->node();
    if (!node || !node->isElementNode())
        return false;
    
    Element* element = toElement(node);
    // The default event handler on SearchFieldCancelButtonElement requires hover.
    element->setHovered(true);
    element->accessKeyAction(true);
    return true;
}

bool AccessibilitySearchFieldCancelButton::computeAccessibilityIsIgnored() const
{
    return accessibilityIsIgnoredByDefault();
}

} // namespace WebCore
