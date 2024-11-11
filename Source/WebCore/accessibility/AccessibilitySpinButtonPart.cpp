/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "AccessibilitySpinButtonPart.h"

#include "AccessibilitySpinButton.h"

namespace WebCore {

AccessibilitySpinButtonPart::AccessibilitySpinButtonPart(AXID axID)
    : AccessibilityMockObject(axID)
    , m_isIncrementor(false)
{
}

Ref<AccessibilitySpinButtonPart> AccessibilitySpinButtonPart::create(AXID axID)
{
    return adoptRef(*new AccessibilitySpinButtonPart(axID));
}

LayoutRect AccessibilitySpinButtonPart::elementRect() const
{
    // FIXME: This logic should exist in the render tree or elsewhere, but there is no
    // relationship that exists that can be queried.

    RefPtr parent = parentObject();
    if (!parent)
        return { };

    LayoutRect parentRect = parent->elementRect();
    if (m_isIncrementor)
        parentRect.setHeight(parentRect.height() / 2);
    else {
        parentRect.setY(parentRect.y() + parentRect.height() / 2);
        parentRect.setHeight(parentRect.height() / 2);
    }

    return parentRect;
}

bool AccessibilitySpinButtonPart::press()
{
    RefPtr spinButton = dynamicDowncast<AccessibilitySpinButton>(m_parent.get());
    if (!spinButton)
        return false;

    if (m_isIncrementor)
        spinButton->step(1);
    else
        spinButton->step(-1);

    return true;
}

} // namespace WebCore
