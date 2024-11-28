/*
 * Copyright (C) 2013-2021 Apple Inc. All rights reserved.
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

#import "config.h"
#import "RemoteLayerBackingStore.h"

#import "ArgumentCoders.h"
#import "DynamicContentScalingImageBufferBackend.h"
#import "GPUProcess.h"
#import "ImageBufferBackendHandleSharing.h"
#import "ImageBufferSet.h"
#import "Logging.h"
#import "PlatformCALayerRemote.h"
#import "RemoteImageBufferSetProxy.h"
#import "RemoteLayerBackingStoreCollection.h"
#import "RemoteLayerTreeContext.h"
#import "RemoteLayerTreeDrawingAreaProxy.h"
#import "RemoteLayerTreeHost.h"
#import "RemoteLayerTreeLayers.h"
#import "RemoteLayerTreeNode.h"
#import "RemoteLayerWithInProcessRenderingBackingStore.h"
#import "RemoteLayerWithRemoteRenderingBackingStore.h"
#import "SwapBuffersDisplayRequirement.h"
#import "WebCoreArgumentCoders.h"
#import "WebPageProxy.h"
#import "WebProcess.h"
#import "WebProcessPool.h"
#import "WebProcessProxy.h"
#import <QuartzCore/QuartzCore.h>
#import <WebCore/BifurcatedGraphicsContext.h>
#import <WebCore/DynamicContentScalingTypes.h>
#import <WebCore/GraphicsContextCG.h>
#import <WebCore/IOSurface.h>
#import <WebCore/ImageBuffer.h>
#import <WebCore/PlatformCALayerClient.h>
#import <WebCore/PlatformCALayerDelegatedContents.h>
#import <WebCore/ShareableBitmap.h>
#import <WebCore/WebCoreCALayerExtras.h>
#import <WebCore/WebLayer.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/Noncopyable.h>
#import <wtf/TZoneMalloc.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/text/TextStream.h>


namespace WebKit {

using namespace WebCore;

namespace {

class DelegatedContentsFenceFlusher final : public ThreadSafeImageBufferSetFlusher {
    WTF_MAKE_TZONE_ALLOCATED(DelegatedContentsFenceFlusher);
    WTF_MAKE_NONCOPYABLE(DelegatedContentsFenceFlusher);
public:
    static std::unique_ptr<DelegatedContentsFenceFlusher> create(Ref<PlatformCALayerDelegatedContentsFence> fence)
    {
        return std::unique_ptr<DelegatedContentsFenceFlusher> { new DelegatedContentsFenceFlusher(WTFMove(fence)) };
    }

    bool flushAndCollectHandles(HashMap<RemoteImageBufferSetIdentifier, std::unique_ptr<BufferSetBackendHandle>>&) final
    {
        return m_fence->waitFor(delegatedContentsFinishedTimeout);
    }

private:
    DelegatedContentsFenceFlusher(Ref<PlatformCALayerDelegatedContentsFence> fence)
        : m_fence(WTFMove(fence))
    {
    }
    Ref<PlatformCALayerDelegatedContentsFence> m_fence;
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(DelegatedContentsFenceFlusher);

}

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteLayerBackingStore);

std::unique_ptr<RemoteLayerBackingStore> RemoteLayerBackingStore::createForLayer(PlatformCALayerRemote& layer)
{
    switch (processModelForLayer(layer)) {
    case ProcessModel::Remote:
        return makeUnique<RemoteLayerWithRemoteRenderingBackingStore>(layer);
    case ProcessModel::InProcess:
        return makeUnique<RemoteLayerWithInProcessRenderingBackingStore>(layer);
    }
}

RemoteLayerBackingStore::RemoteLayerBackingStore(PlatformCALayerRemote& layer)
    : m_layer(layer)
    , m_lastDisplayTime(-MonotonicTime::infinity())
{
    if (RefPtr collection = backingStoreCollection())
        collection->backingStoreWasCreated(*this);
}

RemoteLayerBackingStore::~RemoteLayerBackingStore()
{
    if (RefPtr collection = backingStoreCollection())
        collection->backingStoreWillBeDestroyed(*this);
}

RemoteLayerBackingStoreCollection* RemoteLayerBackingStore::backingStoreCollection() const
{
    if (auto* context = m_layer->context())
        return &context->backingStoreCollection();

    return nullptr;
}

void RemoteLayerBackingStore::ensureBackingStore(const Parameters& parameters)
{
    if (m_parameters == parameters)
        return;

    m_parameters = parameters;
    clearBackingStore();
}

RemoteLayerBackingStore::ProcessModel RemoteLayerBackingStore::processModelForLayer(PlatformCALayerRemote& layer)
{
    if (WebProcess::singleton().shouldUseRemoteRenderingFor(WebCore::RenderingPurpose::DOM) && !layer.needsPlatformContext())
        return ProcessModel::Remote;
    return ProcessModel::InProcess;
}

#if !LOG_DISABLED
static bool hasValue(const ImageBufferBackendHandle& backendHandle)
{
    return WTF::switchOn(backendHandle,
        [&] (const ShareableBitmap::Handle& handle) {
            return true;
        },
        [&] (const MachSendRight& machSendRight) {
            return !!machSendRight;
        }
#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
        , [&] (const WebCore::DynamicContentScalingDisplayList& handle) {
            return true;
        }
#endif
    );
}
#endif

void RemoteLayerBackingStore::encode(IPC::Encoder& encoder) const
{
    encoder << m_parameters.isOpaque;
    encoder << m_parameters.type;

    // FIXME: For simplicity this should be moved to the end of display() once the buffer handles can be created once
    // and stored in m_bufferHandle. http://webkit.org/b/234169
    std::optional<ImageBufferBackendHandle> handle;
    if (m_contentsBufferHandle) {
        ASSERT(m_parameters.type == Type::IOSurface);
        handle = ImageBufferBackendHandle { *m_contentsBufferHandle };
    } else
        handle = frontBufferHandle();

    // It would be nice to ASSERT(handle && hasValue(*handle)) here, but when we hit the timeout in RemoteImageBufferProxy::ensureBackendCreated(), we don't have a handle.
#if !LOG_DISABLED
    if (!(handle && hasValue(*handle)))
        LOG_WITH_STREAM(RemoteLayerBuffers, stream << "RemoteLayerBackingStore " << m_layer->layerID() << " encode - no buffer handle; did ensureBackendCreated() time out?");
#endif

    encoder << WTFMove(handle);

    encoder << bufferSetIdentifier();

    encodeBufferAndBackendInfos(encoder);
    encoder << m_contentsRenderingResourceIdentifier;
    encoder << m_previouslyPaintedRect;

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
    encoder << displayListHandle();
#endif
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteLayerBackingStoreProperties);

void RemoteLayerBackingStoreProperties::dump(TextStream& ts) const
{
    auto dumpBuffer = [&](ASCIILiteral name, const std::optional<BufferAndBackendInfo>& bufferInfo) {
        ts.startGroup();
        ts << name << " "_s;
        if (bufferInfo)
            ts << bufferInfo->resourceIdentifier << " backend generation "_s << bufferInfo->backendGeneration;
        else
            ts << "none"_s;
        ts.endGroup();
    };
    dumpBuffer("front buffer"_s, m_frontBufferInfo);
    dumpBuffer("back buffer"_s, m_backBufferInfo);
    dumpBuffer("secondaryBack buffer"_s, m_secondaryBackBufferInfo);

    ts.dumpProperty("is opaque"_s, isOpaque());
    ts.dumpProperty("has buffer handle"_s, !!bufferHandle());
}

bool RemoteLayerBackingStore::layerWillBeDisplayed()
{
    RefPtr collection = backingStoreCollection();
    if (!collection) {
        ASSERT_NOT_REACHED();
        return false;
    }

    return collection->backingStoreWillBeDisplayed(*this);
}

bool RemoteLayerBackingStore::layerWillBeDisplayedWithRenderingSuppression()
{
    RefPtr collection = backingStoreCollection();
    if (!collection) {
        ASSERT_NOT_REACHED();
        return false;
    }

    return collection->backingStoreWillBeDisplayedWithRenderingSuppression(*this);
}

void RemoteLayerBackingStore::setNeedsDisplay(const IntRect rect)
{
    m_dirtyRegion.unite(intersection(layerBounds(), rect));
}

void RemoteLayerBackingStore::setNeedsDisplay()
{
    m_dirtyRegion.unite(layerBounds());
}

WebCore::IntRect RemoteLayerBackingStore::layerBounds() const
{
    return IntRect { { }, expandedIntSize(m_parameters.size) };
}

ImageBufferPixelFormat RemoteLayerBackingStore::pixelFormat() const
{
    switch (contentsFormat()) {
    case ContentsFormat::RGBA8:
        return m_parameters.isOpaque ? ImageBufferPixelFormat::BGRX8 : ImageBufferPixelFormat::BGRA8;

#if HAVE(IOSURFACE_RGB10)
    case ContentsFormat::RGBA10:
        return m_parameters.isOpaque ? ImageBufferPixelFormat::RGB10 : ImageBufferPixelFormat::RGB10A8;
#endif
#if HAVE(HDR_SUPPORT)
    case ContentsFormat::RGBA16F:
        return ImageBufferPixelFormat::RGBA16F;
#endif
    }
}

unsigned RemoteLayerBackingStore::bytesPerPixel() const
{
    switch (pixelFormat()) {
    case ImageBufferPixelFormat::BGRX8: return 4;
    case ImageBufferPixelFormat::BGRA8: return 4;
    case ImageBufferPixelFormat::RGB10: return 4;
    case ImageBufferPixelFormat::RGB10A8: return 5;
#if HAVE(HDR_SUPPORT)
    case ImageBufferPixelFormat::RGBA16F: return 8;
#endif
    }
    return 4;
}

bool RemoteLayerBackingStore::supportsPartialRepaint() const
{
#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
    // FIXME: Find a way to support partial repaint for backing store that
    // includes a display list without allowing unbounded memory growth.
    if (m_parameters.includeDisplayList == IncludeDisplayList::Yes)
        return false;
#endif

    const unsigned maxSmallLayerBackingArea = 64u * 64u;
    auto checkedArea = ImageBuffer::calculateBackendSize(m_parameters.size, m_parameters.scale).area<RecordOverflow>();
    if (!checkedArea.hasOverflowed() && checkedArea <= maxSmallLayerBackingArea)
        return false;
    return true;

}

bool RemoteLayerBackingStore::drawingRequiresClearedPixels() const
{
    return !m_parameters.isOpaque && !m_layer->owner()->platformCALayerShouldPaintUsingCompositeCopy();
}

PlatformCALayerRemote& RemoteLayerBackingStore::layer() const
{
    return m_layer;
}

void RemoteLayerBackingStore::setDelegatedContents(const PlatformCALayerRemoteDelegatedContents& contents)
{
    m_contentsBufferHandle = ImageBufferBackendHandle { contents.surface };
    if (contents.finishedFence)
        m_frontBufferFlushers.append(DelegatedContentsFenceFlusher::create(Ref { *contents.finishedFence }));
    if (contents.surfaceIdentifier)
        m_contentsRenderingResourceIdentifier = *contents.surfaceIdentifier;
    else
        m_contentsRenderingResourceIdentifier = std::nullopt;
    m_dirtyRegion = { };
    m_paintingRects.clear();
}

bool RemoteLayerBackingStore::needsDisplay() const
{
    RefPtr collection = backingStoreCollection();
    if (!collection) {
        ASSERT_NOT_REACHED();
        return false;
    }

    if (m_layer->owner()->platformCALayerDelegatesDisplay(m_layer.ptr())) {
        LOG_WITH_STREAM(RemoteLayerBuffers, stream << "RemoteLayerBackingStore " << m_layer->layerID() << " needsDisplay() - delegates display");
        return true;
    }

    auto needsDisplayReason = [&]() {
        if (size().isEmpty())
            return BackingStoreNeedsDisplayReason::None;

        if (!hasFrontBuffer())
            return BackingStoreNeedsDisplayReason::NoFrontBuffer;

        if (frontBufferMayBeVolatile())
            return BackingStoreNeedsDisplayReason::FrontBufferIsVolatile;

        return hasEmptyDirtyRegion() ? BackingStoreNeedsDisplayReason::None : BackingStoreNeedsDisplayReason::HasDirtyRegion;
    }();

    LOG_WITH_STREAM(RemoteLayerBuffers, stream << "RemoteLayerBackingStore " << m_layer->layerID() << " size " << size() << " needsDisplay() - needs display reason: " << needsDisplayReason);
    return needsDisplayReason != BackingStoreNeedsDisplayReason::None;
}

bool RemoteLayerBackingStore::performDelegatedLayerDisplay()
{
    auto& layerOwner = *m_layer->owner();
    if (layerOwner.platformCALayerDelegatesDisplay(m_layer.ptr())) {
        // This can call back to setContents(), setting m_contentsBufferHandle.
        layerOwner.platformCALayerLayerDisplay(m_layer.ptr());
        layerOwner.platformCALayerLayerDidDisplay(m_layer.ptr());
        return true;
    }
    
    return false;
}

void RemoteLayerBackingStore::dirtyRepaintCounterIfNecessary()
{
    if (m_layer->owner()->platformCALayerShowRepaintCounter(m_layer.ptr())) {
        IntRect indicatorRect(0, 0, 52, 27);
        m_dirtyRegion.unite(indicatorRect);
    }
}

void RemoteLayerBackingStore::paintContents()
{
    LOG_WITH_STREAM(RemoteLayerBuffers, stream << "RemoteLayerBackingStore " << m_layer->layerID() << " paintContents() - has dirty region " << !hasEmptyDirtyRegion());
    if (m_layer->owner()->platformCALayerDelegatesDisplay(m_layer.ptr()))
        return;

    if (hasEmptyDirtyRegion()) {
        if (auto flusher = createFlusher(ThreadSafeImageBufferSetFlusher::FlushType::BackendHandlesOnly))
            m_frontBufferFlushers.append(WTFMove(flusher));
        return;
    }

    m_lastDisplayTime = MonotonicTime::now();
    m_paintingRects = ImageBufferSet::computePaintingRects(m_dirtyRegion, m_parameters.scale);

    createContextAndPaintContents();
}

void RemoteLayerBackingStore::drawInContext(GraphicsContext& context)
{
    GraphicsContextStateSaver stateSaver(context);
    IntRect dirtyBounds = m_dirtyRegion.bounds();

#ifndef NDEBUG
    if (m_parameters.isOpaque)
        context.fillRect(this->layerBounds(), SRGBA<uint8_t> { 255, 47, 146 });
#endif

    OptionSet<WebCore::GraphicsLayerPaintBehavior> paintBehavior;
    if (auto* context = m_layer->context(); context && context->nextRenderingUpdateRequiresSynchronousImageDecoding())
        paintBehavior.add(GraphicsLayerPaintBehavior::ForceSynchronousImageDecode);
    
    // FIXME: This should be moved to PlatformCALayerRemote for better layering.
    switch (m_layer->layerType()) {
    case PlatformCALayer::LayerType::LayerTypeSimpleLayer:
    case PlatformCALayer::LayerType::LayerTypeTiledBackingTileLayer:
        m_layer->owner()->platformCALayerPaintContents(m_layer.ptr(), context, dirtyBounds, paintBehavior);
        break;
    case PlatformCALayer::LayerType::LayerTypeWebLayer:
    case PlatformCALayer::LayerType::LayerTypeBackdropLayer:
        PlatformCALayer::drawLayerContents(context, m_layer.ptr(), m_paintingRects, paintBehavior);
        break;
    case PlatformCALayer::LayerType::LayerTypeLayer:
    case PlatformCALayer::LayerType::LayerTypeTransformLayer:
    case PlatformCALayer::LayerType::LayerTypeTiledBackingLayer:
    case PlatformCALayer::LayerType::LayerTypePageTiledBackingLayer:
    case PlatformCALayer::LayerType::LayerTypeRootLayer:
    case PlatformCALayer::LayerType::LayerTypeAVPlayerLayer:
    case PlatformCALayer::LayerType::LayerTypeContentsProvidedLayer:
    case PlatformCALayer::LayerType::LayerTypeShapeLayer:
    case PlatformCALayer::LayerType::LayerTypeScrollContainerLayer:
#if ENABLE(MODEL_ELEMENT)
    case PlatformCALayer::LayerType::LayerTypeModelLayer:
#endif
    case PlatformCALayer::LayerType::LayerTypeCustom:
    case PlatformCALayer::LayerType::LayerTypeHost:
        ASSERT_NOT_REACHED();
        break;
    };

    stateSaver.restore();

    m_dirtyRegion = { };
    m_paintingRects.clear();

    m_layer->owner()->platformCALayerLayerDidDisplay(m_layer.ptr());

    m_previouslyPaintedRect = dirtyBounds;
    if (auto flusher = createFlusher())
        m_frontBufferFlushers.append(WTFMove(flusher));
}

void RemoteLayerBackingStore::enumerateRectsBeingDrawn(GraphicsContext& context, void (^block)(FloatRect))
{
    CGAffineTransform inverseTransform = CGAffineTransformInvert(context.getCTM());

    // We don't want to un-apply the flipping or contentsScale,
    // because they're not applied to repaint rects.
    inverseTransform = CGAffineTransformScale(inverseTransform, m_parameters.scale, -m_parameters.scale);
    inverseTransform = CGAffineTransformTranslate(inverseTransform, 0, -m_parameters.size.height());

    for (const auto& rect : m_paintingRects) {
        CGRect rectToDraw = CGRectApplyAffineTransform(rect, inverseTransform);
        block(rectToDraw);
    }
}

RetainPtr<id> RemoteLayerBackingStoreProperties::layerContentsBufferFromBackendHandle(ImageBufferBackendHandle&& backendHandle, LayerContentsType contentsType)
{
    RetainPtr<id> contents;
    WTF::switchOn(backendHandle,
        [&] (ShareableBitmap::Handle& handle) {
            if (auto bitmap = ShareableBitmap::create(WTFMove(handle), SharedMemory::Protection::ReadOnly))
                contents = bridge_id_cast(bitmap->makeCGImageCopy());
        },
        [&] (MachSendRight& machSendRight) {
            switch (contentsType) {
            case LayerContentsType::IOSurface: {
                auto surface = WebCore::IOSurface::createFromSendRight(WTFMove(machSendRight));
                contents = surface ? surface->asLayerContents() : nil;
                break;
            }
            case LayerContentsType::CAMachPort:
                contents = bridge_id_cast(adoptCF(CAMachPortCreate(machSendRight.leakSendRight())));
                break;
            case LayerContentsType::CachedIOSurface:
                auto surface = WebCore::IOSurface::createFromSendRight(WTFMove(machSendRight));
                contents = surface ? surface->asCAIOSurfaceLayerContents() : nil;
                break;
            }
        }
#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
        , [&] (WebCore::DynamicContentScalingDisplayList& handle) {
            ASSERT_NOT_REACHED();
        }
#endif
    );

    return contents;
}

void RemoteLayerBackingStoreProperties::applyBackingStoreToLayer(CALayer *layer, LayerContentsType contentsType, std::optional<WebCore::RenderingResourceIdentifier> asyncContentsIdentifier, bool replayDynamicContentScalingDisplayListsIntoBackingStore)
{
    if (asyncContentsIdentifier && m_contentsRenderingResourceIdentifier && *asyncContentsIdentifier >= m_contentsRenderingResourceIdentifier)
        return;

    layer.contentsOpaque = m_isOpaque;

    RetainPtr<id> contents;
    // m_bufferHandle can be unset here if IPC with the GPU process timed out.
    if (m_contentsBuffer)
        contents = m_contentsBuffer;
    else if (m_bufferHandle)
        contents = layerContentsBufferFromBackendHandle(WTFMove(*m_bufferHandle), contentsType);

    if (!contents) {
        [layer _web_clearContents];
        return;
    }

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
    if (m_displayListBufferHandle) {
        ASSERT([layer isKindOfClass:[WKCompositingLayer class]]);
        if (![layer isKindOfClass:[WKCompositingLayer class]])
            return;

        layer.drawsAsynchronously = (m_type == RemoteLayerBackingStore::Type::IOSurface);

        if (!replayDynamicContentScalingDisplayListsIntoBackingStore) {
            [layer setValue:@1 forKeyPath:WKDynamicContentScalingEnabledKey];
            [layer setValue:@1 forKeyPath:WKDynamicContentScalingBifurcationEnabledKey];
            [layer setValue:@(layer.contentsScale) forKeyPath:WKDynamicContentScalingBifurcationScaleKey];
        } else
            layer.opaque = m_isOpaque;
        [(WKCompositingLayer *)layer _setWKContents:contents.get() withDisplayList:WTFMove(std::get<DynamicContentScalingDisplayList>(*m_displayListBufferHandle)) replayForTesting:replayDynamicContentScalingDisplayListsIntoBackingStore];
        return;
    } else
        [layer _web_clearDynamicContentScalingDisplayListIfNeeded];
#else
    UNUSED_PARAM(replayDynamicContentScalingDisplayListsIntoBackingStore);
#endif

    layer.contents = contents.get();
    if ([CALayer instancesRespondToSelector:@selector(contentsDirtyRect)]) {
        if (m_paintedRect) {
            FloatRect painted = *m_paintedRect;
            painted.scale(layer.contentsScale);

            // Most of the time layer.contentsDirtyRect should be the null rect, since CA clears this on every commit,
            // but in some scenarios we don't get a CA commit for every remote layer tree transaction.
            auto existingDirtyRect = layer.contentsDirtyRect;
            if (CGRectIsNull(existingDirtyRect))
                layer.contentsDirtyRect = painted;
            else
                layer.contentsDirtyRect = CGRectUnion(existingDirtyRect, painted);
        }
    }
}

void RemoteLayerBackingStoreProperties::updateCachedBuffers(RemoteLayerTreeNode& node, LayerContentsType contentsType)
{
    ASSERT(!m_contentsBuffer);

    Vector<RemoteLayerTreeNode::CachedContentsBuffer> cachedBuffers = node.takeCachedContentsBuffers();

    if (contentsType != LayerContentsType::CachedIOSurface || !m_frontBufferInfo || !m_bufferHandle || !std::holds_alternative<MachSendRight>(*m_bufferHandle))
        return;

    cachedBuffers.removeAllMatching([&](const RemoteLayerTreeNode::CachedContentsBuffer& current) {
        if (m_frontBufferInfo && *m_frontBufferInfo == current.imageBufferInfo)
            return false;

        if (m_backBufferInfo && *m_backBufferInfo== current.imageBufferInfo)
            return false;

        if (m_secondaryBackBufferInfo && *m_secondaryBackBufferInfo == current.imageBufferInfo)
            return false;

        return true;
    });

    for (auto& current : cachedBuffers) {
        if (m_frontBufferInfo->resourceIdentifier == current.imageBufferInfo.resourceIdentifier) {
            m_contentsBuffer = current.buffer;
            break;
        }
    }

    if (!m_contentsBuffer) {
        m_contentsBuffer = layerContentsBufferFromBackendHandle(WTFMove(*m_bufferHandle), LayerContentsType::CachedIOSurface);
        cachedBuffers.append({ *m_frontBufferInfo, m_contentsBuffer });
    }

    node.setCachedContentsBuffers(WTFMove(cachedBuffers));
}

void RemoteLayerBackingStoreProperties::setBackendHandle(BufferSetBackendHandle& bufferSetHandle)
{
    m_bufferHandle = std::exchange(bufferSetHandle.bufferHandle, std::nullopt);
    m_frontBufferInfo = bufferSetHandle.frontBufferInfo;
    m_backBufferInfo = bufferSetHandle.backBufferInfo;
    m_secondaryBackBufferInfo = bufferSetHandle.secondaryBackBufferInfo;
}

Vector<std::unique_ptr<ThreadSafeImageBufferSetFlusher>> RemoteLayerBackingStore::takePendingFlushers()
{
    return std::exchange(m_frontBufferFlushers, { });
}

void RemoteLayerBackingStore::purgeFrontBufferForTesting()
{
    if (RefPtr collection = backingStoreCollection())
        collection->purgeFrontBufferForTesting(*this);
}

void RemoteLayerBackingStore::purgeBackBufferForTesting()
{
    if (RefPtr collection = backingStoreCollection())
        collection->purgeBackBufferForTesting(*this);
}

void RemoteLayerBackingStore::markFrontBufferVolatileForTesting()
{
    if (RefPtr collection = backingStoreCollection())
        collection->markFrontBufferVolatileForTesting(*this);
}

TextStream& operator<<(TextStream& ts, const RemoteLayerBackingStore& backingStore)
{
    backingStore.dump(ts);
    return ts;
}

TextStream& operator<<(TextStream& ts, const RemoteLayerBackingStoreProperties& properties)
{
    properties.dump(ts);
    return ts;
}

TextStream& operator<<(TextStream& ts, BackingStoreNeedsDisplayReason reason)
{
    switch (reason) {
    case BackingStoreNeedsDisplayReason::None: ts << "none"; break;
    case BackingStoreNeedsDisplayReason::NoFrontBuffer: ts << "no front buffer"; break;
    case BackingStoreNeedsDisplayReason::FrontBufferIsVolatile: ts << "volatile front buffer"; break;
    case BackingStoreNeedsDisplayReason::FrontBufferHasNoSharingHandle: ts << "no front buffer sharing handle"; break;
    case BackingStoreNeedsDisplayReason::HasDirtyRegion: ts << "has dirty region"; break;
    }

    return ts;
}

RemoteLayerBackingStoreOrProperties::RemoteLayerBackingStoreOrProperties(std::unique_ptr<RemoteLayerBackingStoreProperties>&& properties)
    : properties(WTFMove(properties)) { }

} // namespace WebKit
