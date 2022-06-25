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

#pragma once

#include "RenderTreeUpdater.h"

namespace WebCore {

class RenderElement;
class RenderObject;
class RenderRubyAsBlock;
class RenderRubyAsInline;
class RenderRubyBase;
class RenderRubyRun;
class RenderTreeBuilder;

class RenderTreeBuilder::Ruby {
    WTF_MAKE_FAST_ALLOCATED;
public:
    Ruby(RenderTreeBuilder&);

    void attach(RenderRubyRun& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild);
    RenderPtr<RenderObject> detach(RenderRubyAsInline& parent, RenderObject& child) WARN_UNUSED_RETURN;
    RenderPtr<RenderObject> detach(RenderRubyAsBlock& parent, RenderObject& child) WARN_UNUSED_RETURN;
    RenderPtr<RenderObject> detach(RenderRubyRun& parent, RenderObject& child) WARN_UNUSED_RETURN;

    void moveChildren(RenderRubyBase& from, RenderRubyBase& to);

    RenderElement& findOrCreateParentForChild(RenderRubyAsBlock& parent, const RenderObject& child, RenderObject*& beforeChild);
    RenderElement& findOrCreateParentForChild(RenderRubyAsInline& parent, const RenderObject& child, RenderObject*& beforeChild);

private:
    void moveInlineChildren(RenderRubyBase& from, RenderRubyBase& to, RenderObject* beforeChild);
    void moveBlockChildren(RenderRubyBase& from, RenderRubyBase& to, RenderObject* beforeChild);
    void moveChildrenInternal(RenderRubyBase& from, RenderRubyBase& to, RenderObject* beforeChild = nullptr);
    RenderRubyBase& rubyBaseSafe(RenderRubyRun&);

    RenderTreeBuilder& m_builder;
};

}

