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

class AccessibilityMenuList;
class AccessibilityMenuListOption;
class HTMLElement;

class AccessibilityMenuListPopup final : public AccessibilityMockObject {
    friend class AXObjectCache;
public:
    static Ref<AccessibilityMenuListPopup> create() { return adoptRef(*new AccessibilityMenuListPopup); }

    bool isEnabled() const override;
    bool isOffScreen() const override;

    void didUpdateActiveOption(int optionIndex);

private:
    AccessibilityMenuListPopup();

    bool isMenuListPopup() const final { return true; }

    LayoutRect elementRect() const final { return LayoutRect(); }
    AccessibilityRole determineAccessibilityRole() final { return AccessibilityRole::MenuListPopup; }

    bool isVisible() const override;
    bool press() override;
    void addChildren() override;
    void handleChildrenChanged();
    bool computeAccessibilityIsIgnored() const override;
    std::optional<AccessibilityChildrenVector> selectedChildren() final;

    AccessibilityMenuListOption* menuListOptionAccessibilityObject(HTMLElement*) const;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::AccessibilityMenuListPopup)
    static bool isType(const WebCore::AccessibilityObject& object) { return object.isMenuListPopup(); }
    static bool isType(const WebCore::AXCoreObject& object)
    {
        auto* accessibilityObject = dynamicDowncast<WebCore::AccessibilityObject>(object);
        return accessibilityObject && accessibilityObject->isMenuListPopup();
    }
SPECIALIZE_TYPE_TRAITS_END()
