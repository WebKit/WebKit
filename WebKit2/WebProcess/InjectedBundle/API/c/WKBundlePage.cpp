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

#include "WKBundlePage.h"
#include "WKBundlePagePrivate.h"

#include "WKAPICast.h"
#include "WKBundleAPICast.h"
#include "WebPage.h"

using namespace WebKit;

WKTypeID WKBundlePageGetTypeID()
{
    return toRef(WebPage::APIType);
}

void WKBundlePageSetEditorClient(WKBundlePageRef pageRef, WKBundlePageEditorClient* wkClient)
{
    if (wkClient && wkClient->version)
        return;
    toWK(pageRef)->initializeInjectedBundleEditorClient(wkClient);
}

void WKBundlePageSetFormClient(WKBundlePageRef pageRef, WKBundlePageFormClient* wkClient)
{
    if (wkClient && wkClient->version)
        return;
    toWK(pageRef)->initializeInjectedBundleFormClient(wkClient);
}

void WKBundlePageSetLoaderClient(WKBundlePageRef pageRef, WKBundlePageLoaderClient* wkClient)
{
    if (wkClient && wkClient->version)
        return;
    toWK(pageRef)->initializeInjectedBundleLoaderClient(wkClient);
}

void WKBundlePageSetUIClient(WKBundlePageRef pageRef, WKBundlePageUIClient* wkClient)
{
    if (wkClient && wkClient->version)
        return;
    toWK(pageRef)->initializeInjectedBundleUIClient(wkClient);
}

WKBundleFrameRef WKBundlePageGetMainFrame(WKBundlePageRef pageRef)
{
    return toRef(toWK(pageRef)->mainFrame());
}

void WKBundlePageStopLoading(WKBundlePageRef pageRef)
{
    toWK(pageRef)->stopLoading();
}

WKStringRef WKBundlePageCopyRenderTreeExternalRepresentation(WKBundlePageRef pageRef)
{
    return toCopiedRef(toWK(pageRef)->renderTreeExternalRepresentation());
}

void WKBundlePageExecuteEditingCommand(WKBundlePageRef pageRef, WKStringRef name, WKStringRef argument)
{
    toWK(pageRef)->executeEditingCommand(toWK(name)->string(), toWK(argument)->string());
}

bool WKBundlePageIsEditingCommandEnabled(WKBundlePageRef pageRef, WKStringRef name)
{
    return toWK(pageRef)->isEditingCommandEnabled(toWK(name)->string());
}

void WKBundlePageClearMainFrameName(WKBundlePageRef pageRef)
{
    toWK(pageRef)->clearMainFrameName();
}

void WKBundlePageClose(WKBundlePageRef pageRef)
{
    toWK(pageRef)->sendClose();
}

float WKBundlePageGetTextZoomFactor(WKBundlePageRef pageRef)
{
    return toWK(pageRef)->textZoomFactor();
}

void WKBundlePageSetTextZoomFactor(WKBundlePageRef pageRef, float zoomFactor)
{
    toWK(pageRef)->setTextZoomFactor(zoomFactor);
}

float WKBundlePageGetPageZoomFactor(WKBundlePageRef pageRef)
{
    return toWK(pageRef)->pageZoomFactor();
}

void WKBundlePageSetPageZoomFactor(WKBundlePageRef pageRef, float zoomFactor)
{
    toWK(pageRef)->setPageZoomFactor(zoomFactor);
}
