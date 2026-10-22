/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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

#include "FloatSize.h"
#include "ImageAnimationTimeline.h"
#include "ImageEnvironmentSettings.h"
#include "ImageDrawingOptions.h"
#include "IntSize.h"
#include "StyleLinkParameters.h"
#include "Timer.h"
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/URL.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class ConcreteObjectSize;
class Document;
class FloatRect;
class GraphicsContext;
class ImageDrawingExtras;
class LocalFrameView;
class Page;
class SVGImage;
class SVGSVGElement;

enum class ImageDrawResult;

namespace Style {
class ImageDrawingExtras;
}

struct SVGImageDocumentState {
    URL fragmentURL;
    Style::LinkParameters linkParameters { CSS::Keyword::None { } };
    IgnoreRootPreserveAspectRatio ignoreRootPreserveAspectRatio { IgnoreRootPreserveAspectRatio::No };
    bool useDarkAppearance { false };
    bool useElevatedUserInterfaceLevel { false };
#if ENABLE(AX_CUSTOM_COLOR_MODE)
    bool invertContent { false };
#endif
    WeakPtr<ImageAnimationTimeline> heldAnimationTimeline;

    bool operator==(const SVGImageDocumentState&) const = default;
};

class SVGImageInstance final : public RefCounted<SVGImageInstance>, public CanMakeWeakPtr<SVGImageInstance> {
    WTF_MAKE_TZONE_ALLOCATED(SVGImageInstance);
public:
    static Ref<SVGImageInstance> create(SVGImage&, const ImageEnvironmentSettings*);
    ~SVGImageInstance();

    const ImageEnvironmentSettings* environmentSettings() const { return m_environmentSettings.get(); }

    Page* page() const { return m_page.get(); }
    RefPtr<SVGSVGElement> rootElement() const;
    LocalFrameView* frameView() const;

    FloatSize intrinsicSize() const { return m_intrinsicSize; }
    size_t approximateMemoryCost() const;
    bool hasHDRContent() const;

    void startAnimation();
    void stopAnimation();
    void resetAnimation();
    bool isAnimating() const;
    bool hasAnimations() const;
    Seconds animationTime() const;
    bool animationIsHeld() const { return m_animationIsHeld; }
    bool heldAnimationTimelineIsGone() const { return m_animationIsHeld && !m_appliedDocumentState.heldAnimationTimeline; }
    void releaseGoneTimeline();

    void copyAnimationTimes(const SVGImageInstance&);

#if ENABLE(AX_CUSTOM_COLOR_MODE)
    // Used by draws with InvertContent::FromResource. The internal page's settings are only changed by
    // applyDocumentState(), so they stay in sync with m_appliedDocumentState.
    void setFallbackInvertContent(bool invertContent) { m_fallbackInvertContent = invertContent; }
#endif

    SVGImageDocumentState documentStateForDraw(ImageDrawingOptions, const ImageDrawingExtras*, bool fallbackUseDarkAppearance) const;

    bool matchesDocumentState(const SVGImageDocumentState&) const;

    std::optional<Style::ImageDrawingExtras> drawingExtrasForChanges() const;
    const SVGImageDocumentState& documentState() const { return m_appliedDocumentState; }

    ImageDrawResult draw(GraphicsContext&, ConcreteObjectSize, const FloatRect& destination, const FloatRect& source, ImageDrawingOptions, const SVGImageDocumentState&);

private:
    SVGImageInstance(Ref<Page>&&, const ImageEnvironmentSettings*);

    void setContainerSize(const FloatSize&);
    IntSize containerSize() const;

    void startAnimationTimerFired();

    void applyDocumentState(Document&, LocalFrameView&, const SVGImageDocumentState&);
    void holdAnimations(Document&, ImageAnimationTimeline::Mode);
    void releaseAnimations(Document&);

    RefPtr<const ImageEnvironmentSettings> m_environmentSettings;
    RefPtr<Page> m_page;
    FloatSize m_intrinsicSize;

    SVGImageDocumentState m_appliedDocumentState;
    bool m_hasBeenDrawn { false };
    bool m_animationIsHeld { false };
#if ENABLE(AX_CUSTOM_COLOR_MODE)
    bool m_fallbackInvertContent { false };
#endif

    Timer m_startAnimationTimer;
};

} // namespace WebCore
