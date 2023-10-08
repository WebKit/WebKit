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

#include <WebCore/AffineTransform.h>
#include <WebCore/FindOptions.h>
#include <WebCore/FloatRect.h>
#include <WebCore/PluginData.h>
#include <WebCore/PluginViewBase.h>
#include <wtf/RetainPtr.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/TypeTraits.h>
#include <wtf/WeakPtr.h>

// FIXME: These Objective-C classes should only appear in PDFPlugin, not here in the base class.
OBJC_CLASS NSDictionary;
OBJC_CLASS PDFDocument;
OBJC_CLASS PDFSelection;

namespace WebCore {
class FragmentedSharedBuffer;
class GraphicsContext;
class Element;
class HTMLPlugInElement;
class ResourceResponse;
class SharedBuffer;
}

namespace WebKit {

class PluginView;
class ShareableBitmap;
class WebFrame;
class WebKeyboardEvent;
class WebMouseEvent;
class WebWheelEvent;
struct WebHitTestResultData;

class PDFPluginBase : public ThreadSafeRefCounted<PDFPluginBase> {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(PDFPluginBase);
public:
    static WebCore::PluginInfo pluginInfo();

    virtual ~PDFPluginBase() = default;

    void destroy();

    virtual bool isUnifiedPDFPlugin() const { return false; }
    virtual bool isLegacyPDFPlugin() const { return false; }

    virtual WebCore::PluginLayerHostingStrategy layerHostingStrategy() const = 0;
    virtual PlatformLayer* platformLayer() const { return nullptr; }
    virtual WebCore::GraphicsLayer* graphicsLayer() const { return nullptr; }

    virtual void setView(PluginView&);

    virtual void willDetachRenderer() { }

    virtual bool isComposited() const { return false; }

    virtual void paint(WebCore::GraphicsContext&, const WebCore::IntRect&) { }

    virtual CGFloat scaleFactor() const = 0;

    // FIXME: Can we use PDFDocument here?
    virtual RetainPtr<PDFDocument> pdfDocumentForPrinting() const = 0;
    virtual WebCore::FloatSize pdfDocumentSizeForPrinting() const = 0;

    virtual void geometryDidChange(const WebCore::IntSize& pluginSize, const WebCore::AffineTransform& pluginToRootViewTransform);
    virtual void visibilityDidChange(bool) { }
    virtual void contentsScaleFactorChanged(float) { }

    virtual void updateControlTints(WebCore::GraphicsContext&) { }

    virtual RefPtr<WebCore::FragmentedSharedBuffer> liveResourceData() const = 0;

    virtual bool wantsWheelEvents() const { return false; }
    virtual bool handleMouseEvent(const WebMouseEvent&) = 0;
    virtual bool handleWheelEvent(const WebWheelEvent&) = 0;
    virtual bool handleMouseEnterEvent(const WebMouseEvent&) = 0;
    virtual bool handleMouseLeaveEvent(const WebMouseEvent&) = 0;
    virtual bool handleContextMenuEvent(const WebMouseEvent&) = 0;
    virtual bool handleKeyboardEvent(const WebKeyboardEvent&) = 0;
    virtual bool handleEditingCommand(StringView commandName) = 0;
    virtual bool isEditingCommandEnabled(StringView commandName) = 0;

    virtual String getSelectionString() const = 0;
    virtual bool existingSelectionContainsPoint(const WebCore::FloatPoint&) const = 0;
    virtual WebCore::FloatRect rectForSelectionInRootView(PDFSelection *) const = 0;

    virtual unsigned countFindMatches(const String& target, WebCore::FindOptions, unsigned maxMatchCount) = 0;
    virtual bool findString(const String& target, WebCore::FindOptions, unsigned maxMatchCount) = 0;

    virtual bool performDictionaryLookupAtLocation(const WebCore::FloatPoint&) = 0;
    virtual std::tuple<String, PDFSelection *, NSDictionary *> lookupTextAtLocation(const WebCore::FloatPoint&, WebHitTestResultData&) const = 0;

    virtual RefPtr<ShareableBitmap> snapshot() = 0;

    virtual id accessibilityHitTest(const WebCore::IntPoint&) const = 0;
    virtual id accessibilityObject() const = 0;
    virtual id accessibilityAssociatedPluginParentForElement(WebCore::Element*) const = 0;

    bool isBeingDestroyed() const { return m_isBeingDestroyed; }

    bool isFullFramePlugin() const;
    WebCore::IntSize size() const { return m_size; }

    void streamDidReceiveResponse(const WebCore::ResourceResponse&);
    void streamDidReceiveData(const WebCore::SharedBuffer&);
    void streamDidFinishLoading();
    void streamDidFail();

    WebCore::IntPoint convertFromRootViewToPlugin(const WebCore::IntPoint&) const;

protected:
    explicit PDFPluginBase(WebCore::HTMLPlugInElement&);

    virtual void teardown() = 0;

    virtual void createPDFDocument() = 0;
    virtual void installPDFDocument() = 0;
    virtual void tryRunScriptsInPDFDocument() { }

    virtual void incrementalPDFStreamDidReceiveData(const WebCore::SharedBuffer&) { }
    virtual bool incrementalPDFStreamDidFinishLoading() { return false; }
    virtual void incrementalPDFStreamDidFail() { }

    void ensureDataBufferLength(uint64_t);
    void addArchiveResource();

    void invalidateRect(const WebCore::IntRect&);

    WeakPtr<PluginView> m_view;
    WeakPtr<WebFrame> m_frame;

    RetainPtr<CFMutableDataRef> m_data;

    String m_suggestedFilename;
    uint64_t m_streamedBytes { 0 };

    WebCore::IntSize m_size;
    WebCore::AffineTransform m_rootViewToPluginTransform;

    bool m_documentFinishedLoading { false };
    bool m_isBeingDestroyed { false };
    bool m_hasBeenDestroyed { false };
};

} // namespace WebKit

#endif // ENABLE(PDF_PLUGIN)
