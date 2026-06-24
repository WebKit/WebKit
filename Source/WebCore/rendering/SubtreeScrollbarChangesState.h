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

#include <WebCore/BoxSides.h>
#include <WebCore/ScrollTypes.h>
#include <wtf/CheckedRef.h>
#include <wtf/Vector.h>

namespace WebCore {

class RenderBlock;

struct RendererScrollbarChange {
    CheckedRef<RenderBlock> renderer;
    // Relative to the subtree root.
    EnumSet<LogicalBoxAxis> sizesAffectedFromScrollbarChanges;
};

struct SubtreeScrollbarChangesState {
    CheckedRef<RenderBlock> subtreeRoot;
    EnumSet<LogicalBoxAxis> sizesAffectedForSubtreeRoot;
    Vector<RendererScrollbarChange> rendererScrollbarChanges;

    EnumSet<LogicalBoxAxis> sizesAffectedForSubtreeRootFromScrollbarChanges(const RenderBlock& rendererWithScrollbarChanges, EnumSet<ScrollbarOrientation> orientationsForChangedScrollbars) const;
    void addRendererWithScrollbarChange(RenderBlock&, EnumSet<LogicalBoxAxis> sizesAffectedFromScrollbarChanges);
    static bool isEligibleForScrollbarHandlingByAncestor(const RenderBlock&);
};

class LocalFrameViewLayoutContext;

class SubtreeScrollbarChangesStateScope {
    WTF_MAKE_NONCOPYABLE(SubtreeScrollbarChangesStateScope);
public:
    SubtreeScrollbarChangesStateScope(LocalFrameViewLayoutContext&, RenderBlock& subtreeRoot, EnumSet<LogicalBoxAxis>);
    ~SubtreeScrollbarChangesStateScope();
private:
    const CheckedRef<LocalFrameViewLayoutContext> m_layoutContext;
};

class SubtreeScrollbarChangesHandler {
public:
    SubtreeScrollbarChangesHandler(RenderBlock&);
    ~SubtreeScrollbarChangesHandler();
private:
    const CheckedRef<RenderBlock> m_rendererHandlingScrollbarChanges;
    Vector<RendererScrollbarChange> m_rendererScrollbarChangesHandledByAncestor;
};

} // namespace WebCore
