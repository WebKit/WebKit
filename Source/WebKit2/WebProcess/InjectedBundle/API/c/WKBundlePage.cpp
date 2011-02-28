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
#include "WKBundlePage.h"
#include "WKBundlePagePrivate.h"

#include "InjectedBundleBackForwardList.h"
#include "WKAPICast.h"
#include "WKBundleAPICast.h"
#include "WebImage.h"
#include "WebPage.h"
#include "WebURL.h"
#include "WebURLRequest.h"

#include <WebCore/KURL.h>

using namespace WebKit;

WKTypeID WKBundlePageGetTypeID()
{
    return toAPI(WebPage::APIType);
}

void WKBundlePageSetContextMenuClient(WKBundlePageRef pageRef, WKBundlePageContextMenuClient* wkClient)
{
    if (wkClient && wkClient->version)
        return;
    toImpl(pageRef)->initializeInjectedBundleContextMenuClient(wkClient);
}

void WKBundlePageSetEditorClient(WKBundlePageRef pageRef, WKBundlePageEditorClient* wkClient)
{
    if (wkClient && wkClient->version)
        return;
    toImpl(pageRef)->initializeInjectedBundleEditorClient(wkClient);
}

void WKBundlePageSetFormClient(WKBundlePageRef pageRef, WKBundlePageFormClient* wkClient)
{
    if (wkClient && wkClient->version)
        return;
    toImpl(pageRef)->initializeInjectedBundleFormClient(wkClient);
}

void WKBundlePageSetPageLoaderClient(WKBundlePageRef pageRef, WKBundlePageLoaderClient* wkClient)
{
    if (wkClient && wkClient->version)
        return;
    toImpl(pageRef)->initializeInjectedBundleLoaderClient(wkClient);
}

void WKBundlePageSetResourceLoadClient(WKBundlePageRef pageRef, WKBundlePageResourceLoadClient* wkClient)
{
    if (wkClient && wkClient->version)
        return;
    toImpl(pageRef)->initializeInjectedBundleResourceLoadClient(wkClient);
}

void WKBundlePageSetPolicyClient(WKBundlePageRef pageRef, WKBundlePagePolicyClient* wkClient)
{
    if (wkClient && wkClient->version)
        return;
    toImpl(pageRef)->initializeInjectedBundlePolicyClient(wkClient);
}

void WKBundlePageSetUIClient(WKBundlePageRef pageRef, WKBundlePageUIClient* wkClient)
{
    if (wkClient && wkClient->version)
        return;
    toImpl(pageRef)->initializeInjectedBundleUIClient(wkClient);
}

WKBundlePageGroupRef WKBundlePageGetPageGroup(WKBundlePageRef pageRef)
{
    return toAPI(toImpl(pageRef)->pageGroup());
}

WKBundleFrameRef WKBundlePageGetMainFrame(WKBundlePageRef pageRef)
{
    return toAPI(toImpl(pageRef)->mainFrame());
}

void WKBundlePageStopLoading(WKBundlePageRef pageRef)
{
    toImpl(pageRef)->stopLoading();
}

void WKBundlePageSetDefersLoading(WKBundlePageRef pageRef, bool defersLoading)
{
    toImpl(pageRef)->setDefersLoading(defersLoading);
}

WKStringRef WKBundlePageCopyRenderTreeExternalRepresentation(WKBundlePageRef pageRef)
{
    return toCopiedAPI(toImpl(pageRef)->renderTreeExternalRepresentation());
}

void WKBundlePageExecuteEditingCommand(WKBundlePageRef pageRef, WKStringRef name, WKStringRef argument)
{
    toImpl(pageRef)->executeEditingCommand(toImpl(name)->string(), toImpl(argument)->string());
}

bool WKBundlePageIsEditingCommandEnabled(WKBundlePageRef pageRef, WKStringRef name)
{
    return toImpl(pageRef)->isEditingCommandEnabled(toImpl(name)->string());
}

