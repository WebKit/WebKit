/*
 * Copyright (C) 2006, 2010, 2011 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "WebEditorClient.h"

#if PLATFORM(MAC)

#import "WebCoreArgumentCoders.h"
#import "WebPage.h"
#import "WebFrame.h"
#import "WebPageProxyMessages.h"
#import "WebProcess.h"
#import <WebCore/ArchiveResource.h>
#import <WebCore/DocumentFragment.h>
#import <WebCore/DOMDocumentFragmentInternal.h>
#import <WebCore/DOMDocumentInternal.h>
#import <WebCore/FocusController.h>
#import <WebCore/Frame.h>
#import <WebCore/KeyboardEvent.h>
#import <WebCore/NotImplemented.h>
#import <WebCore/Page.h>
#import <WebCore/WebCoreNSURLExtras.h>

using namespace WebCore;

@interface NSAttributedString (WebNSAttributedStringDetails)
- (DOMDocumentFragment*)_documentFromRange:(NSRange)range document:(DOMDocument*)document documentAttributes:(NSDictionary *)dict subresources:(NSArray **)subresources;
@end

@interface NSObject (WebResourceInternal)
- (WebCore::ArchiveResource*)_coreResource;
@end

namespace WebKit {
    
void WebEditorClient::handleKeyboardEvent(KeyboardEvent* event)
{
    if (m_page->handleEditingKeyboardEvent(event, false))
        event->setDefaultHandled();
}

void WebEditorClient::handleInputMethodKeydown(KeyboardEvent* event)
{
    if (m_page->handleEditingKeyboardEvent(event, true))
        event->setDefaultHandled();
}
    
NSString *WebEditorClient::userVisibleString(NSURL *url)
{
    return WebCore::userVisibleString(url);
}

NSURL *WebEditorClient::canonicalizeURL(NSURL *url)
{
    return URLByCanonicalizingURL(url);
}

NSURL *WebEditorClient::canonicalizeURLString(NSString *URLString)
{
    NSURL *URL = nil;
    if (looksLikeAbsoluteURL(URLString))
        URL = URLByCanonicalizingURL(URLWithUserTypedString(URLString, nil));
    return URL;
}
    
static NSArray *createExcludedElementsForAttributedStringConversion()
{
    NSArray *elements = [[NSArray alloc] initWithObjects: 
        // Omit style since we want style to be inline so the fragment can be easily inserted.
        @"style", 
        // Omit xml so the result is not XHTML.
        @"xml", 
        // Omit tags that will get stripped when converted to a fragment anyway.
        @"doctype", @"html", @"head", @"body", 
        // Omit deprecated tags.
        @"applet", @"basefont", @"center", @"dir", @"font", @"isindex", @"menu", @"s", @"strike", @"u", 
        // Omit object so no file attachments are part of the fragment.
        @"object", nil];
    CFRetain(elements);
    return elements;
}

DocumentFragment* WebEditorClient::documentFragmentFromAttributedString(NSAttributedString *string, Vector<RefPtr<ArchiveResource>>& resources)
{
    static NSArray *excludedElements = createExcludedElementsForAttributedStringConversion();
    
    NSDictionary *dictionary = [NSDictionary dictionaryWithObject:excludedElements forKey:NSExcludedElementsDocumentAttribute];
    
    NSArray *subResources;
    Document* document = m_page->mainFrame()->document();

    // FIXME: Isntead of calling this WebKit1 method, the code should be factored out and moved into WebCore.
    DOMDocumentFragment* fragment = [string _documentFromRange:NSMakeRange(0, [string length])
                                                      document:kit(document)
                                            documentAttributes:dictionary
                                                  subresources:&subResources];
    for (id resource in subResources)
        resources.append([resource _coreResource]);

    return core(fragment);
}

void WebEditorClient::setInsertionPasteboard(const String&)
{
    // This is used only by Mail, no need to implement it now.
    notImplemented();
}


static void changeWordCase(WebPage* page, SEL selector)
{
    Frame& frame = page->corePage()->focusController().focusedOrMainFrame();
    if (!frame.editor().canEdit())
        return;

    frame.editor().command("selectWord").execute();

    NSString *selectedString = frame.displayStringModifiedByEncoding(frame.editor().selectedText());
    page->replaceSelectionWithText(&frame, [selectedString performSelector:selector]);
}

#if USE(APPKIT)
void WebEditorClient::uppercaseWord()
{
    changeWordCase(m_page, @selector(uppercaseString));
}

void WebEditorClient::lowercaseWord()
{
    changeWordCase(m_page, @selector(lowercaseString));
}

void WebEditorClient::capitalizeWord()
{
    changeWordCase(m_page, @selector(capitalizedString));
}
#endif

#if USE(AUTOMATIC_TEXT_REPLACEMENT)
void WebEditorClient::showSubstitutionsPanel(bool)
{
    notImplemented();
}

bool WebEditorClient::substitutionsPanelIsShowing()
{
    bool isShowing;
    m_page->sendSync(Messages::WebPageProxy::SubstitutionsPanelIsShowing(), Messages::WebPageProxy::SubstitutionsPanelIsShowing::Reply(isShowing));
    return isShowing;
}

void WebEditorClient::toggleSmartInsertDelete()
{
    // This is handled in the UI process.
    ASSERT_NOT_REACHED();
}

bool WebEditorClient::isAutomaticQuoteSubstitutionEnabled()
{
    return WebProcess::shared().textCheckerState().isAutomaticQuoteSubstitutionEnabled;
}

void WebEditorClient::toggleAutomaticQuoteSubstitution()
{
    // This is handled in the UI process.
    ASSERT_NOT_REACHED();
}

bool WebEditorClient::isAutomaticLinkDetectionEnabled()
{
    return WebProcess::shared().textCheckerState().isAutomaticLinkDetectionEnabled;
}

void WebEditorClient::toggleAutomaticLinkDetection()
{
    // This is handled in the UI process.
    ASSERT_NOT_REACHED();
}

bool WebEditorClient::isAutomaticDashSubstitutionEnabled()
{
    return WebProcess::shared().textCheckerState().isAutomaticDashSubstitutionEnabled;
}

void WebEditorClient::toggleAutomaticDashSubstitution()
{
    // This is handled in the UI process.
    ASSERT_NOT_REACHED();
}

bool WebEditorClient::isAutomaticTextReplacementEnabled()
{
    return WebProcess::shared().textCheckerState().isAutomaticTextReplacementEnabled;
}

void WebEditorClient::toggleAutomaticTextReplacement()
{
    // This is handled in the UI process.
    ASSERT_NOT_REACHED();
}

bool WebEditorClient::isAutomaticSpellingCorrectionEnabled()
{
    return WebProcess::shared().textCheckerState().isAutomaticSpellingCorrectionEnabled;
}

void WebEditorClient::toggleAutomaticSpellingCorrection()
{
    notImplemented();
}
#endif // USE(AUTOMATIC_TEXT_REPLACEMENT)

} // namespace WebKit

#endif // PLATFORM(MAC)
