/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "WebDateTimeChooser.h"

#if ENABLE(DATE_AND_TIME_INPUT_TYPES)

#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include "WebProcess.h"
#include <WebCore/DateTimeChooserClient.h>
#include <WebCore/DateTimeChooserParameters.h>

namespace WebKit {

WebDateTimeChooser::WebDateTimeChooser(WebPage& page, WebCore::DateTimeChooserClient& client)
    : m_client(client)
    , m_page(page)
{
}

void WebDateTimeChooser::didChooseDate(StringView date)
{
    m_client.didChooseValue(date);
}

void WebDateTimeChooser::didEndChooser()
{
    m_client.didEndChooser();
}

void WebDateTimeChooser::endChooser()
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebPageProxy::EndDateTimePicker(), m_page.identifier());
}

void WebDateTimeChooser::showChooser(const WebCore::DateTimeChooserParameters& params)
{
    m_page.setActiveDateTimeChooser(*this);
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebPageProxy::ShowDateTimePicker(params), m_page.identifier());
}

} // namespace WebKit

#endif
