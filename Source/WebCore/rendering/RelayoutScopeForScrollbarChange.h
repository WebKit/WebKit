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

#pragma once

#include <WebCore/ScrollbarUpdateScope.h>
#include <wtf/CheckedPtr.h>

namespace WebCore {

class RenderBlock;

enum class InOverflowRelayout : bool;

// When a renderer goes through layout it may gain or lose a scrollbar which may
// affect the layout of its contents. We learn from ScrollbarUpdaterScope if this
// happens and use this scope to potentially relayout the renderer in response.
class RelayoutScopeForScrollbarChange {
public:
    RelayoutScopeForScrollbarChange(RenderBlock&, InOverflowRelayout);
    ~RelayoutScopeForScrollbarChange();

    RelayoutScopeForScrollbarChange(const RelayoutScopeForScrollbarChange&) = delete;
    RelayoutScopeForScrollbarChange& operator=(const RelayoutScopeForScrollbarChange&) = delete;
    RelayoutScopeForScrollbarChange(RelayoutScopeForScrollbarChange&&) = default;

private:
    const CheckedRef<RenderBlock> m_renderBlock;
    InOverflowRelayout m_inOverflowRelayout;

    // When |this| is being destroyed, whichs means we may have ran layout on
    // the renderer again, ScrollbarUpdateScope's destructor will know that
    // the associated renderer's geometry and content's geometry may have changed.
    std::optional<ScrollbarUpdateScope> m_scrollbarUpdateScope;
};

}
