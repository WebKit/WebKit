/*
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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

#import "config.h"
#import "DynamicContentScalingImageBufferBackend.h"

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)

#import "Logging.h"
#import <CoreRE/RECGCommandsContext.h>
#import <WebCore/DynamicContentScalingDisplayList.h>
#import <WebCore/GraphicsContextCG.h>
#import <WebCore/PixelBuffer.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/IsoMallocInlines.h>
#import <wtf/MachSendRight.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/cocoa/VectorCocoa.h>

template<> struct WTF::CFTypeTrait<CAMachPortRef> {
    static inline CFTypeID typeID(void) { return CAMachPortGetTypeID(); }
};

namespace WebKit {

static CFDictionaryRef makeContextOptions(const DynamicContentScalingImageBufferBackend::Parameters& parameters)
{
    RetainPtr colorSpace = parameters.colorSpace.platformColorSpace();
    if (!colorSpace)
        return nil;
    return (CFDictionaryRef)@{
        @"colorspace" : (id)colorSpace.get()
    };
}

class GraphicsContextDynamicContentScaling : public WebCore::GraphicsContextCG {
public:
    GraphicsContextDynamicContentScaling(const DynamicContentScalingImageBufferBackend::Parameters& parameters, WebCore::RenderingMode renderingMode)
        : GraphicsContextCG(adoptCF(RECGCommandsContextCreate(parameters.backendSize, makeContextOptions(parameters))).autorelease(), GraphicsContextCG::Unknown, renderingMode)
    {
    }

    bool canUseShadowBlur() const final { return false; }
};

WTF_MAKE_ISO_ALLOCATED_IMPL(DynamicContentScalingImageBufferBackend);

size_t DynamicContentScalingImageBufferBackend::calculateMemoryCost(const Parameters& parameters)
{
    // FIXME: This is fairly meaningless, because we don't actually have a bitmap, and
    // should really be based on the encoded data size.
    return WebCore::ImageBufferBackend::calculateMemoryCost(parameters.backendSize, calculateBytesPerRow(parameters.backendSize));
}

std::unique_ptr<DynamicContentScalingImageBufferBackend> DynamicContentScalingImageBufferBackend::create(const Parameters& parameters, const WebCore::ImageBufferCreationContext& creationContext)
{
    if (parameters.backendSize.isEmpty())
        return nullptr;

    return std::unique_ptr<DynamicContentScalingImageBufferBackend>(new DynamicContentScalingImageBufferBackend(parameters, creationContext, WebCore::RenderingMode::Unaccelerated));
}

DynamicContentScalingImageBufferBackend::DynamicContentScalingImageBufferBackend(const Parameters& parameters, const WebCore::ImageBufferCreationContext& creationContext, WebCore::RenderingMode renderingMode)
    : ImageBufferCGBackend { parameters }
    , m_resourceCache(bridge_id_cast(adoptCF(RECGCommandsCacheCreate(nullptr))))
    , m_renderingMode(renderingMode)
{
}

std::optional<ImageBufferBackendHandle> DynamicContentScalingImageBufferBackend::createBackendHandle(SharedMemory::Protection) const
{
    if (!m_context)
        return std::nullopt;

    RetainPtr<NSDictionary> options;
    RetainPtr<NSMutableArray> ports;
    if (m_resourceCache) {
        ports = adoptNS([[NSMutableArray alloc] init]);
        options = @{
            @"ports": ports.get(),
            @"cache": m_resourceCache.get()
        };
    }

    auto data = adoptCF(RECGCommandsContextCopyEncodedDataWithOptions(m_context->platformContext(), bridge_cast(options.get())));
    ASSERT(data);

    Vector<MachSendRight> sendRights;
    if (m_resourceCache) {
        sendRights = makeVector(ports.get(), [] (id port) -> std::optional<MachSendRight> {
            // We `create` instead of `adopt` because CAMachPort has no API to leak its reference.
            return { MachSendRight::create(CAMachPortGetPort(checked_cf_cast<CAMachPortRef>(port))) };
        });
    }

    return WebCore::DynamicContentScalingDisplayList { WebCore::SharedBuffer::create(data.get()), WTFMove(sendRights) };
}

WebCore::GraphicsContext& DynamicContentScalingImageBufferBackend::context()
{
    if (!m_context) {
        m_context = makeUnique<GraphicsContextDynamicContentScaling>(m_parameters, m_renderingMode);
        applyBaseTransform(*m_context);
    }
    return *m_context;
}

unsigned DynamicContentScalingImageBufferBackend::bytesPerRow() const
{
    return calculateBytesPerRow(m_parameters.backendSize);
}

void DynamicContentScalingImageBufferBackend::releaseGraphicsContext()
{
    m_context = nullptr;
}

RefPtr<WebCore::NativeImage> DynamicContentScalingImageBufferBackend::copyNativeImage()
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

RefPtr<WebCore::NativeImage> DynamicContentScalingImageBufferBackend::createNativeImageReference()
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

void DynamicContentScalingImageBufferBackend::getPixelBuffer(const WebCore::IntRect&, WebCore::PixelBuffer&)
{
    ASSERT_NOT_REACHED();
}

void DynamicContentScalingImageBufferBackend::putPixelBuffer(const WebCore::PixelBuffer&, const WebCore::IntRect&, const WebCore::IntPoint&, WebCore::AlphaPremultiplication)
{
    ASSERT_NOT_REACHED();
}

String DynamicContentScalingImageBufferBackend::debugDescription() const
{
    TextStream stream;
    stream << "DynamicContentScalingImageBufferBackend " << this;
    return stream.release();
}

}

#endif // ENABLE(RE_DYNAMIC_CONTENT_SCALING)
