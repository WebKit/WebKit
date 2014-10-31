/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#import "WebActionMenuController.h"

#import "DOMElementInternal.h"
#import "DOMNodeInternal.h"
#import "WebUIDelegatePrivate.h"
#import "WebViewInternal.h"
#import <WebCore/Element.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameView.h>
#import <WebCore/NSViewSPI.h>
#import <WebCore/SoftLinking.h>
#import <objc/objc-class.h>
#import <objc/objc.h>

SOFT_LINK_FRAMEWORK_IN_UMBRELLA(Quartz, QuickLookUI)
SOFT_LINK_CLASS(QuickLookUI, QLPreviewBubble)

@class QLPreviewBubble;
@interface NSObject (WKQLPreviewBubbleDetails)
@property (copy) NSArray * controls;
@property NSSize maximumSize;
@property NSRectEdge preferredEdge;
@property (retain) IBOutlet NSWindow* parentWindow;
- (void)showPreviewItem:(id)previewItem itemFrame:(NSRect)frame;
- (void)setAutomaticallyCloseWithMask:(NSEventMask)autocloseMask filterMask:(NSEventMask)filterMask block:(void (^)(void))block;
@end

using namespace WebCore;

@implementation WebActionMenuController

- (id)initWithWebView:(WebView *)webView
{
    if (!(self = [super init]))
        return nil;

    _webView = webView;
    _type = WebActionMenuNone;

    return self;
}

- (void)webViewClosed
{
    _webView = nil;
}

- (void)prepareForMenu:(NSMenu *)menu withEvent:(NSEvent *)event
{
    if (!_webView)
        return;

    NSMenu *actionMenu = _webView.actionMenu;
    if (menu != actionMenu)
        return;

    [actionMenu removeAllItems];

    NSDictionary *hitTestResult = [_webView elementAtPoint:[_webView convertPoint:event.locationInWindow fromView:nil]];

    NSArray *menuItems = [self _defaultMenuItemsForHitTestResult:hitTestResult];

    // Allow clients to customize the menu items.
    if ([[_webView UIDelegate] respondsToSelector:@selector(_webView:actionMenuItemsForHitTestResult:withType:defaultActionMenuItems:)])
        menuItems = [[_webView UIDelegate] _webView:_webView actionMenuItemsForHitTestResult:hitTestResult withType:_type defaultActionMenuItems:menuItems];

    for (NSMenuItem *item in menuItems)
        [actionMenu addItem:item];
}

- (void)_openURLFromActionMenu:(id)sender
{
    if (!_webView)
        return;

    NSURL *url = [sender representedObject];
    if (!url)
        return;

    ASSERT([url isKindOfClass:[NSURL class]]);

    [[NSWorkspace sharedWorkspace] openURL:url];
}

- (void)_addToReadingListFromActionMenu:(id)sender
{
    if (!_webView)
        return;

    NSURL *url = [sender representedObject];
    if (!url)
        return;

    ASSERT([url isKindOfClass:[NSURL class]]);

    NSSharingService *service = [NSSharingService sharingServiceNamed:NSSharingServiceNameAddToSafariReadingList];
    [service performWithItems:@[ url ]];
}

- (NSRect)_elementBoundingBoxFromDOMElement:(DOMElement *)domElement
{
    if (!domElement)
        return NSZeroRect;

    Node* node = core(domElement);
    Frame* frame = node->document().frame();
    if (!frame)
        return NSZeroRect;

    FrameView* view = frame->view();
    if (!view)
        return NSZeroRect;

    return view->contentsToWindow(node->pixelSnappedBoundingBox());
}

