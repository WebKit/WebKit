/*
 * Copyright (c) 2021-2023 Apple Inc. All rights reserved.
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
#import "Device.h"

#import "API.h"
#import "APIConversions.h"
#import "BindGroup.h"
#import "BindGroupLayout.h"
#import "Buffer.h"
#import "CommandEncoder.h"
#import "ComputePipeline.h"
#import "MetalSPI.h"
#import "PipelineLayout.h"
#import "PresentationContext.h"
#import "QuerySet.h"
#import "Queue.h"
#import "RenderBundleEncoder.h"
#import "RenderPipeline.h"
#import "Sampler.h"
#import "ShaderModule.h"
#import "Texture.h"
#import "XRSubImage.h"
#import <algorithm>
#import <notify.h>
#import <wtf/StdLibExtras.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/WeakPtr.h>

namespace WebGPU {

struct GPUFrameCapture {
    static void captureSingleFrameIfNeeded(id<MTLDevice> captureObject)
    {
        if (enabled) {
            captureFrame(captureObject);
            enabled = false;
        }
    }

    static void registerForFrameCapture(id<MTLDevice> captureObject)
    {
        // Allow GPU frame capture "notifyutil -p com.apple.WebKit.WebGPU.CaptureFrame" when process is
        // run with __XPC_METAL_CAPTURE_ENABLED=1
        // notifyutil -s com.apple.WebKit.WebGPU.CaptureFrame 10 --> captures 10 GPUQueue.submit calls
        static std::once_flag onceFlag;
        std::call_once(onceFlag, [] {
            int captureFrameToken;
            notify_register_dispatch("com.apple.WebKit.WebGPU.CaptureFrame", &captureFrameToken, dispatch_get_main_queue(), ^(int token) {
                uint64_t state;
                notify_get_state(token, &state);
                maxSubmitCallsToCapture = std::max<int>(1, state);
                enabled = true;
            });

            int captureFirstFrameToken;
            notify_register_dispatch("com.apple.WebKit.WebGPU.ToggleCaptureFirstFrame", &captureFirstFrameToken, dispatch_get_main_queue(), ^(int) {
                captureFirstFrame = !captureFirstFrame;
            });
        });

        if (captureFirstFrame)
            captureFrame(captureObject);
    }

    static bool shouldStopCaptureAfterSubmit()
    {
        ++submitCallsCaptured;
        auto result = submitCallsCaptured >= maxSubmitCallsToCapture;
        if (result)
            submitCallsCaptured = 0;

        return result;
    }

private:
    static void captureFrame(id<MTLDevice> captureObject)
    {
        MTLCaptureManager* captureManager = [MTLCaptureManager sharedCaptureManager];
        if ([captureManager isCapturing])
            return;

        MTLCaptureDescriptor* captureDescriptor = [[MTLCaptureDescriptor alloc] init];
        captureDescriptor.captureObject = captureObject;
        captureDescriptor.destination = MTLCaptureDestinationGPUTraceDocument;
        captureDescriptor.outputURL = [[NSFileManager.defaultManager temporaryDirectory] URLByAppendingPathComponent:[NSString stringWithFormat:@"%@.gputrace", NSUUID.UUID.UUIDString]];

        NSError *error;
        if (![captureManager startCaptureWithDescriptor:captureDescriptor error:&error])
            WTFLogAlways("Failed to start GPU frame capture at path %@, error %@", captureDescriptor.outputURL.absoluteString, error);
        else
            WTFLogAlways("Success starting GPU frame capture at path %@ - frame count = %d", captureDescriptor.outputURL.absoluteString, maxSubmitCallsToCapture);
    }

    static bool captureFirstFrame;
    static bool enabled;
    static int submitCallsCaptured;
    static int maxSubmitCallsToCapture;
};

bool GPUFrameCapture::captureFirstFrame = false;
bool GPUFrameCapture::enabled = false;
int GPUFrameCapture::submitCallsCaptured = 0;
int GPUFrameCapture::maxSubmitCallsToCapture = 1;

WTF_MAKE_TZONE_ALLOCATED_IMPL(Device);

bool Device::shouldStopCaptureAfterSubmit()
{
    return GPUFrameCapture::shouldStopCaptureAfterSubmit();
}

bool Device::isDestroyed() const
{
    return m_destroyed;
}

Ref<Device> Device::create(id<MTLDevice> device, String&& deviceLabel, HardwareCapabilities&& capabilities, Adapter& adapter)
{
    id<MTLCommandQueue> commandQueue = [device newCommandQueueWithMaxCommandBufferCount:4096];
    if (!commandQueue)
        return Device::createInvalid(adapter);

    // See the comment in Device::setLabel() about why we're not setting the label on the MTLDevice here.

    commandQueue.label = @"Default queue";
    if (!deviceLabel.isEmpty())
        commandQueue.label = [NSString stringWithFormat:@"Default queue for device %s", deviceLabel.utf8().data()];

    return adoptRef(*new Device(device, commandQueue, WTFMove(capabilities), adapter));
}

Device::Device(id<MTLDevice> device, id<MTLCommandQueue> defaultQueue, HardwareCapabilities&& capabilities, Adapter& adapter)
    : m_device(device)
    , m_defaultQueue(Queue::create(defaultQueue, *this))
    , m_xrSubImage(XRSubImage::create(*this))
    , m_capabilities(WTFMove(capabilities))
    , m_adapter(adapter)
{
#if PLATFORM(MAC)
    auto devices = MTLCopyAllDevicesWithObserver(&m_deviceObserver, [weakThis = ThreadSafeWeakPtr { *this }](id<MTLDevice> device, MTLDeviceNotificationName) {
        RefPtr<Device> protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        if (auto instance = protectedThis->instance(); instance.get()) {
            instance->scheduleWork([protectedThis = WTFMove(protectedThis), device = device]() {
                if (![protectedThis->m_device isEqual:device])
                    return;
                protectedThis->loseTheDevice(WGPUDeviceLostReason_Undefined);
            });
        }
    });

#if ASSERT_ENABLED
    bool found = false;
    for (id<MTLDevice> observedDevice in devices) {
        if ([observedDevice isEqual:device]) {
            found = true;
            break;
        }
    }
    ASSERT(found);
#else
    UNUSED_VARIABLE(devices);
#endif
#endif

#if HAVE(COREVIDEO_METAL_SUPPORT)
    CVMetalTextureCacheRef coreVideoTextureCache;
    CVReturn result = CVMetalTextureCacheCreate(nullptr, nullptr, device, nullptr, &coreVideoTextureCache);
    ASSERT_UNUSED(result, result == kCVReturnSuccess);
    m_coreVideoTextureCache = coreVideoTextureCache;
#endif
    GPUFrameCapture::registerForFrameCapture(m_device);

    m_placeholderBuffer = safeCreateBuffer(1, MTLStorageModeShared);
    auto desc = [MTLTextureDescriptor new];
    desc.width = 1;
    desc.height = 1;
    desc.mipmapLevelCount = 1;
    desc.pixelFormat = MTLPixelFormatBGRA8Unorm;
    desc.textureType = MTLTextureType2D;
#if PLATFORM(MAC)
    desc.storageMode = hasUnifiedMemory() ? MTLStorageModeShared : MTLStorageModeManaged;
#else
    desc.storageMode = MTLStorageModeShared;
#endif
    desc.usage = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;
    m_placeholderTexture = [m_device newTextureWithDescriptor:desc];
    desc.pixelFormat = MTLPixelFormatDepth32Float_Stencil8;
    desc.storageMode = MTLStorageModePrivate;
    m_placeholderDepthStencilTexture = [m_device newTextureWithDescriptor:desc];
}

Device::Device(Adapter& adapter)
    : m_defaultQueue(Queue::createInvalid(*this))
    , m_adapter(adapter)
{
    if (!m_adapter->isValid())
        makeInvalid();
}

Device::~Device()
{
#if PLATFORM(MAC)
    MTLRemoveDeviceObserver(m_deviceObserver);
#endif
    if (m_deviceLostCallback) {
        m_deviceLostCallback(WGPUDeviceLostReason_Destroyed, ""_s);
        m_deviceLostCallback = nullptr;
    }

    if (m_uncapturedErrorCallback) {
        m_uncapturedErrorCallback(WGPUErrorType_NoError, ""_s);
        m_uncapturedErrorCallback = nullptr;
    }
}

RefPtr<XRSubImage> Device::getXRViewSubImage(XRProjectionLayer& projectionLayer)
{
    m_xrSubImage->update(projectionLayer.colorTexture(), projectionLayer.depthTexture(), projectionLayer.reusableTextureIndex(), projectionLayer.completionEvent());
    return m_xrSubImage;
}

void Device::makeInvalid()
{
    m_device = nil;
    m_defaultQueue->makeInvalid();
}

void Device::loseTheDevice(WGPUDeviceLostReason reason)
{
    m_device = nil;

    m_adapter->makeInvalid();

    if (m_deviceLostCallback) {
        m_deviceLostCallback(reason, "Device lost."_s);
        m_deviceLostCallback = nullptr;
    }

    m_defaultQueue->makeInvalid();
    m_isLost = true;
}

static void setOwnerWithIdentity(id<MTLResourceSPI> resource, auto webProcessID)
{
    if (!resource)
        return;

    if (![resource respondsToSelector:@selector(setOwnerWithIdentity:)])
        return;

    [resource setOwnerWithIdentity:webProcessID];
}

void Device::setOwnerWithIdentity(id<MTLResource> resource) const
{
    if (auto optionalWebProcessID = webProcessID()) {
        auto webProcessID = optionalWebProcessID->sendRight();
        if (!webProcessID)
            return;

        WebGPU::setOwnerWithIdentity((id<MTLResourceSPI>)resource, webProcessID);
    }
}

void Device::destroy()
{
    m_destroyed = true;

    loseTheDevice(WGPUDeviceLostReason_Destroyed);
}

size_t Device::enumerateFeatures(WGPUFeatureName* features)
{
    // The API contract for this requires that sufficient space has already been allocated for the output.
    // This requires the caller calling us twice: once to get the amount of space to allocate, and once to fill the space.
    if (features)
        std::copy(m_capabilities.features.begin(), m_capabilities.features.end(), features);
    return m_capabilities.features.size();
}

bool Device::getLimits(WGPUSupportedLimits& limits)
{
    if (limits.nextInChain != nullptr)
        return false;

    limits.limits = m_capabilities.limits;
    return true;
}

id<MTLBuffer> Device::placeholderBuffer() const
{
    return m_placeholderBuffer;
}

id<MTLTexture> Device::placeholderTexture(WGPUTextureFormat format) const
{
    return Texture::isDepthOrStencilFormat(format) ? m_placeholderDepthStencilTexture : m_placeholderTexture;
}

bool Device::hasFeature(WGPUFeatureName feature) const
{
    return m_capabilities.features.contains(feature);
}

auto Device::currentErrorScope(WGPUErrorFilter type) -> ErrorScope*
{
    // https://gpuweb.github.io/gpuweb/#abstract-opdef-current-error-scope

    for (auto iterator = m_errorScopeStack.rbegin(); iterator != m_errorScopeStack.rend(); ++iterator) {
        if (iterator->filter == type)
            return &*iterator;
    }
    return nullptr;
}

void Device::generateAValidationError(NSString * message)
{
    generateAValidationError(String { message });
}

void Device::generateAValidationError(String&& message)
{
    // https://gpuweb.github.io/gpuweb/#abstract-opdef-generate-a-validation-error
    auto* scope = currentErrorScope(WGPUErrorFilter_Validation);
    if (scope) {
        if (!scope->error)
            scope->error = Error { WGPUErrorType_Validation, WTFMove(message) };
        return;
    }

    if (m_uncapturedErrorCallback) {
        m_uncapturedErrorCallback(WGPUErrorType_Validation, WTFMove(message));
        m_uncapturedErrorCallback = nullptr;
    }
}

void Device::generateAnOutOfMemoryError(String&& message)
{
    // https://gpuweb.github.io/gpuweb/#abstract-opdef-generate-an-out-of-memory-error

    auto* scope = currentErrorScope(WGPUErrorFilter_OutOfMemory);

    if (scope) {
        if (!scope->error)
            scope->error = Error { WGPUErrorType_OutOfMemory, WTFMove(message) };
        return;
    }

    if (m_uncapturedErrorCallback) {
        m_uncapturedErrorCallback(WGPUErrorType_OutOfMemory, WTFMove(message));
        m_uncapturedErrorCallback = nullptr;
    }
}

void Device::generateAnInternalError(String&& message)
{
    // https://gpuweb.github.io/gpuweb/#abstract-opdef-generate-an-internal-error

    auto* scope = currentErrorScope(WGPUErrorFilter_Internal);

    if (scope) {
        if (!scope->error)
            scope->error = Error { WGPUErrorType_Internal, WTFMove(message) };
        return;
    }

    if (m_uncapturedErrorCallback) {
        m_uncapturedErrorCallback(WGPUErrorType_Internal, WTFMove(message));
        m_uncapturedErrorCallback = nullptr;
    }
}

uint32_t Device::maxBuffersPlusVertexBuffersForVertexStage() const
{
    ASSERT(m_capabilities.limits.maxBindGroupsPlusVertexBuffers > 0);
    return m_capabilities.limits.maxBindGroupsPlusVertexBuffers;
}

uint32_t Device::maxBuffersForFragmentStage() const
{
    return m_capabilities.limits.maxBindGroups;
}

uint32_t Device::vertexBufferIndexForBindGroup(uint32_t groupIndex) const
{
    ASSERT(maxBuffersPlusVertexBuffersForVertexStage() > 0);
    return WGSL::vertexBufferIndexForBindGroup(groupIndex, maxBuffersPlusVertexBuffersForVertexStage() - 1);
}

id<MTLBuffer> Device::newBufferWithBytes(const void* pointer, size_t length, MTLResourceOptions options) const
{
    id<MTLBuffer> buffer = [m_device newBufferWithBytes:pointer length:length options:options];
    setOwnerWithIdentity(buffer);
    return buffer;
}

id<MTLBuffer> Device::newBufferWithBytesNoCopy(void* pointer, size_t length, MTLResourceOptions options) const
{
    id<MTLBuffer> buffer = [m_device newBufferWithBytesNoCopy:pointer length:length options:options deallocator:nil];
    setOwnerWithIdentity(buffer);
    return buffer;
}

id<MTLTexture> Device::newTextureWithDescriptor(MTLTextureDescriptor *textureDescriptor, IOSurfaceRef ioSurface, NSUInteger plane) const
{
    id<MTLTexture> texture = ioSurface ? [m_device newTextureWithDescriptor:textureDescriptor iosurface:ioSurface plane:plane] : [m_device newTextureWithDescriptor:textureDescriptor];
    setOwnerWithIdentity(texture);
    return texture;
}

void Device::captureFrameIfNeeded() const
{
    GPUFrameCapture::captureSingleFrameIfNeeded(m_device);
}

std::optional<WGPUErrorType> Device::validatePopErrorScope() const
{
    if (m_isLost)
        return WGPUErrorType_NoError;

    if (m_errorScopeStack.isEmpty())
        return WGPUErrorType_Unknown;

    return std::nullopt;
}

bool Device::popErrorScope(CompletionHandler<void(WGPUErrorType, String&&)>&& callback)
{
    // https://gpuweb.github.io/gpuweb/#dom-gpudevice-poperrorscope

    if (auto errorType = validatePopErrorScope()) {
        callback(*errorType, "popErrorScope() failed validation."_s);
        return false;
    }

    auto scope = m_errorScopeStack.takeLast();

    if (auto inst = instance(); inst.get()) {
        inst->scheduleWork([scope = WTFMove(scope), callback = WTFMove(callback)]() mutable {
            if (scope.error)
                callback(scope.error->type, WTFMove(scope.error->message));
            else
                callback(WGPUErrorType_NoError, { });
        });
    } else
        callback(WGPUErrorType_NoError, { });

    // FIXME: Make sure this is the right thing to return.
    return true;
}

void Device::pushErrorScope(WGPUErrorFilter filter)
{
    // https://gpuweb.github.io/gpuweb/#dom-gpudevice-pusherrorscope

    ErrorScope scope { std::nullopt, filter };

    m_errorScopeStack.append(WTFMove(scope));
}

void Device::setDeviceLostCallback(Function<void(WGPUDeviceLostReason, String&&)>&& callback)
{
    if (m_deviceLostCallback)
        m_deviceLostCallback(WGPUDeviceLostReason_Destroyed, ""_s);

    m_deviceLostCallback = WTFMove(callback);
    if (m_isLost)
        loseTheDevice(WGPUDeviceLostReason_Destroyed);
    else if (!m_adapter->isValid())
        loseTheDevice(WGPUDeviceLostReason_Undefined);
}

void Device::setUncapturedErrorCallback(Function<void(WGPUErrorType, String&&)>&& callback)
{
    if (m_uncapturedErrorCallback)
        m_uncapturedErrorCallback(WGPUErrorType_NoError, ""_s);
    m_uncapturedErrorCallback = WTFMove(callback);
}

void Device::setLabel(String&&)
{
    // Because MTLDevices are process-global, we can't set the label on it, because 2 contexts' labels would fight each other.
}

const std::optional<const MachSendRight> Device::webProcessID() const
{
    auto scheduler = instance();
    return scheduler ? scheduler->webProcessID() : std::nullopt;
}

id<MTLBuffer> Device::dispatchCallBuffer()
{
    if (!m_device)
        return nil;

    if (!m_dispatchCallBuffer) {
        m_dispatchCallBuffer = [m_device newBufferWithLength:sizeof(MTLDispatchThreadgroupsIndirectArguments) options:MTLResourceStorageModePrivate];
        setOwnerWithIdentity(m_dispatchCallBuffer);
    }
    return m_dispatchCallBuffer;
}

id<MTLComputePipelineState> Device::dispatchCallPipelineState(id<MTLFunction> function)
{
    if (!m_device)
        return nil;

    if (!m_dispatchCallPipelineState) {
        NSError* error = nil;
        m_dispatchCallPipelineState = [m_device newComputePipelineStateWithFunction:function error:&error];
        if (error)
            WTFLogAlways("Metal code failure: %@", error);
    }
    return m_dispatchCallPipelineState;
}

id<MTLRenderPipelineState> Device::copyIndexIndirectArgsPipeline(NSUInteger rasterSampleCount)
{
    if (!m_device)
        return nil;

    id<MTLRenderPipelineState> result = rasterSampleCount > 1 ? m_copyIndexedIndirectArgsPSOMS : m_copyIndexedIndirectArgsPSO;
    if (result)
        return result;

    static id<MTLFunction> function = nil;
    NSError *error = nil;
    if (!function) {
        MTLCompileOptions* options = [MTLCompileOptions new];
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        options.fastMathEnabled = YES;
        ALLOW_DEPRECATED_DECLARATIONS_END
        /* NOLINT */ id<MTLLibrary> library = [m_device newLibraryWithSource:@R"(
    using namespace metal;
    [[vertex]] void vs(device MTLDrawIndexedPrimitivesIndirectArguments& indexedOutput [[buffer(0)]], constant const MTLDrawIndexedPrimitivesIndirectArguments& indirectArguments [[buffer(1)]], const constant uint* instanceCount [[buffer(2)]]) {
        bool condition = indirectArguments.baseInstance + indirectArguments.instanceCount > instanceCount[0] || indirectArguments.baseInstance >= instanceCount[0];
        indexedOutput.indexCount = metal::select(indirectArguments.indexCount, 0u, condition);
        indexedOutput.instanceCount = metal::select(indirectArguments.instanceCount, 0u, condition);
        indexedOutput.indexStart = indirectArguments.indexStart;
        indexedOutput.baseVertex = indirectArguments.baseVertex;
        indexedOutput.baseInstance = indirectArguments.baseInstance;
    })" /* NOLINT */ options:options error:&error];
        if (error) {
            WTFLogAlways("%@", error);
            return nil;
        }

        function = [library newFunctionWithName:@"vs"];
    }

    MTLRenderPipelineDescriptor* mtlRenderPipelineDescriptor = [MTLRenderPipelineDescriptor new];
    mtlRenderPipelineDescriptor.vertexFunction = function;
    mtlRenderPipelineDescriptor.rasterizationEnabled = false;
    mtlRenderPipelineDescriptor.rasterSampleCount = rasterSampleCount;
    mtlRenderPipelineDescriptor.fragmentFunction = nil;
    mtlRenderPipelineDescriptor.inputPrimitiveTopology = MTLPrimitiveTopologyClassPoint;

    if (rasterSampleCount > 1)
        result = m_copyIndexedIndirectArgsPSOMS = [m_device newRenderPipelineStateWithDescriptor:mtlRenderPipelineDescriptor error:&error];
    else
        result = m_copyIndexedIndirectArgsPSO = [m_device newRenderPipelineStateWithDescriptor:mtlRenderPipelineDescriptor error:&error];

    if (error) {
        WTFLogAlways("%@", error);
        return nil;
    }
    return result;
}

