/*
 * Copyright (C) 2017 Igalia S.L. All rights reserved.
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

#import "config.h"
#import "ScrollingTreeScrollingNodeDelegate.h"

#if PLATFORM(IOS_FAMILY) && ENABLE(ASYNC_SCROLLING)

#import "ScrollingTreeScrollingNode.h"

namespace WebCore {

ScrollingTreeScrollingNodeDelegate::ScrollingTreeScrollingNodeDelegate(ScrollingTreeScrollingNode& scrollingNode)
    : m_scrollingNode(scrollingNode)
{
}

ScrollingTreeScrollingNodeDelegate::~ScrollingTreeScrollingNodeDelegate() = default;

ScrollingTree& ScrollingTreeScrollingNodeDelegate::scrollingTree() const
{
    return m_scrollingNode.scrollingTree();
}

FloatPoint ScrollingTreeScrollingNodeDelegate::lastCommittedScrollPosition() const
{
    return m_scrollingNode.lastCommittedScrollPosition();
}

const FloatSize& ScrollingTreeScrollingNodeDelegate::totalContentsSize()
{
    return m_scrollingNode.totalContentsSize();
}

const FloatSize& ScrollingTreeScrollingNodeDelegate::reachableContentsSize()
{
    return m_scrollingNode.reachableContentsSize();
}

const IntPoint& ScrollingTreeScrollingNodeDelegate::scrollOrigin() const
{
    return m_scrollingNode.scrollOrigin();
}

} // namespace WebCore

#endif // PLATFORM(IOS_FAMILY) && ENABLE(ASYNC_SCROLLING)
