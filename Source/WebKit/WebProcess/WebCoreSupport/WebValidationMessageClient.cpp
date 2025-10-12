/*
 * Copyright (C) 2016-2025 Apple Inc. All rights reserved.
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
#include "WebValidationMessageClient.h"

#include "MessageSenderInlines.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include <WebCore/Element.h>
#include <WebCore/LocalFrame.h>
#include <WebCore/NodeDocument.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebValidationMessageClient);

WebValidationMessageClient::WebValidationMessageClient(WebPage& page)
    : m_page(page)
{
}

WebValidationMessageClient::~WebValidationMessageClient()
{
    if (RefPtr anchor = m_currentAnchor.get())
        hideValidationMessage(*anchor);
}

void WebValidationMessageClient::documentDetached(Document& document)
{
    RefPtr anchor = m_currentAnchor.get();
    if (!anchor)
        return;
    if (&anchor->document() == &document)
        hideValidationMessage(*anchor);
}

void WebValidationMessageClient::showValidationMessage(const Element& anchor, String&& message)
{
    if (RefPtr currentAnchor = m_currentAnchor.get())
        hideValidationMessage(*currentAnchor);

    m_currentAnchor = anchor;
    m_currentAnchorRect = anchor.boundingBoxInRootViewCoordinates();
    Ref { *m_page }->send(Messages::WebPageProxy::ShowValidationMessage(m_currentAnchorRect, WTFMove(message)));
}

void WebValidationMessageClient::hideValidationMessage(const Element& anchor)
{
    RefPtr page = m_page.get();
    if (!isValidationMessageVisible(anchor) || !page)
        return;

    m_currentAnchor = nullptr;
    m_currentAnchorRect = { };
    page->send(Messages::WebPageProxy::HideValidationMessage());
}

void WebValidationMessageClient::hideAnyValidationMessage()
{
    RefPtr page = m_page.get();
    if (!m_currentAnchor || !page)
        return;

    m_currentAnchor = nullptr;
    m_currentAnchorRect = { };
    page->send(Messages::WebPageProxy::HideValidationMessage());
}

bool WebValidationMessageClient::isValidationMessageVisible(const Element& anchor)
{
    return m_currentAnchor == &anchor;
}

void WebValidationMessageClient::updateValidationBubbleStateIfNeeded()
{
    RefPtr anchor = m_currentAnchor.get();
    if (!anchor)
        return;

    // We currently hide the validation bubble if its position is outdated instead of trying
    // to update its position.
    if (m_currentAnchorRect != anchor->boundingBoxInRootViewCoordinates())
        hideValidationMessage(*anchor);
}

} // namespace WebKit
