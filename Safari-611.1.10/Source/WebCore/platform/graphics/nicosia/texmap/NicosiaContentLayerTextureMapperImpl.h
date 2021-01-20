/*
 * Copyright (C) 2018 Metrological Group B.V.
 * Copyright (C) 2018 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if USE(TEXTURE_MAPPER)

#include "NicosiaPlatformLayer.h"
#include <wtf/Lock.h>
#include <wtf/Ref.h>

namespace WebCore {
class TextureMapperPlatformLayerProxy;
}

namespace Nicosia {

class ContentLayerTextureMapperImpl final : public ContentLayer::Impl {
    WTF_MAKE_FAST_ALLOCATED;
public:
    class Client {
    public:
        virtual ~Client();
        virtual void swapBuffersIfNeeded() = 0;
    };

    static Factory createFactory(Client&);

    explicit ContentLayerTextureMapperImpl(Client&);
    virtual ~ContentLayerTextureMapperImpl();
    bool isTextureMapperImpl() const override { return true; }

    void invalidateClient();

    bool flushUpdate();

    WebCore::TextureMapperPlatformLayerProxy& proxy() const { return m_proxy; }
    void swapBuffersIfNeeded();

private:
    Ref<WebCore::TextureMapperPlatformLayerProxy> m_proxy;
    struct {
        Lock lock;
        Client* client { nullptr };
        bool pendingUpdate { true }; // Starts off with a pending update.
    } m_client;
};

} // namespace Nicosia

SPECIALIZE_TYPE_TRAITS_NICOSIA_CONTENTLAYER_IMPL(ContentLayerTextureMapperImpl, isTextureMapperImpl());

#endif // USE(TEXTURE_MAPPER)
