/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
 * Copyright (C) 2011 Igalia S.L.
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

#pragma once

#include "APIPageConfiguration.h"
#include "GestureController.h"
#include "InputMethodState.h"
#include "SameDocumentNavigationType.h"
#include "ShareableBitmap.h"
#include "ViewGestureController.h"
#include "ViewSnapshotStore.h"
#include "WebContextMenuProxyGtk.h"
#include "WebHitTestResultData.h"
#include "WebInspectorProxy.h"
#include "WebKitInputMethodContext.h"
#include "WebKitWebViewBase.h"
#include "WebPageProxy.h"
#include <WebCore/DragActions.h>
#include <WebCore/SelectionData.h>
#include <wtf/Optional.h>

WebKitWebViewBase* webkitWebViewBaseCreate(const API::PageConfiguration&);
WebKit::WebPageProxy* webkitWebViewBaseGetPage(WebKitWebViewBase*);
void webkitWebViewBaseCreateWebPage(WebKitWebViewBase*, Ref<API::PageConfiguration>&&);
void webkitWebViewBaseSetTooltipText(WebKitWebViewBase*, const char*);
void webkitWebViewBaseSetTooltipArea(WebKitWebViewBase*, const WebCore::IntRect&);
void webkitWebViewBaseSetMouseIsOverScrollbar(WebKitWebViewBase*, WebKit::WebHitTestResultData::IsScrollbar);
void webkitWebViewBaseForwardNextKeyEvent(WebKitWebViewBase*);
void webkitWebViewBaseForwardNextWheelEvent(WebKitWebViewBase*);
void webkitWebViewBaseChildMoveResize(WebKitWebViewBase*, GtkWidget*, const WebCore::IntRect&);
void webkitWebViewBaseEnterFullScreen(WebKitWebViewBase*);
void webkitWebViewBaseExitFullScreen(WebKitWebViewBase*);
bool webkitWebViewBaseIsFullScreen(WebKitWebViewBase*);
void webkitWebViewBaseSetInspectorViewSize(WebKitWebViewBase*, unsigned size);
void webkitWebViewBaseSetActiveContextMenuProxy(WebKitWebViewBase*, WebKit::WebContextMenuProxyGtk*);
WebKit::WebContextMenuProxyGtk* webkitWebViewBaseGetActiveContextMenuProxy(WebKitWebViewBase*);
GdkEvent* webkitWebViewBaseTakeContextMenuEvent(WebKitWebViewBase*);
void webkitWebViewBaseSetInputMethodState(WebKitWebViewBase*, Optional<WebKit::InputMethodState>&&);
void webkitWebViewBaseUpdateTextInputState(WebKitWebViewBase*);
void webkitWebViewBaseSetContentsSize(WebKitWebViewBase*, const WebCore::IntSize&);

void webkitWebViewBaseSetFocus(WebKitWebViewBase*, bool focused);
bool webkitWebViewBaseIsInWindowActive(WebKitWebViewBase*);
bool webkitWebViewBaseIsFocused(WebKitWebViewBase*);
bool webkitWebViewBaseIsVisible(WebKitWebViewBase*);
bool webkitWebViewBaseIsInWindow(WebKitWebViewBase*);

void webkitWebViewBaseAddDialog(WebKitWebViewBase*, GtkWidget*);
void webkitWebViewBaseAddWebInspector(WebKitWebViewBase*, GtkWidget* inspector, WebKit::AttachmentSide);
void webkitWebViewBaseRemoveWebInspector(WebKitWebViewBase*, GtkWidget*);
void webkitWebViewBaseResetClickCounter(WebKitWebViewBase*);
void webkitWebViewBaseEnterAcceleratedCompositingMode(WebKitWebViewBase*, const WebKit::LayerTreeContext&);
void webkitWebViewBaseUpdateAcceleratedCompositingMode(WebKitWebViewBase*, const WebKit::LayerTreeContext&);
void webkitWebViewBaseExitAcceleratedCompositingMode(WebKitWebViewBase*);
bool webkitWebViewBaseMakeGLContextCurrent(WebKitWebViewBase*);
void webkitWebViewBaseWillSwapWebProcess(WebKitWebViewBase*);
void webkitWebViewBaseDidExitWebProcess(WebKitWebViewBase*);
void webkitWebViewBaseDidRelaunchWebProcess(WebKitWebViewBase*);
void webkitWebViewBasePageClosed(WebKitWebViewBase*);

#if ENABLE(DRAG_SUPPORT)
void webkitWebViewBaseStartDrag(WebKitWebViewBase*, WebCore::SelectionData&&, OptionSet<WebCore::DragOperation>, RefPtr<WebKit::ShareableBitmap>&&);
void webkitWebViewBaseDidPerformDragControllerAction(WebKitWebViewBase*);
#endif

#if !USE(GTK4)
WebKit::GestureController& webkitWebViewBaseGestureController(WebKitWebViewBase*);
#endif

RefPtr<WebKit::ViewSnapshot> webkitWebViewBaseTakeViewSnapshot(WebKitWebViewBase*, Optional<WebCore::IntRect>&&);
void webkitWebViewBaseSetEnableBackForwardNavigationGesture(WebKitWebViewBase*, bool enabled);
#if !USE(GTK4)
WebKit::ViewGestureController* webkitWebViewBaseViewGestureController(WebKitWebViewBase*);
#endif

bool webkitWebViewBaseBeginBackSwipeForTesting(WebKitWebViewBase*);
bool webkitWebViewBaseCompleteBackSwipeForTesting(WebKitWebViewBase*);

void webkitWebViewBaseDidStartProvisionalLoadForMainFrame(WebKitWebViewBase*);
void webkitWebViewBaseDidFirstVisuallyNonEmptyLayoutForMainFrame(WebKitWebViewBase*);
void webkitWebViewBaseDidFinishNavigation(WebKitWebViewBase*, API::Navigation*);
void webkitWebViewBaseDidFailNavigation(WebKitWebViewBase*, API::Navigation*);
void webkitWebViewBaseDidSameDocumentNavigationForMainFrame(WebKitWebViewBase*, WebKit::SameDocumentNavigationType);
void webkitWebViewBaseDidRestoreScrollPosition(WebKitWebViewBase*);

void webkitWebViewBaseShowEmojiChooser(WebKitWebViewBase*, const WebCore::IntRect&, CompletionHandler<void(String)>&&);

#if USE(WPE_RENDERER)
int webkitWebViewBaseRenderHostFileDescriptor(WebKitWebViewBase*);
#endif

void webkitWebViewBaseRequestPointerLock(WebKitWebViewBase*);
void webkitWebViewBaseDidLosePointerLock(WebKitWebViewBase*);

void webkitWebViewBaseSetInputMethodContext(WebKitWebViewBase*, WebKitInputMethodContext*);
WebKitInputMethodContext* webkitWebViewBaseGetInputMethodContext(WebKitWebViewBase*);
void webkitWebViewBaseSynthesizeCompositionKeyPress(WebKitWebViewBase*, const String& text, Optional<Vector<WebCore::CompositionUnderline>>&&, Optional<WebKit::EditingRange>&&);
