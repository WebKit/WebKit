/*
 * Copyright (C) 2018-2024 Apple Inc. All rights reserved.
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
#include "RenderTreeBuilderInline.h"

#include "RenderBlockFlow.h"
#include "RenderBlockInlines.h"
#include "RenderBoxInlines.h"
#include "RenderChildIterator.h"
#include "RenderInline.h"
#include "RenderObjectInlines.h"
#include "RenderTable.h"
#include "RenderTreeBuilderMultiColumn.h"
#include "RenderTreeBuilderTable.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderTreeBuilder::Inline);

RenderTreeBuilder::Inline::Inline(RenderTreeBuilder& builder)
    : m_builder(builder)
{
}

void RenderTreeBuilder::Inline::attach(RenderInline& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    auto* beforeChildOrPlaceholder = beforeChild;
    if (auto* fragmentedFlow = parent.enclosingFragmentedFlow())
        beforeChildOrPlaceholder = m_builder.multiColumnBuilder().resolveMovedChild(*fragmentedFlow, beforeChild);

    // Make sure we don't append things after :after-generated content if we have it.
    if (!beforeChildOrPlaceholder && RenderElement::isAfterContent(dynamicDowncast<RenderElement>(parent.lastChild())))
        beforeChildOrPlaceholder = parent.lastChild();

    if (beforeChildOrPlaceholder && beforeChildOrPlaceholder->parent() != &parent)
        beforeChildOrPlaceholder = m_builder.splitAnonymousBoxesAroundChild(parent, *beforeChildOrPlaceholder);

    auto& childToAdd = *child;
    m_builder.attachToRenderElement(parent, WTF::move(child), beforeChildOrPlaceholder);
    childToAdd.setNeedsLayoutAndPreferredWidthsUpdate();
}

}
