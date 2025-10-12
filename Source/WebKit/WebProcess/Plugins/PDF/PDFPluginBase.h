/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#if ENABLE(PDF_PLUGIN)

#include "CursorContext.h"
#include "FrameInfoData.h"
#include "PDFPluginIdentifier.h"
#include "WebFoundTextRange.h"
#include "WebMouseEvent.h"
#include <WebCore/AffineTransform.h>
#include <WebCore/FindOptions.h>
#include <WebCore/FloatRect.h>
#include <WebCore/PageIdentifier.h>
#include <WebCore/PlatformMouseEvent.h>
#include <WebCore/PluginData.h>
#include <WebCore/PluginViewBase.h>
#include <WebCore/ScrollTypes.h>
#include <WebCore/ScrollableArea.h>
#include <WebCore/TextIndicator.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Identified.h>
#include <wtf/Lock.h>
#include <wtf/MainThread.h>
#include <wtf/Range.h>
#include <wtf/RangeSet.h>
#include <wtf/RetainPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/TypeTraits.h>
#include <wtf/WeakPtr.h>

OBJC_CLASS NSData;
OBJC_CLASS NSDictionary;
OBJC_CLASS NSString;
OBJC_CLASS PDFAnnotation;
OBJC_CLASS PDFDocument;
OBJC_CLASS PDFSelection;
OBJC_CLASS WKAccessibilityPDFDocumentObject;

namespace WebCore {
class Color;
class FragmentedSharedBuffer;
class GraphicsContext;
class Element;
class HTMLPlugInElement;
class NetscapePlugInStreamLoaderClient;
class ResourceResponse;
class Scrollbar;
class ShareableBitmap;
class SharedBuffer;
enum class PlatformCursorType : uint8_t;
struct DictionaryPopupInfo;
}

namespace WebKit {

class PDFIncrementalLoader;
class PDFPluginAnnotation;
class PluginView;
class WebFrame;
class WebPage;
class WebKeyboardEvent;
class WebMouseEvent;
class WebWheelEvent;
enum class SelectionEndpoint : bool;
enum class SelectionWasFlipped : bool;
struct DocumentEditingContextRequest;
struct DocumentEditingContext;
struct EditorState;
struct WebHitTestResultData;

enum class ByteRangeRequestIdentifierType;
using ByteRangeRequestIdentifier = ObjectIdentifier<ByteRangeRequestIdentifierType>;

enum class CheckValidRanges : bool { No, Yes };

template<typename T>
concept CanMakeFloatRect = requires(T t)
{
    { WebCore::FloatRect { t } } -> std::same_as<WebCore::FloatRect>;
};

struct PDFPluginPasteboardItem {
    RetainPtr<NSData> data;
    RetainPtr<NSString> type;
};

class PDFPluginBase : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<PDFPluginBase, WTF::DestructionThread::Main>, public CanMakeThreadSafeCheckedPtr<PDFPluginBase>, public WebCore::ScrollableArea, public Identified<PDFPluginIdentifier> {
    WTF_MAKE_NONCOPYABLE(PDFPluginBase);
    WTF_MAKE_TZONE_ALLOCATED(PDFPluginBase);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(PDFPluginBase);
    friend class PDFIncrementalLoader;
    friend class PDFPluginChoiceAnnotation;
    friend class PDFPluginTextAnnotation;
public:
    static WebCore::PluginInfo pluginInfo();

    virtual ~PDFPluginBase();

    // CheckedPtr interface
    uint32_t checkedPtrCount() const final { return CanMakeThreadSafeCheckedPtr::checkedPtrCount(); }
    uint32_t checkedPtrCountWithoutThreadCheck() const final { return CanMakeThreadSafeCheckedPtr::checkedPtrCountWithoutThreadCheck(); }
    void incrementCheckedPtrCount() const final { CanMakeThreadSafeCheckedPtr::incrementCheckedPtrCount(); }
    void decrementCheckedPtrCount() const final { CanMakeThreadSafeCheckedPtr::decrementCheckedPtrCount(); }

    void startLoading();
    void destroy();

