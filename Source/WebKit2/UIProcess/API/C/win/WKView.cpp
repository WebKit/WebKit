/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WKView.h"

#include "WKAPICast.h"
#include "WebView.h"

using namespace WebKit;

WKTypeID WKViewGetTypeID()
{
    return toAPI(APIObject::TypeView);
}

WKViewRef WKViewCreate(RECT rect, WKContextRef contextRef, WKPageGroupRef pageGroupRef, HWND parentWindow)
{
    RefPtr<WebView> view = WebView::create(rect, toImpl(contextRef), toImpl(pageGroupRef), parentWindow);
    return toAPI(view.release().releaseRef());
}

HWND WKViewGetWindow(WKViewRef viewRef)
{
    return toImpl(viewRef)->window();
}

WKPageRef WKViewGetPage(WKViewRef viewRef)
{
    return toAPI(toImpl(viewRef)->page());
}

void WKViewSetParentWindow(WKViewRef viewRef, HWND hostWindow)
{
    toImpl(viewRef)->setParentWindow(hostWindow);
}

void WKViewWindowAncestryDidChange(WKViewRef viewRef)
{
    toImpl(viewRef)->windowAncestryDidChange();
}

void WKViewSetIsInWindow(WKViewRef viewRef, bool isInWindow)
{
    toImpl(viewRef)->setIsInWindow(isInWindow);
}

void WKViewSetInitialFocus(WKViewRef viewRef, bool forward)
{
    toImpl(viewRef)->setInitialFocus(forward);
}

void WKViewSetScrollOffsetOnNextResize(WKViewRef viewRef, WKSize scrollOffset)
{
    toImpl(viewRef)->setScrollOffsetOnNextResize(toIntSize(scrollOffset));
}

void WKViewSetFindIndicatorCallback(WKViewRef viewRef, WKViewFindIndicatorCallback callback, void* context)
{
    toImpl(viewRef)->setFindIndicatorCallback(callback, context);
}

WKViewFindIndicatorCallback WKViewGetFindIndicatorCallback(WKViewRef viewRef, void** context)
{    
    return toImpl(viewRef)->getFindIndicatorCallback(context);
}

void WKViewSetViewUndoClient(WKViewRef viewRef, const WKViewUndoClient* wkClient)
{
    if (wkClient && wkClient->version)
        return;
    toImpl(viewRef)->initializeUndoClient(wkClient);
}

void WKViewReapplyEditCommand(WKViewRef viewRef, WKEditCommandRef command)
{
    toImpl(viewRef)->reapplyEditCommand(toImpl(command));
}

void WKViewUnapplyEditCommand(WKViewRef viewRef, WKEditCommandRef command)
{
    toImpl(viewRef)->unapplyEditCommand(toImpl(command));
}
