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
#include "WebFrame.h"
#include "WebImage.h"
#include "WebLocalFrameLoaderClient.h"
#include <JavaScriptCore/APICast.h>
#include <WebCore/Document.h>
#include <WebCore/FrameLoader.h>
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
#include <WebCore/LocalFrame.h>
#include <WebCore/LocalFrameView.h>
#include <WebCore/Node.h>
#include <WebCore/Page.h>
#include <WebCore/Position.h>
#include <WebCore/Range.h>
#include <WebCore/RenderElement.h>
#include <WebCore/ShareableBitmap.h>
#include <WebCore/SimpleRange.h>
#include <WebCore/Text.h>
#include <WebCore/VisiblePosition.h>
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/WeakHashMap.h>
#include <wtf/WeakRef.h>
#include <wtf/text/WTFString.h>

namespace WebKit {
using namespace WebCore;
using namespace HTMLNames;

using DOMNodeHandleCache = WeakHashMap<Node, WeakRef<InjectedBundleNodeHandle>, WeakPtrImplWithEventTargetData>;

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
    if (auto existingHandle = domNodeHandleCache().get(node))
        return Ref<InjectedBundleNodeHandle>(*existingHandle);

    auto nodeHandle = InjectedBundleNodeHandle::create(node);
    if (nodeHandle->coreNode())
        domNodeHandleCache().add(*nodeHandle->coreNode(), nodeHandle.get());
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
        domNodeHandleCache().remove(*m_node);
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
    RefPtr element = dynamicDowncast<Element>(m_node);
    if (!element)
        return IntRect();

    return element->boundsInRootViewSpace();
}
    
IntRect InjectedBundleNodeHandle::absoluteBoundingRect(bool* isReplaced)
{
    if (!m_node)
        return { };

    return m_node->pixelSnappedAbsoluteBoundingRect(isReplaced);
}

static RefPtr<WebImage> imageForRect(LocalFrameView* frameView, const IntRect& paintingRect, const std::optional<float>& bitmapWidth, SnapshotOptions options)
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
    if (!snapshot->context())
        return nullptr;

    auto& graphicsContext = *snapshot->context();

    graphicsContext.clearRect(IntRect(IntPoint(), bitmapSize));
    graphicsContext.applyDeviceScaleFactor(deviceScaleFactor);
    graphicsContext.scale(bitmapScaleFactor);
    graphicsContext.translate(-paintingRect.location());

    auto shouldPaintSelection = LocalFrameView::IncludeSelection;
    if (options.contains(SnapshotOption::ExcludeSelectionHighlighting))
        shouldPaintSelection = LocalFrameView::ExcludeSelection;

    auto paintBehavior = frameView->paintBehavior() | PaintBehavior::FlattenCompositingLayers | PaintBehavior::Snapshotting;
    if (options.contains(SnapshotOption::ForceBlackText))
        paintBehavior.add(PaintBehavior::ForceBlackText);
    if (options.contains(SnapshotOption::ForceWhiteText))
        paintBehavior.add(PaintBehavior::ForceWhiteText);

    auto oldPaintBehavior = frameView->paintBehavior();
    frameView->setPaintBehavior(paintBehavior);
    frameView->paintContentsForSnapshot(graphicsContext, paintingRect, shouldPaintSelection, LocalFrameView::DocumentCoordinates);
    frameView->setPaintBehavior(oldPaintBehavior);

    return snapshot;
}

RefPtr<WebImage> InjectedBundleNodeHandle::renderedImage(SnapshotOptions options, bool shouldExcludeOverflow, const std::optional<float>& bitmapWidth)
{
    if (!m_node)
        return nullptr;

    auto* frame = m_node->document().frame();
    if (!frame)
        return nullptr;

    auto* frameView = frame->view();
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
    RefPtr input = dynamicDowncast<HTMLInputElement>(m_node);
    if (!input)
        return;

    input->setValueForUser(value);
}

void InjectedBundleNodeHandle::setHTMLInputElementSpellcheckEnabled(bool enabled)
{
    RefPtr input = dynamicDowncast<HTMLInputElement>(m_node);
    if (!input)
        return;

    input->setSpellcheckDisabledExceptTextReplacement(!enabled);
}

bool InjectedBundleNodeHandle::isHTMLInputElementAutoFilled() const
{
    RefPtr input = dynamicDowncast<HTMLInputElement>(m_node);
    if (!input)
        return false;
    
    return input->isAutoFilled();
}

bool InjectedBundleNodeHandle::isHTMLInputElementAutoFilledAndViewable() const
{
    RefPtr input = dynamicDowncast<HTMLInputElement>(m_node);
    if (!input)
        return false;

    return input->isAutoFilledAndViewable();
}

bool InjectedBundleNodeHandle::isHTMLInputElementAutoFilledAndObscured() const
{
    RefPtr input = dynamicDowncast<HTMLInputElement>(m_node);
    if (!input)
        return false;

    return input->isAutoFilledAndObscured();
}

