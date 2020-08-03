/*
 * Copyright (C) 2007-2020 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "WebDragClient.h"

#if ENABLE(DRAG_SUPPORT)

#import "DOMElementInternal.h"
#import "WebArchive.h"
#import "WebDOMOperations.h"
#import "WebFrame.h"
#import "WebFrameInternal.h"
#import "WebFrameView.h"
#import "WebHTMLViewInternal.h"
#import "WebKitLogging.h"
#import "WebKitNSStringExtras.h"
#import "WebNSURLExtras.h"
#import "WebUIDelegate.h"
#import "WebUIDelegatePrivate.h"
#import "WebViewInternal.h"

#if PLATFORM(MAC)
#import "WebNSPasteboardExtras.h"
#endif

#import <WebCore/DataTransfer.h>
#import <WebCore/DragData.h>
#import <WebCore/Editor.h>
#import <WebCore/EditorClient.h>
#import <WebCore/EventHandler.h>
#import <WebCore/FloatPoint.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameView.h>
#import <WebCore/Image.h>
#import <WebCore/Page.h>
#import <WebCore/Pasteboard.h>
#import <WebCore/PasteboardWriter.h>

using namespace WebCore;

WebDragClient::WebDragClient(WebView* webView)
    : m_webView(webView) 
{
    UNUSED_PARAM(m_webView);
}

#if PLATFORM(MAC)

static OptionSet<WebCore::DragSourceAction> coreDragSourceActionMask(WebDragSourceAction action)
{
    OptionSet<WebCore::DragSourceAction> result;

    if (action & WebDragSourceActionDHTML)
        result.add(WebCore::DragSourceAction::DHTML);
    if (action & WebDragSourceActionImage)
        result.add(WebCore::DragSourceAction::Image);
    if (action & WebDragSourceActionLink)
        result.add(WebCore::DragSourceAction::Link);
    if (action & WebDragSourceActionSelection)
        result.add(WebCore::DragSourceAction::Selection);

    return result;
}

static WebDragDestinationAction kit(WebCore::DragDestinationAction action)
{
    switch (action) {
    case WebCore::DragDestinationAction::DHTML:
        return WebDragDestinationActionDHTML;
    case WebCore::DragDestinationAction::Edit:
        return WebDragDestinationActionEdit;
    case WebCore::DragDestinationAction::Load:
        return WebDragDestinationActionLoad;
    }
    ASSERT_NOT_REACHED();
    return WebDragDestinationActionNone;
}

bool WebDragClient::useLegacyDragClient()
{
    return false;
}

void WebDragClient::didConcludeEditDrag()
{
}

static WebHTMLView *getTopHTMLView(Frame* frame)
{
    ASSERT(frame);
    ASSERT(frame->page());
    return (WebHTMLView*)[[kit(&frame->page()->mainFrame()) frameView] documentView];
}

void WebDragClient::willPerformDragDestinationAction(WebCore::DragDestinationAction action, const WebCore::DragData& dragData)
{
    [[m_webView _UIDelegateForwarder] webView:m_webView willPerformDragDestinationAction:kit(action) forDraggingInfo:dragData.platformData()];
}


OptionSet<WebCore::DragSourceAction> WebDragClient::dragSourceActionMaskForPoint(const IntPoint& rootViewPoint)
{
    NSPoint viewPoint = [m_webView _convertPointFromRootView:rootViewPoint];
    return coreDragSourceActionMask([[m_webView _UIDelegateForwarder] webView:m_webView dragSourceActionMaskForPoint:viewPoint]);
}

void WebDragClient::willPerformDragSourceAction(WebCore::DragSourceAction action, const WebCore::IntPoint& mouseDownPoint, WebCore::DataTransfer& dataTransfer)
{
    [[m_webView _UIDelegateForwarder] webView:m_webView willPerformDragSourceAction:kit(action) fromPoint:mouseDownPoint withPasteboard:[NSPasteboard pasteboardWithName:dataTransfer.pasteboard().name()]];
}

void WebDragClient::startDrag(DragItem dragItem, DataTransfer& dataTransfer, Frame& frame)
{
    auto& dragImage = dragItem.image;
    auto dragLocationInContentCoordinates = dragItem.dragLocationInContentCoordinates;

    RetainPtr<WebHTMLView> htmlView = (WebHTMLView*)[[kit(&frame) frameView] documentView];
    if (![htmlView.get() isKindOfClass:[WebHTMLView class]])
        return;
    
    NSEvent *event = (dragItem.sourceAction && *dragItem.sourceAction == DragSourceAction::Link) ? frame.eventHandler().currentNSEvent() : [htmlView.get() _mouseDownEvent];
    WebHTMLView* topHTMLView = getTopHTMLView(&frame);
    RetainPtr<WebHTMLView> topViewProtector = topHTMLView;
    
    [topHTMLView _stopAutoscrollTimer];
    NSPasteboard *pasteboard = [NSPasteboard pasteboardWithName:dataTransfer.pasteboard().name()];

    NSImage *dragNSImage = dragImage.get().get();
    WebHTMLView *sourceHTMLView = htmlView.get();

    IntSize size([dragNSImage size]);
    size.scale(1 / frame.page()->deviceScaleFactor());
    [dragNSImage setSize:size];

    id delegate = [m_webView UIDelegate];
    SEL selector = @selector(webView:dragImage:at:offset:event:pasteboard:source:slideBack:forView:);
    if ([delegate respondsToSelector:selector]) {
        @try {
            [delegate webView:m_webView dragImage:dragNSImage at:dragLocationInContentCoordinates offset:NSZeroSize event:event pasteboard:pasteboard source:sourceHTMLView slideBack:YES forView:topHTMLView];
        } @catch (id exception) {
            ReportDiscardedDelegateException(selector, exception);
        }
    } else
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        [topHTMLView dragImage:dragNSImage at:dragLocationInContentCoordinates offset:NSZeroSize event:event pasteboard:pasteboard source:sourceHTMLView slideBack:YES];
        ALLOW_DEPRECATED_DECLARATIONS_END
}

void WebDragClient::beginDrag(DragItem dragItem, Frame& frame, const IntPoint& mouseDownPosition, const IntPoint& mouseDraggedPosition, DataTransfer& dataTransfer, DragSourceAction dragSourceAction)
{
    ASSERT(!dataTransfer.pasteboard().hasData());

    RetainPtr<WebHTMLView> topWebHTMLView = dynamic_objc_cast<WebHTMLView>(m_webView.mainFrame.frameView.documentView);
    ASSERT(topWebHTMLView);

    [topWebHTMLView _stopAutoscrollTimer];

    auto draggingItem = adoptNS([[NSDraggingItem alloc] initWithPasteboardWriter:createPasteboardWriter(dragItem.data).get()]);

    auto dragImageSize = IntSize { [dragItem.image.get() size] };

    dragImageSize.scale(1 / frame.page()->deviceScaleFactor());
    [dragItem.image.get() setSize:dragImageSize];

    NSRect draggingFrame = NSMakeRect(mouseDraggedPosition.x() - dragImageSize.width() * dragItem.imageAnchorPoint.x(), mouseDraggedPosition.y() - dragImageSize.height() * dragItem.imageAnchorPoint.y(), dragImageSize.width(), dragImageSize.height());
    [draggingItem setDraggingFrame:draggingFrame contents:dragItem.image.get().get()];

    // FIXME: We should be able to make a fake event with the mosue dragged coordinates.
    NSEvent *event = frame.eventHandler().currentNSEvent();
    [topWebHTMLView.get() beginDraggingSessionWithItems:@[ draggingItem.get() ] event:event source:topWebHTMLView.get()];
}

void WebDragClient::declareAndWriteDragImage(const String& pasteboardName, Element& element, const URL& url, const String& title, WebCore::Frame* frame)
{
    ASSERT(pasteboardName);
    [[NSPasteboard pasteboardWithName:pasteboardName] _web_declareAndWriteDragImageForElement:kit(&element) URL:url title:title archive:[kit(&element) webArchive] source:getTopHTMLView(frame)];
}

#elif !PLATFORM(IOS_FAMILY) || !ENABLE(DRAG_SUPPORT)

bool WebDragClient::useLegacyDragClient()
{
    return false;
}

void WebDragClient::didConcludeEditDrag()
{
}

void WebDragClient::willPerformDragDestinationAction(WebCore::DragDestinationAction, const WebCore::DragData&)
{
}

OptionSet<WebCore::DragSourceAction> WebDragClient::dragSourceActionMaskForPoint(const IntPoint&)
{
    return { };
}

void WebDragClient::willPerformDragSourceAction(WebCore::DragSourceAction, const WebCore::IntPoint&, WebCore::DataTransfer&)
{
}

void WebDragClient::startDrag(WebCore::DragItem, DataTransfer&, Frame&)
{
}

void WebDragClient::beginDrag(DragItem, Frame&, const IntPoint&, const IntPoint&, DataTransfer&, DragSourceAction)
{
}

void WebDragClient::declareAndWriteDragImage(const String&, Element&, const URL&, const String&, WebCore::Frame*)
{
}

#endif

#if PLATFORM(IOS_FAMILY)

bool WebDragClient::useLegacyDragClient()
{
    // FIXME: Move the iOS drag and drop implementation for WebKit1 off of the legacy drag client.
    return true;
}

void WebDragClient::willPerformDragDestinationAction(WebCore::DragDestinationAction, const DragData&)
{
}

OptionSet<WebCore::DragSourceAction> WebDragClient::dragSourceActionMaskForPoint(const IntPoint&)
{
    return WebCore::anyDragSourceAction();
}

void WebDragClient::willPerformDragSourceAction(WebCore::DragSourceAction, const IntPoint&, DataTransfer&)
{
}

void WebDragClient::startDrag(DragItem dragItem, DataTransfer&, Frame&)
{
    [m_webView _startDrag:dragItem];
}

void WebDragClient::beginDrag(DragItem, Frame&, const IntPoint&, const IntPoint&, DataTransfer&, DragSourceAction)
{
}

void WebDragClient::declareAndWriteDragImage(const String& pasteboardName, Element& element, const URL& url, const String& label, Frame*)
{
    if (auto* frame = element.document().frame())
        frame->editor().writeImageToPasteboard(*Pasteboard::createForDragAndDrop(), element, url, label);
}

void WebDragClient::didConcludeEditDrag()
{
    [m_webView _didConcludeEditDrag];
}

#endif // PLATFORM(IOS_FAMILY)

#endif // ENABLE(DRAG_SUPPORT)
