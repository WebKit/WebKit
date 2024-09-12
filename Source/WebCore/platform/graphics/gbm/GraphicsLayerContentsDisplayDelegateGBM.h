/*
 * Copyright (C) 2024 Igalia S.L.
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

#if ENABLE(WEBGL) && USE(COORDINATED_GRAPHICS) && USE(GBM)
#include "GraphicsLayerContentsDisplayDelegateTextureMapper.h"

namespace WebCore {
class DMABufBuffer;
class GLFence;

class GraphicsLayerContentsDisplayDelegateGBM final : public GraphicsLayerContentsDisplayDelegateTextureMapper {
public:
    static Ref<GraphicsLayerContentsDisplayDelegateGBM> create(bool isOpaque)
    {
        return adoptRef(*new GraphicsLayerContentsDisplayDelegateGBM(isOpaque));
    }
    virtual ~GraphicsLayerContentsDisplayDelegateGBM();

    void setDisplayBuffer(RefPtr<DMABufBuffer>&, std::unique_ptr<GLFence>&&);

private:
    explicit GraphicsLayerContentsDisplayDelegateGBM(bool isOpaque);

    bool m_isOpaque { false };
    RefPtr<DMABufBuffer> m_buffer;
    std::unique_ptr<GLFence> m_fence;
};

} // namespace WebCore

#endif // ENABLE(WEBGL) && USE(COORDINATED_GRAPHICS) && USE(GBM)
