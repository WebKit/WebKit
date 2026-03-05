/*
 * Copyright (C) 2025, 2026 Igalia S.L.
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

#include "config.h"
#include "SkiaReplayAtlas.h"

#if USE(SKIA)

#include "PlatformDisplay.h"
#include "SkiaGPUAtlas.h"
#include <wtf/TZoneMallocInlines.h>

WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkColorSpace.h>
#include <skia/gpu/ganesh/GrDirectContext.h>
#include <skia/gpu/ganesh/SkImageGanesh.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SkiaReplayAtlas);

SkiaReplayAtlas::SkiaReplayAtlas(Ref<const SkiaGPUAtlas>&& gpuAtlas, sk_sp<SkImage>&& rewrappedTexture)
    : m_gpuAtlas(WTF::move(gpuAtlas))
    , m_rewrappedTexture(WTF::move(rewrappedTexture))
{
}

SkiaReplayAtlas::~SkiaReplayAtlas() = default;

std::unique_ptr<SkiaReplayAtlas> SkiaReplayAtlas::create(const SkiaGPUAtlas& gpuAtlas)
{
    if (!gpuAtlas.backendTexture().isValid())
        return nullptr;

    auto* grContext = PlatformDisplay::sharedDisplay().skiaGrContext();
    RELEASE_ASSERT(grContext);

    // Always rewrap the texture for this worker context.
    // The underlying GL texture was created on the main thread context but is
    // shareable via GL context sharing. However, the Skia SkImage wrapper is
    // context-specific and must be rewrapped for each worker's GrDirectContext.
    auto rewrapped = SkImages::BorrowTextureFrom(grContext, gpuAtlas.backendTexture(), kTopLeft_GrSurfaceOrigin, kRGBA_8888_SkColorType, kPremul_SkAlphaType, SkColorSpace::MakeSRGB());
    if (!rewrapped)
        return nullptr;

    return std::unique_ptr<SkiaReplayAtlas>(new SkiaReplayAtlas(gpuAtlas, WTF::move(rewrapped)));
}

std::optional<SkRect> SkiaReplayAtlas::rectForImage(const SkImage& image) const
{
    auto it = m_gpuAtlas->imageToRect().find(&image);
    if (it == m_gpuAtlas->imageToRect().end())
        return std::nullopt;
    return it->value;
}

} // namespace WebCore

#endif // USE(SKIA)
