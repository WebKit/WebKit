/*
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

#if ENABLE(UNIFIED_PDF_DATA_DETECTION)

#include "PDFDocumentLayout.h"
#include <WebCore/DataDetectorHighlight.h>
#include <WebCore/PageOverlay.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/OptionSet.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeWeakPtr.h>

namespace WebCore {
class GraphicsContext;
class GraphicsLayer;
class GraphicsLayerClient;
class IntRect;
class LocalFrame;
class PlatformMouseEvent;

enum class RenderingUpdateStep : uint32_t;
}

namespace WebKit {

class PDFDataDetectorItem;
class UnifiedPDFPlugin;
class WebMouseEvent;

class PDFDataDetectorOverlayController final : public RefCounted<PDFDataDetectorOverlayController>, private WebCore::PageOverlayClient, WebCore::DataDetectorHighlightClient {
    WTF_MAKE_TZONE_ALLOCATED(PDFDataDetectorOverlayController);
    WTF_MAKE_NONCOPYABLE(PDFDataDetectorOverlayController);
public:
    static Ref<PDFDataDetectorOverlayController> create(UnifiedPDFPlugin&);
    virtual ~PDFDataDetectorOverlayController();
    void teardown();

    // WebCore::DataDetectorHighlightClient.
    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    bool handleMouseEvent(const WebMouseEvent&, PDFDocumentLayout::PageIndex);
    RefPtr<WebCore::PageOverlay> protectedOverlay() const { return m_overlay; }

    enum class ShouldUpdatePlatformHighlightData : bool { No, Yes };
    enum class ActiveHighlightChanged : bool { No, Yes };
    void didInvalidateHighlightOverlayRects(std::optional<PDFDocumentLayout::PageIndex> = { }, ShouldUpdatePlatformHighlightData = ShouldUpdatePlatformHighlightData::Yes, ActiveHighlightChanged = ActiveHighlightChanged::No);
    void hideActiveHighlightOverlay();

private:
    explicit PDFDataDetectorOverlayController(UnifiedPDFPlugin&);

    // PageOverlayClient
    void willMoveToPage(WebCore::PageOverlay&, WebCore::Page*) final;
    void didMoveToPage(WebCore::PageOverlay&, WebCore::Page*) final { }
    void drawRect(WebCore::PageOverlay&, WebCore::GraphicsContext&, const WebCore::IntRect&) final { }
    bool mouseEvent(WebCore::PageOverlay&, const WebCore::PlatformMouseEvent&) final { return false; }
    void didScrollFrame(WebCore::PageOverlay&, WebCore::LocalFrame&) final { }

    // DataDetectorHighlightClient
    WebCore::DataDetectorHighlight* activeHighlight() const final { return m_activeDataDetectorItemWithHighlight.second.get(); }
    void scheduleRenderingUpdate(OptionSet<WebCore::RenderingUpdateStep>) final;
    float deviceScaleFactor() const final;
    RefPtr<WebCore::GraphicsLayer> createGraphicsLayer(WebCore::GraphicsLayerClient&) final;

    Ref<WebCore::PageOverlay> installProtectedOverlayIfNeeded();
    void uninstallOverlay();

    RetainPtr<DDHighlightRef> createPlatformDataDetectorHighlight(PDFDataDetectorItem&) const;
    void updatePlatformHighlightData(PDFDocumentLayout::PageIndex);
    void updateDataDetectorHighlightsIfNeeded(PDFDocumentLayout::PageIndex);

    bool handleDataDetectorAction(const WebCore::IntPoint&, PDFDataDetectorItem&);

    RefPtr<UnifiedPDFPlugin> protectedPlugin() const;

    ThreadSafeWeakPtr<UnifiedPDFPlugin> m_plugin;

    RefPtr<WebCore::PageOverlay> m_overlay;

    using PDFDataDetectorItemWithHighlight = std::pair<Ref<PDFDataDetectorItem>, Ref<WebCore::DataDetectorHighlight>>;
    using PDFDataDetectorItemWithHighlightPtr = std::pair<RefPtr<PDFDataDetectorItem>, RefPtr<WebCore::DataDetectorHighlight>>;
    using PDFDataDetectorItemsWithHighlights = Vector<PDFDataDetectorItemWithHighlight>;
    template <typename Key, typename Value>
    using HashMapWithUnsignedIntegralZeroKeyAllowed = HashMap<Key, Value, WTF::IntHash<Key>, WTF::UnsignedWithZeroKeyHashTraits<Key>>;
    using PDFDataDetectorItemsWithHighlightsMap = HashMapWithUnsignedIntegralZeroKeyAllowed<PDFDocumentLayout::PageIndex, PDFDataDetectorItemsWithHighlights>;

    PDFDataDetectorItemsWithHighlightsMap m_pdfDataDetectorItemsWithHighlightsMap;
    PDFDataDetectorItemWithHighlightPtr m_activeDataDetectorItemWithHighlight;
    PDFDataDetectorItemWithHighlightPtr m_staleDataDetectorItemWithHighlight;
};

} // namespace WebKit

#endif // ENABLE(UNIFIED_PDF_DATA_DETECTION)