id<MTLRenderPipelineState> Device::indexBufferClampPipeline(MTLIndexType indexType, NSUInteger rasterSampleCount)
{
    if (!m_device)
        return nil;

    bool isUint16 = indexType == MTLIndexTypeUInt16;
    id<MTLRenderPipelineState> result = isUint16 ? (rasterSampleCount > 1 ? m_indexBufferClampUshortPSOMS : m_indexBufferClampUshortPSO) : (rasterSampleCount > 1 ? m_indexBufferClampUintPSOMS : m_indexBufferClampUintPSO);
    if (result)
        return result;

    static id<MTLFunction> function = nil;
    static id<MTLFunction> functionUshort = nil;
    NSError *error = nil;
    if (!function) {
        MTLCompileOptions* options = [MTLCompileOptions new];
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        options.fastMathEnabled = YES;
        ALLOW_DEPRECATED_DECLARATIONS_END
        /* NOLINT */ id<MTLLibrary> library = [m_device newLibraryWithSource:@R"(
#define vertexCount 0
#define primitiveRestart 1
    using namespace metal;
    [[vertex]] void vsUshort(device const ushort* indexBuffer [[buffer(0)]], device MTLDrawIndexedPrimitivesIndirectArguments& indexedOutput [[buffer(1)]], const constant uint* data [[buffer(2)]], uint indexId [[vertex_id]]) {
        ushort vertexIndex = data[primitiveRestart] + indexBuffer[indexId];
        if (vertexIndex + indexedOutput.baseVertex >= data[vertexCount] + data[primitiveRestart])
            indexedOutput.indexCount = 0u;
    }
    [[vertex]] void vsUint(device const uint* indexBuffer [[buffer(0)]], device MTLDrawIndexedPrimitivesIndirectArguments& indexedOutput [[buffer(1)]], const constant uint* data [[buffer(2)]], uint indexId [[vertex_id]]) {
        uint vertexIndex = data[primitiveRestart] + indexBuffer[indexId];
        if (vertexIndex + indexedOutput.baseVertex >= data[vertexCount] + data[primitiveRestart])
            indexedOutput.indexCount = 0u;
    })" /* NOLINT */ options:options error:&error];
        if (error) {
            WTFLogAlways("%@", error);
            return nil;
        }

        function = [library newFunctionWithName:@"vsUint"];
        functionUshort = [library newFunctionWithName:@"vsUshort"];
    }

    MTLRenderPipelineDescriptor* mtlRenderPipelineDescriptor = [MTLRenderPipelineDescriptor new];
    mtlRenderPipelineDescriptor.vertexFunction = isUint16 ? functionUshort : function;
    mtlRenderPipelineDescriptor.rasterizationEnabled = false;
    mtlRenderPipelineDescriptor.rasterSampleCount = rasterSampleCount;
    mtlRenderPipelineDescriptor.fragmentFunction = nil;
    mtlRenderPipelineDescriptor.inputPrimitiveTopology = MTLPrimitiveTopologyClassPoint;

    if (isUint16) {
        if (rasterSampleCount > 1)
            result = m_indexBufferClampUshortPSOMS = [m_device newRenderPipelineStateWithDescriptor:mtlRenderPipelineDescriptor error:&error];
        else
            result = m_indexBufferClampUshortPSO = [m_device newRenderPipelineStateWithDescriptor:mtlRenderPipelineDescriptor error:&error];
    } else {
        if (rasterSampleCount > 1)
            result = m_indexBufferClampUintPSOMS = [m_device newRenderPipelineStateWithDescriptor:mtlRenderPipelineDescriptor error:&error];
        else
            result = m_indexBufferClampUintPSO = [m_device newRenderPipelineStateWithDescriptor:mtlRenderPipelineDescriptor error:&error];
    }

    if (error) {
        WTFLogAlways("%@", error);
        return nil;
    }
    return result;
}

