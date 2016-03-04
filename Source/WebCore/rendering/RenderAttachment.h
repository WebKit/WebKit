/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef RenderAttachment_h
#define RenderAttachment_h

#if ENABLE(ATTACHMENT_ELEMENT)

#include "RenderReplaced.h"

namespace WebCore {

class HTMLAttachmentElement;

class RenderAttachment final : public RenderReplaced {
public:
    RenderAttachment(HTMLAttachmentElement&, Ref<RenderStyle>&&);

    HTMLAttachmentElement& attachmentElement() const;

    void invalidate();

private:
    void element() const = delete;
    bool isAttachment() const override { return true; }
    const char* renderName() const override { return "RenderAttachment"; }

    bool shouldDrawSelectionTint() const override { return false; }

    void layout() override;

    int baselinePosition(FontBaseline, bool, LineDirectionMode, LinePositionMode) const override;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderAttachment, isAttachment())

#endif // ENABLE(ATTACHMENT_ELEMENT)
#endif // RenderAttachment_h
