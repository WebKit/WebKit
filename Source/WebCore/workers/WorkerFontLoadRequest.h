/*
 * Copyright (C) 2021 Metrological Group B.V.
 * Copyright (C) 2021 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "FontLoadRequest.h"
#include "ResourceLoaderOptions.h"
#include "ThreadableLoaderClient.h"
#include <wtf/URL.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class ScriptExecutionContext;
class SharedBuffer;
class WorkerGlobalScope;

struct FontCustomPlatformData;

class WorkerFontLoadRequest : public FontLoadRequest, public ThreadableLoaderClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WorkerFontLoadRequest(URL&&, LoadedFromOpaqueSource);
    ~WorkerFontLoadRequest() = default;

    void load(WorkerGlobalScope&);

private:
    const URL& url() const final { return m_url; }
    bool isPending() const final { return !m_isLoading && !m_errorOccurred && !m_data; }
    bool isLoading() const final { return m_isLoading; }
    bool errorOccurred() const final { return m_errorOccurred; }

    bool ensureCustomFontData(const AtomString& remoteURI) final;
    RefPtr<Font> createFont(const FontDescription&, const AtomString& remoteURI, bool syntheticBold, bool syntheticItalic, const FontFeatureSettings&, FontSelectionSpecifiedCapabilities) final;

    void setClient(FontLoadRequestClient*) final;

    bool isWorkerFontLoadRequest() const final { return true; }

    void didReceiveResponse(unsigned long identifier, const ResourceResponse&) final;
    void didReceiveData(const uint8_t* data, int dataLength) final;
    void didFinishLoading(unsigned long identifier) final;
    void didFail(const ResourceError&) final;

    URL m_url;
    LoadedFromOpaqueSource m_loadedFromOpaqueSource;

    bool m_isLoading { false };
    bool m_notifyOnClientSet { false };
    bool m_errorOccurred { false };
    FontLoadRequestClient* m_fontLoadRequestClient { nullptr };

    WeakPtr<ScriptExecutionContext> m_context;
    RefPtr<SharedBuffer> m_data;
    std::unique_ptr<FontCustomPlatformData> m_fontCustomPlatformData;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_FONTLOADREQUEST(WebCore::WorkerFontLoadRequest, isWorkerFontLoadRequest())
