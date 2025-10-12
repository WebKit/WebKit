/*
 * Copyright (C) 2025 Igalia S.L.
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
#include "SkiaReplayCanvas.h"

#if USE(COORDINATED_GRAPHICS) && USE(SKIA)
#include "GLContext.h"
#include "GLFence.h"
#include "PlatformDisplay.h"

WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/gpu/ganesh/GrBackendSurface.h>
#include <skia/gpu/ganesh/SkImageGanesh.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END

namespace WebCore {

SkiaReplayCanvas::SkiaReplayCanvas(const IntSize& size, const RefPtr<SkiaRecordingResult>& recording)
    : SkNWayCanvas(size.width(), size.height())
    , m_recording(recording)
{
}

SkiaReplayCanvas::~SkiaReplayCanvas() = default;

Ref<SkiaReplayCanvas> SkiaReplayCanvas::create(const IntSize& size, const RefPtr<SkiaRecordingResult>& recodingResult)
{
    return adoptRef(*new SkiaReplayCanvas(size, recodingResult));
}

sk_sp<SkImage> SkiaReplayCanvas::waitForRenderingCompletionAndRewrapImageIfNeeded(const SkImage* image)
{
    if (!image || !image->isTextureBacked())
        return nullptr;

    auto* glContext = PlatformDisplay::sharedDisplay().skiaGLContext();
    if (!glContext || !glContext->makeContextCurrent())
        return nullptr;

    m_recording->waitForFenceIfNeeded(*image);

    auto* grContext = PlatformDisplay::sharedDisplay().skiaGrContext();
    if (image->isValid(grContext->asRecorder()))
        return nullptr;

    // FIXME: Add error reporting mechanism, a failure from GetBackendTextureFromImage() should be visible / reported.
    GrBackendTexture backendTexture;
    if (!SkImages::GetBackendTextureFromImage(image, &backendTexture, false))
        return nullptr;

    return SkImages::BorrowTextureFrom(grContext, backendTexture, kTopLeft_GrSurfaceOrigin, image->colorType(), image->alphaType(), image->refColorSpace());
}

void SkiaReplayCanvas::invokeDrawFunctionWithImage(const SkImage* image, Function<void(const SkImage*)>&& drawFunction)
{
    if (auto wrappedImage = waitForRenderingCompletionAndRewrapImageIfNeeded(image)) {
        drawFunction(wrappedImage.get());
        return;
    }

    drawFunction(image);
}

void SkiaReplayCanvas::invokeDrawFunctionWithPaint(const SkPaint& paint, Function<void(const SkPaint&)>&& drawFunction)
{
    auto* shader = paint.getShader();

    SkMatrix localMatrix;
    std::array<SkTileMode, 2> mode;
    auto* image = shader ? shader->isAImage(&localMatrix, mode.data()) : nullptr;
    if (auto wrappedImage = waitForRenderingCompletionAndRewrapImageIfNeeded(image)) {
        // FIXME: There is no way to get the SkSamplingOptions that were used to create the original shader.
        // Add Skia API? (SkImageShader stores SkSamplingOptions but is private and not installed).
        auto paintCopy = paint;
        paintCopy.setShader(wrappedImage->makeShader(mode[0], mode[1], SkSamplingOptions(), &localMatrix));
        drawFunction(paintCopy);
        return;
    }

    drawFunction(paint);
}

void SkiaReplayCanvas::invokeDrawFunctionWithShader(const SkShader* shader, Function<void(const SkShader*)>&& drawFunction)
{
    SkMatrix localMatrix;
    std::array<SkTileMode, 2> mode;
    auto* image = shader ? shader->isAImage(&localMatrix, mode.data()) : nullptr;
    if (auto wrappedImage = waitForRenderingCompletionAndRewrapImageIfNeeded(image)) {
        // FIXME: There is no way to get the SkSamplingOptions that were used to create the original shader.
        // Add Skia API? (SkImageShader stores SkSamplingOptions but is private and not installed).
        auto shaderCopy = wrappedImage->makeShader(mode[0], mode[1], SkSamplingOptions(), &localMatrix);
        drawFunction(shaderCopy.get());
        return;
    }

    drawFunction(shader);
}

void SkiaReplayCanvas::onClipShader(sk_sp<SkShader> shader, SkClipOp clipOp)
{
    invokeDrawFunctionWithShader(shader.get(), [&](const SkShader* shader) {
        SkNWayCanvas::onClipShader(sk_ref_sp(shader), clipOp);
    });
}

void SkiaReplayCanvas::onDrawArc(const SkRect& rect, SkScalar startAngle, SkScalar sweepAngle, bool useCenter, const SkPaint& paint)
{
    invokeDrawFunctionWithPaint(paint, [&](const SkPaint& paint) {
        SkNWayCanvas::onDrawArc(rect, startAngle, sweepAngle, useCenter, paint);
    });
}

void SkiaReplayCanvas::onDrawAtlas2(const SkImage* atlas, const SkRSXform xform[], const SkRect tex[], const SkColor colors[], int count, SkBlendMode mode, const SkSamplingOptions& sampling, const SkRect* cull, const SkPaint* paint)
{
    invokeDrawFunctionWithImage(atlas, [&](const SkImage* atlas) {
        SkNWayCanvas::onDrawAtlas2(atlas, xform, tex, colors, count, mode, sampling, cull, paint);
    });
}

void SkiaReplayCanvas::onDrawBehind(const SkPaint& paint)
{
    invokeDrawFunctionWithPaint(paint, [&](const SkPaint& paint) {
        SkNWayCanvas::onDrawBehind(paint);
    });
}

void SkiaReplayCanvas::onDrawDRRect(const SkRRect& outer, const SkRRect& inner, const SkPaint& paint)
{
    invokeDrawFunctionWithPaint(paint, [&](const SkPaint& paint) {
        SkNWayCanvas::onDrawDRRect(outer, inner, paint);
    });
}

void SkiaReplayCanvas::onDrawEdgeAAImageSet2(const ImageSetEntry set[], int count, const SkPoint dstClips[], const SkMatrix preViewMatrices[], const SkSamplingOptions& sampling, const SkPaint* paint, SrcRectConstraint constraint)
{
    if (!paint) {
        SkNWayCanvas::onDrawEdgeAAImageSet2(set, count, dstClips, preViewMatrices, sampling, nullptr, constraint);
        return;
    }

    invokeDrawFunctionWithPaint(*paint, [&](const SkPaint& paint) {
        SkNWayCanvas::onDrawEdgeAAImageSet2(set, count, dstClips, preViewMatrices, sampling, &paint, constraint);
    });
}

void SkiaReplayCanvas::onDrawGlyphRunList(const sktext::GlyphRunList& glyphRunList, const SkPaint& paint)
{
    invokeDrawFunctionWithPaint(paint, [&](const SkPaint& paint) {
        SkNWayCanvas::onDrawGlyphRunList(glyphRunList, paint);
    });
}

void SkiaReplayCanvas::onDrawImage2(const SkImage* image, SkScalar x, SkScalar y, const SkSamplingOptions& sampling, const SkPaint* paint)
{
    invokeDrawFunctionWithImage(image, [&](const SkImage* image) {
        SkNWayCanvas::onDrawImage2(image, x, y, sampling, paint);
    });
}

void SkiaReplayCanvas::onDrawImageLattice2(const SkImage* image, const Lattice& lattice, const SkRect& dst, SkFilterMode filter, const SkPaint* paint)
{
    invokeDrawFunctionWithImage(image, [&](const SkImage* image) {
        SkNWayCanvas::onDrawImageLattice2(image, lattice, dst, filter, paint);
    });
}

void SkiaReplayCanvas::onDrawImageRect2(const SkImage* image, const SkRect& src, const SkRect& dst, const SkSamplingOptions& sampling, const SkPaint* paint, SrcRectConstraint constraint)
{
    invokeDrawFunctionWithImage(image, [&](const SkImage* image) {
        SkNWayCanvas::onDrawImageRect2(image, src, dst, sampling, paint, constraint);
    });
}

void SkiaReplayCanvas::onDrawOval(const SkRect& oval, const SkPaint& paint)
{
    invokeDrawFunctionWithPaint(paint, [&](const SkPaint& paint) {
        SkNWayCanvas::onDrawOval(oval, paint);
    });
}

void SkiaReplayCanvas::onDrawPaint(const SkPaint& paint)
{
    invokeDrawFunctionWithPaint(paint, [&](const SkPaint& paint) {
        SkNWayCanvas::onDrawPaint(paint);
    });
}

void SkiaReplayCanvas::onDrawPoints(PointMode mode, size_t count, const SkPoint points[], const SkPaint& paint)
{
    invokeDrawFunctionWithPaint(paint, [&](const SkPaint& paint) {
        SkNWayCanvas::onDrawPoints(mode, count, points, paint);
    });
}

void SkiaReplayCanvas::onDrawPatch(const SkPoint cubics[12], const SkColor colors[4], const SkPoint texCoords[4], SkBlendMode blendMode, const SkPaint& paint)
{
    invokeDrawFunctionWithPaint(paint, [&](const SkPaint& paint) {
        SkNWayCanvas::onDrawPatch(cubics, colors, texCoords, blendMode, paint);
    });
}

void SkiaReplayCanvas::onDrawPath(const SkPath& path, const SkPaint& paint)
{
    invokeDrawFunctionWithPaint(paint, [&](const SkPaint& paint) {
        SkNWayCanvas::onDrawPath(path, paint);
    });
}

void SkiaReplayCanvas::onDrawRRect(const SkRRect& rrect, const SkPaint& paint)
{
    invokeDrawFunctionWithPaint(paint, [&](const SkPaint& paint) {
        SkNWayCanvas::onDrawRRect(rrect, paint);
    });
}

void SkiaReplayCanvas::onDrawRect(const SkRect& rect, const SkPaint& paint)
{
    invokeDrawFunctionWithPaint(paint, [&](const SkPaint& paint) {
        SkNWayCanvas::onDrawRect(rect, paint);
    });
}

void SkiaReplayCanvas::onDrawRegion(const SkRegion& region, const SkPaint& paint)
{
    invokeDrawFunctionWithPaint(paint, [&](const SkPaint& paint) {
        SkNWayCanvas::onDrawRegion(region, paint);
    });
}

void SkiaReplayCanvas::onDrawSlug(const sktext::gpu::Slug* slug, const SkPaint& paint)
{
    invokeDrawFunctionWithPaint(paint, [&](const SkPaint& paint) {
        SkNWayCanvas::onDrawSlug(slug, paint);
    });
}

void SkiaReplayCanvas::onDrawTextBlob(const SkTextBlob* blob, SkScalar x, SkScalar y, const SkPaint& paint)
{
    invokeDrawFunctionWithPaint(paint, [&](const SkPaint& paint) {
        SkNWayCanvas::onDrawTextBlob(blob, x, y, paint);
    });
}

void SkiaReplayCanvas::onDrawVerticesObject(const SkVertices* vertices, SkBlendMode mode, const SkPaint& paint)
{
    invokeDrawFunctionWithPaint(paint, [&](const SkPaint& paint) {
        SkNWayCanvas::onDrawVerticesObject(vertices, mode, paint);
    });
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && USE(SKIA)