id<MTLRenderPipelineState> Device::indexedIndirectBufferClampPipeline(NSUInteger rasterSampleCount)
{
    if (!m_device)
        return nil;

    id<MTLRenderPipelineState> result = rasterSampleCount > 1 ? m_indexedIndirectBufferClampPSOMS : m_indexedIndirectBufferClampPSO;
    if (result)
        return result;

    static id<MTLFunction> function = nil;
    NSError *error = nil;
    if (!function) {
        MTLCompileOptions* options = [MTLCompileOptions new];
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        options.fastMathEnabled = YES;
        ALLOW_DEPRECATED_DECLARATIONS_END
        /* NOLINT */ id<MTLLibrary> library = [m_device newLibraryWithSource:@R"(
    using namespace metal;
    [[vertex]] void vs(device const MTLDrawIndexedPrimitivesIndirectArguments& input [[buffer(0)]], device MTLDrawIndexedPrimitivesIndirectArguments& indexedOutput [[buffer(1)]], device MTLDrawPrimitivesIndirectArguments& output [[buffer(2)]], const constant uint* indexBufferCount [[buffer(3)]]) {

        bool condition = input.indexCount + input.indexStart > indexBufferCount[0] || input.instanceCount + input.baseInstance > indexBufferCount[1] || input.baseInstance >= indexBufferCount[1];

        indexedOutput.indexCount = metal::select(input.indexCount, 0u, condition);
        indexedOutput.instanceCount = input.instanceCount;
        indexedOutput.indexStart = metal::select(input.indexStart, 0u, condition);
        indexedOutput.baseVertex = input.baseVertex;
        indexedOutput.baseInstance = input.baseInstance;

        output.vertexCount = metal::select(input.indexCount, 0u, condition);
        output.instanceCount = 1;
        output.vertexStart = input.indexStart;
        output.baseInstance = 0;
    })" /* NOLINT */ options:options error:&error];
        if (error) {
            WTFLogAlways("%@", error);
            return nil;
        }

        function = [library newFunctionWithName:@"vs"];
    }

    MTLRenderPipelineDescriptor* mtlRenderPipelineDescriptor = [MTLRenderPipelineDescriptor new];
    mtlRenderPipelineDescriptor.vertexFunction = function;
    mtlRenderPipelineDescriptor.rasterizationEnabled = false;
    mtlRenderPipelineDescriptor.rasterSampleCount = rasterSampleCount;
    mtlRenderPipelineDescriptor.fragmentFunction = nil;
    mtlRenderPipelineDescriptor.inputPrimitiveTopology = MTLPrimitiveTopologyClassPoint;

    if (rasterSampleCount > 1)
        result = m_indexedIndirectBufferClampPSOMS = [m_device newRenderPipelineStateWithDescriptor:mtlRenderPipelineDescriptor error:&error];
    else
        result = m_indexedIndirectBufferClampPSO = [m_device newRenderPipelineStateWithDescriptor:mtlRenderPipelineDescriptor error:&error];

    if (error) {
        WTFLogAlways("%@", error);
        return nil;
    }
    return result;
}

