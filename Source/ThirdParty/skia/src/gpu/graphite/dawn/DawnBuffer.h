/*
 * Copyright 2022 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef skgpu_graphite_DawnBuffer_DEFINED
#define skgpu_graphite_DawnBuffer_DEFINED

#include "webgpu/webgpu_cpp.h"  // NO_G3_REWRITE

#include "include/core/SkRefCnt.h"
#include "include/gpu/graphite/dawn/DawnTypes.h"
#include "include/private/base/SkTArray.h"
#include "src/gpu/RefCntedCallback.h"
#include "src/gpu/graphite/Buffer.h"
#include "src/gpu/graphite/dawn/DawnAsyncWait.h"
#include "src/gpu/graphite/dawn/DawnSharedContext.h"

namespace skgpu::graphite {

class DawnBuffer : public Buffer {
public:
    static sk_sp<DawnBuffer> Make(const DawnSharedContext*,
                                  size_t size,
                                  BufferType type,
                                  AccessPattern);
    static sk_sp<DawnBuffer> Make(const DawnSharedContext*,
                                  size_t size,
                                  BufferType type,
                                  AccessPattern,
                                  const char* label);

    bool isUnmappable() const override;

    const wgpu::Buffer& dawnBuffer() const { return fBuffer; }

    void prepareForReturnToCache(const std::function<void()>& takeRef) override;

private:
    DawnBuffer(const DawnSharedContext*,
               size_t size,
               wgpu::Buffer,
               void* mapAtCreationPtr);

    void onMap() override;
    void onAsyncMap(GpuFinishedProc, GpuFinishedContext) override;
    void onUnmap() override;

    void freeGpuData() override;

    const DawnSharedContext* dawnSharedContext() const {
        return static_cast<const DawnSharedContext*>(this->sharedContext());
    }

    wgpu::Buffer fBuffer;
    SkMutex fAsyncMutex;
    skia_private::TArray<sk_sp<RefCntedCallback>> fAsyncMapCallbacks SK_GUARDED_BY(fAsyncMutex);
};

} // namespace skgpu::graphite

#endif // skgpu_graphite_DawnBuffer_DEFINED

