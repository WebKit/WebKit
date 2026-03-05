/*
 * Copyright (C) 2007-2025 Apple Inc. All rights reserved.
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

#if ENABLE(VIDEO)

#include <WebCore/RenderImage.h>

namespace WebCore {

class RenderMedia : public RenderImage {
    WTF_MAKE_TZONE_ALLOCATED(RenderMedia);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RenderMedia);
public:
    RenderMedia(Type, HTMLMediaElement&, RenderStyle&&);
    virtual ~RenderMedia();

    inline HTMLMediaElement& mediaElement() const; // Defined in RenderMediaInlines.h

    bool shouldDisplayBrokenImageIcon() const final { return false; }

protected:
    void layout() override;
    void styleDidChange(Style::Difference, const RenderStyle* oldStyle) override;

    void visibleInViewportStateChanged() override { }

private:
    void element() const = delete;

    bool canHaveChildren() const final { return true; }

    ASCIILiteral renderName() const override { return "RenderMedia"_s; }
    bool isImage() const final { return false; }
    void paintReplaced(PaintInfo&, const LayoutPoint&) override;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderMedia, isRenderMedia())

#endif // ENABLE(VIDEO)
