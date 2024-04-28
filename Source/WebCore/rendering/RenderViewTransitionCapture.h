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

#include "RenderReplaced.h"

namespace WebCore {

class RenderViewTransitionCapture final : public RenderReplaced {
    WTF_MAKE_ISO_ALLOCATED(RenderViewTransitionCapture);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RenderViewTransitionCapture);
public:
    RenderViewTransitionCapture(Type, Document&, RenderStyle&&);
    virtual ~RenderViewTransitionCapture();

    void setImage(RefPtr<ImageBuffer>);
    void setSize(const LayoutSize&, const LayoutRect& overflowRect);

    void paintReplaced(PaintInfo&, const LayoutPoint& paintOffset) override;

    void layout() override;

    FloatSize scale() const { return m_scale; }

    // Rect covered by the captured contents, relative to the
    // intrinsic size.
    LayoutRect captureOverflowRect() const { return m_overflowRect; }

    // Inset of the scaled capture from the visualOverflowRect()
    LayoutPoint captureContentInset() const;

    bool canUseExistingLayers() const { return !hasNonVisibleOverflow(); }

private:
    ASCIILiteral renderName() const override { return "RenderViewTransitionCapture"_s; }
    String debugDescription() const override;

    void updateFromStyle() override;

    RefPtr<ImageBuffer> m_oldImage;
    LayoutRect m_overflowRect;
    FloatSize m_scale;
    LayoutRect m_localOverflowRect;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderViewTransitionCapture, isRenderViewTransitionCapture())
