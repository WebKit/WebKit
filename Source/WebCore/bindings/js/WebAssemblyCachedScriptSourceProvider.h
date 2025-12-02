/*
 * Copyright (C) 2022 Igalia S.L. All rights reserved.
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(WEBASSEMBLY)

#include "CachedResourceClient.h"
#include "CachedResourceHandle.h"
#include "CachedScript.h"
#include "CachedScriptFetcher.h"
#include "SharedBuffer.h"
#include <JavaScriptCore/SourceProvider.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class WebAssemblyCachedScriptSourceProvider final : public JSC::BaseWebAssemblySourceProvider, public CachedResourceClient {
    WTF_MAKE_TZONE_ALLOCATED(WebAssemblyCachedScriptSourceProvider);
public:
    static Ref<WebAssemblyCachedScriptSourceProvider> create(CachedScript* cachedScript, Ref<CachedScriptFetcher>&& scriptFetcher)
    {
        return adoptRef(*new WebAssemblyCachedScriptSourceProvider(cachedScript, JSC::SourceOrigin { cachedScript->response().url(), WTFMove(scriptFetcher) }, cachedScript->response().url().string()));
    }

    virtual ~WebAssemblyCachedScriptSourceProvider()
    {
        m_cachedScript->removeClient(*this);
    }

    // CachedResourceClient.
    void ref() const final { JSC::BaseWebAssemblySourceProvider::ref(); }
    void deref() const final { JSC::BaseWebAssemblySourceProvider::deref(); }

    unsigned hash() const final { return m_cachedScript->scriptHash(); }
    StringView source() const final { return m_cachedScript->script(); }
    size_t size() const final { return m_buffer ? m_buffer->size() : 0; }

    const uint8_t* data() final
    {
        if (!m_buffer)
            return nullptr;

        if (!m_buffer->isContiguous())
            m_buffer = RefPtr { m_buffer }->makeContiguous();

        return downcast<SharedBuffer>(m_buffer)->span().data();
    }

    void lockUnderlyingBufferImpl() final
    {
        ASSERT(!m_buffer);
        m_buffer = m_cachedScript->resourceBuffer();
    }

    void unlockUnderlyingBufferImpl() final
    {
        ASSERT(m_buffer);
        m_buffer = nullptr;
    }

private:
    WebAssemblyCachedScriptSourceProvider(CachedScript* cachedScript, const JSC::SourceOrigin& sourceOrigin, String sourceURL)
        : BaseWebAssemblySourceProvider(sourceOrigin, WTFMove(sourceURL))
        , m_cachedScript(cachedScript)
        , m_buffer(nullptr)
    {
        m_cachedScript->addClient(*this);
    }

    CachedResourceHandle<CachedScript> m_cachedScript;
    RefPtr<FragmentedSharedBuffer> m_buffer;
};

} // namespace WebCore

#endif // ENABLE(WEBASSEMBLY)
