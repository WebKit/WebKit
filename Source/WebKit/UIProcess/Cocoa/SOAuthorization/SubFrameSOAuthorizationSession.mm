/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#import "config.h"
#import "SubFrameSOAuthorizationSession.h"

#if HAVE(APP_SSO)

#import "APIFrameHandle.h"
#import "APINavigationAction.h"
#import "DataReference.h"
#import "WebPageProxy.h"
#import "WebProcessProxy.h"
#import <WebCore/ResourceResponse.h>
#import <wtf/RunLoop.h>

namespace WebKit {
using namespace WebCore;

const char* soAuthorizationPostDidStartMessageToParent = "<script>parent.postMessage('SOAuthorizationDidStart', '*');</script>";
const char* soAuthorizationPostDidCancelMessageToParent = "parent.postMessage('SOAuthorizationDidCancel', '*');";

Ref<SOAuthorizationSession> SubFrameSOAuthorizationSession::create(SOAuthorization *soAuthorization, Ref<API::NavigationAction>&& navigationAction, WebPageProxy& page, Callback&& completionHandler)
{
    return adoptRef(*new SubFrameSOAuthorizationSession(soAuthorization, WTFMove(navigationAction), page, WTFMove(completionHandler)));
}

SubFrameSOAuthorizationSession::SubFrameSOAuthorizationSession(SOAuthorization *soAuthorization, Ref<API::NavigationAction>&& navigationAction, WebPageProxy& page, Callback&& completionHandler)
    : NavigationSOAuthorizationSession(soAuthorization, WTFMove(navigationAction), page, InitiatingAction::SubFrame, WTFMove(completionHandler))
{
}

void SubFrameSOAuthorizationSession::fallBackToWebPathInternal()
{
    // Instead of issuing a load, we execute the Javascript directly. This provides us a callback
    // to ensure the final load is issued after the message is posted.
    postDidCancelMessageToParent([weakThis = makeWeakPtr(*this)] {
        if (!weakThis)
            return;
        auto* page = weakThis->page();
        auto* navigationActionPtr = weakThis->navigationAction();
        if (!page || !navigationActionPtr)
            return;

        if (auto* targetFrame = navigationActionPtr->targetFrame()) {
            if (auto* frame = page->process().webFrame(targetFrame->handle().frameID())) {
                page->setShouldSuppressSOAuthorizationInNextNavigationPolicyDecision();
                // Issue a new load to the original URL as the original load is aborted before start.
                frame->loadURL(navigationActionPtr->request().url(), navigationActionPtr->request().httpReferrer());
            }
        }
    });
}

void SubFrameSOAuthorizationSession::abortInternal()
{
}

void SubFrameSOAuthorizationSession::completeInternal(const WebCore::ResourceResponse& response, NSData *data)
{
    if (response.httpStatusCode() != 200) {
        fallBackToWebPathInternal();
        return;
    }
    loadDataToFrame(IPC::DataReference(reinterpret_cast<const uint8_t*>(data.bytes), data.length), response.url());
}

void SubFrameSOAuthorizationSession::beforeStart()
{
    // Cancelled the current load before loading the data to post SOAuthorizationDidStart to the parent frame.
    invokeCallback(true);
    // Currently in the middle of decidePolicyForNavigationAction, should start a new load after.
    RunLoop::main().dispatch([weakThis = makeWeakPtr(*this)] {
        if (!weakThis || !weakThis->page() || !weakThis->navigationAction())
            return;
        // Instead of executing the Javascript directly, issuing a load. This will set the origin properly.
        weakThis->loadDataToFrame(IPC::DataReference(reinterpret_cast<const uint8_t*>(soAuthorizationPostDidStartMessageToParent), strlen(soAuthorizationPostDidStartMessageToParent)), weakThis->navigationAction()->request().url());
    });
}

void SubFrameSOAuthorizationSession::loadDataToFrame(const IPC::DataReference& data, const URL& baseURL)
{
    auto* page = this->page();
    auto* navigationActionPtr = navigationAction();
    if (!page || !navigationActionPtr)
        return;

    if (auto* targetFrame = navigationActionPtr->targetFrame()) {
        if (auto* frame = page->process().webFrame(targetFrame->handle().frameID())) {
            page->setShouldSuppressSOAuthorizationInNextNavigationPolicyDecision();
            frame->loadData(data, "text/html", "UTF-8", baseURL);
        }
    }
}

void SubFrameSOAuthorizationSession::postDidCancelMessageToParent(Function<void()>&& callback)
{
    auto* page = this->page();
    auto* navigationActionPtr = navigationAction();
    if (!page || !navigationActionPtr)
        return;

    if (auto* targetFrame = navigationActionPtr->targetFrame()) {
        page->runJavaScriptInFrame(targetFrame->handle().frameID(), soAuthorizationPostDidCancelMessageToParent, false, [callback = WTFMove(callback)] (API::SerializedScriptValue*, bool, const ExceptionDetails&, ScriptValueCallback::Error) {
            callback();
        });
    }
}

} // namespace WebKit

#endif
