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

#pragma once

#include "AccessibilityMockObject.h"

namespace WebCore {

class AccessibilitySpinButtonPart final : public AccessibilityMockObject {
public:
    static Ref<AccessibilitySpinButtonPart> create(AXID);
    virtual ~AccessibilitySpinButtonPart() = default;

    bool isIncrementor() const final { return m_isIncrementor; }
    void setIsIncrementor(bool value) { m_isIncrementor = value; }

private:
    explicit AccessibilitySpinButtonPart(AXID);

    bool press() final;
    AccessibilityRole determineAccessibilityRole() final { return AccessibilityRole::SpinButtonPart; }
    bool isSpinButtonPart() const final { return true; }
    LayoutRect elementRect() const final;

    bool m_isIncrementor { true };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::AccessibilitySpinButtonPart) \
    static bool isType(const WebCore::AccessibilityObject& object) { return object.isSpinButtonPart(); } \
SPECIALIZE_TYPE_TRAITS_END()
