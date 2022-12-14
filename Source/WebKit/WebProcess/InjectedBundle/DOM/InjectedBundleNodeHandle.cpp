/*
 * Copyright (C) 2010-2022 Apple Inc. All rights reserved.
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

#include "config.h"
#include "InjectedBundleNodeHandle.h"

#include "InjectedBundleRangeHandle.h"
#include "ShareableBitmap.h"
#include "WebFrame.h"
#include "WebFrameLoaderClient.h"
#include "WebImage.h"
#include <JavaScriptCore/APICast.h>
#include <WebCore/Document.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/FrameView.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/HTMLFrameElement.h>
#include <WebCore/HTMLIFrameElement.h>
#include <WebCore/HTMLInputElement.h>
#include <WebCore/HTMLNames.h>
#include <WebCore/HTMLSelectElement.h>
#include <WebCore/HTMLTableCellElement.h>
#include <WebCore/HTMLTextAreaElement.h>
#include <WebCore/IntRect.h>
#include <WebCore/JSNode.h>
#include <WebCore/Node.h>
#include <WebCore/Page.h>
#include <WebCore/Position.h>
#include <WebCore/Range.h>
#include <WebCore/RenderObject.h>
#include <WebCore/SimpleRange.h>
#include <WebCore/Text.h>
#include <WebCore/VisiblePosition.h>
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/WTFString.h>

namespace WebKit {
using namespace WebCore;
using namespace HTMLNames;

typedef HashMap<Node*, InjectedBundleNodeHandle*> DOMNodeHandleCache;

static DOMNodeHandleCache& domNodeHandleCache()
{
    static NeverDestroyed<DOMNodeHandleCache> cache;
    return cache;
}

RefPtr<InjectedBundleNodeHandle> InjectedBundleNodeHandle::getOrCreate(JSContextRef, JSObjectRef object)
{
    Node* node = JSNode::toWrapped(toJS(object)->vm(), toJS(object));
    return getOrCreate(node);
}

RefPtr<InjectedBundleNodeHandle> InjectedBundleNodeHandle::getOrCreate(Node* node)
{
    if (!node)
        return nullptr;

    return InjectedBundleNodeHandle::getOrCreate(*node);
}

Ref<InjectedBundleNodeHandle> InjectedBundleNodeHandle::getOrCreate(Node& node)
{
    if (auto* existingHandle = domNodeHandleCache().get(&node))
        return Ref<InjectedBundleNodeHandle>(*existingHandle);

    auto nodeHandle = InjectedBundleNodeHandle::create(node);
    if (nodeHandle->coreNode())
        domNodeHandleCache().add(nodeHandle->coreNode(), nodeHandle.ptr());
    return nodeHandle;
}

Ref<InjectedBundleNodeHandle> InjectedBundleNodeHandle::create(Node& node)
{
    auto handle = adoptRef(*new InjectedBundleNodeHandle(node));
    handle->suspendIfNeeded();
    return handle;
}

InjectedBundleNodeHandle::InjectedBundleNodeHandle(Node& node)
    : ActiveDOMObject(node.document())
    , m_node(&node)
{
}

InjectedBundleNodeHandle::~InjectedBundleNodeHandle()
{
    if (m_node)
        domNodeHandleCache().remove(m_node.get());
}

Node* InjectedBundleNodeHandle::coreNode()
{
    return m_node.get();
}

RefPtr<InjectedBundleNodeHandle> InjectedBundleNodeHandle::document()
{
    if (!m_node)
        return nullptr;

    return getOrCreate(m_node->document());
}

// Additional DOM Operations
// Note: These should only be operations that are not exposed to JavaScript.

IntRect InjectedBundleNodeHandle::elementBounds()
{
    if (!is<Element>(m_node))
        return IntRect();

    return downcast<Element>(*m_node).boundsInRootViewSpace();
}
    
IntRect InjectedBundleNodeHandle::renderRect(bool* isReplaced)
{
    if (!m_node)
        return { };

    return m_node->pixelSnappedRenderRect(isReplaced);
}

static RefPtr<WebImage> imageForRect(FrameView* frameView, const IntRect& paintingRect, const std::optional<float>& bitmapWidth, SnapshotOptions options)
{
    if (paintingRect.isEmpty())
        return nullptr;

    float bitmapScaleFactor;
    IntSize bitmapSize;
    if (bitmapWidth) {
        bitmapScaleFactor = bitmapWidth.value() / paintingRect.width();
        bitmapSize = roundedIntSize(FloatSize(bitmapWidth.value(), paintingRect.height() * bitmapScaleFactor));
    } else {
        bitmapScaleFactor = 1;
        bitmapSize = paintingRect.size();
    }

    float deviceScaleFactor = frameView->frame().page()->deviceScaleFactor();
    bitmapSize.scale(deviceScaleFactor);

    if (bitmapSize.isEmpty())
        return nullptr;

    auto snapshot = WebImage::create(bitmapSize, snapshotOptionsToImageOptions(options), DestinationColorSpace::SRGB());
    if (!snapshot)
        return nullptr;

    auto& graphicsContext = snapshot->context();

    graphicsContext.clearRect(IntRect(IntPoint(), bitmapSize));
    graphicsContext.applyDeviceScaleFactor(deviceScaleFactor);
    graphicsContext.scale(bitmapScaleFactor);
    graphicsContext.translate(-paintingRect.location());

    FrameView::SelectionInSnapshot shouldPaintSelection = FrameView::IncludeSelection;
    if (options & SnapshotOptionsExcludeSelectionHighlighting)
        shouldPaintSelection = FrameView::ExcludeSelection;

    auto paintBehavior = frameView->paintBehavior() | PaintBehavior::FlattenCompositingLayers | PaintBehavior::Snapshotting;
    if (options & SnapshotOptionsForceBlackText)
        paintBehavior.add(PaintBehavior::ForceBlackText);
    if (options & SnapshotOptionsForceWhiteText)
        paintBehavior.add(PaintBehavior::ForceWhiteText);

    auto oldPaintBehavior = frameView->paintBehavior();
    frameView->setPaintBehavior(paintBehavior);
    frameView->paintContentsForSnapshot(graphicsContext, paintingRect, shouldPaintSelection, FrameView::DocumentCoordinates);
    frameView->setPaintBehavior(oldPaintBehavior);

    return snapshot;
}

RefPtr<WebImage> InjectedBundleNodeHandle::renderedImage(SnapshotOptions options, bool shouldExcludeOverflow, const std::optional<float>& bitmapWidth)
{
    if (!m_node)
        return nullptr;

    Frame* frame = m_node->document().frame();
    if (!frame)
        return nullptr;

    FrameView* frameView = frame->view();
    if (!frameView)
        return nullptr;

    m_node->document().updateLayout();

    RenderObject* renderer = m_node->renderer();
    if (!renderer)
        return nullptr;

    IntRect paintingRect;
    if (shouldExcludeOverflow)
        paintingRect = renderer->absoluteBoundingBoxRectIgnoringTransforms();
    else {
        LayoutRect topLevelRect;
        paintingRect = snappedIntRect(renderer->paintingRootRect(topLevelRect));
    }

    frameView->setNodeToDraw(m_node.get());
    auto image = imageForRect(frameView, paintingRect, bitmapWidth, options);
    frameView->setNodeToDraw(0);

    return image;
}

RefPtr<InjectedBundleRangeHandle> InjectedBundleNodeHandle::visibleRange()
{
    if (!m_node)
        return nullptr;
    VisiblePosition start = firstPositionInNode(m_node.get());
    VisiblePosition end = lastPositionInNode(m_node.get());
    return createHandle(makeSimpleRange(start, end));
}

void InjectedBundleNodeHandle::setHTMLInputElementValueForUser(const String& value)
{
    if (!is<HTMLInputElement>(m_node))
        return;

    downcast<HTMLInputElement>(*m_node).setValueForUser(value);
}

void InjectedBundleNodeHandle::setHTMLInputElementSpellcheckEnabled(bool enabled)
{
    if (!is<HTMLInputElement>(m_node))
        return;

    downcast<HTMLInputElement>(*m_node).setSpellcheckDisabledExceptTextReplacement(!enabled);
}

bool InjectedBundleNodeHandle::isHTMLInputElementAutoFilled() const
{
    if (!is<HTMLInputElement>(m_node))
        return false;
    
    return downcast<HTMLInputElement>(*m_node).isAutoFilled();
}

bool InjectedBundleNodeHandle::isHTMLInputElementAutoFilledAndViewable() const
{
    if (!is<HTMLInputElement>(m_node))
        return false;

    return downcast<HTMLInputElement>(*m_node).isAutoFilledAndViewable();
}

bool InjectedBundleNodeHandle::isHTMLInputElementAutoFilledAndObscured() const
{
    if (!is<HTMLInputElement>(m_node))
        return false;

    return downcast<HTMLInputElement>(*m_node).isAutoFilledAndObscured();
}

void InjectedBundleNodeHandle::setHTMLInputElementAutoFilled(bool filled)
{
    if (!is<HTMLInputElement>(m_node))
        return;

    downcast<HTMLInputElement>(*m_node).setAutoFilled(filled);
}

void InjectedBundleNodeHandle::setHTMLInputElementAutoFilledAndViewable(bool autoFilledAndViewable)
{
    if (!is<HTMLInputElement>(m_node))
        return;

    downcast<HTMLInputElement>(*m_node).setAutoFilledAndViewable(autoFilledAndViewable);
}

void InjectedBundleNodeHandle::setHTMLInputElementAutoFilledAndObscured(bool autoFilledAndObscured)
{
    if (!is<HTMLInputElement>(m_node))
        return;

    downcast<HTMLInputElement>(*m_node).setAutoFilledAndObscured(autoFilledAndObscured);
}

bool InjectedBundleNodeHandle::isHTMLInputElementAutoFillButtonEnabled() const
{
    if (!is<HTMLInputElement>(m_node))
        return false;
    
    return downcast<HTMLInputElement>(*m_node).autoFillButtonType() != AutoFillButtonType::None;
}

void InjectedBundleNodeHandle::setHTMLInputElementAutoFillButtonEnabled(AutoFillButtonType autoFillButtonType)
{
    if (!is<HTMLInputElement>(m_node))
        return;

    downcast<HTMLInputElement>(*m_node).setShowAutoFillButton(autoFillButtonType);
}

AutoFillButtonType InjectedBundleNodeHandle::htmlInputElementAutoFillButtonType() const
{
    if (!is<HTMLInputElement>(m_node))
        return AutoFillButtonType::None;
    return downcast<HTMLInputElement>(*m_node).autoFillButtonType();
}

AutoFillButtonType InjectedBundleNodeHandle::htmlInputElementLastAutoFillButtonType() const
{
    if (!is<HTMLInputElement>(m_node))
        return AutoFillButtonType::None;
    return downcast<HTMLInputElement>(*m_node).lastAutoFillButtonType();
}

bool InjectedBundleNodeHandle::isAutoFillAvailable() const
{
    if (!is<HTMLInputElement>(m_node))
        return false;

    return downcast<HTMLInputElement>(*m_node).isAutoFillAvailable();
}

void InjectedBundleNodeHandle::setAutoFillAvailable(bool autoFillAvailable)
{
    if (!is<HTMLInputElement>(m_node))
        return;

    downcast<HTMLInputElement>(*m_node).setAutoFillAvailable(autoFillAvailable);
}

IntRect InjectedBundleNodeHandle::htmlInputElementAutoFillButtonBounds()
{
    if (!is<HTMLInputElement>(m_node))
        return IntRect();

    auto autoFillButton = downcast<HTMLInputElement>(*m_node).autoFillButtonElement();
    if (!autoFillButton)
        return IntRect();

    return autoFillButton->boundsInRootViewSpace();
}

bool InjectedBundleNodeHandle::htmlInputElementLastChangeWasUserEdit()
{
    if (!is<HTMLInputElement>(m_node))
        return false;

    return downcast<HTMLInputElement>(*m_node).lastChangeWasUserEdit();
}

bool InjectedBundleNodeHandle::htmlTextAreaElementLastChangeWasUserEdit()
{
    if (!is<HTMLTextAreaElement>(m_node))
        return false;

    return downcast<HTMLTextAreaElement>(*m_node).lastChangeWasUserEdit();
}

bool InjectedBundleNodeHandle::isTextField() const
{
    if (!is<HTMLInputElement>(m_node))
        return false;

    return downcast<HTMLInputElement>(*m_node).isTextField();
}

bool InjectedBundleNodeHandle::isSelectElement() const
{
    return is<HTMLSelectElement>(m_node);
}

bool InjectedBundleNodeHandle::isSelectableTextNode() const
{
    if (!is<Text>(m_node))
        return false;

    auto renderer = m_node->renderer();
    return renderer && renderer->style().effectiveUserSelect() != UserSelect::None;
}

RefPtr<InjectedBundleNodeHandle> InjectedBundleNodeHandle::htmlTableCellElementCellAbove()
{
    if (!is<HTMLTableCellElement>(m_node))
        return nullptr;

    return getOrCreate(downcast<HTMLTableCellElement>(*m_node).cellAbove());
}

RefPtr<WebFrame> InjectedBundleNodeHandle::documentFrame()
{
    if (!m_node || !m_node->isDocumentNode())
        return nullptr;

    Frame* frame = downcast<Document>(*m_node).frame();
    if (!frame)
        return nullptr;

    return WebFrame::fromCoreFrame(*frame);
}

RefPtr<WebFrame> InjectedBundleNodeHandle::htmlFrameElementContentFrame()
{
    if (!is<HTMLFrameElement>(m_node))
        return nullptr;

    auto* frame = dynamicDowncast<LocalFrame>(downcast<HTMLFrameElement>(*m_node).contentFrame());
    if (!frame)
        return nullptr;

    return WebFrame::fromCoreFrame(*frame);
}

RefPtr<WebFrame> InjectedBundleNodeHandle::htmlIFrameElementContentFrame()
{
    if (!is<HTMLIFrameElement>(m_node))
        return nullptr;

    auto* frame = dynamicDowncast<LocalFrame>(downcast<HTMLFrameElement>(*m_node).contentFrame());
    if (!frame)
        return nullptr;

    return WebFrame::fromCoreFrame(*frame);
}

void InjectedBundleNodeHandle::stop()
{
    // Invalidate handles to nodes inside documents that are about to be destroyed in order to prevent leaks.
    if (m_node) {
        domNodeHandleCache().remove(m_node.get());
        m_node = nullptr;
    }
}

const char* InjectedBundleNodeHandle::activeDOMObjectName() const
{
    return "InjectedBundleNodeHandle";
}

} // namespace WebKit
