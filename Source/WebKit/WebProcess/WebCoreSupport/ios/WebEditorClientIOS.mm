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

#import "config.h"
#import "WebEditorClient.h"

#if PLATFORM(IOS_FAMILY)

#import "WebPage.h"
#import <WebCore/DocumentFragment.h>
#import <WebCore/KeyboardEvent.h>
#import <WebCore/NotImplemented.h>

namespace WebKit {
using namespace WebCore;
    
void WebEditorClient::handleKeyboardEvent(KeyboardEvent& event)
{
    if (m_page->handleEditingKeyboardEvent(event))
        event.setDefaultHandled();
}

void WebEditorClient::handleInputMethodKeydown(KeyboardEvent& event)
{
    if (event.handledByInputMethod())
        event.setDefaultHandled();
}

void WebEditorClient::setInsertionPasteboard(const String&)
{
    // This is used only by Mail, no need to implement it now.
    notImplemented();
}

void WebEditorClient::startDelayingAndCoalescingContentChangeNotifications()
{
    notImplemented();
}

void WebEditorClient::stopDelayingAndCoalescingContentChangeNotifications()
{
    notImplemented();
}

bool WebEditorClient::hasRichlyEditableSelection()
{
    return m_page->hasRichlyEditableSelection();
}

int WebEditorClient::getPasteboardItemsCount()
{
    notImplemented();
    return 0;
}

RefPtr<WebCore::DocumentFragment> WebEditorClient::documentFragmentFromDelegate(int)
{
    notImplemented();
    return nullptr;
}

bool WebEditorClient::performsTwoStepPaste(WebCore::DocumentFragment*)
{
    notImplemented();
    return false;
}

void WebEditorClient::updateStringForFind(const String& findString)
{
    m_page->updateStringForFind(findString);
}

void WebEditorClient::overflowScrollPositionChanged()
{
    m_page->didChangeOverflowScrollPosition();
}

void WebEditorClient::subFrameScrollPositionChanged()
{
    m_page->didChangeOverflowScrollPosition();
}

bool WebEditorClient::shouldAllowSingleClickToChangeSelection(WebCore::Node& targetNode, const WebCore::VisibleSelection& newSelection) const
{
    // The text selection assistant will handle selection in the case where we are already editing the node
    auto* editableRoot = newSelection.rootEditableElement();
    return !editableRoot || editableRoot != targetNode.rootEditableElement() || !m_page->isShowingInputViewForFocusedElement();
}

bool WebEditorClient::shouldRevealCurrentSelectionAfterInsertion() const
{
    return m_page->shouldRevealCurrentSelectionAfterInsertion();
}

bool WebEditorClient::shouldSuppressPasswordEcho() const
{
    return m_page->screenIsBeingCaptured() || m_page->hardwareKeyboardIsAttached();
}

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY)
