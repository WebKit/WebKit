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
#include <WebCore/GeometryUtilities.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/IntRect.h>
#include <WebCore/JSRange.h>
#include <WebCore/Page.h>
#include <WebCore/Range.h>
#include <WebCore/RenderView.h>
#include <WebCore/SimpleRange.h>
#include <WebCore/TextIterator.h>
#include <WebCore/VisibleSelection.h>
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>

#if PLATFORM(MAC)
#include <WebCore/LocalDefaultSystemAppearance.h>
#endif

namespace WebKit {
using namespace WebCore;

using DOMRangeHandleCache = HashMap<Range*, InjectedBundleRangeHandle*>;

static DOMRangeHandleCache& domRangeHandleCache()
{
    static NeverDestroyed<DOMRangeHandleCache> cache;
    return cache;
}

RefPtr<InjectedBundleRangeHandle> InjectedBundleRangeHandle::getOrCreate(JSContextRef context, JSObjectRef object)
{
    return getOrCreate(JSRange::toWrapped(toJS(context)->vm(), toJS(object)));
}

RefPtr<InjectedBundleRangeHandle> InjectedBundleRangeHandle::getOrCreate(Range* range)
{
    if (!range)
        return nullptr;
    auto result = domRangeHandleCache().add(range, nullptr);
    if (!result.isNewEntry)
        return result.iterator->value;
    auto rangeHandle = adoptRef(*new InjectedBundleRangeHandle(*range));
    result.iterator->value = rangeHandle.ptr();
    return rangeHandle;
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
    return InjectedBundleNodeHandle::getOrCreate(m_range->startContainer().document());
}

WebCore::IntRect InjectedBundleRangeHandle::boundingRectInWindowCoordinates() const
{
    auto range = makeSimpleRange(m_range);
    auto frame = range.start.document().frame();
    if (!frame)
        return { };
    auto view = frame->view();
    if (!view)
        return { };
    return view->contentsToWindow(enclosingIntRect(unionRectIgnoringZeroRects(RenderObject::absoluteBorderAndTextRects(range))));
}

RefPtr<WebImage> InjectedBundleRangeHandle::renderedImage(SnapshotOptions options)
{
    auto range = makeSimpleRange(m_range);

    auto document = makeRef(range.start.document());

    auto frame = makeRefPtr(document->frame());
    if (!frame)
        return nullptr;

    auto frameView = frame->view();
    if (!frameView)
        return nullptr;

#if PLATFORM(MAC)
    LocalDefaultSystemAppearance localAppearance(frameView->useDarkAppearance());
#endif

    VisibleSelection oldSelection = frame->selection().selection();
    frame->selection().setSelection(range);

    float scaleFactor = (options & SnapshotOptionsExcludeDeviceScaleFactor) ? 1 : frame->page()->deviceScaleFactor();
    IntRect paintRect = enclosingIntRect(unionRectIgnoringZeroRects(RenderObject::absoluteBorderAndTextRects(range)));
    IntSize backingStoreSize = paintRect.size();
    backingStoreSize.scale(scaleFactor);

    auto backingStore = ShareableBitmap::createShareable(backingStoreSize, { });
    if (!backingStore)
        return nullptr;

    auto graphicsContext = backingStore->createGraphicsContext();
    if (!graphicsContext)
        return nullptr;

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
    document->updateLayout();

    frameView->paint(*graphicsContext, paintRect);
    frameView->setPaintBehavior(oldPaintBehavior);

    frame->selection().setSelection(oldSelection);

    return WebImage::create(backingStore.releaseNonNull());
}

String InjectedBundleRangeHandle::text() const
{
    auto range = makeSimpleRange(m_range);
    range.start.document().updateLayout();
    return plainText(range);
}

RefPtr<InjectedBundleRangeHandle> createHandle(const std::optional<WebCore::SimpleRange>& range)
{
    return InjectedBundleRangeHandle::getOrCreate(createLiveRange(range).get());
}

} // namespace WebKit
