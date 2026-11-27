/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
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

#if !__has_feature(objc_arc)
#error This header requires ARC. Include it only from files compiled with -fobjc-arc.
#endif

#import "ConstantValue.h"
#import "ShaderModule.h"
#import <wtf/Function.h>
#import <wtf/Scope.h>

namespace WebGPU::Metal {

class BindGroup;
class Device;
class Instance;
class ShaderModule;

using BufferBindingSizesForBindGroup = HashMap<uint32_t, uint64_t>;
using BufferBindingSizesForPipeline = HashMap<uint32_t, BufferBindingSizesForBindGroup>;

struct PreparedLibrary {
    id<MTLLibrary> library { nil };
    String msl;
    String label;
    WGSL::DeviceState deviceState;
    WGSL::Reflection::EntryPointInformation entryPointInformation; // FIXME(PERFORMANCE): This is big. Don't copy this around.
    HashMap<String, WGSL::ConstantValue> wgslConstantValues;
};

std::optional<PreparedLibrary> prepareLibrary(const ShaderModule&, PipelineLayout*, const String& entryPointName, NSString *label, std::span<const WebGPU::ConstantEntry> constants, BufferBindingSizesForPipeline&, NSError **);

struct LibraryCompileRequest {
    id<MTLLibrary> cachedLibrary { nil };
    String mslKey;
    String labelKey;
    WGSL::DeviceState deviceState;

    RetainPtr<NSString> mslString() const { return mslKey.createNSString(); }
    RetainPtr<NSString> labelString() const { return labelKey.isEmpty() ? nil : labelKey.createNSString(); }
};

LibraryCompileRequest libraryCompileRequest(const PreparedLibrary&);

struct RenderPSODescriptorSignature {
    uint16_t depthFormat { 0 };
    uint16_t stencilFormat { 0 };
    uint8_t rasterSampleCount { 0 };
    uint8_t topologyAndFlags { 0 };
    uint32_t descriptorSampleMask { 0xFFFFFFFFu };
    Vector<uint64_t> words;

    friend bool operator==(const RenderPSODescriptorSignature&, const RenderPSODescriptorSignature&) = default;
};

struct CachedRenderPipelineState {
    RetainPtr<id<MTLFunction>> vertexFunction;
    RetainPtr<id<MTLFunction>> fragmentFunction;
    RetainPtr<id<MTLRenderPipelineState>> pso;
};

id<MTLLibrary> lookupCachedLibrary(id<MTLDevice>, const LibraryCompileRequest&);
void cacheCompiledLibrary(id<MTLDevice>, const LibraryCompileRequest&, id<MTLLibrary>);
id<MTLComputePipelineState> lookupCachedComputePipelineState(id<MTLLibrary>, const String& mangledName, uint32_t bindGroupCount, uint32_t validationState);
void cacheComputePipelineState(id<MTLLibrary>, const String& mangledName, uint32_t bindGroupCount, uint32_t validationState, id<MTLComputePipelineState>);
std::optional<CachedRenderPipelineState> lookupCachedRenderPipelineState(id<MTLLibrary> vertexLibrary, id<MTLLibrary> fragmentLibrary, const String& vertexMangledName, const String& fragmentMangledName, const RenderPSODescriptorSignature&);
void cacheRenderPipelineState(id<MTLLibrary> vertexLibrary, id<MTLLibrary> fragmentLibrary, const String& vertexMangledName, const String& fragmentMangledName, const RenderPSODescriptorSignature&, id<MTLFunction> vertexFunction, id<MTLFunction> fragmentFunction, id<MTLRenderPipelineState>);
void compileOrCoalesceLibraryAsync(id<MTLDevice>, const LibraryCompileRequest&, Function<void(id<MTLLibrary>, NSError *)>&&);
void compileOrCoalesceComputePSOAsync(id<MTLDevice>, id<MTLLibrary>, const String& mangledName, NSString *label, uint32_t bindGroupCount, uint32_t validationState, Function<void(id<MTLComputePipelineState>, NSString *errorMessage, bool isCompileFailure)>&&);
void compileOrCoalesceRenderPSOAsync(id<MTLDevice>, id<MTLLibrary> vertexLibrary, id<MTLLibrary> fragmentLibrary, const String& vertexMangledName, const String& fragmentMangledName, const RenderPSODescriptorSignature&, MTLRenderPipelineDescriptor *, NSString *label, Function<void(id<MTLFunction> vertexFunction, id<MTLFunction> fragmentFunction, id<MTLRenderPipelineState>, NSString *functionError)>&&);
void purgeMetalPipelineCaches();

id<MTLLibrary> compileLibrary(id<MTLDevice>, const LibraryCompileRequest&, NSError **);

void compileLibraryAsync(id<MTLDevice>, Instance&, const LibraryCompileRequest&, CompletionHandler<void(id<MTLLibrary>, NSError *)>&&);
void createComputePipelineStateAsync(id<MTLDevice>, Instance&, MTLComputePipelineDescriptor *, CompletionHandler<void(id<MTLComputePipelineState>, NSError *)>&&);
void createRenderPipelineStateAsync(id<MTLDevice>, Instance&, MTLRenderPipelineDescriptor *, CompletionHandler<void(id<MTLRenderPipelineState>, NSError *)>&&);

ScopeExit<Function<void()>> scopedErrorReporting(Device&, bool suppressErrors);

id<MTLFunction> createFunction(id<MTLLibrary>, const WGSL::Reflection::EntryPointInformation&, NSString *label);
id<MTLFunction> createFunction(id<MTLLibrary>, NSString *functionName, NSString *label);

NSString* errorValidatingBindGroup(const BindGroup&, const BufferBindingSizesForBindGroup*, const Vector<uint32_t>*);

#if !defined(NDEBUG) || (defined(ENABLE_LIBFUZZER) && ENABLE_LIBFUZZER && defined(ASAN_ENABLED) && ASAN_ENABLED)
void dumpMetalReproCaseComputePSO(String&& shaderSource, String&& functionName);
bool dumpMetalReproCaseRenderPSO(String&& vertexShaderSource, String&& vertexFunctionName, String&& fragmentShaderSource, String&& fragmentFunctionName, MTLRenderPipelineDescriptor*, ShaderModule::VertexStageIn& shaderLocations, const Device&);
void clearMetalPSORepro();
#endif

} // namespace WebGPU::Metal