void InjectedBundleNodeHandle::setHTMLInputElementAutoFilled(bool filled)
{
    RefPtr input = dynamicDowncast<HTMLInputElement>(m_node);
    if (!input)
        return;

    input->setAutoFilled(filled);
}

void InjectedBundleNodeHandle::setHTMLInputElementAutoFilledAndViewable(bool autoFilledAndViewable)
{
    RefPtr input = dynamicDowncast<HTMLInputElement>(m_node);
    if (!input)
        return;

    input->setAutoFilledAndViewable(autoFilledAndViewable);
}

void InjectedBundleNodeHandle::setHTMLInputElementAutoFilledAndObscured(bool autoFilledAndObscured)
{
    RefPtr input = dynamicDowncast<HTMLInputElement>(m_node);
    if (!input)
        return;

    input->setAutoFilledAndObscured(autoFilledAndObscured);
}

bool InjectedBundleNodeHandle::isHTMLInputElementAutoFillButtonEnabled() const
{
    RefPtr input = dynamicDowncast<HTMLInputElement>(m_node);
    if (!input)
        return false;
    
    return input->autoFillButtonType() != AutoFillButtonType::None;
}

void InjectedBundleNodeHandle::setHTMLInputElementAutoFillButtonEnabled(AutoFillButtonType autoFillButtonType)
{
    RefPtr input = dynamicDowncast<HTMLInputElement>(m_node);
    if (!input)
        return;

    input->setShowAutoFillButton(autoFillButtonType);
}

AutoFillButtonType InjectedBundleNodeHandle::htmlInputElementAutoFillButtonType() const
{
    RefPtr input = dynamicDowncast<HTMLInputElement>(m_node);
    if (!input)
        return AutoFillButtonType::None;

    return input->autoFillButtonType();
}

AutoFillButtonType InjectedBundleNodeHandle::htmlInputElementLastAutoFillButtonType() const
{
    RefPtr input = dynamicDowncast<HTMLInputElement>(m_node);
    if (!input)
        return AutoFillButtonType::None;

    return input->lastAutoFillButtonType();
}

bool InjectedBundleNodeHandle::isAutoFillAvailable() const
{
    RefPtr input = dynamicDowncast<HTMLInputElement>(m_node);
    if (!input)
        return false;

    return input->isAutoFillAvailable();
}

void InjectedBundleNodeHandle::setAutoFillAvailable(bool autoFillAvailable)
{
    RefPtr input = dynamicDowncast<HTMLInputElement>(m_node);
    if (!input)
        return;

    input->setAutoFillAvailable(autoFillAvailable);
}

IntRect InjectedBundleNodeHandle::htmlInputElementAutoFillButtonBounds()
{
    RefPtr input = dynamicDowncast<HTMLInputElement>(m_node);
    if (!input)
        return IntRect();

    auto autoFillButton = input->autoFillButtonElement();
    if (!autoFillButton)
        return IntRect();

    return autoFillButton->boundsInRootViewSpace();
}

bool InjectedBundleNodeHandle::htmlInputElementLastChangeWasUserEdit()
{
    RefPtr input = dynamicDowncast<HTMLInputElement>(m_node);
    if (!input)
        return false;

    return input->lastChangeWasUserEdit();
}

bool InjectedBundleNodeHandle::htmlTextAreaElementLastChangeWasUserEdit()
{
    RefPtr textarea = dynamicDowncast<HTMLTextAreaElement>(m_node);
    if (!textarea)
        return false;

    return textarea->lastChangeWasUserEdit();
}

bool InjectedBundleNodeHandle::isTextField() const
{
    RefPtr input = dynamicDowncast<HTMLInputElement>(m_node);
    if (!input)
        return false;

    return input->isTextField();
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
    return renderer && renderer->style().usedUserSelect() != UserSelect::None;
}

RefPtr<InjectedBundleNodeHandle> InjectedBundleNodeHandle::htmlTableCellElementCellAbove()
{
    RefPtr tableCell = dynamicDowncast<HTMLTableCellElement>(m_node);
    if (!tableCell)
        return nullptr;

    return getOrCreate(tableCell->cellAbove());
}

RefPtr<WebFrame> InjectedBundleNodeHandle::documentFrame()
{
    RefPtr document = dynamicDowncast<Document>(m_node);
    if (!document)
        return nullptr;

    auto* frame = document->frame();
    if (!frame)
        return nullptr;

    return WebFrame::fromCoreFrame(*frame);
}

RefPtr<WebFrame> InjectedBundleNodeHandle::htmlIFrameElementContentFrame()
{
    auto* iframeElement = dynamicDowncast<HTMLIFrameElement>(m_node.get());
    if (!iframeElement)
        return nullptr;

    auto* frame = iframeElement->contentFrame();
    if (!frame)
        return nullptr;

    return WebFrame::fromCoreFrame(*frame);
}

void InjectedBundleNodeHandle::stop()
{
    // Invalidate handles to nodes inside documents that are about to be destroyed in order to prevent leaks.
    if (m_node) {
        domNodeHandleCache().remove(*m_node);
        m_node = nullptr;
    }
}

} // namespace WebKit
