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

#if PLATFORM(IOS)

#import "WebPage.h"
#import <WebCore/KeyboardEvent.h>
#import <WebCore/NotImplemented.h>

using namespace WebCore;

namespace WebKit {
    
void WebEditorClient::handleKeyboardEvent(KeyboardEvent* event)
{
    if (m_page->handleEditingKeyboardEvent(event))
        event->setDefaultHandled();
}

void WebEditorClient::handleInputMethodKeydown(KeyboardEvent* event)
{
    notImplemented();
}

NSString *WebEditorClient::userVisibleString(NSURL *)
{
    notImplemented();
    return nil;
}

NSURL *WebEditorClient::canonicalizeURL(NSURL *)
{
    notImplemented();
    return nil;
}

NSURL *WebEditorClient::canonicalizeURLString(NSString *)
{
    notImplemented();
    return nil;
}

DocumentFragment* WebEditorClient::documentFragmentFromAttributedString(NSAttributedString *, Vector<RefPtr<ArchiveResource> >&)
{
    notImplemented();
    return 0;
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

void WebEditorClient::writeDataToPasteboard(NSDictionary*)
{
    notImplemented();
}

NSArray* WebEditorClient::supportedPasteboardTypesForCurrentSelection()
{
    notImplemented();
    return 0;
}

NSArray* WebEditorClient::readDataFromPasteboard(NSString*, int)
{
    notImplemented();
    return 0;
}

bool WebEditorClient::hasRichlyEditableSelection()
{
    notImplemented();
    return false;
}

int WebEditorClient::getPasteboardItemsCount()
{
    notImplemented();
    return 0;
}

WebCore::DocumentFragment* WebEditorClient::documentFragmentFromDelegate(int)
{
    notImplemented();
    return 0;
}

bool WebEditorClient::performsTwoStepPaste(WebCore::DocumentFragment*)
{
    notImplemented();
    return false;
}

int WebEditorClient::pasteboardChangeCount()
{
    notImplemented();
    return 0;
}

} // namespace WebKit

#endif // PLATFORM(IOS)
