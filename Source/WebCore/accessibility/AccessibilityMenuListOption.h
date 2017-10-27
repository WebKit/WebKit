/*
 * Copyright (C) 2010 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#include "AccessibilityMockObject.h"

namespace WebCore {

class HTMLElement;

class AccessibilityMenuListOption final : public AccessibilityMockObject {
public:
    static Ref<AccessibilityMenuListOption> create() { return adoptRef(*new AccessibilityMenuListOption); }

    void setElement(HTMLElement*);

private:
    AccessibilityMenuListOption();

    bool isMenuListOption() const override { return true; }

    AccessibilityRole roleValue() const override { return AccessibilityRole::MenuListOption; }
    bool canHaveChildren() const override { return false; }

    Element* actionElement() const override;
    bool isEnabled() const override;
    bool isVisible() const override;
    bool isOffScreen() const override;
    bool isSelected() const override;
    String nameForMSAA() const override;
    void setSelected(bool) override;
    bool canSetSelectedAttribute() const override;
    LayoutRect elementRect() const override;
    String stringValue() const override;
    bool computeAccessibilityIsIgnored() const override;

    RefPtr<HTMLElement> m_element;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_ACCESSIBILITY(AccessibilityMenuListOption, isMenuListOption())
