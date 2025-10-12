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
    if (RefPtr page = m_page.get(); page && page->handleEditingKeyboardEvent(event))
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
    RefPtr page = m_page.get();
    return page ? page->hasRichlyEditableSelection() : false;
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
    if (RefPtr page = m_page.get())
        page->updateStringForFind(findString);
}

void WebEditorClient::overflowScrollPositionChanged()
{
    if (RefPtr page = m_page.get())
        page->didScrollSelection();
}

void WebEditorClient::subFrameScrollPositionChanged()
{
    if (RefPtr page = m_page.get())
        page->didScrollSelection();
}

bool WebEditorClient::shouldAllowSingleClickToChangeSelection(WebCore::Node& targetNode, const WebCore::VisibleSelection& newSelection) const
{
    RefPtr page = m_page.get();
    return page ? page->shouldAllowSingleClickToChangeSelection(targetNode, newSelection) : false;
}

bool WebEditorClient::shouldRevealCurrentSelectionAfterInsertion() const
{
    RefPtr page = m_page.get();
    return page ? page->shouldRevealCurrentSelectionAfterInsertion() : false;
}

bool WebEditorClient::shouldSuppressPasswordEcho() const
{
    RefPtr page = m_page.get();
    return page ? (page->screenIsBeingCaptured() || page->hardwareKeyboardIsAttached()) : false;
}

bool WebEditorClient::shouldRemoveDictationAlternativesAfterEditing() const
{
    RefPtr page = m_page.get();
    return page ? page->shouldRemoveDictationAlternativesAfterEditing() : false;
}

bool WebEditorClient::shouldDrawVisuallyContiguousBidiSelection() const
{
    RefPtr page = m_page.get();
    return page ? page->shouldDrawVisuallyContiguousBidiSelection() : false;
}

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY)
