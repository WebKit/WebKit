/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "WebDragClient.h"

#import "WebArchive.h"
#import "WebDOMOperations.h"
#import "WebFrame.h"
#import "WebFrameInternal.h"
#import "WebHTMLViewInternal.h"
#import "WebHTMLViewPrivate.h"
#import "WebKitLogging.h"
#import "WebKitNSStringExtras.h"
#import "WebNSPasteboardExtras.h"
#import "WebNSURLExtras.h"
#import "WebStringTruncator.h"
#import "WebUIDelegate.h"
#import "WebUIDelegatePrivate.h"
#import "WebViewInternal.h"
#import <WebCore/ClipboardMac.h>
#import <WebCore/DragData.h>
#import <WebCore/Editor.h>
#import <WebCore/EditorClient.h>
#import <WebCore/EventHandler.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameView.h>
#import <WebCore/Image.h>
#import <WebCore/Page.h>

using namespace WebCore;

const float DragLabelBorderX = 4;
//Keep border_y in synch with DragController::LinkDragBorderInset
const float DragLabelBorderY = 2;
const float DragLabelRadius = 5;
const float LabelBorderYOffset = 2;

const float MinDragLabelWidthBeforeClip = 120;
const float MaxDragLabelWidth = 320;

const float DragLinkLabelFontsize = 11;
const float DragLinkUrlFontSize = 10;

WebDragClient::WebDragClient(WebView* webView)
    : m_webView(webView) 
{
}

static WebHTMLView *getTopHTMLView(Frame* frame)
{
    ASSERT(frame);
    ASSERT(frame->page());
    return (WebHTMLView*)[[kit(frame->page()->mainFrame()) frameView] documentView];
}

WebCore::DragDestinationAction WebDragClient::actionMaskForDrag(WebCore::DragData* dragData)
{
    return (WebCore::DragDestinationAction)[[m_webView _UIDelegateForwarder] webView:m_webView dragDestinationActionMaskForDraggingInfo:dragData->platformData()];
}

void WebDragClient::willPerformDragDestinationAction(WebCore::DragDestinationAction action, WebCore::DragData* dragData)
{
    [[m_webView _UIDelegateForwarder] webView:m_webView willPerformDragDestinationAction:(WebDragDestinationAction)action forDraggingInfo:dragData->platformData()];
}


WebCore::DragSourceAction WebDragClient::dragSourceActionMaskForPoint(const IntPoint& windowPoint)
{
    NSPoint viewPoint = [m_webView convertPoint:windowPoint fromView:nil];
    return (DragSourceAction)[[m_webView _UIDelegateForwarder] webView:m_webView dragSourceActionMaskForPoint:viewPoint];
}

void WebDragClient::willPerformDragSourceAction(WebCore::DragSourceAction action, const WebCore::IntPoint& mouseDownPoint, WebCore::Clipboard* clipboard)
{
    ASSERT(clipboard);
    [[m_webView _UIDelegateForwarder] webView:m_webView willPerformDragSourceAction:(WebDragSourceAction)action fromPoint:mouseDownPoint withPasteboard:static_cast<WebCore::ClipboardMac*>(clipboard)->pasteboard()];
}

void WebDragClient::startDrag(DragImageRef dragImage, const IntPoint& at, const IntPoint& eventPos, Clipboard* clipboard, Frame* frame, bool linkDrag)
{
    if (!frame)
        return;
    ASSERT(clipboard);
    RetainPtr<WebHTMLView> htmlView = (WebHTMLView*)[[kit(frame) frameView] documentView];
    if (![htmlView.get() isKindOfClass:[WebHTMLView class]])
        return;
    
    NSEvent *event = linkDrag ? frame->eventHandler()->currentNSEvent() : [htmlView.get() _mouseDownEvent];
    WebHTMLView* topHTMLView = getTopHTMLView(frame);
    RetainPtr<WebHTMLView> topViewProtector = topHTMLView;
    
    [topHTMLView _stopAutoscrollTimer];
    NSPasteboard *pasteboard = static_cast<ClipboardMac*>(clipboard)->pasteboard();

    NSImage *dragNSImage = dragImage.get();
    WebHTMLView *sourceHTMLView = htmlView.get();

    id delegate = [m_webView UIDelegate];
    SEL selector = @selector(webView:dragImage:at:offset:event:pasteboard:source:slideBack:forView:);
    if ([delegate respondsToSelector:selector]) {
        if ([m_webView _catchesDelegateExceptions]) {
            @try {
                [delegate webView:m_webView dragImage:dragNSImage at:at offset:NSZeroSize event:event pasteboard:pasteboard source:sourceHTMLView slideBack:YES forView:topHTMLView];
            } @catch (id exception) {
                ReportDiscardedDelegateException(selector, exception);
            }
        } else
            [delegate webView:m_webView dragImage:dragNSImage at:at offset:NSZeroSize event:event pasteboard:pasteboard source:sourceHTMLView slideBack:YES forView:topHTMLView];
    } else
        [topHTMLView dragImage:dragNSImage at:at offset:NSZeroSize event:event pasteboard:pasteboard source:sourceHTMLView slideBack:YES];
}

