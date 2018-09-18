/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "RenderTreeBuilderFormControls.h"

#include "RenderButton.h"
#include "RenderMenuList.h"
#include "RenderTreeBuilderBlock.h"

namespace WebCore {

RenderTreeBuilder::FormControls::FormControls(RenderTreeBuilder& builder)
    : m_builder(builder)
{
}

void RenderTreeBuilder::FormControls::attach(RenderButton& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    m_builder.blockBuilder().attach(findOrCreateParentForChild(parent), WTFMove(child), beforeChild);
}

void RenderTreeBuilder::FormControls::attach(RenderMenuList& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    auto& newChild = *child.get();
    m_builder.blockBuilder().attach(findOrCreateParentForChild(parent), WTFMove(child), beforeChild);
    parent.didAttachChild(newChild, beforeChild);
}

RenderPtr<RenderObject> RenderTreeBuilder::FormControls::detach(RenderMenuList& parent, RenderObject& child)
{
    auto* innerRenderer = parent.innerRenderer();
    if (!innerRenderer || &child == innerRenderer)
        return m_builder.blockBuilder().detach(parent, child);
    return m_builder.detach(*innerRenderer, child);
}

RenderPtr<RenderObject> RenderTreeBuilder::FormControls::detach(RenderButton& parent, RenderObject& child)
{
    auto* innerRenderer = parent.innerRenderer();
    if (!innerRenderer || &child == innerRenderer || child.parent() == &parent) {
        ASSERT(&child == innerRenderer || !innerRenderer);
        return m_builder.blockBuilder().detach(parent, child);
    }
    return m_builder.detach(*innerRenderer, child);
}


RenderBlock& RenderTreeBuilder::FormControls::findOrCreateParentForChild(RenderButton& parent)
{
    auto* innerRenderer = parent.innerRenderer();
    if (innerRenderer)
        return *innerRenderer;

    auto wrapper = parent.createAnonymousBlock(parent.style().display());
    innerRenderer = wrapper.get();
    m_builder.blockBuilder().attach(parent, WTFMove(wrapper), nullptr);
    parent.setInnerRenderer(*innerRenderer);
    return *innerRenderer;
}

RenderBlock& RenderTreeBuilder::FormControls::findOrCreateParentForChild(RenderMenuList& parent)
{
    auto* innerRenderer = parent.innerRenderer();
    if (innerRenderer)
        return *innerRenderer;

    auto wrapper = parent.createAnonymousBlock();
    innerRenderer = wrapper.get();
    m_builder.blockBuilder().attach(parent, WTFMove(wrapper), nullptr);
    parent.setInnerRenderer(*innerRenderer);
    return *innerRenderer;
}

}
