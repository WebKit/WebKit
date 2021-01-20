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
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(ATTACHMENT_ELEMENT)

#include "AccessibilityRenderObject.h"

namespace WebCore {
    
class HTMLAttachmentElement;
class RenderAttachment;
    
class AccessibilityAttachment final : public AccessibilityRenderObject {
public:
    static Ref<AccessibilityAttachment> create(RenderAttachment*);
    HTMLAttachmentElement* attachmentElement() const;
    bool hasProgress(float* progress = nullptr) const;
    
private:
    AccessibilityRole roleValue() const override { return AccessibilityRole::Button; }
    bool isAttachmentElement() const override { return true; }

    String roleDescription() const override;
    float valueForRange() const override;
    bool computeAccessibilityIsIgnored() const override;
    void accessibilityText(Vector<AccessibilityText>&) const override;
    explicit AccessibilityAttachment(RenderAttachment*);
};
    
} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_ACCESSIBILITY(AccessibilityAttachment, isAttachmentElement())

#endif // ENABLE(ATTACHMENT_ELEMENT)
