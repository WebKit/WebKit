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

#pragma once

#if ENABLE(APPLICATION_MANIFEST)

#include "ApplicationManifest.h"
#include "CachedResource.h"
#include <wtf/Optional.h>

namespace WebCore {

class ScriptExecutionContext;
class TextResourceDecoder;

class CachedApplicationManifest final : public CachedResource {
public:
    CachedApplicationManifest(CachedResourceRequest&&, const PAL::SessionID&, const CookieJar*);

    Optional<struct ApplicationManifest> process(const URL& manifestURL, const URL& documentURL, RefPtr<ScriptExecutionContext> = nullptr);

private:
    void finishLoading(SharedBuffer*, const NetworkLoadMetrics&) override;
    const TextResourceDecoder* textResourceDecoder() const override { return m_decoder.ptr(); }
    void setEncoding(const String&) override;
    String encoding() const override;

    Ref<TextResourceDecoder> m_decoder;
    Optional<String> m_text;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CACHED_RESOURCE(CachedApplicationManifest, CachedResource::Type::ApplicationManifest)

#endif // ENABLE(APPLICATION_MANIFEST)
