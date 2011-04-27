/*
 * Copyright (C) 2005, 2009 Apple Computer, Inc.  All rights reserved.
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

#import <WebKit/WebFrameInternal.h>
#import <WebKit/WebViewInternal.h>
#import <WebKit/WebHTMLViewInternal.h>
#import <WebKit/WebNSUserDefaultsExtras.h>
#import <WebKit/WebNSObjectExtras.h>
#import <WebKit/WebNSViewExtras.h>
#import <WebKitSystemInterface.h>
#import <wtf/Assertions.h>

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

- (CGRect)accessibilityConvertScreenRect:(CGRect)bounds
{
    NSArray *screens = [NSScreen screens];
    if ([screens count]) {
        CGFloat screenHeight = NSHeight([(NSScreen *)[screens objectAtIndex:0] frame]);
        bounds.origin.y = (screenHeight - (bounds.origin.y + bounds.size.height));
    } else
        bounds = CGRectZero;    

    return bounds;
}

- (void)unregisterUniqueIdForUIElement:(id)element
{
    WKUnregisterUniqueIdForElement(element);
}

@end
