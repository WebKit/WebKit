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

#include "WebCoreArgumentCoders.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include "WebProcess.h"
#include <WebCore/DataListSuggestionsClient.h>

namespace WebKit {

WebDataListSuggestionPicker::WebDataListSuggestionPicker(WebPage& page, WebCore::DataListSuggestionsClient& client)
    : m_client(client)
    , m_page(page)
{
}

void WebDataListSuggestionPicker::handleKeydownWithIdentifier(const String& key)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebPageProxy::HandleKeydownInDataList(key), m_page.identifier());
}

void WebDataListSuggestionPicker::didSelectOption(const String& selectedOption)
{
    m_client.didSelectDataListOption(selectedOption);
}

void WebDataListSuggestionPicker::didCloseSuggestions()
{
    m_client.didCloseSuggestions();
}

void WebDataListSuggestionPicker::close()
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebPageProxy::EndDataListSuggestions(), m_page.identifier());
}

void WebDataListSuggestionPicker::displayWithActivationType(WebCore::DataListSuggestionActivationType type)
{
    auto suggestions = m_client.suggestions();
    if (suggestions.isEmpty()) {
        close();
        return;
    }

    m_page.setActiveDataListSuggestionPicker(*this);

    WebCore::DataListSuggestionInformation info { type, WTFMove(suggestions), m_client.elementRectInRootViewCoordinates() };
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebPageProxy::ShowDataListSuggestions(info), m_page.identifier());
}

} // namespace WebKit

#endif
