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

#pragma once

#include "WKSharedAPICast.h"
#include "WKBundlePage.h"
#include "WKBundlePagePrivate.h"
#include "WKBundlePrivate.h"
#include <WebCore/EditorInsertAction.h>
#include <WebCore/TextAffinity.h>

namespace WebKit {

class InjectedBundle;
class InjectedBundleBackForwardList;
class InjectedBundleBackForwardListItem;
class InjectedBundleCSSStyleDeclarationHandle;
class InjectedBundleDOMWindowExtension;
class InjectedBundleHitTestResult;
class InjectedBundleNavigationAction;
class InjectedBundleNodeHandle;
class InjectedBundleRangeHandle;
class InjectedBundleScriptWorld;
class PageBanner;
class WebFrame;
class WebInspector;
class WebPage;
class WebPageGroupProxy;
class WebPageOverlay;

WK_ADD_API_MAPPING(WKBundleBackForwardListItemRef, InjectedBundleBackForwardListItem)
WK_ADD_API_MAPPING(WKBundleBackForwardListRef, InjectedBundleBackForwardList)
WK_ADD_API_MAPPING(WKBundleCSSStyleDeclarationRef, InjectedBundleCSSStyleDeclarationHandle)
WK_ADD_API_MAPPING(WKBundleDOMWindowExtensionRef, InjectedBundleDOMWindowExtension)
WK_ADD_API_MAPPING(WKBundleFrameRef, WebFrame)
WK_ADD_API_MAPPING(WKBundleHitTestResultRef, InjectedBundleHitTestResult)
WK_ADD_API_MAPPING(WKBundleInspectorRef, WebInspector)
WK_ADD_API_MAPPING(WKBundleNavigationActionRef, InjectedBundleNavigationAction)
WK_ADD_API_MAPPING(WKBundleNodeHandleRef, InjectedBundleNodeHandle)
WK_ADD_API_MAPPING(WKBundlePageBannerRef, PageBanner)
WK_ADD_API_MAPPING(WKBundlePageOverlayRef, WebPageOverlay)
WK_ADD_API_MAPPING(WKBundlePageRef, WebPage)
WK_ADD_API_MAPPING(WKBundleRangeHandleRef, InjectedBundleRangeHandle)
WK_ADD_API_MAPPING(WKBundleRef, InjectedBundle)
WK_ADD_API_MAPPING(WKBundleScriptWorldRef, InjectedBundleScriptWorld)

inline WKInsertActionType toAPI(WebCore::EditorInsertAction action)
{
    switch (action) {
    case WebCore::EditorInsertAction::Typed:
        return kWKInsertActionTyped;
    case WebCore::EditorInsertAction::Pasted:
        return kWKInsertActionPasted;
    case WebCore::EditorInsertAction::Dropped:
        return kWKInsertActionDropped;
    }
    ASSERT_NOT_REACHED();
    return kWKInsertActionTyped;
}

inline WKAffinityType toAPI(WebCore::Affinity affinity)
{
    switch (affinity) {
    case WebCore::Affinity::Upstream:
        return kWKAffinityUpstream;
    case WebCore::Affinity::Downstream:
        return kWKAffinityDownstream;
    }
    ASSERT_NOT_REACHED();
    return kWKAffinityUpstream;
}

} // namespace WebKit
