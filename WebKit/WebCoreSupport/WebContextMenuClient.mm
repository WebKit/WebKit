/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
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

#import "WebContextMenuClient.h"

#import "WebElementDictionary.h"
#import "WebFrame.h"
#import "WebFrameInternal.h"
#import "WebHTMLView.h"
#import "WebHTMLViewInternal.h"
#import "WebNSPasteboardExtras.h"
#import "WebUIDelegate.h"
#import "WebUIDelegatePrivate.h"
#import "WebView.h"
#import "WebViewFactory.h"
#import "WebViewInternal.h"
#import <WebCore/ContextMenu.h>
#import <WebCore/KURL.h>

using namespace WebCore;

@interface NSApplication (AppKitSecretsIKnowAbout)
- (void)speakString:(NSString *)string;
@end

WebContextMenuClient::WebContextMenuClient(WebView *webView) 
    : m_webView(webView)
{
}

void WebContextMenuClient::contextMenuDestroyed()
{
    delete this;
}

static NSMutableArray *fixMenusForOldClients(NSMutableArray *defaultMenuItems)
{
    // Here we change all SPI tags listed in WebUIDelegatePrivate.h to WebMenuItemTagOther
    // to match our old WebKit context menu behavior.
    
    unsigned defaultItemsCount = [defaultMenuItems count];
    for (unsigned i = 0; i < defaultItemsCount; ++i) {
        NSMenuItem *item = [defaultMenuItems objectAtIndex:i];
        if ([item tag] >= WEBMENUITEMTAG_SPI_START)
            [item setTag:WebMenuItemTagOther];
    }
    
    return defaultMenuItems;
}

static NSMutableArray *fixMenusFromOldClients(NSMutableArray *newMenuItems, NSMutableArray *defaultMenuItems)
{    
    // The WebMenuItemTag enum has changed since Tiger, so clients built against Tiger WebKit might reference incorrect values for tags.
    // Here we fix up Tiger Mail and Tiger Safari.
    
    if ([newMenuItems count] && [[newMenuItems objectAtIndex:0] isSeparatorItem]) {
        if ([[newMenuItems objectAtIndex:1] isSeparatorItem]) {
            // Versions of Mail compiled with older WebKits will end up without three context menu items, 
            // though with the separators between them. Here we check for that problem and reinsert the 
            // three missing items. This shouldn't affect any clients other than Mail since the tags for 
            // the three items were not public API. We can remove this code when we no longer support 
            // previously-built versions of Mail on Tiger. See 4498606 for more details.
            ASSERT([[newMenuItems objectAtIndex:1] isSeparatorItem]);
            ASSERT([[defaultMenuItems objectAtIndex:1] tag] == WebMenuItemTagSearchWeb);
            ASSERT([[defaultMenuItems objectAtIndex:2] isSeparatorItem]);
            ASSERT([[defaultMenuItems objectAtIndex:3] tag] == WebMenuItemTagLookUpInDictionary);
            ASSERT([[defaultMenuItems objectAtIndex:4] isSeparatorItem]);
            [newMenuItems insertObject:[defaultMenuItems objectAtIndex:0] atIndex:0];
            [newMenuItems insertObject:[defaultMenuItems objectAtIndex:1] atIndex:1];
            [newMenuItems insertObject:[defaultMenuItems objectAtIndex:3] atIndex:3];
            
            return [newMenuItems autorelease];
        }
        
        unsigned defaultItemsCount = [defaultMenuItems count];
        for (unsigned i = 0; i < defaultItemsCount; ++i)
            if ([[defaultMenuItems objectAtIndex:i] tag] == WebMenuItemTagSpellingMenu) {
                // Tiger Safari doesn't realize we're popping up an editing menu.
                // http://bugs.webkit.org/show_bug.cgi?id=12134 has details.
                [newMenuItems release];
                return defaultMenuItems;
            }
    }
    
    unsigned defaultItemsCount = [defaultMenuItems count];
    for (unsigned i = 0; i < defaultItemsCount; ++i) {
        NSMenuItem *item = [defaultMenuItems objectAtIndex:i];
        if ([item tag] != WebMenuItemTagOther)
            continue;
        
        NSString *title = [item title];
        if (title == [[WebViewFactory sharedFactory] contextMenuItemTagOpenLink])
            [item setTag:WebMenuItemTagOpenLink];
        else if (title == [[WebViewFactory sharedFactory] contextMenuItemTagIgnoreGrammar])
            [item setTag:WebMenuItemTagIgnoreGrammar];
        else if (title == [[WebViewFactory sharedFactory] contextMenuItemTagSpellingMenu])
            [item setTag:WebMenuItemTagSpellingMenu];
        else if (title == [[WebViewFactory sharedFactory] contextMenuItemTagShowSpellingPanel:true]
            || title == [[WebViewFactory sharedFactory] contextMenuItemTagShowSpellingPanel:false])
            [item setTag:WebMenuItemTagShowSpellingPanel];
        else if (title == [[WebViewFactory sharedFactory] contextMenuItemTagCheckSpelling])
            [item setTag:WebMenuItemTagCheckSpelling];
        else if (title == [[WebViewFactory sharedFactory] contextMenuItemTagCheckSpellingWhileTyping])
            [item setTag:WebMenuItemTagCheckSpellingWhileTyping];
        else if (title == [[WebViewFactory sharedFactory] contextMenuItemTagCheckGrammarWithSpelling])
            [item setTag:WebMenuItemTagCheckGrammarWithSpelling];
        else if (title == [[WebViewFactory sharedFactory] contextMenuItemTagFontMenu])
            [item setTag:WebMenuItemTagFontMenu];
        else if (title == [[WebViewFactory sharedFactory] contextMenuItemTagShowFonts])
            [item setTag:WebMenuItemTagShowFonts];
        else if (title == [[WebViewFactory sharedFactory] contextMenuItemTagBold])
            [item setTag:WebMenuItemTagBold];
        else if (title == [[WebViewFactory sharedFactory] contextMenuItemTagItalic])
            [item setTag:WebMenuItemTagItalic];
        else if (title == [[WebViewFactory sharedFactory] contextMenuItemTagUnderline])
            [item setTag:WebMenuItemTagUnderline];
        else if (title == [[WebViewFactory sharedFactory] contextMenuItemTagOutline])
            [item setTag:WebMenuItemTagOutline];
        else if (title == [[WebViewFactory sharedFactory] contextMenuItemTagStyles])
            [item setTag:WebMenuItemTagStyles];
        else if (title == [[WebViewFactory sharedFactory] contextMenuItemTagShowColors])
            [item setTag:WebMenuItemTagShowColors];
        else if (title == [[WebViewFactory sharedFactory] contextMenuItemTagSpeechMenu])
            [item setTag:WebMenuItemTagSpeechMenu];
        else if (title == [[WebViewFactory sharedFactory] contextMenuItemTagStartSpeaking])
            [item setTag:WebMenuItemTagStartSpeaking];
        else if (title == [[WebViewFactory sharedFactory] contextMenuItemTagStopSpeaking])
            [item setTag:WebMenuItemTagStopSpeaking];
        else if (title == [[WebViewFactory sharedFactory] contextMenuItemTagWritingDirectionMenu])
            [item setTag:WebMenuItemTagWritingDirectionMenu];
        else if (title == [[WebViewFactory sharedFactory] contextMenuItemTagDefaultDirection])
            [item setTag:WebMenuItemTagDefaultDirection];
        else if (title == [[WebViewFactory sharedFactory] contextMenuItemTagLeftToRight])
            [item setTag:WebMenuItemTagLeftToRight];
        else if (title == [[WebViewFactory sharedFactory] contextMenuItemTagRightToLeft])
            [item setTag:WebMenuItemTagRightToLeft];
    }

    return [newMenuItems autorelease];
}

