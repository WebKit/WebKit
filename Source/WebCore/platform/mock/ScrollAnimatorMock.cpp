/*
 * Copyright (c) 2016 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ScrollAnimatorMock.h"

#include "ScrollableArea.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

ScrollAnimatorMock::ScrollAnimatorMock(ScrollableArea& scrollableArea, WTF::Function<void(const String&)>&& logger)
    : ScrollAnimator(scrollableArea)
    , m_logger(WTFMove(logger))
{
}

ScrollAnimatorMock::~ScrollAnimatorMock() = default;

void ScrollAnimatorMock::didAddVerticalScrollbar(Scrollbar* scrollbar)
{
    m_verticalScrollbar = scrollbar;
    m_logger("didAddVerticalScrollbar");
    ScrollAnimator::didAddVerticalScrollbar(scrollbar);
}

void ScrollAnimatorMock::didAddHorizontalScrollbar(Scrollbar* scrollbar)
{
    m_horizontalScrollbar = scrollbar;
    m_logger("didAddHorizontalScrollbar");
    ScrollAnimator::didAddHorizontalScrollbar(scrollbar);
}

void ScrollAnimatorMock::willRemoveVerticalScrollbar(Scrollbar* scrollbar)
{
    m_logger("willRemoveVerticalScrollbar");
    ScrollAnimator::willRemoveVerticalScrollbar(scrollbar);
    m_verticalScrollbar = nullptr;
}

void ScrollAnimatorMock::willRemoveHorizontalScrollbar(Scrollbar* scrollbar)
{
    m_logger("willRemoveHorizontalScrollbar");
    ScrollAnimator::willRemoveHorizontalScrollbar(scrollbar);
    m_horizontalScrollbar = nullptr;
}

void ScrollAnimatorMock::mouseEnteredContentArea()
{
    m_logger("mouseEnteredContentArea");
    ScrollAnimator::mouseEnteredContentArea();
}

void ScrollAnimatorMock::mouseMovedInContentArea()
{
    m_logger("mouseMovedInContentArea");
    ScrollAnimator::mouseMovedInContentArea();
}

void ScrollAnimatorMock::mouseExitedContentArea()
{
    m_logger("mouseExitedContentArea");
    ScrollAnimator::mouseExitedContentArea();
}

void ScrollAnimatorMock::mouseEnteredScrollbar(Scrollbar* scrollbar) const
{
    StringBuilder message;
    message.appendLiteral("mouseEntered");
    if (scrollbar == m_verticalScrollbar)
        message.appendLiteral("Vertical");
    else if (scrollbar == m_horizontalScrollbar)
        message.appendLiteral("Horizontal");
    else
        message.appendLiteral("Unknown");
    message.appendLiteral("Scrollbar");
    m_logger(message.toString());
    ScrollAnimator::mouseEnteredScrollbar(scrollbar);
}

void ScrollAnimatorMock::mouseExitedScrollbar(Scrollbar* scrollbar) const
{
    StringBuilder message;
    message.appendLiteral("mouseExited");
    if (scrollbar == m_verticalScrollbar)
        message.appendLiteral("Vertical");
    else if (scrollbar == m_horizontalScrollbar)
        message.appendLiteral("Horizontal");
    else
        message.appendLiteral("Unknown");
    message.appendLiteral("Scrollbar");
    m_logger(message.toString());
    ScrollAnimator::mouseExitedScrollbar(scrollbar);
}

void ScrollAnimatorMock::mouseIsDownInScrollbar(Scrollbar* scrollbar, bool isPressed) const
{
    StringBuilder message;
    message.appendLiteral("mouseIs");
    if (isPressed)
        message.appendLiteral("Down");
    else
        message.appendLiteral("Up");
    message.appendLiteral("In");
    if (scrollbar == m_verticalScrollbar)
        message.appendLiteral("Vertical");
    else if (scrollbar == m_horizontalScrollbar)
        message.appendLiteral("Horizontal");
    else
        message.appendLiteral("Unknown");
    message.appendLiteral("Scrollbar");
    m_logger(message.toString());
    ScrollAnimator::mouseIsDownInScrollbar(scrollbar, isPressed);
}

} // namespace WebCore
