/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "PrivateClickMeasurementNetworkLoader.h"

#include "NetworkLoad.h"
#include "NetworkSession.h"
#include <WebCore/MIMETypeRegistry.h>
#include <WebCore/TextResourceDecoder.h>

#include <wtf/JSONValues.h>

namespace WebKit {

using namespace WebCore;

void PrivateClickMeasurementNetworkLoader::start(NetworkSession& session, NetworkLoadParameters&& parameters, Callback&& completionHandler)
{
    auto loader = std::unique_ptr<PrivateClickMeasurementNetworkLoader>(new PrivateClickMeasurementNetworkLoader(session, WTFMove(parameters), WTFMove(completionHandler)));
    session.addPrivateClickMeasurementNetworkLoader(WTFMove(loader));
}

PrivateClickMeasurementNetworkLoader::PrivateClickMeasurementNetworkLoader(NetworkSession& session, NetworkLoadParameters&& parameters, Callback&& completionHandler)
    : m_session(makeWeakPtr(session))
    , m_completionHandler(WTFMove(completionHandler))
{
    m_networkLoad = makeUnique<NetworkLoad>(*this, nullptr, WTFMove(parameters), *m_session);
    m_networkLoad->start();
}

PrivateClickMeasurementNetworkLoader::~PrivateClickMeasurementNetworkLoader()
{
    cancel();
}

void PrivateClickMeasurementNetworkLoader::fail(ResourceError&& error)
{
    if (!m_completionHandler)
        return;

    m_completionHandler(WTFMove(error), { }, nullptr);
    didComplete();
}

void PrivateClickMeasurementNetworkLoader::cancel()
{
    fail(ResourceError { ResourceError::Type::Cancellation });
}

void PrivateClickMeasurementNetworkLoader::willSendRedirectedRequest(ResourceRequest&&, ResourceRequest&&, ResourceResponse&&)
{
    cancel();
}

void PrivateClickMeasurementNetworkLoader::didReceiveResponse(ResourceResponse&& response, ResponseCompletionHandler&& completionHandler)
{
    if (!MIMETypeRegistry::isSupportedJSONMIMEType(response.mimeType())) {
        fail({ errorDomainWebKitInternal, 0, response.url(), "MIME Type is not a JSON MIME type"_s });
        completionHandler(PolicyAction::Ignore);
        return;
    }

    m_response = WTFMove(response);
    completionHandler(PolicyAction::Use);
}

void PrivateClickMeasurementNetworkLoader::didReceiveBuffer(Ref<SharedBuffer>&& buffer, int reportedEncodedDataLength)
{
    if (!m_decoder)
        m_decoder = TextResourceDecoder::create("application/json"_s, m_response.textEncodingName().isEmpty() ? WebCore::TextEncoding("UTF-8") : WebCore::TextEncoding(m_response.textEncodingName()));

    if (auto size = buffer->size())
        m_jsonString.append(m_decoder->decode(buffer->data(), size));
}

void PrivateClickMeasurementNetworkLoader::didFinishLoading(const WebCore::NetworkLoadMetrics&)
{
    if (m_decoder)
        m_jsonString.append(m_decoder->flush());

    if (auto jsonValue = JSON::Value::parseJSON(m_jsonString.toString()))
        m_completionHandler({ }, m_response, jsonValue->asObject());
    else
        m_completionHandler({ }, m_response, nullptr);

    didComplete();
}

void PrivateClickMeasurementNetworkLoader::didFailLoading(const ResourceError& error)
{
    fail(ResourceError(error));
}

void PrivateClickMeasurementNetworkLoader::didComplete()
{
    m_networkLoad = nullptr;
    if (m_session)
        m_session->removePrivateClickMeasurementNetworkLoader(this);
}

} // namespace WebKit
