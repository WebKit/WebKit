/*
 * Copyright (C) 2018-2026 Apple Inc. All rights reserved.
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

#include <WebCore/LayoutElementBox.h>
#include <wtf/CheckedRef.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class RenderView;

namespace Layout {

// The only box without a renderer. It is the root of the box tree, with RenderView's box as its only child.
// RenderView itself can't be the initial containing block: it can be an inline formatting context root, which needs a parent.
class InitialContainingBlock final : public ElementBox {
    WTF_MAKE_TZONE_ALLOCATED(InitialContainingBlock);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(InitialContainingBlock);
public:
    InitialContainingBlock(RenderView&, Style::ComputedStyle&&, std::unique_ptr<Style::ComputedStyle>&& firstLineStyle = nullptr);
    ~InitialContainingBlock();

    RenderView& renderView() const;

private:
    const ElementBox& parent() const = delete;
    const Box* nextSibling() const = delete;
    const Box* nextInFlowSibling() const = delete;
    const Box* nextInFlowOrFloatingSibling() const = delete;
    const Box* previousSibling() const = delete;
    const Box* previousInFlowSibling() const = delete;
    const Box* previousInFlowOrFloatingSibling() const = delete;

    const CheckedRef<RenderView> m_renderView;
};

const InitialContainingBlock& LEGACY_NODELETE initialContainingBlock(const Box&);

}
}

SPECIALIZE_TYPE_TRAITS_LAYOUT_BOX(InitialContainingBlock, isInitialContainingBlock())

