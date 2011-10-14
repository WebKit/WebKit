/*
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef DefaultClientCallbacksQt_h
#define DefaultClientCallbacksQt_h

#include <WebKit2/WKPage.h>

#ifdef __cplusplus
extern "C" {
#endif

// loader client
void qt_wk_didStartProvisionalLoadForFrame(WKPageRef, WKFrameRef, WKTypeRef, const void* clientInfo);
void qt_wk_didFailProvisionalLoadWithErrorForFrame(WKPageRef, WKFrameRef, WKErrorRef, WKTypeRef, const void* clientInfo);
void qt_wk_didCommitLoadForFrame(WKPageRef, WKFrameRef, WKTypeRef, const void* clientInfo);
void qt_wk_didFinishLoadForFrame(WKPageRef, WKFrameRef, WKTypeRef, const void* clientInfo);
void qt_wk_didFailLoadWithErrorForFrame(WKPageRef, WKFrameRef, WKErrorRef, WKTypeRef, const void* clientInfo);
void qt_wk_didReceiveTitleForFrame(WKPageRef, WKStringRef title, WKFrameRef, WKTypeRef, const void* clientInfo);
void qt_wk_didStartProgress(WKPageRef, const void* clientInfo);
void qt_wk_didChangeProgress(WKPageRef, const void* clientInfo);
void qt_wk_didFinishProgress(WKPageRef, const void* clientInfo);
void qt_wk_didSameDocumentNavigationForFrame(WKPageRef, WKFrameRef, WKSameDocumentNavigationType, WKTypeRef, const void* clientInfo);

// ui client
void qt_wk_runJavaScriptAlert(WKPageRef, WKStringRef alertText, WKFrameRef, const void* clientInfo);
bool qt_wk_runJavaScriptConfirm(WKPageRef, WKStringRef message, WKFrameRef, const void* clientInfo);
WKStringRef qt_wk_runJavaScriptPrompt(WKPageRef, WKStringRef message, WKStringRef defaultValue, WKFrameRef, const void* clientInfo);
void qt_wk_setStatusText(WKPageRef page, WKStringRef text, const void *clientInfo);
void qt_wk_runOpenPanel(WKPageRef, WKFrameRef, WKOpenPanelParametersRef, WKOpenPanelResultListenerRef, const void* clientInfo);
void qt_wk_mouseDidMoveOverElement(WKPageRef, WKHitTestResultRef, WKEventModifiers, WKTypeRef, const void* clientInfo);

// Policy client.
void qt_wk_decidePolicyForNavigationAction(WKPageRef, WKFrameRef, WKFrameNavigationType, WKEventModifiers, WKEventMouseButton, WKURLRequestRef, WKFramePolicyListenerRef, WKTypeRef userData, const void* clientInfo);

#ifdef __cplusplus
}
#endif

#endif /* DefaultClientCallbacksQt_h */