id<MTLRenderPipelineState> Device::indirectBufferClampPipeline(NSUInteger rasterSampleCount)
{
    if (!m_device)
        return nil;

    id<MTLRenderPipelineState> result = rasterSampleCount > 1 ? m_indirectBufferClampPSOMS : m_indirectBufferClampPSO;
    if (result)
        return result;

    static id<MTLFunction> function = nil;
    NSError *error = nil;
    if (!function) {
        MTLCompileOptions* options = [MTLCompileOptions new];
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        options.fastMathEnabled = YES;
        ALLOW_DEPRECATED_DECLARATIONS_END
        /* NOLINT */ id<MTLLibrary> library = [m_device newLibraryWithSource:@R"(
    using namespace metal;
    [[vertex]] void vs(device const MTLDrawPrimitivesIndirectArguments& input [[buffer(0)]], device MTLDrawPrimitivesIndirectArguments& output [[buffer(1)]], const constant uint* minCounts [[buffer(2)]]) {
        bool vertexCondition = input.vertexCount + input.vertexStart > minCounts[0] || input.vertexStart >= minCounts[0];
        bool instanceCondition = input.baseInstance + input.instanceCount > minCounts[1] || input.baseInstance >= minCounts[1];
        bool condition = vertexCondition || instanceCondition;
        output.vertexCount = metal::select(input.vertexCount, 0u, condition);
        output.instanceCount = input.instanceCount;
        output.vertexStart = input.vertexStart;
        output.baseInstance = input.baseInstance;
    })" /* NOLINT */ options:options error:&error];
        if (error) {
            WTFLogAlways("%@", error);
            return nil;
        }

        function = [library newFunctionWithName:@"vs"];
    }

    MTLRenderPipelineDescriptor* mtlRenderPipelineDescriptor = [MTLRenderPipelineDescriptor new];
    mtlRenderPipelineDescriptor.vertexFunction = function;
    mtlRenderPipelineDescriptor.rasterizationEnabled = false;
    mtlRenderPipelineDescriptor.rasterSampleCount = rasterSampleCount;
    mtlRenderPipelineDescriptor.fragmentFunction = nil;
    mtlRenderPipelineDescriptor.inputPrimitiveTopology = MTLPrimitiveTopologyClassPoint;

    if (rasterSampleCount > 1)
        result = m_indirectBufferClampPSOMS = [m_device newRenderPipelineStateWithDescriptor:mtlRenderPipelineDescriptor error:&error];
    else
        result = m_indirectBufferClampPSO = [m_device newRenderPipelineStateWithDescriptor:mtlRenderPipelineDescriptor error:&error];

    if (error) {
        WTFLogAlways("%@", error);
        return nil;
    }
    return result;
}

