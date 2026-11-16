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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ImageContentSettings.h"
#include "ImageRenditionOptions.h"
#include "IntSize.h"
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class FloatRect;
class GraphicsContext;
class LocalFrameView;
class Page;
class SVGImage;
class SVGSVGElement;
struct ImageDrawDescriptor;

// The settings an SVG image's document reads when it is parsed. A draw with other ones needs a document of its own.
struct SVGImageParseSettings {
    bool layerBasedSVGEngineEnabled { false };
    bool cssDPropertyEnabled { false };

    bool operator==(const SVGImageParseSettings&) const = default;
};

struct SVGImageInstanceKey {
    SVGImageParseSettings parseSettings;

    bool operator==(const SVGImageInstanceKey&) const = default;
};

class SVGImageInstance final : public RefCounted<SVGImageInstance> {
    WTF_MAKE_TZONE_ALLOCATED(SVGImageInstance);
public:
    static Ref<SVGImageInstance> create(SVGImage&, const std::optional<ImageContentSettings>&);
    ~SVGImageInstance();

    static SVGImageInstanceKey key(const ImageContentSettings&);
    SVGImageInstanceKey key() const { return { m_parseSettings }; }

    void detachFromImage();

    void updateRendering();

    Page* page() const { return m_page.get(); }
    RefPtr<SVGSVGElement> rootElement() const;
    LocalFrameView* frameView() const;

    void startAnimation();
    void stopAnimation();
    void resetAnimation();
    bool isAnimating() const;
    bool isAnimationPaused() const;

    void copyAnimationTimes(const SVGImageInstance&);

    size_t approximateMemoryCost() const;

    bool hasHDRContent() const;

#if ENABLE(AX_CUSTOM_COLOR_MODE)
    // Used by draws with InvertContent::FromResource. The page's settings are only changed by
    // create() and applyDocumentState(), so they stay in sync with m_appliedDocumentState.
    void setFallbackInvertContent(bool invertContent) { m_fallbackInvertContent = invertContent; }
#endif

    struct DocumentState {
        IntSize containerSize;
        ImageRenditionOptions rendition;

        bool operator==(const DocumentState&) const = default;
    };

    DocumentState documentStateForDraw(const ImageDrawDescriptor&) const;
    const DocumentState& appliedDocumentState() const { return m_appliedDocumentState; }
    void applyDocumentState(const DocumentState&);

    void paint(GraphicsContext&, const FloatRect& source);

private:
    SVGImageInstance(Ref<Page>&&);

    void setContainerSize(const FloatSize&);

    RefPtr<Page> m_page;
    SVGImageParseSettings m_parseSettings;
    DocumentState m_appliedDocumentState;
#if ENABLE(AX_CUSTOM_COLOR_MODE)
    bool m_fallbackInvertContent { false };
#endif
};

} // namespace WebCore