    virtual bool isUnifiedPDFPlugin() const { return false; }
    virtual bool isLegacyPDFPlugin() const { return false; }

    virtual WebCore::PluginLayerHostingStrategy layerHostingStrategy() const = 0;
    virtual PlatformLayer* platformLayer() const { return nullptr; }
    virtual WebCore::GraphicsLayer* graphicsLayer() const { return nullptr; }
    RefPtr<WebCore::GraphicsLayer> protectedGraphicsLayer() const;

    virtual void setView(PluginView&);

    virtual void willDetachRenderer();

    virtual bool isComposited() const { return false; }

    virtual bool shouldCreateTransientPaintingSnapshot() const { return false; }
    virtual RefPtr<WebCore::ShareableBitmap> snapshot() { return nullptr; }
    virtual void paint(WebCore::GraphicsContext&, const WebCore::IntRect&) { }

    virtual double scaleFactor() const = 0;
    virtual void setPageScaleFactor(double, std::optional<WebCore::IntPoint> origin) = 0;
    virtual void mainFramePageScaleFactorDidChange() { }

    virtual double minScaleFactor() const { return 0.25; }
    virtual double maxScaleFactor() const { return 5; }

    bool isLocked() const;

    RetainPtr<PDFDocument> pdfDocument() const { return m_pdfDocument; }
    WebCore::FloatSize pdfDocumentSizeForPrinting() const;

    virtual bool geometryDidChange(const WebCore::IntSize& pluginSize, const WebCore::AffineTransform& pluginToRootViewTransform);
    virtual void visibilityDidChange(bool);
    virtual void deviceScaleFactorChanged(float) { }

    bool handlesPageScaleFactor() const;
    virtual void didBeginMagnificationGesture() { }
    virtual void didEndMagnificationGesture() { }

    void updateControlTints(WebCore::GraphicsContext&);

    RefPtr<WebCore::FragmentedSharedBuffer> liveResourceData() const;

    virtual bool wantsWheelEvents() const = 0;
    virtual bool handleMouseEvent(const WebMouseEvent&) = 0;
    virtual bool handleWheelEvent(const WebWheelEvent&) = 0;
    virtual bool handleMouseEnterEvent(const WebMouseEvent&) = 0;
    virtual bool handleMouseLeaveEvent(const WebMouseEvent&) = 0;
    virtual bool handleContextMenuEvent(const WebMouseEvent&) = 0;
    virtual bool handleKeyboardEvent(const WebKeyboardEvent&) = 0;
    virtual bool handleEditingCommand(const String& commandName, const String& argument) = 0;
    virtual bool isEditingCommandEnabled(const String& commandName) = 0;

    virtual String fullDocumentString() const { return { }; }
    virtual String selectionString() const = 0;
    virtual std::pair<String, String> stringsBeforeAndAfterSelection(int /* characterCount */) const { return { }; }
    virtual bool existingSelectionContainsPoint(const WebCore::FloatPoint&) const = 0;
    virtual WebCore::FloatRect rectForSelectionInRootView(PDFSelection *) const = 0;

    unsigned countFindMatches(const String& target, WebCore::FindOptions, unsigned maxMatchCount);
    virtual bool findString(const String& target, WebCore::FindOptions, unsigned maxMatchCount) = 0;
    virtual Vector<WebCore::FloatRect> rectsForTextMatchesInRect(const WebCore::IntRect&) const { return { }; }
    virtual bool drawsFindOverlay() const = 0;
    virtual RefPtr<WebCore::TextIndicator> textIndicatorForCurrentSelection(OptionSet<WebCore::TextIndicatorOption>, WebCore::TextIndicatorPresentationTransition) { return { }; }
    virtual WebCore::DictionaryPopupInfo dictionaryPopupInfoForSelection(PDFSelection *, WebCore::TextIndicatorPresentationTransition) = 0;

