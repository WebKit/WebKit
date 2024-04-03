/*
 * Copyright 2021 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/ganesh/GrWritePixelsRenderTask.h"

#include "src/gpu/ganesh/GrGpu.h"
#include "src/gpu/ganesh/GrOpFlushState.h"
#include "src/gpu/ganesh/GrResourceAllocator.h"

sk_sp<GrRenderTask> GrWritePixelsTask::Make(GrDrawingManager* dm,
                                            sk_sp<GrSurfaceProxy> dst,
                                            SkIRect rect,
                                            GrColorType srcColorType,
                                            GrColorType dstColorType,
                                            const GrMipLevel texels[],
                                            int levelCount) {
    return sk_sp<GrRenderTask>(new GrWritePixelsTask(dm,
                                                     std::move(dst),
                                                     rect,
                                                     srcColorType,
                                                     dstColorType,
                                                     texels,
                                                     levelCount));
}

GrWritePixelsTask::GrWritePixelsTask(GrDrawingManager* dm,
                                     sk_sp<GrSurfaceProxy> dst,
                                     SkIRect rect,
                                     GrColorType srcColorType,
                                     GrColorType dstColorType,
                                     const GrMipLevel texels[],
                                     int levelCount)
        : fRect(rect)
        , fSrcColorType(srcColorType)
        , fDstColorType(dstColorType) {
    this->addTarget(dm, std::move(dst));
    fLevels.reset(levelCount);
    std::copy_n(texels, levelCount, fLevels.get());
}

void GrWritePixelsTask::gatherProxyIntervals(GrResourceAllocator* alloc) const {
    alloc->addInterval(this->target(0), alloc->curOp(), alloc->curOp(),
                       GrResourceAllocator::ActualUse::kYes,
                       GrResourceAllocator::AllowRecycling::kYes);
    alloc->incOps();
}

GrRenderTask::ExpectedOutcome GrWritePixelsTask::onMakeClosed(GrRecordingContext*,
                                                              SkIRect* targetUpdateBounds) {
    *targetUpdateBounds = fRect;
    return ExpectedOutcome::kTargetDirty;
}

bool GrWritePixelsTask::onExecute(GrOpFlushState* flushState) {
    GrSurfaceProxy* dstProxy = this->target(0);
    if (!dstProxy->isInstantiated()) {
        return false;
    }
    GrSurface* dstSurface = dstProxy->peekSurface();
    return flushState->gpu()->writePixels(dstSurface,
                                          fRect,
                                          fDstColorType,
                                          fSrcColorType,
                                          fLevels.get(),
                                          fLevels.count());
}
