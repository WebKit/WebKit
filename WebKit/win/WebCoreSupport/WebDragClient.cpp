/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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

#include "config.h"
#include "WebDragClient.h"
#include "WebDropSource.h"
#include "WebKitGraphics.h"
#include "WebView.h"

#include <shlobj.h>

#pragma warning(push, 0) 
#include <WebCore/ClipboardWin.h>
#include <WebCore/DragController.h>
#include <WebCore/DragData.h>
#include <WebCore/Font.h>
#include <WebCore/FontDescription.h>
#include <WebCore/FontSelector.h>
#include <WebCore/FrameView.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/Page.h>
#include <WebCore/Settings.h>
#include <WebCore/StringTruncator.h>
#include <WebCore/WebCoreTextRenderer.h>
#pragma warning(pop) 

namespace WebCore {
    HBITMAP allocImage(HDC dc, IntSize size, PlatformGraphicsContext** targetRef);
    void deallocContext(PlatformGraphicsContext* target);
}


#define DRAG_LABEL_BORDER_X             4
// Keep border_y in synch with DragController::LinkDragBorderInset
#define DRAG_LABEL_BORDER_Y             2
#define DRAG_LABEL_RADIUS               5
#define DRAG_LABEL_BORDER_Y_OFFSET              2

#define MIN_DRAG_LABEL_WIDTH_BEFORE_CLIP        120

// Observation says max size is 200x200
#define MAX_DRAG_LABEL_WIDTH                    200
#define MAX_DRAG_LABEL_STRING_WIDTH             (MAX_DRAG_LABEL_WIDTH - 2 * DRAG_LABEL_BORDER_X)

#define DRAG_LINK_LABEL_FONT_SIZE   11
#define DRAG_LINK_URL_FONT_SIZE   10

using namespace WebCore;

static DWORD draggingSourceOperationMaskToDragCursors(DragOperation op)
{
    DWORD result = DROPEFFECT_NONE;
    if (op == DragOperationEvery)
        return DROPEFFECT_COPY | DROPEFFECT_LINK | DROPEFFECT_MOVE; 
    if (op & DragOperationCopy)
        result |= DROPEFFECT_COPY; 
    if (op & DragOperationLink)
        result |= DROPEFFECT_LINK; 
    if (op & DragOperationMove)
        result |= DROPEFFECT_MOVE;
    if (op & DragOperationGeneric)
        result |= DROPEFFECT_MOVE;
    return result;
}

WebDragClient::WebDragClient(WebView* webView)
    : m_webView(webView) 
{
    ASSERT(webView);
}

DragDestinationAction WebDragClient::actionMaskForDrag(DragData* dragData)
{
    COMPtr<IWebUIDelegate> delegateRef = 0;
    //Default behaviour (eg. no delegate, or callback not implemented) is to allow
    //any action
    WebDragDestinationAction mask = WebDragDestinationActionAny;
    if (SUCCEEDED(m_webView->uiDelegate(&delegateRef)))
        delegateRef->dragDestinationActionMaskForDraggingInfo(m_webView, dragData->platformData(), &mask);

    return (DragDestinationAction)mask;
}

void WebDragClient::willPerformDragDestinationAction(DragDestinationAction action, DragData* dragData)
{
    //Default delegate for willPerformDragDestinationAction has no side effects
    //so we just call the delegate, and don't worry about whether it's implemented
    COMPtr<IWebUIDelegate> delegateRef = 0;
    if (SUCCEEDED(m_webView->uiDelegate(&delegateRef)))
        delegateRef->willPerformDragDestinationAction(m_webView, (WebDragDestinationAction)action, dragData->platformData());
}

DragSourceAction WebDragClient::dragSourceActionMaskForPoint(const IntPoint& windowPoint)
{
   COMPtr<IWebUIDelegate> delegateRef = 0;
   WebDragSourceAction action = WebDragSourceActionAny;
   POINT localpt = core(m_webView)->mainFrame()->view()->windowToContents(windowPoint);
   if (SUCCEEDED(m_webView->uiDelegate(&delegateRef)))
       delegateRef->dragSourceActionMaskForPoint(m_webView, &localpt, &action);
   return (DragSourceAction)action;
}

void WebDragClient::willPerformDragSourceAction(DragSourceAction action, const IntPoint& intPoint, Clipboard* clipboard)
{
    COMPtr<IWebUIDelegate> uiDelegate;
    if (!SUCCEEDED(m_webView->uiDelegate(&uiDelegate)))
        return;

    POINT point = intPoint;
    COMPtr<IDataObject> dataObject = static_cast<ClipboardWin*>(clipboard)->dataObject();

    COMPtr<IDataObject> newDataObject;
    HRESULT result = uiDelegate->willPerformDragSourceAction(m_webView, static_cast<WebDragSourceAction>(action), &point, dataObject.get(), &newDataObject);
    if (result == S_OK && newDataObject != dataObject)
        static_cast<ClipboardWin*>(clipboard)->setExternalDataObject(newDataObject.get());
}

