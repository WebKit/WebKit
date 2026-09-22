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

#include "config.h"
#include "RenderTreeBuilderCanvas.h"

#include "RenderBlockFlow.h"
#include "RenderBlockInlines.h"
#include "RenderHTMLCanvas.h"
#include "RenderTreeBuilderBlock.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderTreeBuilder::Canvas);

RenderTreeBuilder::Canvas::Canvas(RenderTreeBuilder& builder)
    : m_builder(builder)
{
}

void RenderTreeBuilder::Canvas::attach(RenderHTMLCanvas& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    m_builder.blockBuilder().attach(findOrCreateParentForChild(protect(parent)), WTF::move(child), beforeChild);
}

RenderPtr<RenderObject> RenderTreeBuilder::Canvas::detach(RenderHTMLCanvas& parent, RenderObject& child, RenderTreeBuilder::WillBeDestroyed willBeDestroyed)
{
    CheckedPtr innerRenderer = parent.innerRenderer();
    if (&child == innerRenderer)
        parent.setInnerRenderer(nullptr);

    return m_builder.detachFromRenderElement(parent, child, willBeDestroyed);
}

CheckedRef<RenderBlock> RenderTreeBuilder::Canvas::findOrCreateParentForChild(RenderHTMLCanvas& parent)
{
    CheckedPtr innerRenderer = parent.innerRenderer();
    if (innerRenderer)
        return innerRenderer.releaseNonNull();

    auto wrapper = m_builder.blockBuilder().createAnonymousBlockWithStyle(protect(parent.document()), protect(parent.style()));
    innerRenderer = wrapper.get();
    m_builder.attachToRenderElement(parent, WTF::move(wrapper), nullptr);
    parent.setInnerRenderer(innerRenderer);
    return innerRenderer.releaseNonNull();
}

} // namespace WebCore
