/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#import "WebValidationMessageClient.h"

#import "WebView.h"
#import "WebViewInternal.h"
#import <WebCore/Element.h>

using namespace WebCore;

WebValidationMessageClient::WebValidationMessageClient(WebView* view)
    : m_view(view)
{
}

WebValidationMessageClient::~WebValidationMessageClient()
{
    if (m_currentAnchor)
        hideValidationMessage(*m_currentAnchor);
}

void WebValidationMessageClient::documentDetached(Document& document)
{
    if (!m_currentAnchor)
        return;
    if (&m_currentAnchor->document() == &document)
        hideValidationMessage(*m_currentAnchor);
}

void WebValidationMessageClient::showValidationMessage(const Element& anchor, const String& message)
{
    if (m_currentAnchor)
        hideValidationMessage(*m_currentAnchor);

    m_currentAnchor = &anchor;
    m_currentAnchorRect = anchor.clientRect();
    [m_view showFormValidationMessage:message withAnchorRect:m_currentAnchorRect];
}

void WebValidationMessageClient::hideValidationMessage(const Element& anchor)
{
    if (!isValidationMessageVisible(anchor))
        return;

    m_currentAnchor = nullptr;
    m_currentAnchorRect = { };
    [m_view hideFormValidationMessage];
}

void WebValidationMessageClient::hideAnyValidationMessage()
{
    if (!m_currentAnchor)
        return;

    m_currentAnchor = nullptr;
    m_currentAnchorRect = { };
    [m_view hideFormValidationMessage];
}

bool WebValidationMessageClient::isValidationMessageVisible(const Element& anchor)
{
    return m_currentAnchor == &anchor;
}

void WebValidationMessageClient::updateValidationBubbleStateIfNeeded()
{
    if (!m_currentAnchor)
        return;

    // We currently hide the validation bubble if its position is outdated instead of trying
    // to update its position.
    if (m_currentAnchorRect != m_currentAnchor->clientRect())
        hideValidationMessage(*m_currentAnchor);
}
