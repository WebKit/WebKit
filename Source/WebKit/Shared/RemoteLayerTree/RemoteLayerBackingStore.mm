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
#import "PrepareBackingStoreBuffersData.h"
#import "RemoteImageBufferSetProxy.h"
#import "RemoteLayerBackingStoreCollection.h"
#import "RemoteLayerTreeContext.h"
#import "RemoteLayerTreeDrawingAreaProxy.h"
#import "RemoteLayerTreeHost.h"
#import "RemoteLayerTreeLayers.h"
#import "RemoteLayerTreeNode.h"
#import "RemoteLayerWithInProcessRenderingBackingStore.h"
#import "RemoteLayerWithRemoteRenderingBackingStore.h"
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

#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
#import "WKSeparatedImageView.h"
#endif


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

    bool flushAndCollectHandles(HashMap<ImageBufferSetIdentifier, std::unique_ptr<BufferSetBackendHandle>>&) final
    {
        return m_fence->waitFor(delegatedContentsFinishedTimeout);
    }

private:
    DelegatedContentsFenceFlusher(Ref<PlatformCALayerDelegatedContentsFence> fence)
        : m_fence(WTFMove(fence))
    {
    }

    const Ref<PlatformCALayerDelegatedContentsFence> m_fence;
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

void RemoteLayerBackingStore::clearBackingStore()
{
    m_contentsBufferHandle = std::nullopt;
    setNeedsDisplay();
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

void RemoteLayerBackingStore::encode(IPC::Encoder& encoder) const
{
    // Only delegated contents encode their handle here. Buffer sets encode their handles
    // out of line (and on a different thread) using the flushAndCollectHandles method
    // on their async flusher.
    std::optional<ImageBufferBackendHandle> handle;
    if (m_contentsBufferHandle) {
        ASSERT(m_parameters.type == Type::IOSurface);
        handle = ImageBufferBackendHandle { *m_contentsBufferHandle };
    }

    encoder << WTFMove(handle);

    encoder << bufferSetIdentifier();

    encoder << m_contentsRenderingResourceIdentifier;
    encoder << m_previouslyPaintedRect;

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
    encoder << displayListHandle();
#endif

    encoder << m_parameters.isOpaque;
    encoder << m_parameters.type;
#if HAVE(SUPPORT_HDR_DISPLAY)
    encoder << m_maxRequestedEDRHeadroom;
#endif
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteLayerBackingStoreProperties);

void RemoteLayerBackingStoreProperties::dump(TextStream& ts) const
{
    auto dumpBuffer = [&](ASCIILiteral name, const std::optional<BufferAndBackendInfo>& bufferInfo) {
        ts.startGroup();
        ts << name << ' ';
        if (bufferInfo)
            ts << bufferInfo->resourceIdentifier << " backend generation "_s << bufferInfo->backendGeneration;
        else
            ts << "none"_s;
        ts.endGroup();
    };
    dumpBuffer("front buffer"_s, m_frontBufferInfo);
    dumpBuffer("back buffer"_s, m_backBufferInfo);
    dumpBuffer("secondaryBack buffer"_s, m_secondaryBackBufferInfo);

    ts.dumpProperty("has buffer handle"_s, !!bufferHandle());
#if HAVE(SUPPORT_HDR_DISPLAY)
    ts.dumpProperty("requested-headroom", m_maxRequestedEDRHeadroom);
#endif
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
#if HAVE(SUPPORT_HDR_DISPLAY)
    m_maxPaintedEDRHeadroom = 1;
    m_maxRequestedEDRHeadroom = 1;
#endif
}

#if HAVE(SUPPORT_HDR_DISPLAY)
bool RemoteLayerBackingStore::setNeedsDisplayIfEDRHeadroomExceeds(float headroom)
{
    if (m_maxPaintedEDRHeadroom > headroom) {
        setNeedsDisplay();
        return true;
    }

    bool wasTonemapped = m_maxRequestedEDRHeadroom > m_maxPaintedEDRHeadroom;
    if (m_maxPaintedEDRHeadroom < headroom && wasTonemapped) {
        setNeedsDisplay();
        return true;
    }
    return false;
}
#endif

WebCore::IntRect RemoteLayerBackingStore::layerBounds() const
{
    return IntRect { { }, expandedIntSize(m_parameters.size) };
}

PixelFormat RemoteLayerBackingStore::pixelFormat() const
{
    switch (contentsFormat()) {
    case ContentsFormat::RGBA8:
        return m_parameters.isOpaque ? PixelFormat::BGRX8 : PixelFormat::BGRA8;

#if ENABLE(PIXEL_FORMAT_RGB10)
    case ContentsFormat::RGBA10:
        return m_parameters.isOpaque ? PixelFormat::RGB10 : PixelFormat::RGB10A8;
#endif
#if ENABLE(PIXEL_FORMAT_RGBA16F)
    case ContentsFormat::RGBA16F:
        return PixelFormat::RGBA16F;
#endif
    }
}

unsigned RemoteLayerBackingStore::bytesPerPixel() const
{
    return contentsFormatBytesPerPixel(contentsFormat(), m_parameters.isOpaque);
}

bool RemoteLayerBackingStore::supportsPartialRepaint() const
{
#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
    // FIXME: Find a way to support partial repaint for backing store that
    // includes a display list without allowing unbounded memory growth.
    if (m_parameters.includeDisplayList == WebCore::IncludeDynamicContentScalingDisplayList::Yes)
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
#if HAVE(SUPPORT_HDR_DISPLAY)
    m_maxRequestedEDRHeadroom = 1;
    m_maxPaintedEDRHeadroom = 1;
#endif
}

bool RemoteLayerBackingStore::needsDisplay() const
{
    RefPtr collection = backingStoreCollection();
    if (!collection) {
        ASSERT_NOT_REACHED();
        return false;
    }

    Ref layer = m_layer.get();
    if (layer->owner()->platformCALayerDelegatesDisplay(layer.ptr())) {
        LOG_WITH_STREAM(RemoteLayerBuffers, stream << "RemoteLayerBackingStore " << layer->layerID() << " needsDisplay() - delegates display");
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

    LOG_WITH_STREAM(RemoteLayerBuffers, stream << "RemoteLayerBackingStore " << layer->layerID() << " size " << size() << " needsDisplay() - needs display reason: " << needsDisplayReason);
    return needsDisplayReason != BackingStoreNeedsDisplayReason::None;
}

bool RemoteLayerBackingStore::performDelegatedLayerDisplay()
{
    Ref layer = m_layer.get();
    auto& layerOwner = *layer->owner();
    if (layerOwner.platformCALayerDelegatesDisplay(layer.ptr())) {
        // This can call back to setContents(), setting m_contentsBufferHandle.
        layerOwner.platformCALayerLayerDisplay(layer.ptr());
        layerOwner.platformCALayerLayerDidDisplay(layer.ptr());
        return true;
    }
    
    return false;
}

void RemoteLayerBackingStore::dirtyRepaintCounterIfNecessary()
{
    Ref layer = m_layer.get();
    if (layer->owner()->platformCALayerShowRepaintCounter(layer.ptr())) {
        IntRect indicatorRect(0, 0, 52, 28);
        m_dirtyRegion.unite(indicatorRect);
    }
}

void RemoteLayerBackingStore::paintContents()
{
    Ref layer = m_layer.get();
    LOG_WITH_STREAM(RemoteLayerBuffers, stream << "RemoteLayerBackingStore " << layer->layerID() << " paintContents() - has dirty region " << !hasEmptyDirtyRegion());
    if (layer->owner()->platformCALayerDelegatesDisplay(layer.ptr()))
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
#if HAVE(SUPPORT_HDR_DISPLAY)
    paintBehavior.add(GraphicsLayerPaintBehavior::TonemapHDRToDisplayHeadroom);
    context.clearMaxEDRHeadrooms();
#endif
    if (auto* context = m_layer->context(); context && context->nextRenderingUpdateRequiresSynchronousImageDecoding())
        paintBehavior.add(GraphicsLayerPaintBehavior::ForceSynchronousImageDecode);
    
    // FIXME: This should be moved to PlatformCALayerRemote for better layering.
    Ref layer = m_layer.get();
    switch (layer->layerType()) {
    case PlatformCALayer::LayerType::LayerTypeSimpleLayer:
#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
    case PlatformCALayer::LayerType::LayerTypeSeparatedImageLayer:
#endif
    case PlatformCALayer::LayerType::LayerTypeTiledBackingTileLayer:
        layer->owner()->platformCALayerPaintContents(layer.ptr(), context, dirtyBounds, paintBehavior);
        break;
    case PlatformCALayer::LayerType::LayerTypeWebLayer:
    case PlatformCALayer::LayerType::LayerTypeBackdropLayer:
#if HAVE(CORE_MATERIAL)
    case PlatformCALayer::LayerType::LayerTypeMaterialLayer:
#endif
        PlatformCALayer::drawLayerContents(context, layer.ptr(), m_paintingRects, paintBehavior);
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
#if HAVE(MATERIAL_HOSTING)
    case PlatformCALayer::LayerType::LayerTypeMaterialHostingLayer:
#endif
        ASSERT_NOT_REACHED();
        break;
    };

    stateSaver.restore();

    m_dirtyRegion = { };
    m_paintingRects.clear();
#if HAVE(SUPPORT_HDR_DISPLAY)
    m_maxPaintedEDRHeadroom = std::max(m_maxPaintedEDRHeadroom, context.maxPaintedEDRHeadroom());
    m_maxRequestedEDRHeadroom = std::max(m_maxRequestedEDRHeadroom, context.maxRequestedEDRHeadroom());
#endif

    layer->owner()->platformCALayerLayerDidDisplay(layer.ptr());

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

RemoteLayerBackingStoreProperties::RemoteLayerBackingStoreProperties(ImageBufferBackendHandle&& handle, WebCore::RenderingResourceIdentifier identifier, bool opaque)
    : m_bufferHandle(WTFMove(handle))
    , m_contentsRenderingResourceIdentifier(identifier)
    , m_isOpaque(opaque)
    , m_type(RemoteLayerBackingStore::Type::IOSurface)
{
}

RemoteLayerBackingStoreProperties::LayerContentsBufferInfo RemoteLayerBackingStoreProperties::layerContentsBufferFromBackendHandle(ImageBufferBackendHandle&& backendHandle, bool isDelegatedDisplay)
{
    bool hasExtendedDynamicRange = false;
    RetainPtr<id> contents;
    WTF::switchOn(backendHandle,
        [&] (ShareableBitmap::Handle& handle) {
            if (auto bitmap = ShareableBitmap::create(WTFMove(handle), SharedMemory::Protection::ReadOnly)) {
                contents = bridge_id_cast(bitmap->createPlatformImage());
                hasExtendedDynamicRange = bitmap->colorSpace().usesExtendedRange();
            }
        },
        [&] (MachSendRight& machSendRight) {
            if (auto surface = WebCore::IOSurface::createFromSendRight(WTFMove(machSendRight))) {
#if ENABLE(PIXEL_FORMAT_RGBA16F)
                if (surface->pixelFormat() == WebCore::IOSurface::Format::RGBA16F) {
                    hasExtendedDynamicRange = true;
#if HAVE(SUPPORT_HDR_DISPLAY_APIS)
                    if (isDelegatedDisplay && !surface->contentEDRHeadroom())
                        surface->loadContentEDRHeadroom();
#endif
                }
#endif
                if (surface->isVolatile())
                    RELEASE_LOG_ERROR(RemoteLayerTree, "Received volatile IOSurface");
                contents = surface->asCAIOSurfaceLayerContents();
            }
        }
#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
        , [&] (WebCore::DynamicContentScalingDisplayList& handle) {
            ASSERT_NOT_REACHED();
        }
#endif
    );

    return { contents, hasExtendedDynamicRange };
}

void RemoteLayerBackingStoreProperties::applyBackingStoreToNode(RemoteLayerTreeNode& node, bool replayDynamicContentScalingDisplayListsIntoBackingStore, UIView* hostingView)
{
    RetainPtr layer = node.layer();
    bool isDelegatedDisplay = !m_frontBufferInfo;

    // FIXME: Ideally we'd just infer wantsExtendedDynamicRangeContent
    // from the format of the buffer itself.
    [layer setContentsOpaque:m_isOpaque];

#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
    if (hostingView && [hostingView isKindOfClass:[WKSeparatedImageView class]]) {
        if (m_bufferHandle) {
            auto machSendRight = std::get<MachSendRight>(WTFMove(*m_bufferHandle));
            auto surface = WebCore::IOSurface::createFromSendRight(WTFMove(machSendRight));
            if (surface) {
                [(WKSeparatedImageView *)hostingView setSurface:surface->surface()];
                return;
            }
        }
        [(WKSeparatedImageView *)hostingView setSurface:nil];
        return;
    }
#endif

    LayerContentsBufferInfo bufferInfo = lookupCachedBuffer(node);
    // m_bufferHandle can be unset here if IPC with the GPU process timed out.
    if (!bufferInfo.buffer && m_bufferHandle)
        bufferInfo = layerContentsBufferFromBackendHandle(WTFMove(*m_bufferHandle), isDelegatedDisplay);

    if (!bufferInfo.buffer) {
        [layer _web_clearContents];
        return;
    }

#if HAVE(SUPPORT_HDR_DISPLAY_APIS)
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if (bufferInfo.hasExtendedDynamicRange) {
        [layer setWantsExtendedDynamicRangeContent:true];
        // Delegated contents set headroom via surface properties, not RemoteLayerBackingStore state.
        if (isDelegatedDisplay)
            [layer setContentsHeadroom:0.f];
        else
            [layer setContentsHeadroom:m_maxRequestedEDRHeadroom];
    } else {
        [layer setWantsExtendedDynamicRangeContent:false];
        [layer setContentsHeadroom:0.f];
    }
    ALLOW_DEPRECATED_DECLARATIONS_END
#endif

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
    if (m_displayListBufferHandle) {
        ASSERT([layer isKindOfClass:[WKCompositingLayer class]]);
        if (![layer isKindOfClass:[WKCompositingLayer class]])
            return;

        [layer setDrawsAsynchronously:(m_type == RemoteLayerBackingStore::Type::IOSurface)];

        if (!replayDynamicContentScalingDisplayListsIntoBackingStore) {
            [layer setValue:@1 forKeyPath:WKDynamicContentScalingEnabledKey];
            [layer setValue:@1 forKeyPath:WKDynamicContentScalingBifurcationEnabledKey];
            [layer setValue:@([layer contentsScale]) forKeyPath:WKDynamicContentScalingBifurcationScaleKey];
        }
        [(WKCompositingLayer *)layer.get() _setWKContents:bufferInfo.buffer.get() withDisplayList:WTFMove(*m_displayListBufferHandle) replayForTesting:replayDynamicContentScalingDisplayListsIntoBackingStore];
        return;
    } else
        [layer _web_clearDynamicContentScalingDisplayListIfNeeded];
#else
    UNUSED_PARAM(replayDynamicContentScalingDisplayListsIntoBackingStore);
#endif

    [layer setContents:bufferInfo.buffer.get()];
    if ([CALayer instancesRespondToSelector:@selector(contentsDirtyRect)]) {
        if (m_paintedRect) {
            FloatRect painted = *m_paintedRect;
            painted.scale([layer contentsScale]);

            // Most of the time layer.contentsDirtyRect should be the null rect, since CA clears this on every commit,
            // but in some scenarios we don't get a CA commit for every remote layer tree transaction.
            auto existingDirtyRect = [layer contentsDirtyRect];
            if (CGRectIsNull(existingDirtyRect))
                [layer setContentsDirtyRect:painted];
            else
                [layer setContentsDirtyRect:CGRectUnion(existingDirtyRect, painted)];
        }
    }
}

RemoteLayerBackingStoreProperties::LayerContentsBufferInfo RemoteLayerBackingStoreProperties::lookupCachedBuffer(RemoteLayerTreeNode& node)
{
    Vector<RemoteLayerTreeNode::CachedContentsBuffer> cachedBuffers = node.takeCachedContentsBuffers();

    if (!m_frontBufferInfo)
        return { { }, false };

    cachedBuffers.removeAllMatching([&](const RemoteLayerTreeNode::CachedContentsBuffer& current) {
        auto matches = [&](std::optional<BufferAndBackendInfo>& backendInfo) {
            if (!backendInfo || *backendInfo != current.imageBufferInfo)
                return false;
            return true;
        };
        if (matches(m_frontBufferInfo))
            return false;

        if (matches(m_backBufferInfo))
            return false;

        if (matches(m_secondaryBackBufferInfo))
            return false;

        return true;
    });

    LayerContentsBufferInfo result = { { }, false };
    for (auto& current : cachedBuffers) {
        if (m_frontBufferInfo->resourceIdentifier == current.imageBufferInfo.resourceIdentifier) {
            result.buffer = current.buffer;
#if ENABLE(PIXEL_FORMAT_RGBA16F)
            if (current.ioSurface->pixelFormat() == WebCore::IOSurface::Format::RGBA16F)
                result.hasExtendedDynamicRange = true;
#endif
            break;
        }
    }

    if (!result.buffer && m_bufferHandle && std::holds_alternative<MachSendRight>(*m_bufferHandle)) {
        if (auto surface = WebCore::IOSurface::createFromSendRight(std::get<MachSendRight>(*std::exchange(m_bufferHandle, std::nullopt)))) {
            result.buffer = surface->asCAIOSurfaceLayerContents();
#if ENABLE(PIXEL_FORMAT_RGBA16F)
            if (surface->pixelFormat() == WebCore::IOSurface::Format::RGBA16F)
                result.hasExtendedDynamicRange = true;
#endif
            if (surface->isVolatile())
                RELEASE_LOG_ERROR(RemoteLayerTree, "Received volatile IOSurface");
            cachedBuffers.append({ *m_frontBufferInfo, result.buffer, WTFMove(surface) });
        }
    }

    node.setCachedContentsBuffers(WTFMove(cachedBuffers));
    return result;
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
    case BackingStoreNeedsDisplayReason::None: ts << "none"_s; break;
    case BackingStoreNeedsDisplayReason::NoFrontBuffer: ts << "no front buffer"_s; break;
    case BackingStoreNeedsDisplayReason::FrontBufferIsVolatile: ts << "volatile front buffer"_s; break;
    case BackingStoreNeedsDisplayReason::FrontBufferHasNoSharingHandle: ts << "no front buffer sharing handle"_s; break;
    case BackingStoreNeedsDisplayReason::HasDirtyRegion: ts << "has dirty region"_s; break;
    }

    return ts;
}

RemoteLayerBackingStoreOrProperties::RemoteLayerBackingStoreOrProperties(std::unique_ptr<RemoteLayerBackingStoreProperties>&& properties)
    : properties(WTFMove(properties)) { }

} // namespace WebKit
