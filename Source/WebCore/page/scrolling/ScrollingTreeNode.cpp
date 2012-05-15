/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#include "ScrollingTreeNode.h"

#if ENABLE(THREADED_SCROLLING)

#include "ScrollingTreeState.h"

namespace WebCore {

ScrollingTreeNode::ScrollingTreeNode(ScrollingTree* scrollingTree)
    : m_scrollingTree(scrollingTree)
    , m_shouldUpdateScrollLayerPositionOnMainThread(false)
    , m_horizontalScrollElasticity(ScrollElasticityNone)
    , m_verticalScrollElasticity(ScrollElasticityNone)
    , m_hasEnabledHorizontalScrollbar(false)
    , m_hasEnabledVerticalScrollbar(false)
    , m_horizontalScrollbarMode(ScrollbarAuto)
    , m_verticalScrollbarMode(ScrollbarAuto)
{
}

ScrollingTreeNode::~ScrollingTreeNode()
{
}

void ScrollingTreeNode::update(ScrollingTreeState* state)
{
    if (state->changedProperties() & ScrollingTreeState::ViewportRect)
        m_viewportRect = state->viewportRect();

    if (state->changedProperties() & ScrollingTreeState::ContentsSize)
        m_contentsSize = state->contentsSize();

    if (state->changedProperties() & ScrollingTreeState::ShouldUpdateScrollLayerPositionOnMainThread)
        m_shouldUpdateScrollLayerPositionOnMainThread = state->shouldUpdateScrollLayerPositionOnMainThread();

    if (state->changedProperties() & ScrollingTreeState::HorizontalScrollElasticity)
        m_horizontalScrollElasticity = state->horizontalScrollElasticity();

    if (state->changedProperties() & ScrollingTreeState::VerticalScrollElasticity)
        m_verticalScrollElasticity = state->verticalScrollElasticity();

    if (state->changedProperties() & ScrollingTreeState::HasEnabledHorizontalScrollbar)
        m_hasEnabledHorizontalScrollbar = state->hasEnabledHorizontalScrollbar();

    if (state->changedProperties() & ScrollingTreeState::HasEnabledVerticalScrollbar)
        m_hasEnabledVerticalScrollbar = state->hasEnabledVerticalScrollbar();

    if (state->changedProperties() & ScrollingTreeState::HorizontalScrollbarMode)
        m_horizontalScrollbarMode = state->horizontalScrollbarMode();

    if (state->changedProperties() & ScrollingTreeState::VerticalScrollbarMode)
        m_verticalScrollbarMode = state->verticalScrollbarMode();

    if (state->changedProperties() & ScrollingTreeState::ScrollOrigin)
        m_scrollOrigin = state->scrollOrigin();
}

} // namespace WebCore

#endif // ENABLE(THREADED_SCROLLING)