    virtual Vector<WebFoundTextRange::PDFData> findTextMatches(const String& target, WebCore::FindOptions) = 0;
    virtual Vector<WebCore::FloatRect> rectsForTextMatchesInRect(const Vector<WebFoundTextRange::PDFData>&, const WebCore::IntRect&) = 0;
    virtual RefPtr<WebCore::TextIndicator> textIndicatorForTextMatch(const WebFoundTextRange::PDFData&, WebCore::TextIndicatorPresentationTransition) { return { }; }
    virtual void scrollToRevealTextMatch(const WebFoundTextRange::PDFData&) { }

    virtual bool performDictionaryLookupAtLocation(const WebCore::FloatPoint&) = 0;
    void performWebSearch(const String& query);

    bool performImmediateActionHitTestAtLocation(const WebCore::FloatPoint&, WebHitTestResultData&);
    virtual std::pair<String, RetainPtr<PDFSelection>> textForImmediateActionHitTestAtPoint(const WebCore::FloatPoint&, WebHitTestResultData&) = 0;

    virtual id accessibilityHitTest(const WebCore::IntPoint&) const = 0;
    virtual id accessibilityObject() const = 0;
    id accessibilityAssociatedPluginParentForElement(WebCore::Element*) const;

    bool isBeingDestroyed() const { return m_isBeingDestroyed; }
    virtual void releaseMemory() { }

    bool isFullFramePlugin() const;
    WebCore::IntSize size() const { return m_size; }

    void streamDidReceiveResponse(const WebCore::ResourceResponse&);
    void streamDidReceiveData(const WebCore::SharedBuffer&);
    void streamDidFinishLoading();
    void streamDidFail();

    template<typename T>
    T convertFromRootViewToPlugin(const T& value) const
    {
        if constexpr (CanMakeFloatRect<T>)
            return m_rootViewToPluginTransform.mapRect(value);
        else
            return m_rootViewToPluginTransform.mapPoint(value);
    }

    template<typename T>
    T convertFromPluginToRootView(const T& value) const
    {
        auto inverseTransform = m_rootViewToPluginTransform.inverse();
        if constexpr (CanMakeFloatRect<T>)
            return inverseTransform->mapRect(value);
        else
            return inverseTransform->mapPoint(value);
    }

    WebCore::IntRect boundsOnScreen() const;

    bool showContextMenuAtPoint(const WebCore::IntPoint&);
    WebCore::AXObjectCache* axObjectCache() const;

    WebCore::ScrollPosition scrollPositionForTesting() const { return scrollPosition(); }
    WebCore::Scrollbar* horizontalScrollbar() const override { return m_horizontalScrollbar.get(); }
    WebCore::Scrollbar* verticalScrollbar() const override { return m_verticalScrollbar.get(); }
    RefPtr<WebCore::Scrollbar> protectedHorizontalScrollbar() const { return horizontalScrollbar(); }
    RefPtr<WebCore::Scrollbar> protectedVerticalScrollbar() const { return verticalScrollbar(); }
    void setScrollOffset(const WebCore::ScrollOffset&) final;

    virtual void willAttachScrollingNode() { }
    virtual void didAttachScrollingNode() { }
    virtual void didChangeSettings() { }

    // HUD Actions.
#if ENABLE(PDF_HUD)
    virtual void zoomIn() = 0;
    virtual void zoomOut() = 0;
    void save(CompletionHandler<void(const String&, const URL&, std::span<const uint8_t>)>&&);
#endif

    void openWithPreview(CompletionHandler<void(const String&, std::optional<FrameInfoData>&&, std::span<const uint8_t>)>&&);

    void notifyCursorChanged(WebCore::PlatformCursorType);

    WebCore::ScrollPosition scrollPosition() const final;

#if PLATFORM(MAC)
    PDFPluginAnnotation* activeAnnotation() const { return m_activeAnnotation.get(); }
    RefPtr<PDFPluginAnnotation> protectedActiveAnnotation() const;
#endif

    enum class IsInPluginCleanup : bool { No, Yes };

    struct SetActiveAnnotationParams {
        RetainPtr<PDFAnnotation> annotation;
        IsInPluginCleanup isInPluginCleanup { IsInPluginCleanup::No };
    };

