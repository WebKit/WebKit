/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "InspectorThreadableLoaderClient.h"

#include "ThreadableLoader.h"

namespace Inspector {

using namespace WebCore;

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(InspectorThreadableLoaderClient);

void InspectorThreadableLoaderClient::didReceiveResponse(ScriptExecutionContextIdentifier, std::optional<ResourceLoaderIdentifier>, const ResourceResponse& response)
{
    m_mimeType = response.mimeType();
    m_statusCode = response.httpStatusCode();

    // FIXME: This assumes text only responses. We should support non-text responses as well.
    PAL::TextEncoding textEncoding(response.textEncodingName());
    bool useDetector = false;
    if (!textEncoding.isValid()) {
        textEncoding = PAL::UTF8Encoding();
        useDetector = true;
    }

    m_decoder = TextResourceDecoder::create("text/plain"_s, textEncoding, useDetector);
}

void InspectorThreadableLoaderClient::didReceiveData(const SharedBuffer& buffer)
{
    if (buffer.isEmpty())
        return;

    m_responseText.append(Ref { *m_decoder }->decode(buffer.span()));
}

void InspectorThreadableLoaderClient::didFinishLoading(ScriptExecutionContextIdentifier, std::optional<ResourceLoaderIdentifier>, const NetworkLoadMetrics&)
{
    if (RefPtr decoder = m_decoder)
        m_responseText.append(decoder->flush());

    m_callback->sendSuccess(m_responseText.toString(), m_mimeType, m_statusCode);
    dispose();
}

void InspectorThreadableLoaderClient::didFail(std::optional<ScriptExecutionContextIdentifier>, const ResourceError& error)
{
    m_callback->sendFailure(error.isAccessControl() ? "Loading resource for inspector failed access control check"_s : "Loading resource for inspector failed"_s);
    dispose();
}

void InspectorThreadableLoaderClient::setLoader(RefPtr<ThreadableLoader>&& loader)
{
    m_loader = WTF::move(loader);
}

void InspectorThreadableLoaderClient::dispose()
{
    m_loader = nullptr;
    if (!m_hasCalledDeref) {
        m_hasCalledDeref = true;
        deref();
    }
}

} // namespace Inspector
