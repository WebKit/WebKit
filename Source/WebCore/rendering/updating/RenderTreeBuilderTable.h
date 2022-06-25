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
class RenderTable;
class RenderTableCell;
class RenderTableSection;
class RenderTableRow;
class RenderTreeBuilder;

class RenderTreeBuilder::Table {
    WTF_MAKE_FAST_ALLOCATED;
public:
    Table(RenderTreeBuilder&);

    RenderElement& findOrCreateParentForChild(RenderTableRow& parent, const RenderObject& child, RenderObject*& beforeChild);
    RenderElement& findOrCreateParentForChild(RenderTableSection& parent, const RenderObject& child, RenderObject*& beforeChild);
    RenderElement& findOrCreateParentForChild(RenderTable& parent, const RenderObject& child, RenderObject*& beforeChild);

    void attach(RenderTable& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild);
    void attach(RenderTableSection& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild);
    void attach(RenderTableRow& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild);

    bool childRequiresTable(const RenderElement& parent, const RenderObject& child);

    void collapseAndDestroyAnonymousSiblingCells(const RenderTableCell& willBeDestroyed);
    void collapseAndDestroyAnonymousSiblingRows(const RenderTableRow& willBeDestroyed);

private:
    template <typename Parent, typename Child>
    RenderPtr<RenderObject> collapseAndDetachAnonymousNextSibling(Parent*, Child* previousChild, Child* nextChild);

    RenderTreeBuilder& m_builder;
};

}