    virtual void setActiveAnnotation(SetActiveAnnotationParams&&) = 0;
    void didMutatePDFDocument() { m_pdfDocumentWasMutated = true; }

    virtual CGRect pluginBoundsForAnnotation(PDFAnnotation*) const = 0;
    virtual void focusNextAnnotation() = 0;
    virtual void focusPreviousAnnotation() = 0;

    virtual Vector<WebCore::FloatRect> annotationRectsForTesting() const { return { }; }
    virtual void setTextAnnotationValueForTesting(unsigned pageIndex, unsigned annotationIndex, const String& value) { }
    virtual void setPDFDisplayModeForTesting(const String&) { }
    void registerPDFTest(RefPtr<WebCore::VoidCallback>&&);

    void navigateToURL(const URL&, std::optional<WebCore::PlatformMouseEvent>&& = std::nullopt);

    virtual void attemptToUnlockPDF(const String& password) = 0;

#if HAVE(INCREMENTAL_PDF_APIS)
    bool incrementalPDFLoadingEnabled() const { return m_incrementalPDFLoadingEnabled; }
    void receivedNonLinearizedPDFSentinel();
    void startByteRangeRequest(WebCore::NetscapePlugInStreamLoaderClient&, ByteRangeRequestIdentifier, uint64_t position, size_t count);
    void adoptBackgroundThreadDocument(RetainPtr<PDFDocument>&&);
    void maybeClearHighLatencyDataProviderFlag();
#endif

    void notifySelectionChanged();

    virtual void windowActivityDidChange() { }

    virtual void didChangeIsInWindow() { }

    virtual void didSameDocumentNavigationForFrame(WebFrame&) { }

    using PasteboardItem = PDFPluginPasteboardItem;
    void writeItemsToGeneralPasteboard(Vector<PasteboardItem>&&) const;

    uint64_t streamedBytes() const;
    std::optional<WebCore::FrameIdentifier> rootFrameID() const final;

#if PLATFORM(IOS_FAMILY)
    virtual void setSelectionRange(WebCore::FloatPoint /* pointInRootView */, WebCore::TextGranularity) { }
    virtual void clearSelection() { }
    virtual std::pair<URL, WebCore::FloatRect> linkURLAndBoundsAtPoint(WebCore::FloatPoint /* pointInRootView */) const { return { }; }
    virtual std::tuple<URL, WebCore::FloatRect, RefPtr<WebCore::TextIndicator>> linkDataAtPoint(WebCore::FloatPoint /* pointInRootView */) { return { }; }
    virtual std::optional<WebCore::FloatRect> highlightRectForTapAtPoint(WebCore::FloatPoint /* pointInRootView */) const { return std::nullopt; }
    virtual void handleSyntheticClick(WebCore::PlatformMouseEvent&&) { }
    virtual SelectionWasFlipped moveSelectionEndpoint(WebCore::FloatPoint /* pointInRootView */, SelectionEndpoint);
    virtual SelectionEndpoint extendInitialSelection(WebCore::FloatPoint /* pointInRootView */, WebCore::TextGranularity);
    virtual CursorContext cursorContext(WebCore::FloatPoint /* pointInRootView */) const { return { }; }
    virtual DocumentEditingContext documentEditingContext(DocumentEditingContextRequest&&) const;
#endif

    bool populateEditorStateIfNeeded(EditorState&) const;

    virtual bool shouldSizeToFitContent() const { return false; }

    virtual WebCore::FloatRect absoluteBoundingRectForSmartMagnificationAtPoint(WebCore::FloatPoint) const { return { }; }

    virtual void frameViewLayoutOrVisualViewportChanged(const WebCore::IntRect&) { }

    virtual bool delegatesScrollingToMainFrame() const { return false; }

protected:
    virtual double contentScaleFactor() const = 0;
    virtual bool platformPopulateEditorStateIfNeeded(EditorState&) const { return false; }

private:
    bool documentFinishedLoading() const { return m_documentFinishedLoading; }
    void ensureDataBufferLength(uint64_t) WTF_REQUIRES_LOCK(m_streamedDataLock);

