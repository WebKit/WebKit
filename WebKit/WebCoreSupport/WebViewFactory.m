/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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

#import <WebKit/WebViewFactory.h>

#import <JavaScriptCore/Assertions.h>
#import <WebKit/WebFrameBridge.h>
#import <WebKit/WebViewInternal.h>
#import <WebKit/WebHTMLViewPrivate.h>
#import <WebKit/WebLocalizableStrings.h>
#import <WebKit/WebNSUserDefaultsExtras.h>
#import <WebKit/WebNSObjectExtras.h>
#import <WebKit/WebNSViewExtras.h>
#import <WebKit/WebPluginDatabase.h>
#import <WebKitSystemInterface.h>

@interface NSMenu (WebViewFactoryAdditions)
- (NSMenuItem *)addItemWithTitle:(NSString *)title action:(SEL)action tag:(int)tag;
@end

@implementation NSMenu (WebViewFactoryAdditions)

- (NSMenuItem *)addItemWithTitle:(NSString *)title action:(SEL)action tag:(int)tag
{
    NSMenuItem *item = [[[NSMenuItem alloc] initWithTitle:title action:action keyEquivalent:@""] autorelease];
    [item setTag:tag];
    [self addItem:item];
    return item;
}

@end

@implementation WebViewFactory

+ (void)createSharedFactory
{
    if (![self sharedFactory]) {
        [[[self alloc] init] release];
    }
    ASSERT([[self sharedFactory] isKindOfClass:self]);
}

- (NSArray *)pluginsInfo
{
    return [[WebPluginDatabase installedPlugins] plugins];
}

- (void)refreshPlugins:(BOOL)reloadPages
{
    [[WebPluginDatabase installedPlugins] refresh];
    if (reloadPages) {
        [WebView _makeAllWebViewsPerformSelector:@selector(_reloadForPluginChanges)];
    }
}

- (WebCoreFrameBridge *)bridgeForView:(NSView *)v
{
    NSView *aView = [v superview];
    
    while (aView) {
        if ([aView isKindOfClass:[WebHTMLView class]]) {
            return [(WebHTMLView *)aView _bridge];
        }
        aView = [aView superview];
    }
    return nil;
}

- (NSString *)inputElementAltText
{
    return UI_STRING_KEY("Submit", "Submit (input element)", "alt text for <input> elements with no alt, title, or value");
}

- (NSString *)resetButtonDefaultLabel
{
    return UI_STRING("Reset", "default label for Reset buttons in forms on web pages");
}

- (NSString *)searchableIndexIntroduction
{
    return UI_STRING("This is a searchable index. Enter search keywords: ",
        "text that appears at the start of nearly-obsolete web pages in the form of a 'searchable index'");
}

- (NSString *)submitButtonDefaultLabel
{
    return UI_STRING("Submit", "default label for Submit buttons in forms on web pages");
}

- (NSMenu *)cellMenuForSearchField
{
    NSMenu* cellMenu = [[[NSMenu alloc] initWithTitle:@""] autorelease];
    [cellMenu addItemWithTitle:UI_STRING("Recent Searches", "label for first item in the menu that appears when clicking on the search field image, used as embedded menu title")
                        action:NULL tag:NSSearchFieldRecentsTitleMenuItemTag];
    [cellMenu addItemWithTitle:@"" action:NULL tag:NSSearchFieldRecentsMenuItemTag];
    NSMenuItem *separator = [NSMenuItem separatorItem];
    [separator setTag:NSSearchFieldRecentsTitleMenuItemTag];
    [cellMenu addItem:separator];
    [cellMenu addItemWithTitle:UI_STRING("Clear Recent Searches", "menu item in Recent Searches menu that empties menu's contents")
                        action:NULL tag:NSSearchFieldClearRecentsMenuItemTag];
    [cellMenu addItemWithTitle:UI_STRING("No recent searches", "Label for only item in menu that appears when clicking on the search field image, when no searches have been performed")
                        action:NULL tag:NSSearchFieldNoRecentsMenuItemTag];
    return cellMenu;
}

- (NSString *)defaultLanguageCode
{
    return [NSUserDefaults _webkit_preferredLanguageCode];
}

- (BOOL)objectIsTextMarker:(id)object
{
    return object != nil && CFGetTypeID(object) == WKGetAXTextMarkerTypeID();
}

- (BOOL)objectIsTextMarkerRange:(id)object
{
    return object != nil && CFGetTypeID(object) == WKGetAXTextMarkerRangeTypeID();
}

- (WebCoreTextMarker *)textMarkerWithBytes:(const void *)bytes length:(size_t)length
{
    return WebCFAutorelease(WKCreateAXTextMarker(bytes, length));
}

- (BOOL)getBytes:(void *)bytes fromTextMarker:(WebCoreTextMarker *)textMarker length:(size_t)length
{
    return WKGetBytesFromAXTextMarker(textMarker, bytes, length);
}

- (WebCoreTextMarkerRange *)textMarkerRangeWithStart:(WebCoreTextMarker *)start end:(WebCoreTextMarker *)end
{
    ASSERT(start != nil);
    ASSERT(end != nil);
    ASSERT(CFGetTypeID(start) == WKGetAXTextMarkerTypeID());
    ASSERT(CFGetTypeID(end) == WKGetAXTextMarkerTypeID());
    return WebCFAutorelease(WKCreateAXTextMarkerRange(start, end));
}

- (WebCoreTextMarker *)startOfTextMarkerRange:(WebCoreTextMarkerRange *)range
{
    ASSERT(range != nil);
    ASSERT(CFGetTypeID(range) == WKGetAXTextMarkerRangeTypeID());
    return WebCFAutorelease(WKCopyAXTextMarkerRangeStart(range));
}

- (WebCoreTextMarker *)endOfTextMarkerRange:(WebCoreTextMarkerRange *)range
{
    ASSERT(range != nil);
    ASSERT(CFGetTypeID(range) == WKGetAXTextMarkerRangeTypeID());
    return WebCFAutorelease(WKCopyAXTextMarkerRangeEnd(range));
}

- (void)accessibilityHandleFocusChanged
{
    WKAccessibilityHandleFocusChanged();
}

- (AXUIElementRef)AXUIElementForElement:(id)element
{
    return WKCreateAXUIElementRef(element);
}

- (void)unregisterUniqueIdForUIElement:(id)element
{
    WKUnregisterUniqueIdForElement(element);
}

@end
