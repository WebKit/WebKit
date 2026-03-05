/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if PLATFORM(MAC)

#include "AccessibilityUIElement.h"
#include <stdint.h>

namespace WTR {

// Client Mac implementation using AXUIElement APIs
class AccessibilityUIElementClientMac final : public AccessibilityUIElement {
public:
    static Ref<AccessibilityUIElementClientMac> create(uint64_t elementToken);
    static Ref<AccessibilityUIElementClientMac> create(const AccessibilityUIElementClientMac&);

    // Create a root element for the UI process (parent process of the current web content process)
    static Ref<AccessibilityUIElementClientMac> createForUIProcess();

    virtual ~AccessibilityUIElementClientMac();

    PlatformUIElement platformUIElement() override;

    // Attribute getters.
    bool isValid() const override;
    JSRetainPtr<JSStringRef> role() override;
    JSRetainPtr<JSStringRef> title() override;
    JSRetainPtr<JSStringRef> description() override;
    JSRetainPtr<JSStringRef> stringValue() override;
    JSRetainPtr<JSStringRef> domIdentifier() const override;
    unsigned childrenCount() override;
    RefPtr<AccessibilityUIElement> childAtIndex(unsigned) override;
    int hierarchicalLevel() const override;
    double minValue() override;
    double maxValue() override;
    double x() override;
    double y() override;
    double width() override;
    double height() override;

    // Helpers.
    JSRetainPtr<JSStringRef> getStringAttribute(const char* attributeName) const;
    double getNumberAttribute(const char* attributeName) const;
    Vector<RefPtr<AccessibilityUIElement>> getChildren() const;
    Vector<RefPtr<AccessibilityUIElement>> getChildrenInRange(unsigned location, unsigned length) const;

private:
    AccessibilityUIElementClientMac(uint64_t elementToken);
    AccessibilityUIElementClientMac(const AccessibilityUIElementClientMac&);

    uint64_t m_elementToken { 0 };
};

} // namespace WTR

#endif // PLATFORM(MAC)
