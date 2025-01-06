/*
 * Copyright (C) 2023-2024 Apple Inc. All rights reserved.
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

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "config.h"
#import "WebExtensionContext.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "WKFrameInfoPrivate.h"
#import "WKWebViewPrivate.h"
#import "WebExtensionFrameIdentifier.h"
#import "WebExtensionFrameParameters.h"
#import "WebExtensionTab.h"
#import "WebExtensionUtilities.h"
#import "WebFrame.h"
#import "_WKFrameTreeNode.h"
#import <wtf/BlockPtr.h>

namespace WebKit {

static WebExtensionFrameParameters frameParametersForFrame(_WKFrameTreeNode *frame, _WKFrameTreeNode *parentFrame, WebExtensionTab* tab, WebExtensionContext* extensionContext, bool includeFrameIdentifier)
{
    auto *frameInfo = frame.info;
    auto frameURL = URL { frameInfo.request.URL };

    return {
        .errorOccurred = static_cast<bool>(frameInfo._errorOccurred),
        .url = extensionContext->hasPermission(frameURL, tab) ? std::optional { frameURL } : std::nullopt,
        .parentFrameIdentifier = parentFrame ? toWebExtensionFrameIdentifier(parentFrame.info) : WebExtensionFrameConstants::NoneIdentifier,
        .frameIdentifier = includeFrameIdentifier ? std::optional { toWebExtensionFrameIdentifier(frameInfo) } : std::nullopt,
        .documentIdentifier = WTF::UUID::fromNSUUID(frameInfo._documentIdentifier)
    };
}

bool WebExtensionContext::isWebNavigationMessageAllowed()
{
    return isLoaded() && hasPermission(WKWebExtensionPermissionWebNavigation);
}

void WebExtensionContext::webNavigationTraverseFrameTreeForFrame(_WKFrameTreeNode *frame, _WKFrameTreeNode *parentFrame, WebExtensionTab* tab, Vector<WebExtensionFrameParameters> &frames)
{
    frames.append(frameParametersForFrame(frame, parentFrame, tab, this, true));

    for (_WKFrameTreeNode *childFrame in frame.childFrames)
        webNavigationTraverseFrameTreeForFrame(childFrame, frame, tab, frames);
}

std::optional<WebExtensionFrameParameters> WebExtensionContext::webNavigationFindFrameIdentifierInFrameTree(_WKFrameTreeNode *frame, _WKFrameTreeNode *parentFrame, WebExtensionTab* tab, WebExtensionFrameIdentifier targetFrameIdentifier)
{
    if (toWebExtensionFrameIdentifier(frame.info) == targetFrameIdentifier)
        return frameParametersForFrame(frame, parentFrame, tab, this, false);

    for (_WKFrameTreeNode *childFrame in frame.childFrames) {
        if (auto matchingChildFrame = webNavigationFindFrameIdentifierInFrameTree(childFrame, frame, tab, targetFrameIdentifier))
            return matchingChildFrame;
    }

    return std::nullopt;
}

void WebExtensionContext::webNavigationGetFrame(WebExtensionTabIdentifier tabIdentifier, WebExtensionFrameIdentifier frameIdentifier, CompletionHandler<void(Expected<std::optional<WebExtensionFrameParameters>, WebExtensionError>&&)>&& completionHandler)
{
    RefPtr tab = getTab(tabIdentifier);
    if (!tab) {
        completionHandler(toWebExtensionError(@"webNavigation.getFrame()", nil, @"tab not found"));
        return;
    }

    auto *webView = tab->webView();
    if (!webView) {
        completionHandler(toWebExtensionError(@"webNavigation.getFrame()", nil, @"tab not found"));
        return;
    }

    [webView _frames:makeBlockPtr([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler), tab, frameIdentifier](_WKFrameTreeNode *mainFrame) mutable {
        if (!mainFrame.info.isMainFrame) {
            RELEASE_LOG_INFO(Extensions, "Skipping frame traversal because the mainFrame is nil");
            completionHandler(toWebExtensionError(@"webNavigation.getFrame()", nil, @"main frame not found"));
            return;
        }

        if (auto frameParameters = webNavigationFindFrameIdentifierInFrameTree(mainFrame, nil, tab.get(), frameIdentifier))
            completionHandler(WTFMove(frameParameters));
        else
            completionHandler(toWebExtensionError(@"webNavigation.getFrame()", nil, @"frame not found"));
    }).get()];
}

void WebExtensionContext::webNavigationGetAllFrames(WebExtensionTabIdentifier tabIdentifier, CompletionHandler<void(Expected<Vector<WebExtensionFrameParameters>, WebExtensionError>&&)>&& completionHandler)
{
    RefPtr tab = getTab(tabIdentifier);
    if (!tab) {
        completionHandler(toWebExtensionError(@"webNavigation.getAllFrames()", nil, @"tab not found"));
        return;
    }

    auto *webView = tab->webView();
    if (!webView) {
        completionHandler(toWebExtensionError(@"webNavigation.getAllFrames()", nil, @"tab not found"));
        return;
    }

    [webView _frames:makeBlockPtr([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler), tab](_WKFrameTreeNode *mainFrame) mutable {
        if (!mainFrame.info.isMainFrame) {
            RELEASE_LOG_INFO(Extensions, "Skipping frame traversal because the mainFrame is nil");
            completionHandler(toWebExtensionError(@"webNavigation.getAllFrames()", nil, @"main frame not found"));
            return;
        }

        Vector<WebExtensionFrameParameters> frameParameters;
        webNavigationTraverseFrameTreeForFrame(mainFrame, nil, tab.get(), frameParameters);

        completionHandler(WTFMove(frameParameters));
    }).get()];
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