void WebDragClient::startDrag(DragImageRef image, const IntPoint& imageOrigin, const IntPoint& dragPoint, Clipboard* clipboard, Frame* frame, bool isLink)
{
    //FIXME: Allow UIDelegate to override behaviour <rdar://problem/5015953>

    //We liberally protect everything, to protect against a load occurring mid-drag
    RefPtr<Frame> frameProtector = frame;
    COMPtr<IDragSourceHelper> helper;
    COMPtr<IDataObject> dataObject;
    COMPtr<WebView> viewProtector = m_webView;
    COMPtr<IDropSource> source;
    if (FAILED(WebDropSource::createInstance(m_webView, &source)))
        return;

    dataObject = static_cast<ClipboardWin*>(clipboard)->dataObject();
    if (source && (image || dataObject)) {
        if (image) {
            if(SUCCEEDED(CoCreateInstance(CLSID_DragDropHelper, 0, CLSCTX_INPROC_SERVER,
                IID_IDragSourceHelper,(LPVOID*)&helper))) {
                BITMAP b;
                GetObject(image, sizeof(BITMAP), &b);
                SHDRAGIMAGE sdi;
                sdi.sizeDragImage.cx = b.bmWidth;
                sdi.sizeDragImage.cy = b.bmHeight;
                sdi.crColorKey = 0xffffffff;
                sdi.hbmpDragImage = image;
                sdi.ptOffset.x = dragPoint.x() - imageOrigin.x();
                sdi.ptOffset.y = dragPoint.y() - imageOrigin.y();
                if (isLink)
                    sdi.ptOffset.y = b.bmHeight - sdi.ptOffset.y;

                helper->InitializeFromBitmap(&sdi, dataObject.get());
            }
        }

        DWORD okEffect = draggingSourceOperationMaskToDragCursors(m_webView->page()->dragController()->sourceDragOperation());
        DWORD effect = DROPEFFECT_NONE;
        COMPtr<IWebUIDelegate> ui;
        HRESULT hr = E_NOTIMPL;
        if (SUCCEEDED(m_webView->uiDelegate(&ui))) {
            COMPtr<IWebUIDelegatePrivate> uiPrivate;
            if (SUCCEEDED(ui->QueryInterface(IID_IWebUIDelegatePrivate, (void**)&uiPrivate)))
                hr = uiPrivate->doDragDrop(m_webView, dataObject.get(), source.get(), okEffect, &effect);
        }
        if (hr == E_NOTIMPL)
            hr = DoDragDrop(dataObject.get(), source.get(), okEffect, &effect);

        DragOperation operation = DragOperationNone;
        if (hr == DRAGDROP_S_DROP) {
            if (effect & DROPEFFECT_COPY)
                operation = DragOperationCopy;
            else if (effect & DROPEFFECT_LINK)
                operation = DragOperationLink;
            else if (effect & DROPEFFECT_MOVE)
                operation = DragOperationMove;
        }
        frame->eventHandler()->dragSourceEndedAt(generateMouseEvent(m_webView, false), operation);
    }
}

static Font dragLabelFont(int size, bool bold, FontRenderingMode renderingMode)
{
    NONCLIENTMETRICS metrics;
    metrics.cbSize = sizeof(metrics);
    SystemParametersInfo(SPI_GETNONCLIENTMETRICS, metrics.cbSize, &metrics, 0);
    FontDescription desc;
    desc.setWeight(bold ? FontWeightBold : FontWeightNormal);
    FontFamily family;
    family.setFamily(metrics.lfSmCaptionFont.lfFaceName);
    desc.setFamily(family);
    desc.setSpecifiedSize((float)size);
    desc.setComputedSize((float)size);
    desc.setRenderingMode(renderingMode);
    Font result = Font(desc, 0, 0); 
    result.update(0);
    return result;
}