    bool haveStreamedDataForRange(uint64_t offset, size_t count) const WTF_REQUIRES_LOCK(m_streamedDataLock);
    // This just checks whether the CFData is large enough; it doesn't know if we filled this range with data.

    void insertRangeRequestData(uint64_t offset, const Vector<uint8_t>&);

    // Returns the number of bytes copied.
    size_t copyDataAtPosition(std::span<uint8_t> buffer, uint64_t sourcePosition) const;
    // FIXME: It would be nice to avoid having both the "copy into a buffer" and "return a pointer" ways of getting data.
    void dataSpanForRange(uint64_t sourcePosition, size_t count, CheckValidRanges, CompletionHandler<void(std::span<const uint8_t>)>&&) const;
    // Returns true only if we can satisfy all of the requests.
    bool getByteRanges(CFMutableArrayRef, std::span<const CFRange>) const;

#if !LOG_DISABLED
    std::optional<uint64_t> streamedBytesForDebugLogging() const;
    void incrementalLoaderLogWithBytes(const String&, std::optional<uint64_t>&& streamedBytes);
#endif

protected:
    explicit PDFPluginBase(WebCore::HTMLPlugInElement&);

    WebPage* webPage() const;
    WebCore::Page* page() const;

    virtual void teardown();

    bool supportsForms() const;

    void createPDFDocument();
    virtual void installPDFDocument() = 0;
    void tryRunScriptsInPDFDocument();

    virtual unsigned firstPageHeight() const = 0;

    NSData *originalData() const;
    NSData *liveData() const;

    void addArchiveResource();

    void invalidateRect(const WebCore::IntRect&);

    void print();

    // ScrollableArea functions.
    WebCore::IntRect scrollCornerRect() const final;
    WebCore::ScrollableArea* enclosingScrollableArea() const final;
    bool scrollAnimatorEnabled() const final { return true; }
#if ENABLE(FORM_CONTROL_REFRESH)
    bool formControlRefreshEnabled() const final;
#endif
    bool isScrollableOrRubberbandable() final { return true; }
    bool hasScrollableOrRubberbandableAncestor() final { return true; }
    WebCore::IntRect scrollableAreaBoundingBox(bool* = nullptr) const final;
    bool isActive() const final;
    bool isScrollCornerVisible() const final { return false; }
    WebCore::ScrollPosition minimumScrollPosition() const final;
    WebCore::ScrollPosition maximumScrollPosition() const final;
    WebCore::IntSize visibleSize() const final { return m_size; }
    WebCore::IntSize overhangAmount() const final;
    WebCore::IntPoint lastKnownMousePositionInView() const override;

    float deviceScaleFactor() const override;
    bool shouldSuspendScrollAnimations() const final { return false; } // If we return true, ScrollAnimatorMac will keep cycling a timer forever, waiting for a good time to animate.
    void scrollbarStyleChanged(WebCore::ScrollbarStyle, bool forceUpdate) override;

    WebCore::IntRect convertFromScrollbarToContainingView(const WebCore::Scrollbar&, const WebCore::IntRect& scrollbarRect) const final;
    WebCore::IntRect convertFromContainingViewToScrollbar(const WebCore::Scrollbar&, const WebCore::IntRect& parentRect) const final;
    WebCore::IntPoint convertFromScrollbarToContainingView(const WebCore::Scrollbar&, const WebCore::IntPoint& scrollbarPoint) const final;
    WebCore::IntPoint convertFromContainingViewToScrollbar(const WebCore::Scrollbar&, const WebCore::IntPoint& parentPoint) const final;

    bool forceUpdateScrollbarsOnMainThreadForPerformanceTesting() const final;
    bool shouldPlaceVerticalScrollbarOnLeft() const final { return false; }

    WebCore::IntRect viewRelativeVerticalScrollbarRect() const;
    WebCore::IntRect viewRelativeHorizontalScrollbarRect() const;
    WebCore::IntRect viewRelativeScrollCornerRect() const;

    String debugDescription() const final;

