/*
 * Copyright (C) 2010, 2015-2016 Apple Inc. All rights reserved.
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
#include "InjectedBundleRangeHandle.h"

#include "InjectedBundleNodeHandle.h"
#include "ShareableBitmap.h"
#include "WebImage.h"
#include <JavaScriptCore/APICast.h>
#include <JavaScriptCore/HeapInlines.h>
#include <WebCore/Document.h>
#include <WebCore/FloatRect.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameSelection.h>
#include <WebCore/FrameView.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/IntRect.h>
#include <WebCore/JSRange.h>
#include <WebCore/Page.h>
#include <WebCore/Range.h>
#include <WebCore/RenderView.h>
#include <WebCore/VisibleSelection.h>
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>

#if PLATFORM(MAC)
#include <WebCore/LocalDefaultSystemAppearance.h>
#endif

namespace WebKit {
using namespace WebCore;

typedef HashMap<Range*, InjectedBundleRangeHandle*> DOMRangeHandleCache;

static DOMRangeHandleCache& domRangeHandleCache()
{
    static NeverDestroyed<DOMRangeHandleCache> cache;
    return cache;
}

RefPtr<InjectedBundleRangeHandle> InjectedBundleRangeHandle::getOrCreate(JSContextRef context, JSObjectRef object)
{
    Range* range = JSRange::toWrapped(toJS(context)->vm(), toJS(object));
    return getOrCreate(range);
}

RefPtr<InjectedBundleRangeHandle> InjectedBundleRangeHandle::getOrCreate(Range* range)
{
    if (!range)
        return nullptr;

    DOMRangeHandleCache::AddResult result = domRangeHandleCache().add(range, nullptr);
    if (!result.isNewEntry)
        return result.iterator->value;

    auto rangeHandle = InjectedBundleRangeHandle::create(*range);
    result.iterator->value = rangeHandle.ptr();
    return WTFMove(rangeHandle);
}

Ref<InjectedBundleRangeHandle> InjectedBundleRangeHandle::create(Range& range)
{
    return adoptRef(*new InjectedBundleRangeHandle(range));
}

InjectedBundleRangeHandle::InjectedBundleRangeHandle(Range& range)
    : m_range(range)
{
}

InjectedBundleRangeHandle::~InjectedBundleRangeHandle()
{
    domRangeHandleCache().remove(m_range.ptr());
}

Range& InjectedBundleRangeHandle::coreRange() const
{
    return m_range.get();
}

Ref<InjectedBundleNodeHandle> InjectedBundleRangeHandle::document()
{
    return InjectedBundleNodeHandle::getOrCreate(m_range->ownerDocument());
}

WebCore::IntRect InjectedBundleRangeHandle::boundingRectInWindowCoordinates() const
{
    FloatRect boundingRect = m_range->absoluteBoundingRect();
    Frame* frame = m_range->ownerDocument().frame();
    return frame->view()->contentsToWindow(enclosingIntRect(boundingRect));
}

RefPtr<WebImage> InjectedBundleRangeHandle::renderedImage(SnapshotOptions options)
{
    Document& ownerDocument = m_range->ownerDocument();
    Frame* frame = ownerDocument.frame();
    if (!frame)
        return nullptr;

    FrameView* frameView = frame->view();
    if (!frameView)
        return nullptr;

#if PLATFORM(MAC)
    LocalDefaultSystemAppearance localAppearance(frameView->useDarkAppearance());
#endif

    Ref<Frame> protector(*frame);

    VisibleSelection oldSelection = frame->selection().selection();
    frame->selection().setSelection(VisibleSelection(m_range));

    float scaleFactor = (options & SnapshotOptionsExcludeDeviceScaleFactor) ? 1 : frame->page()->deviceScaleFactor();
    IntRect paintRect = enclosingIntRect(m_range->absoluteBoundingRect());
    IntSize backingStoreSize = paintRect.size();
    backingStoreSize.scale(scaleFactor);

    RefPtr<ShareableBitmap> backingStore = ShareableBitmap::createShareable(backingStoreSize, { });
    if (!backingStore)
        return nullptr;

    auto graphicsContext = backingStore->createGraphicsContext();
    graphicsContext->scale(scaleFactor);

    paintRect.move(frameView->frameRect().x(), frameView->frameRect().y());
    paintRect.moveBy(-frameView->scrollPosition());

    graphicsContext->translate(-paintRect.location());

    OptionSet<PaintBehavior> oldPaintBehavior = frameView->paintBehavior();
    OptionSet<PaintBehavior> paintBehavior = oldPaintBehavior;
    paintBehavior.add({ PaintBehavior::SelectionOnly, PaintBehavior::FlattenCompositingLayers, PaintBehavior::Snapshotting });
    if (options & SnapshotOptionsForceBlackText)
        paintBehavior.add(PaintBehavior::ForceBlackText);
    if (options & SnapshotOptionsForceWhiteText)
        paintBehavior.add(PaintBehavior::ForceWhiteText);

    frameView->setPaintBehavior(paintBehavior);
    ownerDocument.updateLayout();

    frameView->paint(*graphicsContext, paintRect);
    frameView->setPaintBehavior(oldPaintBehavior);

    frame->selection().setSelection(oldSelection);

    return WebImage::create(backingStore.releaseNonNull());
}

String InjectedBundleRangeHandle::text() const
{
    return m_range->text();
}

} // namespace WebKit