DragImageRef WebDragClient::createDragImageForLink(KURL& url, const String& inLabel, Frame* frame)
{
    // This is more or less an exact match for the Mac OS X code.

    const Font* labelFont;
    const Font* urlFont;

    if (frame->settings() && frame->settings()->fontRenderingMode() == AlternateRenderingMode) {
        static const Font alternateRenderingModeLabelFont = dragLabelFont(DRAG_LINK_LABEL_FONT_SIZE, true, AlternateRenderingMode);
        static const Font alternateRenderingModeURLFont = dragLabelFont(DRAG_LINK_URL_FONT_SIZE, false, AlternateRenderingMode);
        labelFont = &alternateRenderingModeLabelFont;
        urlFont = &alternateRenderingModeURLFont;
    } else {
        static const Font normalRenderingModeLabelFont = dragLabelFont(DRAG_LINK_LABEL_FONT_SIZE, true, NormalRenderingMode);
        static const Font normalRenderingModeURLFont = dragLabelFont(DRAG_LINK_URL_FONT_SIZE, false, NormalRenderingMode);
        labelFont = &normalRenderingModeLabelFont;
        urlFont = &normalRenderingModeURLFont;
    }

    bool drawURLString = true;
    bool clipURLString = false;
    bool clipLabelString = false;

    String urlString = url.string(); 
    String label = inLabel;
    if (label.isEmpty()) {
        drawURLString = false;
        label = urlString;
    }

    //First step in drawing the link drag image width
    TextRun labelRun(label.impl());
    TextRun urlRun(urlString.impl());
    IntSize labelSize(labelFont->width(labelRun), labelFont->ascent() + labelFont->descent());

    if (labelSize.width() > MAX_DRAG_LABEL_STRING_WIDTH){
        labelSize.setWidth(MAX_DRAG_LABEL_STRING_WIDTH);
        clipLabelString = true;
    }
    
    IntSize urlStringSize;
    IntSize imageSize(labelSize.width() + DRAG_LABEL_BORDER_X * 2, 
                      labelSize.height() + DRAG_LABEL_BORDER_Y * 2);

    if (drawURLString) {
        urlStringSize.setWidth(urlFont->width(urlRun));
        urlStringSize.setHeight(urlFont->ascent() + urlFont->descent()); 
        imageSize.setHeight(imageSize.height() + urlStringSize.height());
        if (urlStringSize.width() > MAX_DRAG_LABEL_STRING_WIDTH) {
            imageSize.setWidth(MAX_DRAG_LABEL_WIDTH);
            clipURLString = true;
        } else {
            imageSize.setWidth(std::max(labelSize.width(), urlStringSize.width()) + DRAG_LABEL_BORDER_X * 2);
        }
    }

    // We now know how big the image needs to be, so we create and
    // fill the background
    HBITMAP image = 0;
    HDC dc = GetDC(0);
    HDC workingDC = CreateCompatibleDC(dc);
    if (!workingDC) {
        ReleaseDC(0, dc);
        return 0;
    }

    PlatformGraphicsContext* contextRef;
    image = allocImage(workingDC, imageSize, &contextRef);
    if (!image) {
        DeleteDC(workingDC);
        ReleaseDC(0, dc);
        return 0;
    }
        
    ::SelectObject(workingDC, image);
    GraphicsContext context(contextRef);
    // On Mac alpha is {0.7, 0.7, 0.7, 0.8}, however we can't control alpha
    // for drag images on win, so we use 1
    static const Color backgroundColor(140, 140, 140);
    static const IntSize radii(DRAG_LABEL_RADIUS, DRAG_LABEL_RADIUS);
    IntRect rect(0, 0, imageSize.width(), imageSize.height());
    context.fillRoundedRect(rect, radii, radii, radii, radii, backgroundColor, DeviceColorSpace);
 
    // Draw the text
    static const Color topColor(0, 0, 0, 255); //original alpha = 0.75
    static const Color bottomColor(255, 255, 255, 127); //original alpha = 0.5
    if (drawURLString) {
        if (clipURLString)
            urlString = StringTruncator::rightTruncate(urlString, imageSize.width() - (DRAG_LABEL_BORDER_X * 2.0f), *urlFont, false);
        IntPoint textPos(DRAG_LABEL_BORDER_X, imageSize.height() - (DRAG_LABEL_BORDER_Y_OFFSET + urlFont->descent()));
        WebCoreDrawDoubledTextAtPoint(context, urlString, textPos, *urlFont, topColor, bottomColor);
    }
    
    if (clipLabelString)
        label = StringTruncator::rightTruncate(label, imageSize.width() - (DRAG_LABEL_BORDER_X * 2.0f), *labelFont, false);

    IntPoint textPos(DRAG_LABEL_BORDER_X, DRAG_LABEL_BORDER_Y + labelFont->pixelSize());
    WebCoreDrawDoubledTextAtPoint(context, label, textPos, *labelFont, topColor, bottomColor);

    deallocContext(contextRef);
    DeleteDC(workingDC);
    ReleaseDC(0, dc);
    return image;
}

void WebDragClient::dragControllerDestroyed()
{
    delete this;
}
