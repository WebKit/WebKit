/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <WebCore/PlatformCALayer.h>
#include <WebCore/PlatformCALayerClient.h>

namespace WebCore {

class FontCascade;
class GraphicsLayer;
class GraphicsLayerCA;

class FrameProcessIndicators final : public PlatformCALayerClient {
    WTF_MAKE_TZONE_ALLOCATED(FrameProcessIndicators);
public:
    explicit FrameProcessIndicators(GraphicsLayerCA&);
    ~FrameProcessIndicators();

    void appendLayers(PlatformCALayerList&) const;
    void updateGeometry(const FloatRect&);
    void updateContentsScale(float);

private:
    static Color borderColor();
    static FontCascade makeFont();

    FloatSize size() const;
    std::pair<float, float> textVerticalBounds(const FontCascade&) const;

    PlatformLayerIdentifier platformCALayerIdentifier() const override { return m_layerIdentifier; }
    void platformCALayerPaintContents(PlatformCALayer*, GraphicsContext&, const FloatRect&, OptionSet<GraphicsLayerPaintBehavior>) override;
    bool platformCALayerContentsOpaque() const override { return false; }
    bool platformCALayerDrawsContent() const override { return true; }
    float platformCALayerDeviceScaleFactor() const override;
    OptionSet<ContentsFormat> screenContentsFormats() const override { return { }; }

    const PlatformLayerIdentifier m_layerIdentifier { PlatformLayerIdentifier::generate() };
    const WeakPtr<GraphicsLayer> m_graphicsLayer;
    const Color m_backgroundColor;
    const String m_text;
    const Ref<PlatformCALayer> m_borderLayer;
    const Ref<PlatformCALayer> m_indicatorLayer;
};

} // namespace WebCore