NSMutableArray* WebContextMenuClient::getCustomMenuFromDefaultItems(ContextMenu* defaultMenu)
{
    id delegate = [m_webView UIDelegate];
    if (![delegate respondsToSelector:@selector(webView:contextMenuItemsForElement:defaultMenuItems:)])
        return defaultMenu->platformDescription();
    
    NSDictionary *element = [[[WebElementDictionary alloc] initWithHitTestResult:defaultMenu->hitTestResult()] autorelease];
    NSMutableArray *defaultMenuItems = fixMenusForOldClients(defaultMenu->platformDescription());
    NSMutableArray *newMenuItems = [[delegate webView:m_webView contextMenuItemsForElement:element defaultMenuItems:defaultMenuItems] mutableCopy];

    return fixMenusFromOldClients(newMenuItems, defaultMenuItems);
}

void WebContextMenuClient::contextMenuItemSelected(ContextMenuItem* item, const ContextMenu* parentMenu)
{
    id delegate = [m_webView UIDelegate];
    if ([delegate respondsToSelector:@selector(webView:contextMenuItemSelected:forElement:)]) {
        NSDictionary *element = [[WebElementDictionary alloc] initWithHitTestResult:parentMenu->hitTestResult()];
        [delegate webView:m_webView contextMenuItemSelected:item->releasePlatformDescription() forElement:element];
        [element release];
    }
}

void WebContextMenuClient::downloadURL(const KURL& url)
{
    [m_webView _downloadURL:url.getNSURL()];
}

void WebContextMenuClient::copyImageToClipboard(const HitTestResult& hitTestResult)
{
    NSDictionary *element = [[[WebElementDictionary alloc] initWithHitTestResult:hitTestResult] autorelease];
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    NSArray *types = [NSPasteboard _web_writableTypesForImageIncludingArchive:(hitTestResult.innerNonSharedNode() != 0)];
    [pasteboard declareTypes:types owner:m_webView];
    [m_webView _writeImageForElement:element withPasteboardTypes:types toPasteboard:pasteboard];
}

void WebContextMenuClient::searchWithSpotlight()
{
    [m_webView _searchWithSpotlightFromMenu:nil];
}

void WebContextMenuClient::searchWithGoogle(const Frame*)
{
    [m_webView _searchWithGoogleFromMenu:nil];
}

void WebContextMenuClient::lookUpInDictionary(Frame* frame)
{
    WebHTMLView* htmlView = (WebHTMLView*)[[kit(frame) frameView] documentView];
    if(![htmlView isKindOfClass:[WebHTMLView class]])
        return;
    [htmlView _lookUpInDictionaryFromMenu:nil];
}

void WebContextMenuClient::speak(const String& string)
{
    [NSApp speakString:[[(NSString*)string copy] autorelease]];
}

void WebContextMenuClient::stopSpeaking()
{
    [NSApp stopSpeaking];
}