id<MTLRenderPipelineState> Device::icbCommandClampPipeline(MTLIndexType indexType, NSUInteger rasterSampleCount)
{
    if (!m_device)
        return nil;

    bool isUint16 = indexType == MTLIndexTypeUInt16;
    id<MTLRenderPipelineState> result = isUint16 ? (rasterSampleCount > 1 ? m_icbCommandClampUshortPSOMS : m_icbCommandClampUshortPSO) : (rasterSampleCount > 1 ? m_icbCommandClampUintPSOMS : m_icbCommandClampUintPSO);
    if (result)
        return result;

    NSError *error = nil;
    MTLRenderPipelineDescriptor* mtlRenderPipelineDescriptor = [MTLRenderPipelineDescriptor new];
    mtlRenderPipelineDescriptor.vertexFunction = icbCommandClampFunction(indexType);
    mtlRenderPipelineDescriptor.rasterizationEnabled = false;
    mtlRenderPipelineDescriptor.rasterSampleCount = rasterSampleCount;
    mtlRenderPipelineDescriptor.fragmentFunction = nil;
    mtlRenderPipelineDescriptor.inputPrimitiveTopology = MTLPrimitiveTopologyClassPoint;

    if (isUint16) {
        if (rasterSampleCount > 1)
            result = m_icbCommandClampUshortPSOMS = [m_device newRenderPipelineStateWithDescriptor:mtlRenderPipelineDescriptor error:&error];
        else
            result = m_icbCommandClampUshortPSO = [m_device newRenderPipelineStateWithDescriptor:mtlRenderPipelineDescriptor error:&error];
    } else {
        if (rasterSampleCount > 1)
            result = m_icbCommandClampUintPSOMS = [m_device newRenderPipelineStateWithDescriptor:mtlRenderPipelineDescriptor error:&error];
        else
            result = m_icbCommandClampUintPSO = [m_device newRenderPipelineStateWithDescriptor:mtlRenderPipelineDescriptor error:&error];
    }

    if (error) {
        WTFLogAlways("%@", error);
        return nil;
    }
    return result;
}

