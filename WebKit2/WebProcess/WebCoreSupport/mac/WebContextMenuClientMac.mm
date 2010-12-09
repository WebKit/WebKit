/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "WebContextMenuClient.h"

#include "WebPage.h"

#include <WebCore/Frame.h>
#include <WebCore/Page.h>
#include <wtf/text/WTFString.h>

#include "NotImplemented.h"

@interface NSApplication (AppKitSecretsIKnowAbout)
- (void)speakString:(NSString *)string;
@end

using namespace WebCore;

namespace WebKit {

void WebContextMenuClient::lookUpInDictionary(Frame*)
{
    // FIXME: <rdar://problem/8750610> - Implement
    notImplemented();
}

bool WebContextMenuClient::isSpeaking()
{
    return [NSApp isSpeaking];
}

void WebContextMenuClient::speak(const String& string)
{
    [NSApp speakString:[[(NSString*)string copy] autorelease]];
}

void WebContextMenuClient::stopSpeaking()
{
    [NSApp stopSpeaking:nil];
}

void WebContextMenuClient::searchWithSpotlight()
{
    Frame* mainFrame = m_page->corePage()->mainFrame();
    
    Frame* selectionFrame = mainFrame;
    for (; selectionFrame; selectionFrame = selectionFrame->tree()->traverseNext(mainFrame)) {
        if (selectionFrame->selection()->isRange())
            break;
    }
    if (!selectionFrame)
        selectionFrame = mainFrame;

    String selectedString = selectionFrame->displayStringModifiedByEncoding(selectionFrame->editor()->selectedText());
    
    if (selectedString.isEmpty())
        return;

    [[NSWorkspace sharedWorkspace] showSearchResultsForQueryString:(NSString *)selectedString];
}

} // namespace WebKit
