/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "MeshImpl.h"

#if ENABLE(GPU_PROCESS_MODEL)

#include "ModelTypes.h"
#include "WebKitMesh.h"
#include <WebCore/CVUtilities.h>
#include <WebCore/IOSurface.h>
#include <WebCore/NativeImage.h>
#include <WebCore/PixelBufferConversion.h>
#include <wtf/Borrow.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(MeshImpl);

MeshImpl::MeshImpl(Ref<WebMesh>&& mesh, Vector<UniqueRef<WebCore::IOSurface>>&& renderBuffers, const WebCore::IntSize& renderBufferSize, bool standardDynamicRange)
    : m_backing(WTF::move(mesh))
    , m_renderBuffers(WTF::move(renderBuffers))
    , m_renderBufferSize(renderBufferSize)
    , m_standardDynamicRange(standardDynamicRange)
{
}

MeshImpl::~MeshImpl() = default;

void MeshImpl::setLabelInternal(const String&)
{
    // FIXME: Implement this.
}

void MeshImpl::updateModel(WebCore::NodeIdentifier nodeIdentifier, Vector<WebModel::UpdateMeshDescriptor>&& descriptor)
{
    m_backing->updateMesh(nodeIdentifier, WTF::move(descriptor));
}

void MeshImpl::updateTexture(Vector<WebModel::UpdateTextureDescriptor>&& descriptor)
{
    m_backing->updateTexture(WTF::move(descriptor));
}

void MeshImpl::updateMaterial(Vector<WebModel::UpdateMaterialDescriptor>&& descriptor)
{
    m_backing->updateMaterial(WTF::move(descriptor));
}

void MeshImpl::render(uint32_t textureIndex, Function<void(bool)>&& completionHandler)
{
    m_backing->render(textureIndex, WTF::move(completionHandler));
}

void MeshImpl::setEntityTransform(const WebModel::Float4x4& transform)
{
    m_backing->setTransform(transform);
}

#if ENABLE(SPATIAL_PORTAL)
void MeshImpl::setPortalChildTransform(WebCore::NodeIdentifier nodeIdentifier, const WebModel::Float4x4& transform)
{
    m_backing->setTransform(nodeIdentifier, transform);
}

bool MeshImpl::updatePortalContentTexture(const WebModel::TypedResourceId& identifier, IOSurfaceRef surface, bool isOpaque, uint64_t bufferIdentifier)
{
    return m_backing->updatePortalContentTexture(identifier, surface, isOpaque, bufferIdentifier);
}

void MeshImpl::removePortalContentTexture(uint64_t bufferIdentifier)
{
    m_backing->removePortalContentTexture(bufferIdentifier);
}

void MeshImpl::clearPortalContentTextures()
{
    m_backing->clearPortalContentTextures();
}
#endif

#if PLATFORM(COCOA)
std::optional<WebModel::Float4x4> MeshImpl::entityTransform() const
{
    return std::nullopt;
}

#if ENABLE(SPATIAL_PORTAL)
std::optional<WebModel::Float4x4> MeshImpl::resolvedPortalTransform() const
{
    return std::nullopt;
}
#endif

void MeshImpl::removeModel(WebCore::NodeIdentifier nodeIdentifier)
{
    m_backing->removeNode(nodeIdentifier);
}
#endif

void MeshImpl::setFOV(float fovY)
{
    m_backing->setFOV(fovY);
}

void MeshImpl::play(bool play)
{
    m_backing->play(play);
}

void MeshImpl::setEnvironmentMap(WebModel::UpdateTextureDescriptor&& imageAsset)
{
    m_backing->setEnvironmentMap(WTF::move(imageAsset));
}

#if ENABLE(SPATIAL_PORTAL)
void MeshImpl::setDefaultEnvironmentMap()
{
    m_backing->setDefaultEnvironmentMap();
}

void MeshImpl::removeEnvironmentMap()
{
    m_backing->removeEnvironmentMap();
}
#endif

void MeshImpl::updateContentsHeadroom(float headroom)
{
#if HAVE(SUPPORT_HDR_DISPLAY) && PLATFORM(COCOA)
    m_contentsHeadroom = headroom;
    for (auto& renderBuffer : m_renderBuffers)
        renderBuffer->setContentEDRHeadroom(headroom);
#else
    UNUSED_PARAM(headroom);
#endif
}

