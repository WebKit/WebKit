/*
 * Copyright (C) 2020 Sony Interactive Entertainment Inc.
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

#pragma once

#include <objc/NSObjCRuntime.h>
#include <wtf/RetainPtr.h>

OBJC_CLASS CAMetalDrawable;
OBJC_CLASS WebGPULayer;

OBJC_PROTOCOL(MTLArgumentEncoder);
OBJC_PROTOCOL(MTLBlitCommandEncoder);
OBJC_PROTOCOL(MTLBuffer);
OBJC_PROTOCOL(MTLCommandBuffer);
OBJC_PROTOCOL(MTLCommandEncoder);
OBJC_PROTOCOL(MTLCommandQueue);
OBJC_PROTOCOL(MTLComputeCommandEncoder);
OBJC_PROTOCOL(MTLComputePipelineState);
OBJC_PROTOCOL(MTLDepthStencilState);
OBJC_PROTOCOL(MTLDevice);
OBJC_PROTOCOL(MTLLibrary);
OBJC_PROTOCOL(MTLRenderCommandEncoder);
OBJC_PROTOCOL(MTLRenderPipelineState);
OBJC_PROTOCOL(MTLResource);
OBJC_PROTOCOL(MTLSamplerState);
OBJC_PROTOCOL(MTLTexture);

namespace WebCore {

using PlatformGPUBufferOffset = NSUInteger;

// Metal types
using PlatformBuffer = MTLBuffer;
using PlatformBufferSmartPtr = RetainPtr<PlatformBuffer>;

using PlatformCommandBuffer = MTLCommandBuffer;
using PlatformCommandBufferSmartPtr = RetainPtr<PlatformCommandBuffer>;

using PlatformComputePassEncoder = MTLComputeCommandEncoder;
using PlatformComputePassEncoderSmartPtr = RetainPtr<PlatformComputePassEncoder>;

using PlatformComputePipeline = MTLComputePipelineState;
using PlatformComputePipelineSmartPtr = RetainPtr<PlatformComputePipeline>;

using PlatformDevice = MTLDevice;
using PlatformDeviceSmartPtr = RetainPtr<PlatformDevice>;

using PlatformDrawable = CAMetalDrawable;
using PlatformDrawableSmartPtr = RetainPtr<PlatformDrawable>;

using PlatformProgrammablePassEncoder = MTLCommandEncoder;
using PlatformProgrammablePassEncoderSmartPtr = RetainPtr<PlatformProgrammablePassEncoder>;

using PlatformQueue = MTLCommandQueue;
using PlatformQueueSmartPtr = RetainPtr<PlatformQueue>;

using PlatformRenderPassEncoder = MTLRenderCommandEncoder;
using PlatformRenderPassEncoderSmartPtr = RetainPtr<PlatformRenderPassEncoder>;

using PlatformRenderPipeline = MTLRenderPipelineState;
using PlatformRenderPipelineSmartPtr = RetainPtr<PlatformRenderPipeline>;

using PlatformSampler = MTLSamplerState;
using PlatformSamplerSmartPtr = RetainPtr<PlatformSampler>;

using PlatformShaderModule = MTLLibrary;
using PlatformShaderModuleSmartPtr = RetainPtr<PlatformShaderModule>;

using PlatformSwapLayer = WebGPULayer;
using PlatformSwapLayerSmartPtr = RetainPtr<PlatformSwapLayer>;

using PlatformTexture = MTLTexture;
using PlatformTextureSmartPtr = RetainPtr<PlatformTexture>;

} // namespace WebCore