void WKBundlePageClearMainFrameName(WKBundlePageRef pageRef)
{
    toImpl(pageRef)->clearMainFrameName();
}

void WKBundlePageClose(WKBundlePageRef pageRef)
{
    toImpl(pageRef)->sendClose();
}

double WKBundlePageGetTextZoomFactor(WKBundlePageRef pageRef)
{
    return toImpl(pageRef)->textZoomFactor();
}

void WKBundlePageSetTextZoomFactor(WKBundlePageRef pageRef, double zoomFactor)
{
    toImpl(pageRef)->setTextZoomFactor(zoomFactor);
}

double WKBundlePageGetPageZoomFactor(WKBundlePageRef pageRef)
{
    return toImpl(pageRef)->pageZoomFactor();
}

void WKBundlePageSetPageZoomFactor(WKBundlePageRef pageRef, double zoomFactor)
{
    toImpl(pageRef)->setPageZoomFactor(zoomFactor);
}

WKBundleBackForwardListRef WKBundlePageGetBackForwardList(WKBundlePageRef pageRef)
{
    return toAPI(toImpl(pageRef)->backForwardList());
}

void WKBundlePageInstallPageOverlay(WKBundlePageRef pageRef, WKBundlePageOverlayRef pageOverlayRef)
{
    toImpl(pageRef)->installPageOverlay(toImpl(pageOverlayRef));
}

void WKBundlePageUninstallPageOverlay(WKBundlePageRef pageRef, WKBundlePageOverlayRef pageOverlayRef)
{
    toImpl(pageRef)->uninstallPageOverlay(toImpl(pageOverlayRef));
}

bool WKBundlePageHasLocalDataForURL(WKBundlePageRef pageRef, WKURLRef urlRef)
{
    return toImpl(pageRef)->hasLocalDataForURL(WebCore::KURL(WebCore::KURL(), toImpl(urlRef)->string()));
}

bool WKBundlePageCanHandleRequest(WKURLRequestRef requestRef)
{
    return WebPage::canHandleRequest(toImpl(requestRef)->resourceRequest());
}

bool WKBundlePageFindString(WKBundlePageRef pageRef, WKStringRef target, WKFindOptions findOptions)
{
    return toImpl(pageRef)->findStringFromInjectedBundle(toImpl(target)->string(), toFindOptions(findOptions));
}

WKImageRef WKBundlePageCreateSnapshotInViewCoordinates(WKBundlePageRef pageRef, WKRect rect, WKImageOptions options)
{
    RefPtr<WebImage> webImage = toImpl(pageRef)->snapshotInViewCoordinates(toIntRect(rect), toImageOptions(options));
    return toAPI(webImage.release().leakRef());
}

WKImageRef WKBundlePageCreateSnapshotInDocumentCoordinates(WKBundlePageRef pageRef, WKRect rect, WKImageOptions options)
{
    RefPtr<WebImage> webImage = toImpl(pageRef)->snapshotInDocumentCoordinates(toIntRect(rect), toImageOptions(options));
    return toAPI(webImage.release().leakRef());
}

WKImageRef WKBundlePageCreateScaledSnapshotInDocumentCoordinates(WKBundlePageRef pageRef, WKRect rect, double scaleFactor, WKImageOptions options)
{
    RefPtr<WebImage> webImage = toImpl(pageRef)->scaledSnapshotInDocumentCoordinates(toIntRect(rect), scaleFactor, toImageOptions(options));
    return toAPI(webImage.release().leakRef());
}

#if defined(ENABLE_INSPECTOR) && ENABLE_INSPECTOR
WKBundleInspectorRef WKBundlePageGetInspector(WKBundlePageRef pageRef)
{
    return toAPI(toImpl(pageRef)->inspector());
}
#endif

void WKBundlePageForceRepaint(WKBundlePageRef page)
{
    toImpl(page)->forceRepaintWithoutCallback();
}
