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

#import "WebDragClient.h"

#import "WebFrame.h"
#import "WebUIDelegate.h"
#import "WebViewInternal.h"
#import <WebCore/DragData.h>

WebDragClient::WebDragClient(WebView* webView)
    : m_webView(webView) 
{
}

WebCore::DragDestinationAction WebDragClient::actionMaskForDrag(WebCore::DragData* dragData)
{
    return (WebCore::DragDestinationAction)[[m_webView _UIDelegateForwarder] webView:m_webView dragDestinationActionMaskForDraggingInfo:dragData->platformData()];
}

void WebDragClient::willPerformDragDestinationAction(WebCore::DragDestinationAction action, WebCore::DragData* dragData)
{
    [[m_webView _UIDelegateForwarder] webView:m_webView willPerformDragDestinationAction:(WebDragDestinationAction)action forDraggingInfo:dragData->platformData()];
}


WebCore::DragSourceAction WebDragClient::dragSourceActionMaskForPoint(const WebCore::IntPoint& windowPoint)
{
    NSPoint viewPoint = [m_webView convertPoint:windowPoint fromView:nil];
    return (WebCore::DragSourceAction)[[m_webView _UIDelegateForwarder] webView:m_webView dragSourceActionMaskForPoint:viewPoint];
}

void WebDragClient::dragControllerDestroyed() 
{
    delete this;
}
