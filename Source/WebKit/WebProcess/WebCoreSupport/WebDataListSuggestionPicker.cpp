/*
 * Copyright (C) 2018-2020 Apple Inc. All rights reserved.
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
#include "WebDataListSuggestionPicker.h"

#if ENABLE(DATALIST_ELEMENT)

#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include "WebProcess.h"
#include <WebCore/DataListSuggestionsClient.h>
#include <WebCore/LocalFrameView.h>

namespace WebKit {

WebDataListSuggestionPicker::WebDataListSuggestionPicker(WebPage& page, WebCore::DataListSuggestionsClient& client)
    : m_client(client)
    , m_page(page)
{
}

WebDataListSuggestionPicker::~WebDataListSuggestionPicker() = default;

void WebDataListSuggestionPicker::handleKeydownWithIdentifier(const String& key)
{
    RefPtr page = m_page.get();
    if (!page)
        return;

    WebProcess::singleton().parentProcessConnection()->send(Messages::WebPageProxy::HandleKeydownInDataList(key), page->identifier());
}

void WebDataListSuggestionPicker::didSelectOption(const String& selectedOption)
{
    if (CheckedPtr client = m_client)
        client->didSelectDataListOption(selectedOption);
}

void WebDataListSuggestionPicker::didCloseSuggestions()
{
    if (CheckedPtr client = m_client)
        client->didCloseSuggestions();
}

void WebDataListSuggestionPicker::close()
{
    RefPtr page = m_page.get();
    if (!page)
        return;

    WebProcess::singleton().parentProcessConnection()->send(Messages::WebPageProxy::EndDataListSuggestions(), page->identifier());
}

void WebDataListSuggestionPicker::displayWithActivationType(WebCore::DataListSuggestionActivationType type)
{
    CheckedPtr client = m_client;
    if (!client)
        return;

    auto suggestions = client->suggestions();
    if (suggestions.isEmpty()) {
        close();
        return;
    }

    RefPtr page = m_page.get();
    if (!page)
        return;

    auto elementRectInRootViewCoordinates = client->elementRectInRootViewCoordinates();
    if (RefPtr view = page->localMainFrameView()) {
        auto unobscuredRootViewRect = view->contentsToRootView(view->unobscuredContentRect());
        if (!unobscuredRootViewRect.intersects(elementRectInRootViewCoordinates))
            return close();
    }

    page->setActiveDataListSuggestionPicker(*this);

    WebCore::DataListSuggestionInformation info { type, WTFMove(suggestions), WTFMove(elementRectInRootViewCoordinates) };
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebPageProxy::ShowDataListSuggestions(info), page->identifier());
}

void WebDataListSuggestionPicker::detach()
{
    m_client = nullptr;
}

} // namespace WebKit

#endif