    // Scrolling, but not ScrollableArea:
    virtual void didChangeScrollOffset() = 0;
    virtual void updateScrollbars();
    virtual Ref<WebCore::Scrollbar> createScrollbar(WebCore::ScrollbarOrientation);
    virtual void destroyScrollbar(WebCore::ScrollbarOrientation);

    void wantsWheelEventsChanged();

    virtual void incrementalLoadingDidProgress() { }
    virtual void incrementalLoadingDidCancel() { }
    virtual void incrementalLoadingDidFinish() { }

#if ENABLE(PDF_HUD)
    void updateHUDLocation();
    WebCore::IntRect frameForHUDInRootViewCoordinates() const;
    bool hudEnabled() const;
    bool shouldShowHUD() const;
    void updateHUDVisibility();
#endif

#if !LOG_DISABLED
    void incrementalLoaderLog(const String&);
#endif

    virtual void teardownPasswordEntryForm() = 0;

    String annotationStyle() const;

    static NSString *htmlPasteboardType();
    static NSString *rtfPasteboardType();
    static NSString *stringPasteboardType();
    static NSString *urlPasteboardType();

#if PLATFORM(MAC)
    void writeStringToFindPasteboard(const String&) const;
#endif

    std::optional<WebCore::PageIdentifier> pageIdentifier() const;

    static WebCore::Color pluginBackgroundColor();

    RefPtr<PluginView> protectedView() const;
    RefPtr<WebFrame> protectedFrame() const;

    SingleThreadWeakPtr<PluginView> m_view;
    WeakPtr<WebFrame> m_frame;
    WeakPtr<WebCore::HTMLPlugInElement, WebCore::WeakPtrImplWithEventTargetData> m_element;

    // m_data grows as we receive data in the primary request (PDFPluginBase::streamDidReceiveData())
    // but also as byte range requests are received via m_incrementalLoader, so it may have "holes"
    // before the main resource is fully loaded.
    mutable Lock m_streamedDataLock;
    RetainPtr<CFMutableDataRef> m_data WTF_GUARDED_BY_LOCK(m_streamedDataLock);
    uint64_t m_streamedBytes WTF_GUARDED_BY_LOCK(m_streamedDataLock) { 0 };
    RangeSet<WTF::Range<uint64_t>> m_validRanges WTF_GUARDED_BY_LOCK(m_streamedDataLock);

    RetainPtr<PDFDocument> m_pdfDocument;

    const RetainPtr<WKAccessibilityPDFDocumentObject> m_accessibilityDocumentObject;

    String m_suggestedFilename;

    String m_lastFindString;

    WebCore::IntSize m_size;
    WebCore::AffineTransform m_rootViewToPluginTransform;

    WebCore::IntSize m_scrollOffset;
    std::optional<WebMouseEvent> m_lastMouseEvent;

    RefPtr<WebCore::Scrollbar> m_horizontalScrollbar;
    RefPtr<WebCore::Scrollbar> m_verticalScrollbar;

    bool m_documentFinishedLoading { false };
    bool m_isBeingDestroyed { false };
    std::atomic<bool> m_hasBeenDestroyed { false };
    bool m_didRunScripts { false };

#if PLATFORM(MAC)
    RefPtr<PDFPluginAnnotation> m_activeAnnotation;
#endif
    RefPtr<WebCore::Element> m_annotationContainer;
    bool m_pdfDocumentWasMutated { false };

#if HAVE(INCREMENTAL_PDF_APIS)
    RefPtr<PDFIncrementalLoader> m_incrementalLoader;
    std::atomic<bool> m_incrementalPDFLoadingEnabled { false };
#endif

    RefPtr<WebCore::VoidCallback> m_pdfTestCallback;

#if ENABLE(PDF_HUD)
    CompletionHandler<void(const String&, const URL&, std::span<const uint8_t>)> m_pendingSaveCompletionHandler;
    CompletionHandler<void(const String&, std::optional<FrameInfoData>&&, std::span<const uint8_t>)> m_pendingOpenCompletionHandler;
#endif

    mutable std::optional<bool> m_cachedIsFullFramePlugin;
};

} // namespace WebKit

#endif // ENABLE(PDF_PLUGIN)
