/*
 * Copyright (C) 2006 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009-2025 Apple Inc. All rights reserved.
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

#include <WebCore/Image.h>
#include <WebCore/Timer.h>
#include <wtf/UniqueRef.h>

namespace WebCore {

class Element;
class ImageBuffer;
class LocalFrameView;
class Page;
class RenderObject;
class RenderReplaced;
class SVGSVGElement;
class SVGImageChromeClient;
class SVGImageInstance;
class SVGImageInstanceCache;
class Settings;

class SVGImage final : public Image {
public:
    static Ref<SVGImage> create(ImageObserver* observer) { return adoptRef(*new SVGImage(observer)); }
    WEBCORE_EXPORT static void tryCreateFromData(std::span<const uint8_t>, CompletionHandler<void(RefPtr<SVGImage>&&)>&&);
    WEBCORE_EXPORT static bool isDataDecodable(const Settings&, std::span<const uint8_t>);

    NODELETE bool isSVGImage() const final { return true; }

    Page* defaultInstancePage() const;
    RenderReplaced* defaultInstanceEmbeddedSVGRoot() const;
    LocalFrameView* defaultInstanceFrameView() const;

    RefPtr<SVGSVGElement> rootElement(const ImageDrawDescriptor&);

    void subresourcesAreFinished(Document*, CompletionHandler<void()>&&) final;

    void updateRendering() final;
    void scheduleRenderingUpdate();

    bool hasSomethingToDraw() const final;
    bool renderingTaintsOrigin() const final;

    void startAnimation() final;
    void stopAnimation() final;
    void resetAnimation() final;
    bool isAnimating() const final;

#if ENABLE(AX_CUSTOM_COLOR_MODE)
    void setFallbackInvertContent(bool);
#endif

    RefPtr<NativeImage> nativeImage(const ImageDrawDescriptor&, const ColorSpace& = ColorSpace::SRGB()) final;

private:
    friend class SVGImageChromeClient;

    WEBCORE_EXPORT explicit SVGImage(ImageObserver*);
    virtual ~SVGImage();

    String filenameExtension() const final;
    NaturalDimensions unorientedNaturalDimensions() const final;
    bool usesConcreteObjectSizeAsViewport() const final { return true; }

    RefPtr<SVGSVGElement> defaultInstanceRootElement() const;

    RefPtr<SVGImageInstance> instanceForDraw(const ImageDrawDescriptor&);
    void didCreateInstance(SVGImageInstance&);
    void reportApproximateMemoryCost(const SVGImageInstance&) const;
    EncodedDataStatus dataChanged(bool allDataReceived) final;

    // FIXME: SVGImages will be unable to prune because destroyDecodedData() is not implemented yet.

    // FIXME: Implement this to be less conservative.
    bool currentFrameKnownToBeOpaque() const final { return false; }
    bool currentFrameIsComplete() const final { return !!defaultInstancePage(); }

    bool hasHDRContent() const final;

    RefPtr<NativeImage> currentNativeImage(const ImageDrawDescriptor&) final;
    RefPtr<NativeImage> currentPreTransformedNativeImage(const ImageDrawDescriptor&) final;

    void startAnimationTimerFired();

    ImageDrawResult drawImage(GraphicsContext&, const ImageDrawDescriptor&, const FloatRect& destination, const FloatRect& source) final;
    std::optional<PatternTile> currentPatternTile(GraphicsContext&, const ImageDrawDescriptor&) final;

    class DrawScope;
    void scheduleRenderingUpdateIfNeeded();

    const UniqueRef<SVGImageInstanceCache> m_instances;
    Timer m_startAnimationTimer;
    bool m_needsRenderingUpdate { false };
};

bool isInSVGImage(const Element*);

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_IMAGE(SVGImage)