- (void)_quickLookURLFromActionMenu:(id)sender
{
    if (!_webView)
        return;

    NSDictionary *hitTestResult = [sender representedObject];
    if (!hitTestResult)
        return;

    ASSERT([hitTestResult isKindOfClass:[NSDictionary class]]);

    NSURL *url = [hitTestResult objectForKey:WebElementLinkURLKey];
    if (!url)
        return;

    DOMElement *domElement = [hitTestResult objectForKey:WebElementDOMNodeKey];
    if (!domElement)
        return;

    NSRect itemFrame = [_webView convertRect:[self _elementBoundingBoxFromDOMElement:domElement] toView:nil];
    NSSize maximumPreviewSize = NSMakeSize(_webView.bounds.size.width * 0.75, _webView.bounds.size.height * 0.75);

    RetainPtr<QLPreviewBubble> bubble = adoptNS([[getQLPreviewBubbleClass() alloc] init]);
    [bubble setParentWindow:_webView.window];
    [bubble setMaximumSize:maximumPreviewSize];
    [bubble setPreferredEdge:NSMaxYEdge];
    [bubble setControls:@[ ]];
    NSEventMask filterMask = NSAnyEventMask & ~(NSAppKitDefinedMask | NSSystemDefinedMask | NSApplicationDefinedMask | NSMouseEnteredMask | NSMouseExitedMask);
    NSEventMask autocloseMask = NSLeftMouseDownMask | NSRightMouseDownMask | NSKeyDownMask;
    [bubble setAutomaticallyCloseWithMask:autocloseMask filterMask:filterMask block:[bubble] {
        [bubble close];
    }];
    [bubble showPreviewItem:url itemFrame:itemFrame];
}

- (RetainPtr<NSMenuItem>)_createActionMenuItemForTag:(uint32_t)tag withHitTestResult:(NSDictionary *)hitTestResult
{
    SEL selector = nullptr;
    NSString *title = nil;
    NSImage *image = nil;
    id representedObject = nil;

    // FIXME: We should localize these strings.
    switch (tag) {
    case WebActionMenuItemTagOpenLinkInDefaultBrowser:
        selector = @selector(_openURLFromActionMenu:);
        title = @"Open";
        image = webKitBundleImageNamed(@"OpenInNewWindowTemplate");
        representedObject = [hitTestResult objectForKey:WebElementLinkURLKey];
        break;

    case WebActionMenuItemTagPreviewLink:
        selector = @selector(_quickLookURLFromActionMenu:);
        title = @"Preview";
        image = [NSImage imageNamed:@"NSActionMenuQuickLook"];
        representedObject = hitTestResult;
        break;

    case WebActionMenuItemTagAddLinkToSafariReadingList:
        selector = @selector(_addToReadingListFromActionMenu:);
        title = @"Add to Safari Reading List";
        image = [NSImage imageNamed:NSImageNameBookmarksTemplate];
        representedObject = [hitTestResult objectForKey:WebElementLinkURLKey];
        break;

    default:
        ASSERT_NOT_REACHED();
        return nil;
    }

    RetainPtr<NSMenuItem> item = adoptNS([[NSMenuItem alloc] initWithTitle:title action:selector keyEquivalent:@""]);
    [item setImage:image];
    [item setTarget:self];
    [item setTag:tag];
    [item setRepresentedObject:representedObject];
    return item;
}

static NSImage *webKitBundleImageNamed(NSString *name)
{
    return [[NSBundle bundleForClass:NSClassFromString(@"WKView")] imageForResource:name];
}

- (NSArray *)_defaultMenuItemsForLink:(NSDictionary *)hitTestResult
{
    NSURL *url = [hitTestResult objectForKey:WebElementLinkURLKey];
    if (!url)
        return @[ ];

    if (!WebCore::protocolIsInHTTPFamily([url absoluteString]))
        return @[ ];

    RetainPtr<NSMenuItem> openLinkItem = [self _createActionMenuItemForTag:WebActionMenuItemTagOpenLinkInDefaultBrowser withHitTestResult:hitTestResult];
    RetainPtr<NSMenuItem> previewLinkItem = [self _createActionMenuItemForTag:WebActionMenuItemTagPreviewLink withHitTestResult:hitTestResult];
    RetainPtr<NSMenuItem> readingListItem = [self _createActionMenuItemForTag:WebActionMenuItemTagAddLinkToSafariReadingList withHitTestResult:hitTestResult];

    return @[ openLinkItem.get(), previewLinkItem.get(), [NSMenuItem separatorItem], readingListItem.get() ];
}

- (NSArray *)_defaultMenuItemsForHitTestResult:(NSDictionary *)hitTestResult
{
    NSURL *url = [hitTestResult objectForKey:WebElementLinkURLKey];

    if (url) {
        _type = WebActionMenuLink;
        return [self _defaultMenuItemsForLink:hitTestResult];
    }

    _type = WebActionMenuNone;
    return @[ ];
}

@end
