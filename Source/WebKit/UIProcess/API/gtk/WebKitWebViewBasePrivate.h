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
#include "InputMethodState.h"
#include "SameDocumentNavigationType.h"
#include "ShareableBitmap.h"
#include "ViewGestureController.h"
#include "ViewSnapshotStore.h"
#include "WebContextMenuProxyGtk.h"
#include "WebHitTestResultData.h"
#include "WebInspectorUIProxy.h"
#include "WebKitInputMethodContext.h"
#include "WebKitWebViewBase.h"
#include "WebKitWebViewBaseInternal.h"
#include "WebPageProxy.h"
#include <WebCore/DragActions.h>
#include <WebCore/GRefPtrGtk.h>
#include <WebCore/GUniquePtrGtk.h>
#include <WebCore/SelectionData.h>

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

#if USE(GTK4)
GRefPtr<GdkEvent> webkitWebViewBaseTakeContextMenuEvent(WebKitWebViewBase*);
#else
GUniquePtr<GdkEvent> webkitWebViewBaseTakeContextMenuEvent(WebKitWebViewBase*);
#endif

void webkitWebViewBaseSetInputMethodState(WebKitWebViewBase*, std::optional<WebKit::InputMethodState>&&);
void webkitWebViewBaseUpdateTextInputState(WebKitWebViewBase*);
void webkitWebViewBaseSetContentsSize(WebKitWebViewBase*, const WebCore::IntSize&);

void webkitWebViewBaseSetFocus(WebKitWebViewBase*, bool focused);
void webkitWebViewBaseSetEditable(WebKitWebViewBase*, bool editable);
WebCore::IntSize webkitWebViewBaseGetViewSize(WebKitWebViewBase*);
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
void webkitWebViewBaseStartDrag(WebKitWebViewBase*, WebCore::SelectionData&&, OptionSet<WebCore::DragOperation>, RefPtr<WebKit::ShareableBitmap>&&, WebCore::IntPoint&& dragImageHotspot);
void webkitWebViewBaseDidPerformDragControllerAction(WebKitWebViewBase*);
#endif

RefPtr<WebKit::ViewSnapshot> webkitWebViewBaseTakeViewSnapshot(WebKitWebViewBase*, std::optional<WebCore::IntRect>&&);
void webkitWebViewBaseSetEnableBackForwardNavigationGesture(WebKitWebViewBase*, bool enabled);
WebKit::ViewGestureController* webkitWebViewBaseViewGestureController(WebKitWebViewBase*);

bool webkitWebViewBaseBeginBackSwipeForTesting(WebKitWebViewBase*);
bool webkitWebViewBaseCompleteBackSwipeForTesting(WebKitWebViewBase*);

GVariant* webkitWebViewBaseContentsOfUserInterfaceItem(WebKitWebViewBase*, const char* userInterfaceItem);

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
void webkitWebViewBaseSynthesizeCompositionKeyPress(WebKitWebViewBase*, const String& text, std::optional<Vector<WebCore::CompositionUnderline>>&&, std::optional<WebKit::EditingRange>&&);
void webkitWebViewBaseSynthesizeWheelEvent(WebKitWebViewBase*, const GdkEvent*, double deltaX, double deltaY, int x, int y, WheelEventPhase, WheelEventPhase momentumPhase, bool hasPreciseDeltas);

void webkitWebViewBaseMakeBlank(WebKitWebViewBase*, bool);
void webkitWebViewBasePageGrabbedTouch(WebKitWebViewBase*);
void webkitWebViewBaseSetShouldNotifyFocusEvents(WebKitWebViewBase*, bool);