DragImageRef WebDragClient::createDragImageForLink(KURL& url, const String& title, Frame* frame)
{
    if (!frame)
        return nil;
    NSString *label = 0;
    if (!title.isEmpty())
        label = title;
    NSURL *cocoaURL = url;
    NSString *urlString = [cocoaURL _web_userVisibleString];

    BOOL drawURLString = YES;
    BOOL clipURLString = NO;
    BOOL clipLabelString = NO;

    if (!label) {
        drawURLString = NO;
        label = urlString;
    }

    NSFont *labelFont = [[NSFontManager sharedFontManager] convertFont:[NSFont systemFontOfSize:DragLinkLabelFontsize]
                                                           toHaveTrait:NSBoldFontMask];
    NSFont *urlFont = [NSFont systemFontOfSize:DragLinkUrlFontSize];
    NSSize labelSize;
    labelSize.width = [label _web_widthWithFont: labelFont];
    labelSize.height = [labelFont ascender] - [labelFont descender];
    if (labelSize.width > MaxDragLabelWidth){
        labelSize.width = MaxDragLabelWidth;
        clipLabelString = YES;
    }

    NSSize imageSize;
    imageSize.width = labelSize.width + DragLabelBorderX * 2;
    imageSize.height = labelSize.height + DragLabelBorderY * 2;
    if (drawURLString) {
        NSSize urlStringSize;
        urlStringSize.width = [urlString _web_widthWithFont: urlFont];
        urlStringSize.height = [urlFont ascender] - [urlFont descender];
        imageSize.height += urlStringSize.height;
        if (urlStringSize.width > MaxDragLabelWidth) {
            imageSize.width = max(MaxDragLabelWidth + DragLabelBorderY * 2, MinDragLabelWidthBeforeClip);
            clipURLString = YES;
        } else
              imageSize.width = max(labelSize.width + DragLabelBorderX * 2, urlStringSize.width + DragLabelBorderX * 2);
    }
    NSImage *dragImage = [[[NSImage alloc] initWithSize: imageSize] autorelease];
    [dragImage lockFocus];

    [[NSColor colorWithDeviceRed: 0.7f green: 0.7f blue: 0.7f alpha: 0.8f] set];

    // Drag a rectangle with rounded corners
    NSBezierPath *path = [NSBezierPath bezierPath];
    [path appendBezierPathWithOvalInRect: NSMakeRect(0, 0, DragLabelRadius * 2, DragLabelRadius * 2)];
    [path appendBezierPathWithOvalInRect: NSMakeRect(0, imageSize.height - DragLabelRadius * 2, DragLabelRadius * 2, DragLabelRadius * 2)];
    [path appendBezierPathWithOvalInRect: NSMakeRect(imageSize.width - DragLabelRadius * 2, imageSize.height - DragLabelRadius * 2, DragLabelRadius * 2, DragLabelRadius * 2)];
    [path appendBezierPathWithOvalInRect: NSMakeRect(imageSize.width - DragLabelRadius * 2, 0, DragLabelRadius * 2, DragLabelRadius * 2)];

    [path appendBezierPathWithRect: NSMakeRect(DragLabelRadius, 0, imageSize.width - DragLabelRadius * 2, imageSize.height)];
    [path appendBezierPathWithRect: NSMakeRect(0, DragLabelRadius, DragLabelRadius + 10, imageSize.height - 2 * DragLabelRadius)];
    [path appendBezierPathWithRect: NSMakeRect(imageSize.width - DragLabelRadius - 20, DragLabelRadius, DragLabelRadius + 20, imageSize.height - 2 * DragLabelRadius)];
    [path fill];

    NSColor *topColor = [NSColor colorWithDeviceWhite:0.0f alpha:0.75f];
    NSColor *bottomColor = [NSColor colorWithDeviceWhite:1.0f alpha:0.5f];
    if (drawURLString) {
        if (clipURLString)
            urlString = [WebStringTruncator centerTruncateString: urlString toWidth:imageSize.width - (DragLabelBorderX * 2) withFont:urlFont];

       [urlString _web_drawDoubledAtPoint:NSMakePoint(DragLabelBorderX, DragLabelBorderY - [urlFont descender]) 
                             withTopColor:topColor bottomColor:bottomColor font:urlFont];
    }

    if (clipLabelString)
        label = [WebStringTruncator rightTruncateString: label toWidth:imageSize.width - (DragLabelBorderX * 2) withFont:labelFont];
    [label _web_drawDoubledAtPoint:NSMakePoint (DragLabelBorderX, imageSize.height - LabelBorderYOffset - [labelFont pointSize])
                      withTopColor:topColor bottomColor:bottomColor font:labelFont];

    [dragImage unlockFocus];

    return dragImage;
}

void WebDragClient::declareAndWriteDragImage(NSPasteboard* pasteboard, DOMElement* element, NSURL* URL, NSString* title, WebCore::Frame* frame) 
{
    ASSERT(pasteboard);
    ASSERT(element);
    WebHTMLView *source = getTopHTMLView(frame);      
    WebArchive *archive = [element webArchive];
    
    [pasteboard _web_declareAndWriteDragImageForElement:element URL:URL title:title archive:archive source:source];
}

void WebDragClient::dragControllerDestroyed() 
{
    delete this;
}
