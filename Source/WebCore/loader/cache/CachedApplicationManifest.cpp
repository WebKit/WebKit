/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "CachedApplicationManifest.h"

#if ENABLE(APPLICATION_MANIFEST)

#include "ApplicationManifestParser.h"
#include "SharedBuffer.h"
#include "TextResourceDecoder.h"

namespace WebCore {

CachedApplicationManifest::CachedApplicationManifest(CachedResourceRequest&& request, PAL::SessionID sessionID)
    : CachedResource(WTFMove(request), Type::ApplicationManifest, sessionID)
    , m_decoder(TextResourceDecoder::create("application/manifest+json", UTF8Encoding()))
{
}

void CachedApplicationManifest::finishLoading(SharedBuffer* data)
{
    m_data = data;
    setEncodedSize(data ? data->size() : 0);
    if (data)
        m_text = m_decoder->decodeAndFlush(data->data(), data->size());
    CachedResource::finishLoading(data);
}

void CachedApplicationManifest::setEncoding(const String& chs)
{
    m_decoder->setEncoding(chs, TextResourceDecoder::EncodingFromHTTPHeader);
}

String CachedApplicationManifest::encoding() const
{
    return m_decoder->encoding().name();
}

Optional<ApplicationManifest> CachedApplicationManifest::process(const URL& manifestURL, const URL& documentURL, RefPtr<ScriptExecutionContext> scriptExecutionContext)
{
    if (!m_text)
        return WTF::nullopt;
    if (scriptExecutionContext)
        return ApplicationManifestParser::parse(*scriptExecutionContext, *m_text, manifestURL, documentURL);
    return ApplicationManifestParser::parse(*m_text, manifestURL, documentURL);
}

} // namespace WebCore

#endif // ENABLE(APPLICATION_MANIFEST)
