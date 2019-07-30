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

#include "config.h"
#include "NavigationSOAuthorizationSession.h"

#if HAVE(APP_SSO)

#import "WebPageProxy.h"
#import <WebCore/ResourceResponse.h>

namespace WebKit {

NavigationSOAuthorizationSession::NavigationSOAuthorizationSession(SOAuthorization *soAuthorization, Ref<API::NavigationAction>&& navigationAction, WebPageProxy& page, InitiatingAction action, Callback&& completionHandler)
    : SOAuthorizationSession(soAuthorization, WTFMove(navigationAction), page, action)
    , m_callback(WTFMove(completionHandler))
{
}

NavigationSOAuthorizationSession::~NavigationSOAuthorizationSession()
{
    if (m_callback)
        m_callback(true);
    if (state() == State::Waiting && page())
        page()->removeObserver(*this);
}

void NavigationSOAuthorizationSession::shouldStartInternal()
{
    auto* page = this->page();
    ASSERT(page);
    beforeStart();
    if (!page->isInWindow()) {
        setState(State::Waiting);
        page->addObserver(*this);
        ASSERT(page->mainFrame());
        m_waitingPageActiveURL = page->pageLoadState().activeURL();
        return;
    }
    start();
}

void NavigationSOAuthorizationSession::webViewDidMoveToWindow()
{
    auto* page = this->page();
    if (state() != State::Waiting || !page || !page->isInWindow())
        return;
    if (pageActiveURLDidChangeDuringWaiting()) {
        abort();
        page->removeObserver(*this);
        return;
    }
    start();
    page->removeObserver(*this);
}

bool NavigationSOAuthorizationSession::pageActiveURLDidChangeDuringWaiting() const
{
    auto* page = this->page();
    return !page || page->pageLoadState().activeURL() != m_waitingPageActiveURL;
}

} // namespace WebKit

#endif