#if PLATFORM(COCOA)
Vector<MachSendRight> MeshImpl::ioSurfaceHandles()
{
    return m_renderBuffers.map([](const auto& renderBuffer) {
        return renderBuffer->createSendRight();
    });
}

void MeshImpl::updateRenderBuffers(WebModel::ResizeMeshDescriptor&& descriptor)
{
    m_backing->updateRenderBuffers(descriptor);
    m_renderBufferSize = { static_cast<int>(descriptor.width), static_cast<int>(descriptor.height) };
    m_renderBuffers = WTF::move(descriptor.renderBuffers);
}

RefPtr<WebCore::NativeImage> MeshImpl::copyNativeImage(uint32_t bufferIndex)
{
    // The render buffer is drawn to again in later frames, so its contents are copied, by the CPU, as
    // there is no presentation context to blit with. The copy has the format of the render buffers,
    // which the Web Process knows, and is not drawn to again.
    auto format = renderBuffersHaveFloatComponents(m_standardDynamicRange) ? WebCore::IOSurface::Format::RGBA16F : WebCore::IOSurface::Format::BGRA;
    const auto& colorSpace = renderBufferColorSpace(m_standardDynamicRange);
    auto copy = WebCore::IOSurface::create(nullptr, m_renderBufferSize, colorSpace, WebCore::IOSurface::Name::WebGPU, format);
    if (!copy)
        return nullptr;
#if HAVE(SUPPORT_HDR_DISPLAY)
    if (m_contentsHeadroom)
        copy->setContentEDRHeadroom(*m_contentsHeadroom);
#endif

    bool didCopy = false;
    if (bufferIndex < m_renderBuffers.size()) {
        Borrow renderBuffers = m_renderBuffers;
        auto& renderBuffer = renderBuffers[bufferIndex].get();
        if (renderBuffer.size() == m_renderBufferSize && renderBuffer.hasFormat({ format, WebCore::UseLosslessCompression::No })) {
            auto source = renderBuffer.lock<WebCore::IOSurface::AccessMode::ReadOnly>();
            auto destination = copy->lock<WebCore::IOSurface::AccessMode::ReadWrite>();
            if (source && destination) {
                auto sourceBytesPerRow = static_cast<unsigned>(renderBuffer.bytesPerRow());
                auto destinationBytesPerRow = static_cast<unsigned>(copy->bytesPerRow());
                WebCore::copyRows(sourceBytesPerRow, std::as_const(*source).surfaceSpan(), destinationBytesPerRow, destination->surfaceSpan(), static_cast<unsigned>(m_renderBufferSize.height()), std::min(sourceBytesPerRow, destinationBytesPerRow));
                didCopy = true;
            }
        }
    }
    if (!didCopy) {
        // The image of a render buffer that cannot be read is transparent, as the reader waits for an
        // image to be published in any case.
        RetainPtr context = copy->createPlatformContext();
        if (!context)
            return nullptr;
        CGContextClearRect(context.get(), CGRectMake(0, 0, m_renderBufferSize.width(), m_renderBufferSize.height()));
        CGContextFlush(context.get());
    }

    // The image holds the copy through a CVPixelBuffer.
    RetainPtr surface = copy->surface();
    auto pixelBuffer = WebCore::createCVPixelBuffer(surface.get());
    if (!pixelBuffer)
        return nullptr;
    auto alphaInfo = renderBufferImageHasAlpha(m_standardDynamicRange) ? kCGImageAlphaPremultipliedFirst : kCGImageAlphaNoneSkipLast;
    // The image has the headroom of the contents, as the render buffers do.
    auto contentHeadroom = WebCore::Headroom::None;
#if HAVE(SUPPORT_HDR_DISPLAY)
    if (m_contentsHeadroom)
        contentHeadroom = WebCore::Headroom { std::max<float>(*m_contentsHeadroom, WebCore::Headroom::None) };
#endif
    return WebCore::NativeImage::createTransient(WTF::move(*pixelBuffer), alphaInfo, colorSpace.platformColorSpace(), contentHeadroom);
}
#endif

void MeshImpl::processRemovals(Vector<WebModel::TypedResourceId>&& meshRemovals, Vector<WebModel::TypedResourceId>&& materialRemovals, Vector<WebModel::TypedResourceId>&& textureRemovals, CompletionHandler<void(bool)>&& completion)
{
    m_backing->processRemovals(WTF::move(meshRemovals), WTF::move(materialRemovals), WTF::move(textureRemovals), WTF::move(completion));
}

}

#endif