int Device::bufferIndexForICBContainer() const
{
    return 1;
}

id<MTLFunction> Device::icbCommandClampFunction(MTLIndexType indexType)
{
    static id<MTLFunction> function = nil;
    static id<MTLFunction> functionUshort = nil;
    NSError *error = nil;
    if (!function) {
        MTLCompileOptions* options = [MTLCompileOptions new];
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        options.fastMathEnabled = YES;
        ALLOW_DEPRECATED_DECLARATIONS_END
        /* NOLINT */ id<MTLLibrary> library = [m_device newLibraryWithSource:@R"(
    using namespace metal;
    struct ICBContainer {
        command_buffer commandBuffer [[ id(0) ]];
    };
    struct IndexDataUshort {
        uint64_t renderCommand { 0 };
        uint32_t minVertexCount { UINT_MAX };
        uint32_t minInstanceCount { UINT_MAX };
        device ushort* indexBuffer;
        uint32_t indexCount { 0 };
        uint32_t instanceCount { 0 };
        uint32_t firstIndex { 0 };
        int32_t baseVertex { 0 };
        uint32_t baseInstance { 0 };
        primitive_type primitiveType { primitive_type::triangle };
    };
    struct IndexDataUint {
        uint64_t renderCommand { 0 };
        uint32_t minVertexCount { UINT_MAX };
        uint32_t minInstanceCount { UINT_MAX };
        device uint* indexBuffer;
        uint32_t indexCount { 0 };
        uint32_t instanceCount { 0 };
        uint32_t firstIndex { 0 };
        int32_t baseVertex { 0 };
        uint32_t baseInstance { 0 };
        primitive_type primitiveType { primitive_type::triangle };
    };

    [[vertex]] void vs(device const IndexDataUint* indexData [[buffer(0)]],
        device ICBContainer *icb_container [[buffer(1)]], // <-- must match Device::bufferIndexForICBContainer()
        uint indexId [[vertex_id]]) {

        device const IndexDataUint& data = *indexData;
        uint32_t k = (data.primitiveType == primitive_type::triangle_strip || data.primitiveType == primitive_type::line_strip) ? 1 : 0;
        uint32_t vertexIndex = data.indexBuffer[indexId] + k;
        if (data.baseVertex + vertexIndex >= data.minVertexCount + k) {
            render_command cmd(icb_container->commandBuffer, data.renderCommand);
            cmd.draw_indexed_primitives(data.primitiveType,
                0u,
                data.indexBuffer,
                data.instanceCount,
                data.baseVertex,
                data.baseInstance);
        }
    }

    [[vertex]] void vsUshort(device const IndexDataUshort* indexData [[buffer(0)]],
        device ICBContainer *icb_container [[buffer(1)]], // <-- must match Device::bufferIndexForICBContainer()
        uint indexId [[vertex_id]]) {

        device const IndexDataUshort& data = *indexData;
        uint32_t k = (data.primitiveType == primitive_type::triangle_strip || data.primitiveType == primitive_type::line_strip) ? 1 : 0;
        ushort vertexIndex = data.indexBuffer[indexId] + k;
        if (data.baseVertex + vertexIndex >= data.minVertexCount + k) {
            render_command cmd(icb_container->commandBuffer, data.renderCommand);
            cmd.draw_indexed_primitives(data.primitiveType,
                0u,
                data.indexBuffer,
                data.instanceCount,
                data.baseVertex,
                data.baseInstance);
        }

    })" /* NOLINT */ options:options error:&error];
        if (error) {
            WTFLogAlways("%@", error);
            return nil;
        }

        function = [library newFunctionWithName:@"vs"];
        functionUshort = [library newFunctionWithName:@"vsUshort"];
    }

    return indexType == MTLIndexTypeUInt16 ? functionUshort : function;
}

} // namespace WebGPU

