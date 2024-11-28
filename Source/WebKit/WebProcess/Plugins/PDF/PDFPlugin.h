/*
 * Copyright (C) 2011-2022 Apple Inc. All rights reserved.
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

#if ENABLE(LEGACY_PDFKIT_PLUGIN)

#include "PDFPluginBase.h"
#include <WebCore/NetscapePlugInStreamLoader.h>
#include <wtf/HashMap.h>
#include <wtf/Identified.h>
#include <wtf/Range.h>
#include <wtf/RangeSet.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Threading.h>
#include <wtf/URL.h>
#include <wtf/Vector.h>

typedef struct objc_object* id;

OBJC_CLASS CALayer;
OBJC_CLASS NSArray;
OBJC_CLASS NSAttributedString;
OBJC_CLASS NSData;
OBJC_CLASS NSDictionary;
OBJC_CLASS NSEvent;
OBJC_CLASS NSString;
OBJC_CLASS PDFAnnotation;
OBJC_CLASS PDFDocument;
OBJC_CLASS PDFLayerController;
OBJC_CLASS PDFSelection;
OBJC_CLASS WKPDFLayerControllerDelegate;
OBJC_CLASS WKPDFPluginAccessibilityObject;

namespace WebCore {
class AXObjectCache;
class Element;
class FloatPoint;
class FloatSize;
class FragmentedSharedBuffer;
class GraphicsContext;
class HTMLPlugInElement;
class ShareableBitmap;
struct PluginInfo;
}

namespace WebKit {

class PDFPluginAnnotation;
class PDFPluginPasswordField;
class PluginView;
class WebFrame;
class WebKeyboardEvent;
class WebWheelEvent;
struct WebHitTestResultData;

class PDFPlugin final : public PDFPluginBase {
    WTF_MAKE_TZONE_ALLOCATED(PDFPlugin);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(PDFPlugin);
public:
    static bool pdfKitLayerControllerIsAvailable();

    static Ref<PDFPlugin> create(WebCore::HTMLPlugInElement&);

    virtual ~PDFPlugin();

    void paintControlForLayerInContext(CALayer *, CGContextRef);
    void setActiveAnnotation(SetActiveAnnotationParams&&) final;

    void notifyContentScaleFactorChanged(CGFloat scaleFactor);
    void notifyDisplayModeChanged(int);

    // HUD Actions.
#if ENABLE(PDF_HUD)
    void zoomIn() final;
    void zoomOut() final;
#endif

    void clickedLink(NSURL *);

    void showDefinitionForAttributedString(NSAttributedString *, CGPoint);

    CGRect pluginBoundsForAnnotation(RetainPtr<PDFAnnotation>&) const final;
    void focusNextAnnotation() final;
    void focusPreviousAnnotation() final;

    void attemptToUnlockPDF(const String& password) final;

    bool showContextMenuAtPoint(const WebCore::IntPoint&);

    WebCore::IntPoint convertFromPluginToPDFView(const WebCore::IntPoint&) const;
    WebCore::IntPoint convertFromPDFViewToRootView(const WebCore::IntPoint&) const;
    WebCore::IntRect convertFromPDFViewToRootView(const WebCore::IntRect&) const;
    WebCore::IntPoint convertFromRootViewToPDFView(const WebCore::IntPoint&) const;
    WebCore::FloatRect convertFromPDFViewToScreen(const WebCore::FloatRect&) const;

    double scaleFactor() const override;
    double contentScaleFactor() const final;
    CGSize contentSizeRespectingZoom() const;

private:
    explicit PDFPlugin(WebCore::HTMLPlugInElement&);
    bool isLegacyPDFPlugin() const override { return true; }

    PDFSelection *nextMatchForString(const String& target, bool searchForward, bool caseSensitive, bool wrapSearch, PDFSelection *initialSelection, bool startInSelection);

    void didChangeScrollOffset() override;

    void invalidateScrollbarRect(WebCore::Scrollbar&, const WebCore::IntRect&) override;
    void invalidateScrollCornerRect(const WebCore::IntRect&) override;
    void updateScrollbars() override;
    Ref<WebCore::Scrollbar> createScrollbar(WebCore::ScrollbarOrientation) override;
    void destroyScrollbar(WebCore::ScrollbarOrientation) override;

    // PDFPluginBase
    WebCore::PluginLayerHostingStrategy layerHostingStrategy() const override { return WebCore::PluginLayerHostingStrategy::PlatformLayer; }
    PlatformLayer* platformLayer() const override;

    void setView(PluginView&) override;
    void teardown() override;
    bool isComposited() const override { return true; }

    void installPDFDocument() override;

    bool geometryDidChange(const WebCore::IntSize& pluginSize, const WebCore::AffineTransform& pluginToRootViewTransform) override;
    void deviceScaleFactorChanged(float) override;

    void setPageScaleFactor(double, std::optional<WebCore::IntPoint> origin) override;

    WebCore::IntSize contentsSize() const override;
    unsigned firstPageHeight() const override;

    RefPtr<WebCore::FragmentedSharedBuffer> liveResourceData() const override;

    bool wantsWheelEvents() const override { return true; }
    bool handleMouseEvent(const WebMouseEvent&) override;
    bool handleWheelEvent(const WebWheelEvent&) override;
    bool handleMouseEnterEvent(const WebMouseEvent&) override;
    bool handleMouseLeaveEvent(const WebMouseEvent&) override;
    bool handleContextMenuEvent(const WebMouseEvent&) override;
    bool handleKeyboardEvent(const WebKeyboardEvent&) override;
    bool handleEditingCommand(const String& commandName, const String& argument) override;
    bool isEditingCommandEnabled(const String& commandName) override;

    String selectionString() const override;
    bool existingSelectionContainsPoint(const WebCore::FloatPoint&) const override;
    WebCore::FloatRect rectForSelectionInRootView(PDFSelection *) const override;

    unsigned countFindMatches(const String& target, WebCore::FindOptions, unsigned maxMatchCount) override;
    bool findString(const String& target, WebCore::FindOptions, unsigned maxMatchCount) override;
    bool drawsFindOverlay() const final { return true; }

    Vector<WebFoundTextRange::PDFData> findTextMatches(const String&, WebCore::FindOptions) final { return { }; }
    Vector<WebCore::FloatRect> rectsForTextMatch(const WebFoundTextRange::PDFData&) final { return { }; }

    WebCore::DictionaryPopupInfo dictionaryPopupInfoForSelection(PDFSelection *, WebCore::TextIndicatorPresentationTransition) override;
    bool performDictionaryLookupAtLocation(const WebCore::FloatPoint&) override;
    std::pair<String, RetainPtr<PDFSelection>> textForImmediateActionHitTestAtPoint(const WebCore::FloatPoint&, WebHitTestResultData&) override;

    bool shouldCreateTransientPaintingSnapshot() const override { return true; }
    RefPtr<WebCore::ShareableBitmap> snapshot() override;

    id accessibilityHitTest(const WebCore::IntPoint&) const override;
    id accessibilityObject() const override;

    NSEvent *nsEventForWebMouseEvent(const WebMouseEvent&);

    void updatePageAndDeviceScaleFactors();

    void createPasswordEntryForm();
    void teardownPasswordEntryForm() override;

    NSData *liveData() const override;

    RetainPtr<CALayer> m_containerLayer;
    RetainPtr<CALayer> m_contentLayer;
    RetainPtr<CALayer> m_horizontalScrollbarLayer;
    RetainPtr<CALayer> m_verticalScrollbarLayer;
    RetainPtr<CALayer> m_scrollCornerLayer;
    RetainPtr<PDFLayerController> m_pdfLayerController;
    RetainPtr<WKPDFPluginAccessibilityObject> m_accessibilityObject;
    
    RefPtr<PDFPluginPasswordField> m_passwordField;

    String m_temporaryPDFUUID;

    RetainPtr<WKPDFLayerControllerDelegate> m_pdfLayerControllerDelegate;

    URL m_sourceURL;
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::PDFPlugin)
    static bool isType(const WebKit::PDFPluginBase& plugin) { return plugin.isLegacyPDFPlugin(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(LEGACY_PDFKIT_PLUGIN)
