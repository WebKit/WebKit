/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "AccessibilityRole.h"
#include "FrameIdentifier.h"

namespace WebCore {

class LocalFrame;

class AXLocalFrame final : public AccessibilityMockObject {
public:
    static Ref<AXLocalFrame> create(AXID, AXObjectCache&);

#if ENABLE(ACCESSIBILITY_LOCAL_FRAME)
    void setLocalFrameView(LocalFrameView*);
    LocalFrameView* localFrameView() const { return m_localFrameView.get(); }
    AccessibilityObject* crossFrameChildObject() const final;
    std::optional<FrameIdentifier> frameID() const { return m_frameID; }
#endif // ENABLE(ACCESSIBILITY_LOCAL_FRAME)

private:
    virtual ~AXLocalFrame() = default;
    explicit AXLocalFrame(AXID, AXObjectCache&);

    AccessibilityRole determineAccessibilityRole() final { return AccessibilityRole::LocalFrame; }
    bool computeIsIgnored() const final;
    bool isAXLocalFrame() const final { return true; }
    LayoutRect elementRect() const final;

#if ENABLE(ACCESSIBILITY_LOCAL_FRAME)
    SingleThreadWeakPtr<LocalFrameView> m_localFrameView;
    std::optional<FrameIdentifier> m_frameID { };
#endif
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_ACCESSIBILITY(AXLocalFrame, isAXLocalFrame())