#pragma mark WGPU Stubs

void wgpuDeviceReference(WGPUDevice device)
{
    WebGPU::fromAPI(device).ref();
}

void wgpuDeviceRelease(WGPUDevice device)
{
    WebGPU::fromAPI(device).deref();
}

WGPUBindGroup wgpuDeviceCreateBindGroup(WGPUDevice device, const WGPUBindGroupDescriptor* descriptor)
{
    return WebGPU::releaseToAPI(WebGPU::protectedFromAPI(device)->createBindGroup(*descriptor));
}

WGPUBindGroupLayout wgpuDeviceCreateBindGroupLayout(WGPUDevice device, const WGPUBindGroupLayoutDescriptor* descriptor)
{
    return WebGPU::releaseToAPI(WebGPU::protectedFromAPI(device)->createBindGroupLayout(*descriptor));
}

WGPUXRBinding wgpuDeviceCreateXRBinding(WGPUDevice device)
{
    return WebGPU::releaseToAPI(WebGPU::protectedFromAPI(device)->createXRBinding());
}

WGPUBuffer wgpuDeviceCreateBuffer(WGPUDevice device, const WGPUBufferDescriptor* descriptor)
{
    return WebGPU::releaseToAPI(WebGPU::protectedFromAPI(device)->createBuffer(*descriptor));
}

WGPUCommandEncoder wgpuDeviceCreateCommandEncoder(WGPUDevice device, const WGPUCommandEncoderDescriptor* descriptor)
{
    return WebGPU::releaseToAPI(WebGPU::protectedFromAPI(device)->createCommandEncoder(*descriptor));
}

WGPUComputePipeline wgpuDeviceCreateComputePipeline(WGPUDevice device, const WGPUComputePipelineDescriptor* descriptor)
{
    return WebGPU::releaseToAPI(WebGPU::protectedFromAPI(device)->createComputePipeline(*descriptor).first);
}

void wgpuDeviceCreateComputePipelineAsync(WGPUDevice device, const WGPUComputePipelineDescriptor* descriptor, WGPUCreateComputePipelineAsyncCallback callback, void* userdata)
{
    WebGPU::protectedFromAPI(device)->createComputePipelineAsync(*descriptor, [callback, userdata](WGPUCreatePipelineAsyncStatus status, Ref<WebGPU::ComputePipeline>&& pipeline, String&& message) {
        callback(status, WebGPU::releaseToAPI(WTFMove(pipeline)), WTFMove(message), userdata);
    });
}

void wgpuDeviceCreateComputePipelineAsyncWithBlock(WGPUDevice device, WGPUComputePipelineDescriptor const * descriptor, WGPUCreateComputePipelineAsyncBlockCallback callback)
{
    WebGPU::protectedFromAPI(device)->createComputePipelineAsync(*descriptor, [callback = WebGPU::fromAPI(WTFMove(callback))](WGPUCreatePipelineAsyncStatus status, Ref<WebGPU::ComputePipeline>&& pipeline, String&& message) {
        callback(status, WebGPU::releaseToAPI(WTFMove(pipeline)), WTFMove(message));
    });
}

WGPUPipelineLayout wgpuDeviceCreatePipelineLayout(WGPUDevice device, const WGPUPipelineLayoutDescriptor* descriptor)
{
    return WebGPU::releaseToAPI(WebGPU::protectedFromAPI(device)->createPipelineLayout(*descriptor, !descriptor->bindGroupLayouts));
}

WGPUQuerySet wgpuDeviceCreateQuerySet(WGPUDevice device, const WGPUQuerySetDescriptor* descriptor)
{
    return WebGPU::releaseToAPI(WebGPU::protectedFromAPI(device)->createQuerySet(*descriptor));
}

WGPURenderBundleEncoder wgpuDeviceCreateRenderBundleEncoder(WGPUDevice device, const WGPURenderBundleEncoderDescriptor* descriptor)
{
    return WebGPU::releaseToAPI(WebGPU::protectedFromAPI(device)->createRenderBundleEncoder(*descriptor));
}

WGPURenderPipeline wgpuDeviceCreateRenderPipeline(WGPUDevice device, const WGPURenderPipelineDescriptor* descriptor)
{
    return WebGPU::releaseToAPI(WebGPU::protectedFromAPI(device)->createRenderPipeline(*descriptor).first);
}

void wgpuDeviceCreateRenderPipelineAsync(WGPUDevice device, const WGPURenderPipelineDescriptor* descriptor, WGPUCreateRenderPipelineAsyncCallback callback, void* userdata)
{
    WebGPU::protectedFromAPI(device)->createRenderPipelineAsync(*descriptor, [callback, userdata](WGPUCreatePipelineAsyncStatus status, Ref<WebGPU::RenderPipeline>&& pipeline, String&& message) {
        callback(status, WebGPU::releaseToAPI(WTFMove(pipeline)), WTFMove(message), userdata);
    });
}

