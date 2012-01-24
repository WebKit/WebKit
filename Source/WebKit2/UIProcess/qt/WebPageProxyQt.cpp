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
#include "WebPageProxy.h"

#include "PageClient.h"
#include "QtNetworkReplyData.h"
#include "QtPageClient.h"
#include "qquicknetworkreply_p.h"
#include "WebPageMessages.h"
#include "WebProcessProxy.h"
#include <WebCore/Editor.h>
#include <WebCore/NotImplemented.h>

using namespace WebCore;

namespace WebKit {

String WebPageProxy::standardUserAgent(const String& applicationNameForUserAgent)
{
    // FIXME: This should not be hard coded.
    return "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_6) AppleWebKit/531.4 (KHTML, like Gecko) Version/4.0.3 Safari/531.4";
}

void WebPageProxy::saveRecentSearches(const String&, const Vector<String>&)
{
    notImplemented();
}

void WebPageProxy::loadRecentSearches(const String&, Vector<String>&)
{
    notImplemented();
}

void WebPageProxy::setComposition(const String& text, Vector<CompositionUnderline> underlines, uint64_t selectionStart, uint64_t selectionEnd, uint64_t replacementRangeStart, uint64_t replacementRangeEnd)
{
    // FIXME: We need to find out how to proper handle the crashes case.
    if (!isValid())
        return;

    process()->send(Messages::WebPage::SetComposition(text, underlines, selectionStart, selectionEnd, replacementRangeStart, replacementRangeEnd), m_pageID);
}

void WebPageProxy::confirmComposition(const String& compositionString, int64_t selectionStart, int64_t selectionLength)
{
    if (!isValid())
        return;

    process()->send(Messages::WebPage::ConfirmComposition(compositionString, selectionStart, selectionLength), m_pageID);
}

void WebPageProxy::cancelComposition()
{
    if (!isValid())
        return;

    process()->send(Messages::WebPage::CancelComposition(), m_pageID);
}

void WebPageProxy::registerApplicationScheme(const String& scheme)
{
    process()->send(Messages::WebPage::RegisterApplicationScheme(scheme), m_pageID);
}

void WebPageProxy::resolveApplicationSchemeRequest(QtNetworkRequestData request)
{
    RefPtr<QtRefCountedNetworkRequestData> requestData = adoptRef(new QtRefCountedNetworkRequestData(request));
    m_applicationSchemeRequests.add(requestData);
    static_cast<QtPageClient*>(m_pageClient)->handleApplicationSchemeRequest(requestData);
}

void WebPageProxy::sendApplicationSchemeReply(const QQuickNetworkReply* reply)
{
    RefPtr<QtRefCountedNetworkRequestData> requestData = reply->networkRequestData();
    if (m_applicationSchemeRequests.contains(requestData)) {
        RefPtr<QtRefCountedNetworkReplyData> replyData = reply->networkReplyData();
        process()->send(Messages::WebPage::ApplicationSchemeReply(replyData->data()), pageID());
        m_applicationSchemeRequests.remove(requestData);
    }
}

} // namespace WebKit
