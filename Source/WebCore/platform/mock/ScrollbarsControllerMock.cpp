/*
 * Copyright (c) 2016 Igalia S.L.
 * Copyright (c) 2021 Apple Inc.
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
#include "ScrollbarsControllerMock.h"

#include "ScrollableArea.h"
#include <wtf/text/MakeString.h>

namespace WebCore {

ScrollbarsControllerMock::ScrollbarsControllerMock(ScrollableArea& scrollableArea, Function<void(const String&)>&& logger)
    : ScrollbarsController(scrollableArea)
    , m_logger(WTFMove(logger))
{
}

ScrollbarsControllerMock::~ScrollbarsControllerMock() = default;

void ScrollbarsControllerMock::didAddVerticalScrollbar(Scrollbar* scrollbar)
{
    m_verticalScrollbar = scrollbar;
    ScrollbarsController::didAddVerticalScrollbar(scrollbar);
}

void ScrollbarsControllerMock::didAddHorizontalScrollbar(Scrollbar* scrollbar)
{
    m_horizontalScrollbar = scrollbar;
    ScrollbarsController::didAddHorizontalScrollbar(scrollbar);
}

void ScrollbarsControllerMock::willRemoveVerticalScrollbar(Scrollbar* scrollbar)
{
    ScrollbarsController::willRemoveVerticalScrollbar(scrollbar);
    m_verticalScrollbar = nullptr;
}

void ScrollbarsControllerMock::willRemoveHorizontalScrollbar(Scrollbar* scrollbar)
{
    ScrollbarsController::willRemoveHorizontalScrollbar(scrollbar);
    m_horizontalScrollbar = nullptr;
}

void ScrollbarsControllerMock::mouseEnteredContentArea()
{
    m_logger("mouseEnteredContentArea"_s);
    ScrollbarsController::mouseEnteredContentArea();
}

void ScrollbarsControllerMock::mouseMovedInContentArea()
{
    m_logger("mouseMovedInContentArea"_s);
    ScrollbarsController::mouseMovedInContentArea();
}

void ScrollbarsControllerMock::mouseExitedContentArea()
{
    m_logger("mouseExitedContentArea"_s);
    ScrollbarsController::mouseExitedContentArea();
}

ASCIILiteral ScrollbarsControllerMock::scrollbarPrefix(Scrollbar* scrollbar) const
{
    return scrollbar == m_verticalScrollbar ? "Vertical"_s : scrollbar == m_horizontalScrollbar ? "Horizontal"_s : "Unknown"_s;
}

void ScrollbarsControllerMock::mouseEnteredScrollbar(Scrollbar* scrollbar) const
{
    m_logger(makeString("mouseEntered"_s, scrollbarPrefix(scrollbar), "Scrollbar"_s));
    ScrollbarsController::mouseEnteredScrollbar(scrollbar);
}

void ScrollbarsControllerMock::mouseExitedScrollbar(Scrollbar* scrollbar) const
{
    m_logger(makeString("mouseExited"_s, scrollbarPrefix(scrollbar), "Scrollbar"_s));
    ScrollbarsController::mouseExitedScrollbar(scrollbar);
}

void ScrollbarsControllerMock::mouseIsDownInScrollbar(Scrollbar* scrollbar, bool isPressed) const
{
    m_logger(makeString(isPressed ? "mouseIsDownIn"_s : "mouseIsUpIn"_s, scrollbarPrefix(scrollbar), "Scrollbar"_s));
    ScrollbarsController::mouseIsDownInScrollbar(scrollbar, isPressed);
}

} // namespace WebCore