void wgpuDeviceCreateRenderPipelineAsyncWithBlock(WGPUDevice device, WGPURenderPipelineDescriptor const * descriptor, WGPUCreateRenderPipelineAsyncBlockCallback callback)
{
    WebGPU::protectedFromAPI(device)->createRenderPipelineAsync(*descriptor, [callback = WebGPU::fromAPI(WTFMove(callback))](WGPUCreatePipelineAsyncStatus status, Ref<WebGPU::RenderPipeline>&& pipeline, String&& message) {
        callback(status, WebGPU::releaseToAPI(WTFMove(pipeline)), WTFMove(message));
    });
}

WGPUSampler wgpuDeviceCreateSampler(WGPUDevice device, const WGPUSamplerDescriptor* descriptor)
{
    return WebGPU::releaseToAPI(WebGPU::protectedFromAPI(device)->createSampler(*descriptor));
}

WGPUExternalTexture wgpuDeviceImportExternalTexture(WGPUDevice device, const WGPUExternalTextureDescriptor* descriptor)
{
    return WebGPU::releaseToAPI(WebGPU::protectedFromAPI(device)->createExternalTexture(*descriptor));
}

WGPUShaderModule wgpuDeviceCreateShaderModule(WGPUDevice device, const WGPUShaderModuleDescriptor* descriptor)
{
    return WebGPU::releaseToAPI(WebGPU::protectedFromAPI(device)->createShaderModule(*descriptor));
}

WGPUSwapChain wgpuDeviceCreateSwapChain(WGPUDevice device, WGPUSurface surface, const WGPUSwapChainDescriptor* descriptor)
{
    return WebGPU::releaseToAPI(WebGPU::protectedFromAPI(device)->createSwapChain(WebGPU::protectedFromAPI(surface), *descriptor));
}

WGPUTexture wgpuDeviceCreateTexture(WGPUDevice device, const WGPUTextureDescriptor* descriptor)
{
    return WebGPU::releaseToAPI(WebGPU::protectedFromAPI(device)->createTexture(*descriptor));
}

void wgpuDeviceDestroy(WGPUDevice device)
{
    WebGPU::protectedFromAPI(device)->destroy();
}

size_t wgpuDeviceEnumerateFeatures(WGPUDevice device, WGPUFeatureName* features)
{
    return WebGPU::protectedFromAPI(device)->enumerateFeatures(features);
}

WGPUBool wgpuDeviceGetLimits(WGPUDevice device, WGPUSupportedLimits* limits)
{
    return WebGPU::protectedFromAPI(device)->getLimits(*limits);
}

WGPUQueue wgpuDeviceGetQueue(WGPUDevice device)
{
    return &WebGPU::protectedFromAPI(device)->getQueue();
}

WGPUBool wgpuDeviceHasFeature(WGPUDevice device, WGPUFeatureName feature)
{
    return WebGPU::protectedFromAPI(device)->hasFeature(feature);
}

void wgpuDevicePopErrorScope(WGPUDevice device, WGPUErrorCallback callback, void* userdata)
{
    WebGPU::protectedFromAPI(device)->popErrorScope([callback, userdata](WGPUErrorType type, String&& message) {
        callback(type, message.utf8().data(), userdata);
    });
}

void wgpuDevicePopErrorScopeWithBlock(WGPUDevice device, WGPUErrorBlockCallback callback)
{
    WebGPU::protectedFromAPI(device)->popErrorScope([callback = WebGPU::fromAPI(WTFMove(callback))](WGPUErrorType type, String&& message) {
        callback(type, message.utf8().data());
    });
}

void wgpuDevicePushErrorScope(WGPUDevice device, WGPUErrorFilter filter)
{
    WebGPU::protectedFromAPI(device)->pushErrorScope(filter);
}

void wgpuDeviceClearDeviceLostCallback(WGPUDevice device)
{
    return WebGPU::protectedFromAPI(device)->setDeviceLostCallback(nullptr);
}
void wgpuDeviceClearUncapturedErrorCallback(WGPUDevice device)
{
    return WebGPU::protectedFromAPI(device)->setUncapturedErrorCallback(nullptr);
}

void wgpuDeviceSetDeviceLostCallback(WGPUDevice device, WGPUDeviceLostCallback callback, void* userdata)
{
    return WebGPU::protectedFromAPI(device)->setDeviceLostCallback([callback, userdata](WGPUDeviceLostReason reason, String&& message) {
        if (callback)
            callback(reason, message.utf8().data(), userdata);
    });
}

void wgpuDeviceSetDeviceLostCallbackWithBlock(WGPUDevice device, WGPUDeviceLostBlockCallback callback)
{
    return WebGPU::protectedFromAPI(device)->setDeviceLostCallback([callback = WebGPU::fromAPI(WTFMove(callback))](WGPUDeviceLostReason reason, String&& message) {
        if (callback)
            callback(reason, message.utf8().data());
    });
}

void wgpuDeviceSetUncapturedErrorCallback(WGPUDevice device, WGPUErrorCallback callback, void* userdata)
{
    return WebGPU::protectedFromAPI(device)->setUncapturedErrorCallback([callback, userdata](WGPUErrorType type, String&& message) {
        if (callback)
            callback(type, message.utf8().data(), userdata);
    });
}

void wgpuDeviceSetUncapturedErrorCallbackWithBlock(WGPUDevice device, WGPUErrorBlockCallback callback)
{
    return WebGPU::protectedFromAPI(device)->setUncapturedErrorCallback([callback = WebGPU::fromAPI(WTFMove(callback))](WGPUErrorType type, String&& message) {
        if (callback)
            callback(type, message.utf8().data());
    });
}

void wgpuDeviceSetLabel(WGPUDevice device, const char* label)
{
    WebGPU::protectedFromAPI(device)->setLabel(WebGPU::fromAPI(label));
}
