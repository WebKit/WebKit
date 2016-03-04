/*
 * Copyright (C) 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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

#ifndef RenderMedia_h
#define RenderMedia_h

#if ENABLE(VIDEO)

#include "HTMLMediaElement.h"
#include "RenderImage.h"

namespace WebCore {

class RenderMedia : public RenderImage {
public:
    RenderMedia(HTMLMediaElement&, Ref<RenderStyle>&&);
    RenderMedia(HTMLMediaElement&, Ref<RenderStyle>&&, const IntSize& intrinsicSize);
    virtual ~RenderMedia();

    HTMLMediaElement& mediaElement() const { return downcast<HTMLMediaElement>(nodeForNonAnonymous()); }

protected:
    void layout() override;

private:
    void element() const = delete;

    bool canHaveChildren() const override final { return true; }

    const char* renderName() const override { return "RenderMedia"; }
    bool isMedia() const override final { return true; }
    bool isImage() const override final { return false; }
    void paintReplaced(PaintInfo&, const LayoutPoint&) override;

    bool requiresForcedStyleRecalcPropagation() const override final { return true; }

    bool shadowControlsNeedCustomLayoutMetrics() const override { return true; }
    void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override final;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderMedia, isMedia())

#endif // ENABLE(VIDEO)

#endif // RenderMedia_h
