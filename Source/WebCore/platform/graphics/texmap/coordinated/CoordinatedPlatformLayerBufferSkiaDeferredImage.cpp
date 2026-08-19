/*
 * Copyright (C) 2026 Igalia S.L.
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
#include "CoordinatedPlatformLayerBufferSkiaDeferredImage.h"

#if USE(COORDINATED_GRAPHICS) && USE(SKIA)
#include "PlatformDisplay.h"
#include "SkiaUtilities.h"

WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/gpu/ganesh/SkSurfaceGanesh.h>
#include <skia/private/chromium/GrSurfaceCharacterization.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END

namespace WebCore {

std::unique_ptr<CoordinatedPlatformLayerBufferSkiaDeferredImage> CoordinatedPlatformLayerBufferSkiaDeferredImage::create(sk_sp<GrDeferredDisplayList>&& displayList)
{
    OptionSet<TextureMapperFlags> flags;
    if (displayList->characterization().imageInfo().alphaType() != kOpaque_SkAlphaType)
        flags.add(TextureMapperFlags::ShouldBlend);
    return makeUnique<CoordinatedPlatformLayerBufferSkiaDeferredImage>(WTF::move(displayList), flags);
}

CoordinatedPlatformLayerBufferSkiaDeferredImage::CoordinatedPlatformLayerBufferSkiaDeferredImage(sk_sp<GrDeferredDisplayList>&& displayList, OptionSet<TextureMapperFlags> flags)
    : CoordinatedPlatformLayerBuffer(Type::SkiaDeferredImage, { displayList->characterization().width(), displayList->characterization().height() }, flags, nullptr)
    , m_displayList(WTF::move(displayList))
{
}

sk_sp<SkImage> CoordinatedPlatformLayerBufferSkiaDeferredImage::skiaImage()
{
    if (!m_surface) {
        auto* grContext = PlatformDisplay::sharedDisplay().skiaGrContext();
        const auto& characterization = m_displayList->characterization();
        m_surface = SkSurfaces::RenderTarget(grContext, skgpu::Budgeted::kYes, characterization.imageInfo(), characterization.sampleCount(), characterization.origin(), &characterization.surfaceProps());
        if (!m_surface)
            return nullptr;

        skgpu::ganesh::DrawDDL(m_surface.get(), m_displayList);
    }

    return m_surface->makeImageSnapshot();
}

void CoordinatedPlatformLayerBufferSkiaDeferredImage::paintToTextureMapper(TextureMapper&, const FloatRect&, const TransformationMatrix&, float)
{
    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && USE(SKIA)
