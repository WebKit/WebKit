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

#pragma once

#include "AccessibilityMockObject.h"
#include "AccessibilitySpinButtonPart.h"
#include "SpinButtonElement.h"

namespace WebCore {

// Currently only represents native spinbuttons (i.e. <input type="number">) and not role="spinbutton" elements.
class AccessibilitySpinButton final : public AccessibilityMockObject {
public:
    static Ref<AccessibilitySpinButton> create(AXID, AXObjectCache&);
    virtual ~AccessibilitySpinButton();

    void setSpinButtonElement(SpinButtonElement* spinButton) { m_spinButtonElement = spinButton; }

    AXCoreObject* incrementButton() final;
    AXCoreObject* decrementButton() final;

    void step(int amount);

private:
    explicit AccessibilitySpinButton(AXID, AXObjectCache&);

    AccessibilityRole determineAccessibilityRole() final { return AccessibilityRole::SpinButton; }
    bool isNativeSpinButton() const override { return true; }
    void clearChildren() final { };
    void addChildren() final;
    LayoutRect elementRect() const override;

    WeakPtr<SpinButtonElement, WeakPtrImplWithEventTargetData> m_spinButtonElement;
    Ref<AccessibilitySpinButtonPart> m_incrementor;
    Ref<AccessibilitySpinButtonPart> m_decrementor;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::AccessibilitySpinButton) \
    static bool isType(const WebCore::AccessibilityObject& object) { return object.isNativeSpinButton(); } \
SPECIALIZE_TYPE_TRAITS_END()
