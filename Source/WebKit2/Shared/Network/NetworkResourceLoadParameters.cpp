/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#include "NetworkResourceLoadParameters.h"

#include "ArgumentCoders.h"
#include "DataReference.h"
#include "DecoderAdapter.h"
#include "EncoderAdapter.h"
#include "WebCoreArgumentCoders.h"

#if ENABLE(NETWORK_PROCESS)

using namespace WebCore;

namespace WebKit {
NetworkResourceLoadParameters::NetworkResourceLoadParameters()
    : m_identifier(0)
    , m_webPageID(0)
    , m_webFrameID(0)
    , m_priority(ResourceLoadPriorityVeryLow)
    , m_contentSniffingPolicy(SniffContent)
    , m_allowStoredCredentials(DoNotAllowStoredCredentials)
    , m_inPrivateBrowsingMode(false)
{
}

NetworkResourceLoadParameters::NetworkResourceLoadParameters(ResourceLoadIdentifier identifier, uint64_t webPageID, uint64_t webFrameID, const ResourceRequest& request, ResourceLoadPriority priority, ContentSniffingPolicy contentSniffingPolicy, StoredCredentials allowStoredCredentials, bool inPrivateBrowsingMode)
    : m_identifier(identifier)
    , m_webPageID(webPageID)
    , m_webFrameID(webFrameID)
    , m_request(request)
    , m_priority(priority)
    , m_contentSniffingPolicy(contentSniffingPolicy)
    , m_allowStoredCredentials(allowStoredCredentials)
    , m_inPrivateBrowsingMode(inPrivateBrowsingMode)
{
}

void NetworkResourceLoadParameters::encode(CoreIPC::ArgumentEncoder& encoder) const
{
    encoder.encode(m_identifier);
    encoder.encode(m_webPageID);
    encoder.encode(m_webFrameID);
    encoder.encode(m_request);

    encoder.encode(static_cast<bool>(m_request.httpBody()));
    if (m_request.httpBody()) {
        EncoderAdapter httpBodyEncoderAdapter;
        m_request.httpBody()->encode(httpBodyEncoderAdapter);
        encoder.encode(httpBodyEncoderAdapter.dataReference());

        const Vector<FormDataElement>& elements = m_request.httpBody()->elements();
        size_t fileCount = 0;
        for (size_t i = 0, count = elements.size(); i < count; ++i) {
            if (elements[i].m_type == FormDataElement::encodedFile)
                ++fileCount;
        }

        SandboxExtension::HandleArray requestBodySandboxExtensions;
        requestBodySandboxExtensions.allocate(fileCount);
        size_t extensionIndex = 0;
        for (size_t i = 0, count = elements.size(); i < count; ++i) {
            const FormDataElement& element = elements[i];
            if (element.m_type == FormDataElement::encodedFile) {
                const String& path = element.m_shouldGenerateFile ? element.m_generatedFilename : element.m_filename;
                SandboxExtension::createHandle(path, SandboxExtension::ReadOnly, requestBodySandboxExtensions[extensionIndex++]);
            }
        }
        encoder.encode(requestBodySandboxExtensions);
    }

    if (m_request.url().isLocalFile()) {
        SandboxExtension::Handle requestSandboxExtension;
        SandboxExtension::createHandle(m_request.url().fileSystemPath(), SandboxExtension::ReadOnly, requestSandboxExtension);
        encoder.encode(requestSandboxExtension);
    }

    encoder.encodeEnum(m_priority);
    encoder.encodeEnum(m_contentSniffingPolicy);
    encoder.encodeEnum(m_allowStoredCredentials);
    encoder.encode(m_inPrivateBrowsingMode);
}

bool NetworkResourceLoadParameters::decode(CoreIPC::ArgumentDecoder* decoder, NetworkResourceLoadParameters& result)
{
    if (!decoder->decode(result.m_identifier))
        return false;

    if (!decoder->decode(result.m_webPageID))
        return false;

    if (!decoder->decode(result.m_webFrameID))
        return false;

    if (!decoder->decode(result.m_request))
        return false;

    bool hasHTTPBody;
    if (!decoder->decode(hasHTTPBody))
        return false;

    if (hasHTTPBody) {
        CoreIPC::DataReference formData;
        if (!decoder->decode(formData))
            return false;
        DecoderAdapter httpBodyDecoderAdapter(formData.data(), formData.size());
        result.m_request.setHTTPBody(FormData::decode(httpBodyDecoderAdapter));

        if (!decoder->decode(result.m_requestBodySandboxExtensions))
            return false;
    }

    if (result.m_request.url().isLocalFile()) {
        if (!decoder->decode(result.m_resourceSandboxExtension))
            return false;
    }

    if (!decoder->decodeEnum(result.m_priority))
        return false;
    if (!decoder->decodeEnum(result.m_contentSniffingPolicy))
        return false;
    if (!decoder->decodeEnum(result.m_allowStoredCredentials))
        return false;
    if (!decoder->decode(result.m_inPrivateBrowsingMode))
        return false;

    return true;
}
    
} // namespace WebKit

#endif // ENABLE(NETWORK_PROCESS)
