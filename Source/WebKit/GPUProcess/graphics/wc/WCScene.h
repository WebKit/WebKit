/*
 * Copyright (C) 2021 Sony Interactive Entertainment Inc.
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

#if USE(GRAPHICS_LAYER_WC)

#include "UpdateInfo.h"
#include <WebCore/ProcessIdentifier.h>
#include <WebCore/TextureMapperFPSCounter.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>

namespace WebCore {
class TextureMapper;
class TextureMapperLayer;
class TextureMapperPlatformLayer;
class TextureMapperTiledBackingStore;
}

namespace WebKit {

class WCSceneContext;
struct WCUpateInfo;

class WCScene {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WCScene(WebCore::ProcessIdentifier, bool usesOffscreenRendering);
    ~WCScene();
    void initialize(WCSceneContext&);
    std::optional<UpdateInfo> update(WCUpateInfo&&);

private:
    struct Layer;
    using LayerMap = HashMap<uint64_t, std::unique_ptr<Layer>>;

    WebCore::ProcessIdentifier m_webProcessIdentifier;
    WCSceneContext* m_context { nullptr };
    std::unique_ptr<WebCore::TextureMapper> m_textureMapper;
    WebCore::TextureMapperFPSCounter m_fpsCounter;
    LayerMap m_layers;
    bool m_usesOffscreenRendering;
};

} // namespace WebKit

#endif // USE(GRAPHICS_LAYER_WC)
