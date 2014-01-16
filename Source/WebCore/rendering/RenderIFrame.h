/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef RenderIFrame_h
#define RenderIFrame_h

#include "RenderFrameBase.h"

namespace WebCore {

class RenderView;

class RenderIFrame final : public RenderFrameBase {
public:
    RenderIFrame(HTMLIFrameElement&, PassRef<RenderStyle>);

    HTMLIFrameElement& iframeElement() const;

    bool flattenFrame() const;
    bool isSeamless() const;

private:
    void frameOwnerElement() const WTF_DELETED_FUNCTION;

    virtual LayoutUnit minPreferredLogicalWidth() const override;
    virtual LayoutUnit maxPreferredLogicalWidth() const override;

    virtual bool shouldComputeSizeAsReplaced() const override;
    virtual bool isInlineBlockOrInlineTable() const override;

    virtual void layout() override;

    virtual bool isRenderIFrame() const override { return true; }

#if PLATFORM(IOS)
    // FIXME: Do we still need this workaround to avoid breaking layout tests?
    virtual const char* renderName() const override { return "RenderPartObject"; }
#else
    virtual const char* renderName() const override { return "RenderIFrame"; }
#endif

    virtual bool requiresLayer() const override;

    void layoutSeamlessly();

    RenderView* contentRootRenderer() const;
};

RENDER_OBJECT_TYPE_CASTS(RenderIFrame, isRenderIFrame())

} // namespace WebCore

#endif // RenderIFrame_h
