/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#import "WebEditorClient.h"

#define DISABLE_NOT_IMPLEMENTED_WARNINGS 1
#include "NotImplemented.h"

#include "WebPage.h"
#include "WebFrame.h"
#include <WebCore/ArchiveResource.h>
#include <WebCore/DocumentFragment.h>
#include <WebCore/DOMDocumentFragmentInternal.h>
#include <WebCore/DOMDocumentInternal.h>
#include <WebCore/Frame.h>
#include <WebKit/WebResource.h>
#include <WebKit/WebNSURLExtras.h>
#if !defined(BUILDING_ON_TIGER) && !defined(BUILDING_ON_LEOPARD) && !defined(BUILDING_ON_SNOW_LEOPARD)
#import <AppKit/NSTextChecker.h>
#endif

using namespace WebCore;
using namespace WTF;

@interface NSAttributedString (WebNSAttributedStringDetails)
- (DOMDocumentFragment*)_documentFromRange:(NSRange)range document:(DOMDocument*)document documentAttributes:(NSDictionary *)dict subresources:(NSArray **)subresources;
@end

@interface WebResource (WebResourceInternal)
- (WebCore::ArchiveResource*)_coreResource;
@end

namespace WebKit {

NSString *WebEditorClient::userVisibleString(NSURL *url)
{
    return [url _web_userVisibleString];
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

DocumentFragment* WebEditorClient::documentFragmentFromAttributedString(NSAttributedString *string, Vector<RefPtr<ArchiveResource> >& resources)
{
    static NSArray *excludedElements = createExcludedElementsForAttributedStringConversion();
    
    NSDictionary *dictionary = [[NSDictionary alloc] initWithObjectsAndKeys: excludedElements,
        NSExcludedElementsDocumentAttribute, nil, @"WebResourceHandler", nil];
    
    NSArray *subResources;
    DOMDocumentFragment* fragment = [string _documentFromRange:NSMakeRange(0, [string length])
                                                      document:kit(m_page->mainFrame()->coreFrame()->document())
                                            documentAttributes:dictionary
                                                  subresources:&subResources];
    for (WebResource* resource in subResources)
        resources.append([resource _coreResource]);
    
    [dictionary release];
    return core(fragment);
}

void WebEditorClient::setInsertionPasteboard(NSPasteboard *)
{
    // This is used only by Mail, no need to implement it now.
    notImplemented();
}

#ifdef BUILDING_ON_TIGER
NSArray *WebEditorClient::pasteboardTypesForSelection(Frame*)
{
    notImplemented();
    return nil;
}
#endif

#if !defined(BUILDING_ON_TIGER) && !defined(BUILDING_ON_LEOPARD)
void WebEditorClient::uppercaseWord()
{
    notImplemented();
}

void WebEditorClient::lowercaseWord()
{
    notImplemented();
}

void WebEditorClient::capitalizeWord()
{
    notImplemented();
}

void WebEditorClient::showSubstitutionsPanel(bool)
{
    notImplemented();
}

bool WebEditorClient::substitutionsPanelIsShowing()
{
    notImplemented();
    return false;
}

void WebEditorClient::toggleSmartInsertDelete()
{
    notImplemented();
}

bool WebEditorClient::isAutomaticQuoteSubstitutionEnabled()
{
    notImplemented();
    return false;
}

void WebEditorClient::toggleAutomaticQuoteSubstitution()
{
    notImplemented();
}

bool WebEditorClient::isAutomaticLinkDetectionEnabled()
{
    notImplemented();
    return false;
}

void WebEditorClient::toggleAutomaticLinkDetection()
{
    notImplemented();
}

bool WebEditorClient::isAutomaticDashSubstitutionEnabled()
{
    notImplemented();
    return false;
}

void WebEditorClient::toggleAutomaticDashSubstitution()
{
    notImplemented();
}

bool WebEditorClient::isAutomaticTextReplacementEnabled()
{
    notImplemented();
    return false;
}

void WebEditorClient::toggleAutomaticTextReplacement()
{
    notImplemented();
}

bool WebEditorClient::isAutomaticSpellingCorrectionEnabled()
{
    notImplemented();
    return false;
}

void WebEditorClient::toggleAutomaticSpellingCorrection()
{
    notImplemented();
}

void WebEditorClient::checkTextOfParagraph(const UChar *, int length, uint64_t, Vector<TextCheckingResult>&)
{
    notImplemented();
}

#endif

#if !defined(BUILDING_ON_TIGER) && !defined(BUILDING_ON_LEOPARD) && !defined(BUILDING_ON_SNOW_LEOPARD)
void WebEditorClient::showCorrectionPanel(const WebCore::FloatRect& boundingBoxOfReplacedString, const WTF::String& replacedString, const WTF::String& replacementString, WebCore::Editor*)
{
    notImplemented();
}

void WebEditorClient::dismissCorrectionPanel(bool correctionAccepted)
{
    notImplemented();
}

bool WebEditorClient::isShowingCorrectionPanel()
{
    notImplemented();
    return false;
}
#endif

} // namespace WebKit
