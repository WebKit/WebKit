/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "RenderTreeBuilder.h"

namespace WebCore {

class RenderTreeBuilder::Inline {
    WTF_MAKE_FAST_ALLOCATED;
public:
    Inline(RenderTreeBuilder&);

    void attach(RenderInline& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild);
    void attachIgnoringContinuation(RenderInline& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild);

    // Make this private once all the mutation code is in RenderTreeBuilder.
    void childBecameNonInline(RenderInline& parent, RenderElement& child);

private:
    void insertChildToContinuation(RenderInline& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild);
    void splitInlines(RenderInline& parent, RenderBlock* fromBlock, RenderBlock* toBlock, RenderBlock* middleBlock, RenderObject* beforeChild, RenderBoxModelObject* oldCont);
    bool newChildIsInline(const RenderInline& parent, const RenderObject& child);
    void splitFlow(RenderInline& parent, RenderObject* beforeChild, RenderPtr<RenderBlock> newBlockBox, RenderPtr<RenderObject> child, RenderBoxModelObject* oldCont);

    RenderTreeBuilder& m_builder;
};

}
