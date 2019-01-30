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
#include "DragAndDropHandler.h"
#include "GestureController.h"
#include "WebContextMenuProxyGtk.h"
#include "WebInspectorProxy.h"
#include "WebKitWebViewBase.h"
#include "WebPageProxy.h"

WebKitWebViewBase* webkitWebViewBaseCreate(const API::PageConfiguration&);
GtkIMContext* webkitWebViewBaseGetIMContext(WebKitWebViewBase*);
WebKit::WebPageProxy* webkitWebViewBaseGetPage(WebKitWebViewBase*);
void webkitWebViewBaseCreateWebPage(WebKitWebViewBase*, Ref<API::PageConfiguration>&&);
void webkitWebViewBaseSetTooltipText(WebKitWebViewBase*, const char*);
void webkitWebViewBaseSetTooltipArea(WebKitWebViewBase*, const WebCore::IntRect&);
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
void webkitWebViewBaseSetInputMethodState(WebKitWebViewBase*, bool enabled);
void webkitWebViewBaseUpdateTextInputState(WebKitWebViewBase*);
void webkitWebViewBaseSetContentsSize(WebKitWebViewBase*, const WebCore::IntSize&);

void webkitWebViewBaseSetFocus(WebKitWebViewBase*, bool focused);
bool webkitWebViewBaseIsInWindowActive(WebKitWebViewBase*);
bool webkitWebViewBaseIsFocused(WebKitWebViewBase*);
bool webkitWebViewBaseIsVisible(WebKitWebViewBase*);
bool webkitWebViewBaseIsInWindow(WebKitWebViewBase*);

void webkitWebViewBaseAddDialog(WebKitWebViewBase*, GtkWidget*);
void webkitWebViewBaseAddWebInspector(WebKitWebViewBase*, GtkWidget* inspector, WebKit::AttachmentSide);
void webkitWebViewBaseResetClickCounter(WebKitWebViewBase*);
void webkitWebViewBaseEnterAcceleratedCompositingMode(WebKitWebViewBase*, const WebKit::LayerTreeContext&);
void webkitWebViewBaseUpdateAcceleratedCompositingMode(WebKitWebViewBase*, const WebKit::LayerTreeContext&);
void webkitWebViewBaseExitAcceleratedCompositingMode(WebKitWebViewBase*);
bool webkitWebViewBaseMakeGLContextCurrent(WebKitWebViewBase*);
void webkitWebViewBaseDidRelaunchWebProcess(WebKitWebViewBase*);
void webkitWebViewBasePageClosed(WebKitWebViewBase*);

#if ENABLE(DRAG_SUPPORT)
WebKit::DragAndDropHandler& webkitWebViewBaseDragAndDropHandler(WebKitWebViewBase*);
#endif

#if HAVE(GTK_GESTURES)
WebKit::GestureController& webkitWebViewBaseGestureController(WebKitWebViewBase*);
#endif
