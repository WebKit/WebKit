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
#import "ShaderModule.h"

#import "APIConversions.h"
#import "ASTBuiltinAttribute.h"
#import "ASTFunction.h"
#import "ASTStructure.h"
#import "ASTStructureMember.h"
#import "Device.h"
#import "Pipeline.h"
#import "PipelineLayout.h"
#import "Types.h"
#import "WGSLShaderModule.h"

#import <WebGPU/WebGPU.h>
#import <atomic>
#import <dispatch/dispatch.h>
#import <wtf/BlockPtr.h>
#import <wtf/Borrow.h>
#import <wtf/CrossThreadCopier.h>
#import <wtf/DataLog.h>
#import <wtf/Lock.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/NumberOfCores.h>
#import <wtf/OSObjectPtr.h>
#import <wtf/StringPrintStream.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/darwin/DispatchExtras.h>
#import <wtf/spi/darwin/DispatchSPI.h>
#import <wtf/threads/BinarySemaphore.h>

namespace WebGPU::Metal {

static MTLCompileOptions *compileOptions(const WGSL::DeviceState& deviceState)
{
    static bool requireSafeMath = false;
#if ENABLE(WEBGPU_BY_DEFAULT)
    static auto mathMode = MTLMathModeRelaxed;
    static auto mathFunctions = MTLMathFloatingPointFunctionsFast;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [&] {
        requireSafeMath = [[NSUserDefaults standardUserDefaults] boolForKey:@"WebKitWebGPUEnableSafeMath"];
        mathMode = requireSafeMath || [[NSUserDefaults standardUserDefaults] boolForKey:@"WebKitWebGPUEnableSafeMathMode"] ? MTLMathModeSafe : MTLMathModeRelaxed;
        mathFunctions = requireSafeMath || [[NSUserDefaults standardUserDefaults] boolForKey:@"WebKitWebGPUEnablePrecisionMathFunctions"] ? MTLMathFloatingPointFunctionsPrecise : MTLMathFloatingPointFunctionsFast;
    });
#endif

    static WTF::Lock cacheLock;
    static LazyNeverDestroyed<HashMap<uint64_t, RetainPtr<MTLCompileOptions>>> optionsCache;
    static std::once_flag cacheOnce;
    std::call_once(cacheOnce, [&] {
        optionsCache.construct();
    });

    uint64_t cacheKey = (static_cast<uint64_t>(deviceState.appleGPUFamily) << 2) | (deviceState.usesInvariant ? 2ull : 0ull) | 1ull;
    {
        Locker locker { cacheLock };
        if (auto it = optionsCache->find(cacheKey); it != optionsCache->end())
            return it->value.get();
    }

    auto options = [MTLCompileOptions new];
    options.preprocessorMacros = @{ @"__wgslMetalAppleGPUFamily" : [NSString stringWithFormat:@"%u", deviceState.appleGPUFamily] };
#if ENABLE(WEBGPU_BY_DEFAULT)
    options.mathMode = mathMode;
    options.mathFloatingPointFunctions = mathFunctions;
#else
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    options.fastMathEnabled = !requireSafeMath;
ALLOW_DEPRECATED_DECLARATIONS_END
#endif

    if (deviceState.usesInvariant)
        options.preserveInvariance = true;

    {
        Locker locker { cacheLock };
        optionsCache->set(cacheKey, options);
    }
    return options;
}

static NSError *libraryCompilationError(NSString *msl, NSError *compilationError)
{
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=250442
    WTFLogAlways("MSL compilation error: %@", [compilationError localizedDescription]); // NOLINT
    return [NSError errorWithDomain:@"WebGPU" code:1 userInfo:@{ NSLocalizedDescriptionKey: adoptNS([[NSString alloc] initWithFormat:@"Failed to compile the shader source, generated metal:\n%@ - error %@", msl, [compilationError localizedDescription]]).get() }];
}

id<MTLLibrary> ShaderModule::createLibrary(id<MTLDevice> device, const String& msl, String&& label, NSError** error, WGSL::DeviceState&& deviceState)
{
    return createLibrary(device, msl.createNSString().get(), label.createNSString().get(), error, deviceState);
}

id<MTLLibrary> ShaderModule::createLibrary(id<MTLDevice> device, NSString *msl, NSString *label, NSError** error, const WGSL::DeviceState& deviceState)
{
    NSError *compilationError = nil;
    id<MTLLibrary> library = [device newLibraryWithSource:msl options:compileOptions(deviceState) error:&compilationError];
    if (!library || compilationError) {
        if (error)
            *error = libraryCompilationError(msl, compilationError);
        return nil;
    }
    library.label = label;
    return library;
}

void ShaderModule::createLibraryAsync(id<MTLDevice> device, NSString *msl, NSString *label, const WGSL::DeviceState& deviceState, CompletionHandler<void(id<MTLLibrary>, NSError *)>&& callback)
{
    RetainPtr<id<MTLDevice>> retainedDevice = device;
    RetainPtr<NSString> retainedMSL = adoptNS([msl copy]);
    RetainPtr<NSString> retainedLabel = adoptNS([label copy]);
    @autoreleasepool {
        dispatch_async(globalDispatchQueueSingleton(QOS_CLASS_USER_INITIATED, 0), makeBlockPtr([retainedDevice = WTF::move(retainedDevice), retainedMSL = WTF::move(retainedMSL), retainedLabel = WTF::move(retainedLabel), deviceState, callback = WTF::move(callback)] mutable {
            @autoreleasepool {
                [retainedDevice.get() newLibraryWithSource:retainedMSL.get() options:compileOptions(deviceState) completionHandler:makeBlockPtr([retainedMSL = WTF::move(retainedMSL), retainedLabel = WTF::move(retainedLabel), callback = WTF::move(callback)](id<MTLLibrary> library, NSError *compilationError) mutable {
                    @autoreleasepool {
                        RetainPtr<id<MTLLibrary>> retainedLibrary = library;
                        RetainPtr<NSError> retainedError = compilationError;
                        dispatch_async(globalDispatchQueueSingleton(QOS_CLASS_USER_INITIATED, 0), makeBlockPtr([retainedMSL = WTF::move(retainedMSL), retainedLabel = WTF::move(retainedLabel), retainedLibrary = WTF::move(retainedLibrary), retainedError = WTF::move(retainedError), callback = WTF::move(callback)] mutable {
                            if (!retainedLibrary || retainedError) {
                                callback(nil, libraryCompilationError(retainedMSL.get(), retainedError.get()));
                                return;
                            }
                            retainedLibrary.get().label = retainedLabel.get();
                            callback(retainedLibrary.get(), nil);
                        }).get());
                    }
                }).get()];
            }
        }).get());
    }
}

static RefPtr<ShaderModule> earlyCompileShaderModule(Device& device, WGSL::SuccessfulCheck&& successfulCheck, String&& source, WGSL::Configuration&& configuration, std::span<const WebGPU::ShaderModuleCompilationHint> suppliedHints, String&& label)
{
    HashMap<String, Ref<PipelineLayout>> hints;
    HashMap<String, WGSL::PipelineLayout*> wgslHints;
    Vector<WGSL::PipelineLayout> wgslPipelineLayouts;
    wgslPipelineLayouts.reserveCapacity(suppliedHints.size());
    for (const auto& hint : suppliedHints) {
        auto& hintKey = hint.entryPoint;
        Ref layout = metal(hint.layout.get());
        hints.add(hintKey, layout);
        WGSL::PipelineLayout* convertedPipelineLayout = nullptr;
        if (layout->numberOfBindGroupLayouts()) {
            wgslPipelineLayouts.append(ShaderModule::convertPipelineLayout(layout));
            convertedPipelineLayout = &wgslPipelineLayouts.last();
        }
        wgslHints.add(hintKey, WTF::move(convertedPipelineLayout));
    }

    auto& shaderModule = successfulCheck.ast;
    auto prepareResult = WGSL::prepare(shaderModule, wgslHints);
    if (std::holds_alternative<WGSL::Error>(prepareResult))
        return nullptr;
    auto& result = std::get<WGSL::PrepareResult>(prepareResult);
    HashMap<String, WGSL::ConstantValue> wgslConstantValues;
    auto generationResult = WGSL::generate(shaderModule, result, wgslConstantValues, WGSL::DeviceState {
        .appleGPUFamily = device.appleGPUFamily(),
        .shaderValidationEnabled = device.isShaderValidationEnabled()
    });
    if (std::holds_alternative<WGSL::Error>(generationResult))
        return nullptr;
    auto& msl = std::get<String>(generationResult);
    NSError *error = nil;
    auto library = ShaderModule::createLibrary(device.device(), msl, WTF::move(label), &error, WGSL::DeviceState {
        .appleGPUFamily = device.appleGPUFamily(),
        .shaderValidationEnabled = device.isShaderValidationEnabled()
    });
    if (!library)
        return nullptr;
    return ShaderModule::create(WTF::move(successfulCheck), WTF::move(source), WTF::move(configuration), WTF::move(hints), WTF::move(result.entryPoints), library, device);
}

static OptionSet<WGSL::Extension> buildSupportedExtensions(const Vector<WebGPU::FeatureName>& features)
{
    OptionSet<WGSL::Extension> result;
    for (auto feature : features) {
        switch (feature) {
        case WebGPU::FeatureName::ClipDistances:
            result.add(WGSL::Extension::ClipDistances);
            break;
        case WebGPU::FeatureName::DualSourceBlending:
            result.add(WGSL::Extension::DualSourceBlending);
            break;
        case WebGPU::FeatureName::ShaderF16:
            result.add(WGSL::Extension::F16);
            break;
        case WebGPU::FeatureName::PrimitiveIndex:
            result.add(WGSL::Extension::PrimitiveIndex);
            break;
        case WebGPU::FeatureName::Subgroups:
            result.add(WGSL::Extension::Subgroups);
            break;
        default:
            break;
        }
    }
    return result;
}

struct ShaderModule::PendingCheck : ThreadSafeRefCounted<PendingCheck> {
    static Ref<PendingCheck> create(String&& isolatedSource, WGSL::Configuration&& isolatedConfiguration)
    {
        return adoptRef(*new PendingCheck(WTF::move(isolatedSource), WTF::move(isolatedConfiguration)));
    }

    String source;
    WGSL::Configuration configuration;
    std::optional<Variant<WGSL::SuccessfulCheck, WGSL::FailedCheck>> result;
    std::atomic<bool> isDone { false };
    BinarySemaphore semaphore;

private:
    PendingCheck(String&& isolatedSource, WGSL::Configuration&& isolatedConfiguration)
        : source(WTF::move(isolatedSource))
        , configuration(WTF::move(isolatedConfiguration))
    {
    }
};

struct ShaderModule::PendingValidationState {
    WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(PendingValidationState);
    Box<Device::ErrorScope> validationScope;
    uint64_t scopeOperationIndex { 0 };
    bool suppressErrors { false };
};

static unsigned maxInFlightParallelShaderChecks()
{
#if ASSERT_ENABLED
    // Debug builds enable CHECK_STRINGVIEW_LIFETIME, which serializes StringView construction on a single global lock.
    static constexpr unsigned limit = 2;
#elif PLATFORM(MAC) || PLATFORM(MACCATALYST)
    static const unsigned limit = std::min<unsigned>(16, std::max<unsigned>(4, WTF::numberOfProcessorCores() * 2));
#else
    static constexpr unsigned limit = 4;
#endif
    return limit;
}
static std::atomic<unsigned> s_inFlightShaderChecks { 0 };

static dispatch_queue_t wgslCompilationQueue()
{
    static LazyNeverDestroyed<OSObjectPtr<dispatch_queue_t>> queue;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        OSObjectPtr<dispatch_queue_attr_t> attr = dispatch_queue_attr_make_with_qos_class(concurrentQueueWithAutoreleasePoolAttrSingleton(), QOS_CLASS_USER_INITIATED, 0);
        queue.construct(dispatch_queue_create("com.apple.WebGPU.WGSLCompiler", attr.get()));
    });
    return queue.get().get();
}

static String formatFailedCheckMessage(const WGSL::FailedCheck& failedCheck)
{
    StringPrintStream message;
    message.print(String::number(failedCheck.errors.size()), " error", failedCheck.errors.size() != 1 ? "s" : "", " generated while compiling the shader:"_s);
    for (const auto& error : borrow(failedCheck.errors))
        message.print("\n"_s, error);
    return message.toString();
}

static Ref<ShaderModule> handleShaderSuccessOrFailure(WebGPU::Metal::Device &object, Variant<WGSL::SuccessfulCheck, WGSL::FailedCheck> &checkResult, const WebGPU::ShaderModuleDescriptor &descriptor, WGSL::Configuration&& configuration)
{
    if (auto* successfulCheck = std::get_if<WGSL::SuccessfulCheck>(&checkResult)) {
        if (!descriptor.hints.empty()) {
            // FIXME: re-enable early compilation later on once deferred compilation is fully implemented
            // https://bugs.webkit.org/show_bug.cgi?id=254258
            UNUSED_PARAM(earlyCompileShaderModule);
        }

        return ShaderModule::create(WTF::move(*successfulCheck), descriptor.code.isolatedCopy(), WTF::move(configuration), { }, { }, nil, object);
    }

    auto& failedCheck = std::get<WGSL::FailedCheck>(checkResult);
    object.generateAValidationError(formatFailedCheckMessage(failedCheck));
    return ShaderModule::createInvalid(object, WTF::move(failedCheck));
}

RefPtr<WebGPU::ShaderModule> Device::createShaderModule(const WebGPU::ShaderModuleDescriptor& descriptor)
{
    if (!isValid())
        return ShaderModule::createInvalid(*this);

    if (!m_hasBuiltWGSLSupportedExtensions) {
        m_wgslSupportedExtensions = buildSupportedExtensions(m_capabilities.features);
        m_hasBuiltWGSLSupportedExtensions = true;
    }

    WGSL::Configuration configuration {
        .maxBuffersPlusVertexBuffersForVertexStage = maxBuffersPlusVertexBuffersForVertexStage(),
        .maxBuffersForFragmentStage = maxBuffersForFragmentStage(),
        .maxBuffersForComputeStage = maxBuffersForComputeStage(),
        .maximumCombinedWorkgroupVariablesSize = limits().maxComputeWorkgroupStorageSize,
        .supportedExtensions = m_wgslSupportedExtensions
    };

    RefPtr inst = weakInstance();
    Box<ErrorScope> validationScope = (!m_supressAllErrors && !m_isLost) ? currentErrorScope(WebGPU::ErrorFilter::Validation) : nullptr;
    if (descriptor.hints.empty() && (validationScope || !m_uncapturedErrorCallback) && inst && inst->hasExternalWorkScheduler() && asynchronousIfPossible() == LibraryCompilation::Asynchronous && s_inFlightShaderChecks.load(std::memory_order_relaxed) < maxInFlightParallelShaderChecks()) {
        s_inFlightShaderChecks.fetch_add(1, std::memory_order_relaxed);

        uint64_t scopeOpIndex = 0;
        if (validationScope) {
            scopeOpIndex = validationScope->nextOperationIndex++;
            ++validationScope->pendingAsyncOperations;
        }

        WGSL::Configuration isolatedConfig {
            .maxBuffersPlusVertexBuffersForVertexStage = configuration.maxBuffersPlusVertexBuffersForVertexStage,
            .maxBuffersForFragmentStage = configuration.maxBuffersForFragmentStage,
            .maxBuffersForComputeStage = configuration.maxBuffersForComputeStage,
            .maximumCombinedWorkgroupVariablesSize = configuration.maximumCombinedWorkgroupVariablesSize,
            .supportedExtensions = m_wgslSupportedExtensions
        };

        auto pendingValidationState = makeUnique<ShaderModule::PendingValidationState>(ShaderModule::PendingValidationState {
            .validationScope = WTF::move(validationScope),
            .scopeOperationIndex = scopeOpIndex,
            .suppressErrors = m_supressAllErrors,
        });
        Ref pendingCheck = ShaderModule::PendingCheck::create(descriptor.code.isolatedCopy(), WTF::move(isolatedConfig));
        Ref shaderModule = ShaderModule::createPending(descriptor.code.isolatedCopy(), WTF::move(configuration), pendingCheck.copyRef(), WTF::move(pendingValidationState), *this);
        ThreadSafeWeakPtr<ShaderModule> weakShaderModule { shaderModule.get() };
        ThreadSafeWeakPtr<Device> weakDevice { *this };
        m_pendingShaderModules.append(weakShaderModule);

        @autoreleasepool {
            dispatch_async(wgslCompilationQueue(), makeBlockPtr([pendingCheck = WTF::move(pendingCheck), asyncCompileInstance = AsyncCompileInstance { *inst }, weakDevice = WTF::move(weakDevice), weakShaderModule = WTF::move(weakShaderModule)] mutable {
                String localSource = WTF::move(pendingCheck->source);
                auto checkResult = WGSL::staticCheck(localSource, std::nullopt, pendingCheck->configuration);
                localSource = String();
                pendingCheck->result = WTF::move(checkResult);
                pendingCheck->isDone.store(true, std::memory_order_release);
                s_inFlightShaderChecks.fetch_sub(1, std::memory_order_relaxed);
                pendingCheck->semaphore.signal();
                auto localInstance = WTF::move(asyncCompileInstance);
                if (RefPtr instance = localInstance.take()) {
                    instance->scheduleWork([weakDevice = WTF::move(weakDevice), weakShaderModule = WTF::move(weakShaderModule)] {
                        if (RefPtr device = weakDevice)
                            device->completeReadyPendingShaderModules();
                        else if (RefPtr shaderModule = weakShaderModule)
                            shaderModule->ensureCompiled();
                    });
                }
            }).get());
        }

        return shaderModule;
    }

    auto checkResult = WGSL::staticCheck(descriptor.code.isolatedCopy(), std::nullopt, configuration);

    return handleShaderSuccessOrFailure(*this, checkResult, descriptor, WTF::move(configuration));
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(ShaderModule);

NODELETE static MTLDataType metalDataTypeFromPrimitive(const WGSL::Types::Primitive *primitiveType, int vectorSize = 1)
{
    switch (vectorSize) {
    case 1:
        switch (primitiveType->kind) {
        case WGSL::Types::Primitive::I32:
            return MTLDataTypeInt;
        case WGSL::Types::Primitive::U32:
            return MTLDataTypeUInt;
        case WGSL::Types::Primitive::F16:
            return MTLDataTypeHalf;
        case WGSL::Types::Primitive::F32:
            return MTLDataTypeFloat;
        case WGSL::Types::Primitive::Bool:
            return MTLDataTypeBool;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    case 2:
        switch (primitiveType->kind) {
        case WGSL::Types::Primitive::I32:
            return MTLDataTypeInt2;
        case WGSL::Types::Primitive::U32:
            return MTLDataTypeUInt2;
        case WGSL::Types::Primitive::F16:
            return MTLDataTypeHalf2;
        case WGSL::Types::Primitive::F32:
            return MTLDataTypeFloat2;
        case WGSL::Types::Primitive::Bool:
            return MTLDataTypeBool2;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    case 3:
        switch (primitiveType->kind) {
        case WGSL::Types::Primitive::I32:
            return MTLDataTypeInt3;
        case WGSL::Types::Primitive::U32:
            return MTLDataTypeUInt3;
        case WGSL::Types::Primitive::F16:
            return MTLDataTypeHalf3;
        case WGSL::Types::Primitive::F32:
            return MTLDataTypeFloat3;
        case WGSL::Types::Primitive::Bool:
            return MTLDataTypeBool3;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    case 4:
        switch (primitiveType->kind) {
        case WGSL::Types::Primitive::I32:
            return MTLDataTypeInt4;
        case WGSL::Types::Primitive::U32:
            return MTLDataTypeUInt4;
        case WGSL::Types::Primitive::F16:
            return MTLDataTypeHalf4;
        case WGSL::Types::Primitive::F32:
            return MTLDataTypeFloat4;
        case WGSL::Types::Primitive::Bool:
            return MTLDataTypeBool4;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

NODELETE static WebGPU::VertexFormat vertexFormatTypeFromPrimitive(const WGSL::Types::Primitive *primitiveType, int vectorSize)
{
    switch (vectorSize) {
    case 1:
        switch (primitiveType->kind) {
        case WGSL::Types::Primitive::I32:
            return WebGPU::VertexFormat::Sint32;
        case WGSL::Types::Primitive::U32:
            return WebGPU::VertexFormat::Uint32;
        case WGSL::Types::Primitive::F16:
            return WebGPU::VertexFormat::Float32;
        case WGSL::Types::Primitive::F32:
            return WebGPU::VertexFormat::Float32;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    case 2:
        switch (primitiveType->kind) {
        case WGSL::Types::Primitive::I32:
            return WebGPU::VertexFormat::Sint32x2;
        case WGSL::Types::Primitive::U32:
            return WebGPU::VertexFormat::Uint32x2;
        case WGSL::Types::Primitive::F16:
            return WebGPU::VertexFormat::Float16x2;
        case WGSL::Types::Primitive::F32:
            return WebGPU::VertexFormat::Float32x2;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    case 3:
        switch (primitiveType->kind) {
        case WGSL::Types::Primitive::I32:
            return WebGPU::VertexFormat::Sint32x3;
        case WGSL::Types::Primitive::U32:
            return WebGPU::VertexFormat::Uint32x3;
        case WGSL::Types::Primitive::F16:
            return WebGPU::VertexFormat::Float16x4;
        case WGSL::Types::Primitive::F32:
            return WebGPU::VertexFormat::Float32x3;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    case 4:
        switch (primitiveType->kind) {
        case WGSL::Types::Primitive::I32:
            return WebGPU::VertexFormat::Sint32x4;
        case WGSL::Types::Primitive::U32:
            return WebGPU::VertexFormat::Uint32x4;
        case WGSL::Types::Primitive::F16:
            return WebGPU::VertexFormat::Float16x4;
        case WGSL::Types::Primitive::F32:
            return WebGPU::VertexFormat::Float32x4;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

static MTLDataType metalDataTypeForStructMember(const WGSL::Type* type)
{
    if (!type)
        return MTLDataTypeNone;

    auto* vectorType = std::get_if<WGSL::Types::Vector>(type);
    auto* primitiveType = std::get_if<WGSL::Types::Primitive>(vectorType ? vectorType->element : type);
    if (!primitiveType)
        return MTLDataTypeNone;

    auto vectorSize = vectorType ? vectorType->size : 1;
    return metalDataTypeFromPrimitive(primitiveType, vectorSize);
}

static WebGPU::VertexFormat vertexFormatTypeForStructMember(const WGSL::Type* type)
{
    if (!type)
        RELEASE_ASSERT_NOT_REACHED();

    auto* vectorType = std::get_if<WGSL::Types::Vector>(type);
    auto* primitiveType = std::get_if<WGSL::Types::Primitive>(vectorType ? vectorType->element : type);
    if (!primitiveType)
        RELEASE_ASSERT_NOT_REACHED();

    auto vectorSize = vectorType ? vectorType->size : 1;
    return vertexFormatTypeFromPrimitive(primitiveType, vectorSize);
}

void ShaderModule::populateOutputState(const String& entryPoint, WGSL::Builtin builtIn)
{
    switch (builtIn) {
    case WGSL::Builtin::SampleMask:
        populateShaderModuleState(entryPoint).usesSampleMaskInOutput = true;
        break;
    case WGSL::Builtin::FragDepth:
        populateShaderModuleState(entryPoint).usesFragDepth = true;
        break;
    default:
        break;
    }
}

ShaderModule::FragmentOutputs ShaderModule::parseFragmentReturnType(const WGSL::Type& type, const WGSL::CallGraph::EntryPoint& entryPoint, const String& entryPointName)
{
    ShaderModule::FragmentOutputs fragmentOutputs;
    if (auto* returnPrimitive = std::get_if<WGSL::Types::Primitive>(&type)) {
        fragmentOutputs.add(entryPoint.function.returnTypeLocation().value_or(0), metalDataTypeFromPrimitive(returnPrimitive));
        return fragmentOutputs;
    }
    if (std::get_if<WGSL::Types::Vector>(&type)) {
        fragmentOutputs.add(entryPoint.function.returnTypeLocation().value_or(0), metalDataTypeForStructMember(&type));
        return fragmentOutputs;
    }
    auto* returnStruct = std::get_if<WGSL::Types::Struct>(&type);
    if (!returnStruct)
        return fragmentOutputs;

    for (auto& member : returnStruct->structure.members()) {
        if (member.builtin())
            populateOutputState(entryPointName, *member.builtin());

        for (auto& attribute : member.attributes()) {
            auto* builtinAttribute = dynamicDowncast<WGSL::AST::BuiltinAttribute>(attribute);
            if (!builtinAttribute)
                continue;
            populateOutputState(entryPointName, builtinAttribute->builtin());
        }

        if (!member.location() || member.builtin())
            continue;

        if (member.blendSrc())
            populateShaderModuleState(entryPointName).usesDualSourceBlending = true;

        auto location = *member.location();
        fragmentOutputs.add(location, metalDataTypeForStructMember(member.type().inferredType()));
    }

    return fragmentOutputs;
}

static ShaderModule::VertexOutputs parseVertexReturnType(const WGSL::Type& type)
{
    ShaderModule::VertexOutputs vertexOutputs;
    if (auto* returnPrimitive = std::get_if<WGSL::Types::Primitive>(&type)) {
        vertexOutputs.add(0, ShaderModule::VertexOutputFragmentInput {
            .dataType = metalDataTypeFromPrimitive(returnPrimitive),
            .interpolation = std::nullopt
        });
        return vertexOutputs;
    }
    if (std::get_if<WGSL::Types::Vector>(&type)) {
        vertexOutputs.add(0, ShaderModule::VertexOutputFragmentInput {
            .dataType = metalDataTypeForStructMember(&type),
            .interpolation = std::nullopt
        });
        return vertexOutputs;
    }
    auto* returnStruct = std::get_if<WGSL::Types::Struct>(&type);
    if (!returnStruct)
        return vertexOutputs;

    for (auto& member : returnStruct->structure.members()) {
        if (!member.location() || member.builtin())
            continue;

        auto location = *member.location();
        vertexOutputs.add(location, ShaderModule::VertexOutputFragmentInput {
            .dataType = metalDataTypeForStructMember(member.type().inferredType()),
            .interpolation = member.interpolation()
        });
    }

    return vertexOutputs;
}

static void populateStageInMap(const WGSL::Type& type, ShaderModule::VertexStageIn& vertexStageIn, const std::optional<unsigned>& optionalLocation)
{
    auto* inputStruct = std::get_if<WGSL::Types::Struct>(&type);
    if (!inputStruct) {
        if (optionalLocation)
            vertexStageIn.add(*optionalLocation, vertexFormatTypeForStructMember(&type));
        return;
    }

    for (auto& member : inputStruct->structure.members()) {
        if (!member.location())
            continue;
        auto location = *member.location();
        auto dataType = vertexFormatTypeForStructMember(member.type().inferredType());
        vertexStageIn.add(location, dataType);
    }
}

const ShaderModule::ShaderModuleState* ShaderModule::shaderModuleState(const String& entryPoint) const
{
    ensureCompiled();
    if (auto it = m_usageInformationPerEntryPoint.find(entryPoint); it != m_usageInformationPerEntryPoint.end())
        return &it->value;

    return nullptr;
}

ShaderModule::ShaderModuleState& ShaderModule::populateShaderModuleState(const String& entryPoint)
{
    if (auto it = m_usageInformationPerEntryPoint.find(entryPoint); it != m_usageInformationPerEntryPoint.end())
        return it->value;

    return m_usageInformationPerEntryPoint.add(entryPoint, ShaderModule::ShaderModuleState()).iterator->value;
}

bool ShaderModule::usesFrontFacingInInput(const String& entryPoint) const
{
    if (auto state = shaderModuleState(entryPoint))
        return state->usesFrontFacingInInput;
    return false;
}
bool ShaderModule::usesSampleIndexInInput(const String& entryPoint) const
{
    if (auto state = shaderModuleState(entryPoint))
        return state->usesSampleIndexInInput;
    return false;
}
bool ShaderModule::usesSampleMaskInInput(const String& entryPoint) const
{
    if (auto state = shaderModuleState(entryPoint))
        return state->usesSampleMaskInInput;
    return false;
}

bool ShaderModule::usesSampleMaskInOutput(const String& entryPoint) const
{
    if (auto state = shaderModuleState(entryPoint))
        return state->usesSampleMaskInOutput;
    return false;
}

bool ShaderModule::usesFragDepth(const String& entryPoint) const
{
    if (auto state = shaderModuleState(entryPoint))
        return state->usesFragDepth;
    return false;
}

bool ShaderModule::usesDualSourceBlending(const String& entryPoint) const
{
    if (auto state = shaderModuleState(entryPoint))
        return state->usesDualSourceBlending;
    return false;
}

bool ShaderModule::usesPrimitiveIndexInInput(const String& entryPoint) const
{
    if (auto state = shaderModuleState(entryPoint))
        return state->usesPrimitiveIndexInInput;
    return false;
}

bool ShaderModule::usesSubgroupInvocationIdInInput(const String& entryPoint) const
{
    if (auto state = shaderModuleState(entryPoint))
        return state->usesSubgroupInvocationIdInInput;
    return false;
}

bool ShaderModule::usesSubgroupSizeInInput(const String& entryPoint) const
{
    if (auto state = shaderModuleState(entryPoint))
        return state->usesSubgroupSizeInInput;
    return false;
}

uint32_t ShaderModule::clipDistancesCount(const String& entryPoint) const
{
    if (auto state = shaderModuleState(entryPoint))
        return state->clipDistancesCount;
    return 0;
}

void ShaderModule::populateFragmentInputs(const WGSL::Type& type, ShaderModule::FragmentInputs& fragmentInputs, const String& entryPointName)
{
    auto* inputStruct = std::get_if<WGSL::Types::Struct>(&type);
    if (!inputStruct)
        return;

    for (auto& member : inputStruct->structure.members()) {
        if (member.builtin()) {
            using enum WGSL::Builtin;
            switch (*member.builtin()) {
            case ClipDistances:
                break;
            case FragDepth:
                populateShaderModuleState(entryPointName).usesFragDepth = true;
                break;
            case FrontFacing:
                populateShaderModuleState(entryPointName).usesFrontFacingInInput = true;
                break;
            case GlobalInvocationId:
            case GlobalInvocationIndex:
                break;
            case InstanceIndex:
                break;
            case LocalInvocationId:
                break;
            case LocalInvocationIndex:
                break;
            case NumWorkgroups:
                break;
            case Position:
                break;
            case PrimitiveIndex:
                populateShaderModuleState(entryPointName).usesPrimitiveIndexInInput = true;
                break;
            case SampleIndex:
                populateShaderModuleState(entryPointName).usesSampleIndexInInput = true;
                break;
            case SampleMask:
                populateShaderModuleState(entryPointName).usesSampleMaskInInput = true;
                break;
            case SubgroupInvocationId:
                populateShaderModuleState(entryPointName).usesSubgroupInvocationIdInInput = true;
                break;
            case SubgroupSize:
                populateShaderModuleState(entryPointName).usesSubgroupSizeInInput = true;
                break;
            case SubgroupId:
                break;
            case NumSubgroups:
                break;
            case VertexIndex:
                break;
            case WorkgroupId:
            case WorkgroupIndex:
                break;
            }
        }
        if (!member.location())
            continue;
        auto location = *member.location();
        auto dataType = metalDataTypeForStructMember(member.type().inferredType());
        fragmentInputs.add(location, ShaderModule::VertexOutputFragmentInput {
            .dataType = dataType,
            .interpolation = member.interpolation()
        });
    }
}

static ShaderModule::VertexStageIn parseStageIn(const WGSL::AST::Function& function)
{
    ShaderModule::VertexStageIn result;
    for (auto& parameter : function.parameters()) {
        if (parameter.role() != WGSL::AST::ParameterRole::UserDefined)
            continue;

        if (auto* inferredType = parameter.typeName().inferredType())
            populateStageInMap(*inferredType, result, parameter.location());
    }

    return result;
}

ShaderModule::FragmentInputs ShaderModule::parseFragmentInputs(const WGSL::AST::Function& function, const String& entryPointName)
{
    ShaderModule::FragmentInputs result;
    for (auto& parameter : function.parameters()) {
        if (parameter.role() != WGSL::AST::ParameterRole::UserDefined)
            continue;

        if (auto* inferredType = parameter.typeName().inferredType())
            populateFragmentInputs(*inferredType, result, entryPointName);
    }

    return result;
}

ShaderModule::ShaderModule(WGSL::SuccessfulCheck&& successfulCheck, String&& source, WGSL::Configuration&& configuration, HashMap<String, Ref<PipelineLayout>>&& pipelineLayoutHints, HashMap<String, WGSL::Reflection::EntryPointInformation>&& entryPointInformation, id<MTLLibrary> library, Device& device)
    : m_checkResult(ValidCheck { WTF::move(successfulCheck.warnings) })
    , m_source(WTF::move(source))
    , m_configuration(WTF::move(configuration))
    , m_ast(successfulCheck.ast.moveToUniquePtr())
    , m_astFootprint(m_ast ? m_ast->memoryFootprint() : 0)
    , m_pipelineLayoutHints(WTF::move(pipelineLayoutHints))
    , m_entryPointInformation(WTF::move(entryPointInformation))
    , m_library(library)
    , m_device(device)
{
    populateEntryPointMetadata();
}

ShaderModule::ShaderModule(String&& source, WGSL::Configuration&& configuration, Ref<PendingCheck>&& pendingCheck, std::unique_ptr<PendingValidationState>&& pendingValidationState, Device& device)
    : m_source(WTF::move(source))
    , m_configuration(WTF::move(configuration))
    , m_pendingCheck(WTF::move(pendingCheck))
    , m_pendingValidationState(WTF::move(pendingValidationState))
    , m_device(device)
{
}

void ShaderModule::populateEntryPointMetadata()
{
    bool allowVertexDefault = true, allowFragmentDefault = true, allowComputeDefault = true;
    for (auto& entryPoint : m_ast->callGraph().entrypoints()) {
        String entryPointName = entryPoint.originalName.isolatedCopy();
        switch (entryPoint.stage) {
        case WGSL::ShaderStage::Vertex: {
            m_stageInTypesForEntryPoint.add(entryPointName, parseStageIn(entryPoint.function));
            if (auto expression = entryPoint.function.maybeReturnType()) {
                if (auto* inferredType = expression->inferredType()) {
                    m_vertexReturnTypeForEntryPoint.add(entryPointName, parseVertexReturnType(*inferredType));

                    // Check for @builtin(clip_distances) in vertex output
                    if (auto* returnStruct = std::get_if<WGSL::Types::Struct>(inferredType)) {
                        for (auto& member : returnStruct->structure.members()) {
                            if (member.builtin() && *member.builtin() == WGSL::Builtin::ClipDistances) {
                                // Extract the array size from array<f32, N>
                                if (auto* arrayType = std::get_if<WGSL::Types::Array>(member.type().inferredType())) {
                                    if (auto* size = std::get_if<unsigned>(&arrayType->size))
                                        populateShaderModuleState(entryPointName).clipDistancesCount = *size;
                                }
                                break;
                            }
                        }
                    }
                }
            }
            if (!allowVertexDefault || m_defaultVertexEntryPoint.length()) {
                allowVertexDefault = false;
                m_defaultVertexEntryPoint = emptyString();
                continue;
            }
            m_defaultVertexEntryPoint = WTF::move(entryPointName);
        } break;
        case WGSL::ShaderStage::Fragment: {
            m_fragmentInputsForEntryPoint.add(entryPointName, parseFragmentInputs(entryPoint.function, entryPointName));
            if (auto expression = entryPoint.function.maybeReturnType()) {
                if (auto* inferredType = expression->inferredType())
                    m_fragmentReturnTypeForEntryPoint.add(entryPointName, parseFragmentReturnType(*inferredType, entryPoint, entryPointName));
            }
            if (!allowFragmentDefault || m_defaultFragmentEntryPoint.length()) {
                allowFragmentDefault = false;
                m_defaultFragmentEntryPoint = emptyString();
                continue;
            }
            m_defaultFragmentEntryPoint = WTF::move(entryPointName);
        } break;
        case WGSL::ShaderStage::Compute: {
            if (!allowComputeDefault || m_defaultComputeEntryPoint.length()) {
                allowComputeDefault = false;
                m_defaultComputeEntryPoint = emptyString();
                continue;
            }
            m_defaultComputeEntryPoint = WTF::move(entryPointName);
        } break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }
}

const ShaderModule::FragmentInputs* ShaderModule::fragmentInputsForEntryPoint(const String& entryPoint) const
{
    ensureCompiled();
    if (auto it = m_fragmentInputsForEntryPoint.find(entryPoint); it != m_fragmentInputsForEntryPoint.end())
        return &it->value;

    return nullptr;
}

const ShaderModule::FragmentOutputs* ShaderModule::fragmentReturnTypeForEntryPoint(const String& entryPoint) const
{
    ensureCompiled();
    if (auto it = m_fragmentReturnTypeForEntryPoint.find(entryPoint); it != m_fragmentReturnTypeForEntryPoint.end())
        return &it->value;

    return nullptr;
}

const ShaderModule::VertexOutputs* ShaderModule::vertexReturnTypeForEntryPoint(const String& entryPoint) const
{
    ensureCompiled();
    if (auto it = m_vertexReturnTypeForEntryPoint.find(entryPoint); it != m_vertexReturnTypeForEntryPoint.end())
        return &it->value;

    return nullptr;
}

const ShaderModule::VertexStageIn* ShaderModule::stageInTypesForEntryPoint(const String& entryPoint) const
{
    ensureCompiled();
    if (!entryPoint.length())
        return nullptr;

    if (auto it = m_stageInTypesForEntryPoint.find(entryPoint); it != m_stageInTypesForEntryPoint.end())
        return &it->value;

    return nullptr;
}

ShaderModule::ShaderModule(Device& device, CheckResult&& checkResult)
    : m_checkResult(WTF::move(checkResult))
    , m_device(device)
{
}

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
static constexpr size_t maxRetainedASTBytes = 100 * 1024 * 1024;
#else
static constexpr size_t maxRetainedASTBytes = 10 * 1024 * 1024;
#endif

struct ASTCacheState {
    WTF::Lock lock;
    ThreadSafeWeakPtr<const ShaderModule> lruHead;
    ThreadSafeWeakPtr<const ShaderModule> lruTail;
    size_t totalBytes { 0 };

    void unlinkLocked(const ShaderModule& module)
    {
        if (!module.m_inASTCache)
            return;
        if (RefPtr prev = module.m_prevCachedAST)
            prev->m_nextCachedAST = module.m_nextCachedAST;
        else
            lruHead = module.m_nextCachedAST;

        if (RefPtr next = module.m_nextCachedAST)
            next->m_prevCachedAST = module.m_prevCachedAST;
        else
            lruTail = module.m_prevCachedAST;

        module.m_prevCachedAST = nullptr;
        module.m_nextCachedAST = nullptr;
        module.m_inASTCache = false;
        ASSERT(totalBytes >= module.m_astFootprint);
        totalBytes -= module.m_astFootprint;
    }

    void insertMRULocked(const ShaderModule& module)
    {
        if (module.m_inASTCache) {
            if (RefPtr tail = lruTail; tail.get() == &module)
                return;
            unlinkLocked(module);
        }
        module.m_prevCachedAST = lruTail;
        module.m_nextCachedAST = nullptr;
        if (RefPtr tail = lruTail)
            tail->m_nextCachedAST = module;
        else
            lruHead = module;
        lruTail = module;
        module.m_inASTCache = true;
        totalBytes += module.m_astFootprint;
    }

    void evictToBudgetLocked(size_t budgetBytes, Vector<std::unique_ptr<WGSL::ShaderModule>, 8>& toDestroy)
    {
        while (totalBytes > budgetBytes) {
            RefPtr candidate = lruHead;
            if (!candidate) {
                lruHead = nullptr;
                lruTail = nullptr;
                totalBytes = 0;
                break;
            }
            unlinkLocked(*candidate);
            candidate->m_astEvicted = true;
            if (!candidate->m_astHandleCount && candidate->m_entryPointInformation.isEmpty() && candidate->m_ast)
                toDestroy.append(WTF::move(candidate->m_ast));
        }
    }
};

static ASTCacheState& astCacheState()
{
    static LazyNeverDestroyed<ASTCacheState> state;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        state.construct();
        constexpr uintptr_t memoryPressureFlags = DISPATCH_MEMORYPRESSURE_WARN | DISPATCH_MEMORYPRESSURE_CRITICAL | DISPATCH_MEMORYPRESSURE_PROC_LIMIT_WARN | DISPATCH_MEMORYPRESSURE_PROC_LIMIT_CRITICAL;
        static LazyNeverDestroyed<OSObjectPtr<dispatch_source_t>> memoryPressureSource;
        memoryPressureSource.construct(dispatch_source_create(DISPATCH_SOURCE_TYPE_MEMORYPRESSURE, 0, memoryPressureFlags, globalDispatchQueueSingleton(QOS_CLASS_UTILITY, 0)));
        if (memoryPressureSource.get()) {
            dispatch_source_set_event_handler(memoryPressureSource.get().get(), ^{
                ShaderModule::purgeAllCachedASTs();
                purgeMetalPipelineCaches();
            });
            dispatch_resume(memoryPressureSource.get().get());
        }
    });
    return state.get();
}

ShaderModule::~ShaderModule()
{
    finalizePendingCheck();
    std::unique_ptr<WGSL::ShaderModule> deadAST;
    {
        auto& cache = astCacheState();
        Locker locker { cache.lock };
        cache.unlinkLocked(*this);
        deadAST = WTF::move(m_ast);
    }
}

bool ShaderModule::isValid() const
{
    ensureCompiled();
    return std::holds_alternative<ValidCheck>(m_checkResult);
}

void ShaderModule::ensureCompiled() const
{
    if (!m_pendingCheck) [[likely]]
        return;
    const_cast<ShaderModule*>(this)->finalizePendingCheck();
}

bool ShaderModule::hasCompletedPendingCheck() const
{
    return !m_pendingCheck || m_pendingCheck->isDone.load(std::memory_order_acquire);
}

void ShaderModule::finalizePendingCheck()
{
    RefPtr pendingCheck = std::exchange(m_pendingCheck, nullptr);
    if (!pendingCheck)
        return;

    auto validationState = std::exchange(m_pendingValidationState, nullptr);
    pendingCheck->semaphore.wait();
    auto checkResult = std::exchange(pendingCheck->result, std::nullopt);
    RELEASE_ASSERT(checkResult.has_value());

    if (auto* successfulCheck = std::get_if<WGSL::SuccessfulCheck>(&*checkResult)) {
        m_checkResult = ValidCheck { WTF::move(successfulCheck->warnings) };
        m_ast = successfulCheck->ast.moveToUniquePtr();
        m_astFootprint = m_ast ? m_ast->memoryFootprint() : 0;
        populateEntryPointMetadata();
        retainASTRecently();
    } else {
        auto& failedCheck = std::get<WGSL::FailedCheck>(*checkResult);
        String errorMessage = formatFailedCheckMessage(failedCheck);
        m_checkResult = WTF::move(failedCheck);
        if (validationState && !validationState->suppressErrors && !m_device->isLost()) {
            if (validationState->validationScope)
                validationState->validationScope->recordErrorAt(validationState->scopeOperationIndex, WebGPU::ErrorType::Validation, WTF::move(errorMessage));
            else
                m_device->reportUncapturedValidationError(WTF::move(errorMessage));
        }
    }

    if (validationState && validationState->validationScope)
        validationState->validationScope->completeAsyncOperation(m_device->isLost());

    auto callbacks = std::exchange(m_pendingCompilationInfoCallbacks, { });
    for (auto& callback : callbacks)
        compilationInfo(WTF::move(callback));
}

struct Messages {
    const Vector<WGSL::CompilationMessage>& messages;
    WebGPU::CompilationMessageType type;
};

static void appendMessages(Vector<WebGPU::CompilationMessage>& result, const Messages& messages)
{
    for (const auto& compilationMessage : borrow(messages.messages)) {
        result.append({
            .message = compilationMessage.message(),
            .type = messages.type,
            .lineNum = compilationMessage.lineNumber(),
            .linePos = compilationMessage.lineOffset() + 1,
            .offset = compilationMessage.offset(),
            .length = compilationMessage.length(),
        });
    }
}

void ShaderModule::compilationInfo(CompletionHandler<void(WebGPU::CompilationInfo&&)>&& callback)
{
    if (m_pendingCheck) {
        m_pendingCompilationInfoCallbacks.append(WTF::move(callback));
        return;
    }

    WebGPU::CompilationInfo compilationInfo;
    WTF::switchOn(m_checkResult, [&](const ValidCheck& validCheck) {
        appendMessages(compilationInfo.messages, { validCheck.warnings, WebGPU::CompilationMessageType::Warning });
    }, [&](const WGSL::FailedCheck& failedCheck) {
        appendMessages(compilationInfo.messages, { failedCheck.errors, WebGPU::CompilationMessageType::Error });
        appendMessages(compilationInfo.messages, { failedCheck.warnings, WebGPU::CompilationMessageType::Warning });
    }, [&](std::monostate) {
    });
    callback(WTF::move(compilationInfo));
}

void ShaderModule::setLabel(String&& label)
{
    if (m_library)
        m_library.label = label.createNSString().get();
}

NODELETE static auto wgslBindingType(BindGroupLayout::BufferBindingType bindingType)
{
    switch (bindingType) {
    case BindGroupLayout::BufferBindingType::Uniform:
        return WGSL::BufferBindingType::Uniform;
    case BindGroupLayout::BufferBindingType::Storage:
        return WGSL::BufferBindingType::Storage;
    case BindGroupLayout::BufferBindingType::ReadOnlyStorage:
        return WGSL::BufferBindingType::ReadOnlyStorage;
    case BindGroupLayout::BufferBindingType::Float3x2:
    case BindGroupLayout::BufferBindingType::Float4x3:
    case BindGroupLayout::BufferBindingType::Float3x4:
    case BindGroupLayout::BufferBindingType::UInt2:
    case BindGroupLayout::BufferBindingType::ArrayLength:
        ASSERT_NOT_REACHED("Unexpected buffer bindingType");
        return WGSL::BufferBindingType::Uniform;
    }
}

NODELETE static auto wgslSamplerType(WebGPU::SamplerBindingType bindingType)
{
    switch (bindingType) {
    case WebGPU::SamplerBindingType::Filtering:
        return WGSL::SamplerBindingType::Filtering;
    case WebGPU::SamplerBindingType::Comparison:
        return WGSL::SamplerBindingType::Comparison;
    case WebGPU::SamplerBindingType::NonFiltering:
        return WGSL::SamplerBindingType::NonFiltering;
    }
}

NODELETE static auto wgslSampleType(WebGPU::TextureSampleType sampleType)
{
    switch (sampleType) {
    case WebGPU::TextureSampleType::Sint:
        return WGSL::TextureSampleType::SignedInt;
    case WebGPU::TextureSampleType::Uint:
        return WGSL::TextureSampleType::UnsignedInt;
    case WebGPU::TextureSampleType::Depth:
        return WGSL::TextureSampleType::Depth;
    case WebGPU::TextureSampleType::Float:
        return WGSL::TextureSampleType::Float;
    case WebGPU::TextureSampleType::UnfilterableFloat:
        return WGSL::TextureSampleType::UnfilterableFloat;
    }
}

NODELETE static auto wgslViewDimension(WebGPU::TextureViewDimension viewDimension)
{
    switch (viewDimension) {
    case WebGPU::TextureViewDimension::Cube:
        return WGSL::TextureViewDimension::Cube;
    case WebGPU::TextureViewDimension::_1d:
        return WGSL::TextureViewDimension::OneDimensional;
    case WebGPU::TextureViewDimension::_2d:
        return WGSL::TextureViewDimension::TwoDimensional;
    case WebGPU::TextureViewDimension::_3d:
        return WGSL::TextureViewDimension::ThreeDimensional;
    case WebGPU::TextureViewDimension::CubeArray:
        return WGSL::TextureViewDimension::CubeArray;
    case WebGPU::TextureViewDimension::_2dArray:
        return WGSL::TextureViewDimension::TwoDimensionalArray;
    }
}

NODELETE static WGSL::StorageTextureAccess wgslAccess(WebGPU::StorageTextureAccess access)
{
    switch (access) {
    case WebGPU::StorageTextureAccess::WriteOnly:
        return WGSL::StorageTextureAccess::WriteOnly;
    case WebGPU::StorageTextureAccess::ReadOnly:
        return WGSL::StorageTextureAccess::ReadOnly;
    case WebGPU::StorageTextureAccess::ReadWrite:
        return WGSL::StorageTextureAccess::ReadWrite;
    }
}

NODELETE static WGSL::TexelFormat wgslFormat(WebGPU::TextureFormat format)
{
    switch (format) {
    case WebGPU::TextureFormat::Bgra8unorm:
        return WGSL::TexelFormat::BGRA8unorm;
    case WebGPU::TextureFormat::R16float:
        return WGSL::TexelFormat::R16float;
    case WebGPU::TextureFormat::R16sint:
        return WGSL::TexelFormat::R16sint;
    case WebGPU::TextureFormat::R16snorm:
        return WGSL::TexelFormat::R16snorm;
    case WebGPU::TextureFormat::R16uint:
        return WGSL::TexelFormat::R16uint;
    case WebGPU::TextureFormat::R16unorm:
        return WGSL::TexelFormat::R16unorm;
    case WebGPU::TextureFormat::R32float:
        return WGSL::TexelFormat::R32float;
    case WebGPU::TextureFormat::R32sint:
        return WGSL::TexelFormat::R32sint;
    case WebGPU::TextureFormat::R32uint:
        return WGSL::TexelFormat::R32uint;
    case WebGPU::TextureFormat::R8sint:
        return WGSL::TexelFormat::R8sint;
    case WebGPU::TextureFormat::R8snorm:
        return WGSL::TexelFormat::R8snorm;
    case WebGPU::TextureFormat::R8uint:
        return WGSL::TexelFormat::R8uint;
    case WebGPU::TextureFormat::R8unorm:
        return WGSL::TexelFormat::R8unorm;
    case WebGPU::TextureFormat::Rg11b10ufloat:
        return WGSL::TexelFormat::RG11B10ufloat;
    case WebGPU::TextureFormat::Rg16float:
        return WGSL::TexelFormat::RG16float;
    case WebGPU::TextureFormat::Rg16sint:
        return WGSL::TexelFormat::RG16sint;
    case WebGPU::TextureFormat::Rg16snorm:
        return WGSL::TexelFormat::RG16snorm;
    case WebGPU::TextureFormat::Rg16uint:
        return WGSL::TexelFormat::RG16uint;
    case WebGPU::TextureFormat::Rg16unorm:
        return WGSL::TexelFormat::RG16unorm;
    case WebGPU::TextureFormat::Rg32float:
        return WGSL::TexelFormat::RG32float;
    case WebGPU::TextureFormat::Rg32sint:
        return WGSL::TexelFormat::RG32sint;
    case WebGPU::TextureFormat::Rg32uint:
        return WGSL::TexelFormat::RG32uint;
    case WebGPU::TextureFormat::Rg8sint:
        return WGSL::TexelFormat::RG8sint;
    case WebGPU::TextureFormat::Rg8snorm:
        return WGSL::TexelFormat::RG8snorm;
    case WebGPU::TextureFormat::Rg8uint:
        return WGSL::TexelFormat::RG8uint;
    case WebGPU::TextureFormat::Rg8unorm:
        return WGSL::TexelFormat::RG8unorm;
    case WebGPU::TextureFormat::Rgb10a2uint:
        return WGSL::TexelFormat::RGB10A2uint;
    case WebGPU::TextureFormat::Rgb10a2unorm:
        return WGSL::TexelFormat::RGB10A2unorm;
    case WebGPU::TextureFormat::Rgba16float:
        return WGSL::TexelFormat::RGBA16float;
    case WebGPU::TextureFormat::Rgba16sint:
        return WGSL::TexelFormat::RGBA16sint;
    case WebGPU::TextureFormat::Rgba16snorm:
        return WGSL::TexelFormat::RGBA16snorm;
    case WebGPU::TextureFormat::Rgba16uint:
        return WGSL::TexelFormat::RGBA16uint;
    case WebGPU::TextureFormat::Rgba16unorm:
        return WGSL::TexelFormat::RGBA16unorm;
    case WebGPU::TextureFormat::Rgba32float:
        return WGSL::TexelFormat::RGBA32float;
    case WebGPU::TextureFormat::Rgba32sint:
        return WGSL::TexelFormat::RGBA32sint;
    case WebGPU::TextureFormat::Rgba32uint:
        return WGSL::TexelFormat::RGBA32uint;
    case WebGPU::TextureFormat::Rgba8sint:
        return WGSL::TexelFormat::RGBA8sint;
    case WebGPU::TextureFormat::Rgba8snorm:
        return WGSL::TexelFormat::RGBA8snorm;
    case WebGPU::TextureFormat::Rgba8uint:
        return WGSL::TexelFormat::RGBA8uint;
    case WebGPU::TextureFormat::Rgba8unorm:
        return WGSL::TexelFormat::RGBA8unorm;
    default:
        ASSERT_NOT_REACHED();
        return WGSL::TexelFormat::BGRA8unorm;
    }
}

static WGSL::BindGroupLayoutEntry::BindingMember convertBindingLayout(const BindGroupLayout::Entry::BindingLayout& bindingLayout)
{
    return WTF::switchOn(bindingLayout, [](const BindGroupLayout::BufferBindingLayout& bindingLayout) -> WGSL::BindGroupLayoutEntry::BindingMember {
        return WGSL::BufferBindingLayout {
            .type = wgslBindingType(bindingLayout.type),
            .hasDynamicOffset = !!bindingLayout.hasDynamicOffset,
            .minBindingSize = bindingLayout.minBindingSize
        };
    }, [](const BindGroupLayout::SamplerBindingLayout& bindingLayout) -> WGSL::BindGroupLayoutEntry::BindingMember {
        return WGSL::SamplerBindingLayout {
            .type = wgslSamplerType(bindingLayout.type)
        };
    }, [](const BindGroupLayout::TextureBindingLayout& bindingLayout) -> WGSL::BindGroupLayoutEntry::BindingMember {
        return WGSL::TextureBindingLayout {
            .sampleType = wgslSampleType(bindingLayout.sampleType),
            .viewDimension = wgslViewDimension(bindingLayout.viewDimension),
            .multisampled = !!bindingLayout.multisampled
        };
    }, [](const BindGroupLayout::StorageTextureBindingLayout& bindingLayout) -> WGSL::BindGroupLayoutEntry::BindingMember {
        return WGSL::StorageTextureBindingLayout {
            .access = wgslAccess(bindingLayout.access),
            .format = wgslFormat(bindingLayout.format),
            .viewDimension = wgslViewDimension(bindingLayout.viewDimension)
        };
    }, [](const BindGroupLayout::ExternalTextureBindingLayout&) -> WGSL::BindGroupLayoutEntry::BindingMember {
        return WGSL::ExternalTextureBindingLayout {
        };
    });
}

WGSL::PipelineLayout ShaderModule::convertPipelineLayout(const PipelineLayout& pipelineLayout)
{
    Vector<WGSL::BindGroupLayout> bindGroupLayouts;

    uint32_t maxVertexOffset = 0, maxFragmentOffset = 0, maxComputeOffset = 0;
    for (size_t i = 0; i < pipelineLayout.numberOfBindGroupLayouts(); ++i) {
        Ref bindGroupLayout = pipelineLayout.bindGroupLayout(i);
        WGSL::BindGroupLayout wgslBindGroupLayout;
        auto vertexOffset = maxVertexOffset, fragmentOffset = maxFragmentOffset, computeOffset = maxComputeOffset;
        for (auto& entry : bindGroupLayout->entries()) {
            WGSL::BindGroupLayoutEntry wgslEntry;
            wgslEntry.binding = entry.value.binding;
            wgslEntry.visibility = wgslEntry.visibility.fromRaw(entry.value.visibility.toRaw());
            wgslEntry.bindingMember = convertBindingLayout(entry.value.bindingLayout);
            wgslEntry.vertexArgumentBufferIndex = entry.value.argumentBufferIndices[WebGPU::Metal::ShaderStage::Vertex];
            wgslEntry.vertexArgumentBufferSizeIndex = entry.value.bufferSizeArgumentBufferIndices[WebGPU::Metal::ShaderStage::Vertex];
            if (entry.value.vertexDynamicOffset) {
                RELEASE_ASSERT(!(entry.value.vertexDynamicOffset.value() % sizeof(uint32_t)));
                wgslEntry.vertexBufferDynamicOffset = (vertexOffset + *entry.value.vertexDynamicOffset) / sizeof(uint32_t);
                maxVertexOffset = std::max<size_t>(maxVertexOffset, *entry.value.vertexDynamicOffset + sizeof(uint32_t) + vertexOffset);
            }
            wgslEntry.fragmentArgumentBufferIndex = entry.value.argumentBufferIndices[WebGPU::Metal::ShaderStage::Fragment];
            wgslEntry.fragmentArgumentBufferSizeIndex = entry.value.bufferSizeArgumentBufferIndices[WebGPU::Metal::ShaderStage::Fragment];
            if (entry.value.fragmentDynamicOffset) {
                RELEASE_ASSERT(!(entry.value.fragmentDynamicOffset.value() % sizeof(uint32_t)));
                wgslEntry.fragmentBufferDynamicOffset = (fragmentOffset + *entry.value.fragmentDynamicOffset) / sizeof(uint32_t) + RenderBundleEncoder::startIndexForFragmentDynamicOffsets;
                maxFragmentOffset = std::max<size_t>(maxFragmentOffset, *entry.value.fragmentDynamicOffset + sizeof(uint32_t) + fragmentOffset);
            }
            wgslEntry.computeArgumentBufferIndex = entry.value.argumentBufferIndices[WebGPU::Metal::ShaderStage::Compute];
            wgslEntry.computeArgumentBufferSizeIndex = entry.value.bufferSizeArgumentBufferIndices[WebGPU::Metal::ShaderStage::Compute];
            if (entry.value.computeDynamicOffset) {
                RELEASE_ASSERT(!(entry.value.computeDynamicOffset.value() % sizeof(uint32_t)));
                wgslEntry.computeBufferDynamicOffset = (computeOffset + *entry.value.computeDynamicOffset) / sizeof(uint32_t);
                maxComputeOffset = std::max<size_t>(maxComputeOffset, *entry.value.computeDynamicOffset + sizeof(uint32_t) + computeOffset);
            }
            wgslBindGroupLayout.entries.append(wgslEntry);
        }

        bindGroupLayouts.append(wgslBindGroupLayout);
    }

    return { .bindGroupLayouts = bindGroupLayouts };
}

ShaderModule::ASTHandle::ASTHandle(const ShaderModule& shaderModule, WGSL::ShaderModule& ast, AdoptIncrementedCountTag)
    : m_shaderModule(&shaderModule)
    , m_ast(&ast)
{
}

ShaderModule::ASTHandle::ASTHandle(ASTHandle&& other)
    : m_shaderModule(WTF::move(other.m_shaderModule))
    , m_ast(std::exchange(other.m_ast, nullptr))
{
}

auto ShaderModule::ASTHandle::operator=(ASTHandle&& other) -> ASTHandle&
{
    ASTHandle moved(WTF::move(other));
    std::swap(m_shaderModule, moved.m_shaderModule);
    std::swap(m_ast, moved.m_ast);
    return *this;
}

ShaderModule::ASTHandle::~ASTHandle()
{
    RefPtr shaderModule = std::exchange(m_shaderModule, nullptr);
    if (!shaderModule)
        return;

    std::unique_ptr<WGSL::ShaderModule> deadAST;
    {
        auto& cache = astCacheState();
        Locker locker { cache.lock };
        ASSERT(shaderModule->m_astHandleCount);
        if (!--shaderModule->m_astHandleCount && shaderModule->m_astEvicted && shaderModule->m_entryPointInformation.isEmpty())
            deadAST = WTF::move(shaderModule->m_ast);
    }
}

void ShaderModule::retainASTRecently() const
{
    if (!m_entryPointInformation.isEmpty())
        return;

    Vector<std::unique_ptr<WGSL::ShaderModule>, 8> evictedASTs;
    {
        auto& cache = astCacheState();
        Locker locker { cache.lock };
        if (!m_ast)
            return;
        m_astEvicted = false;
        cache.insertMRULocked(*this);
        cache.evictToBudgetLocked(maxRetainedASTBytes, evictedASTs);
    }
}

void ShaderModule::evictAST() const
{
    std::unique_ptr<WGSL::ShaderModule> deadAST;
    {
        auto& cache = astCacheState();
        Locker locker { cache.lock };
        cache.unlinkLocked(*this);
        m_astEvicted = true;
        if (!m_astHandleCount && m_entryPointInformation.isEmpty())
            deadAST = WTF::move(m_ast);
    }
}

void ShaderModule::purgeAllCachedASTs()
{
    Vector<std::unique_ptr<WGSL::ShaderModule>, 8> evictedASTs;
    {
        auto& cache = astCacheState();
        Locker locker { cache.lock };
        cache.evictToBudgetLocked(0, evictedASTs);
    }
}

auto ShaderModule::ast() const -> ASTHandle
{
    if (!isValid())
        return { };

    Vector<std::unique_ptr<WGSL::ShaderModule>, 8> evictedASTs;
    {
        auto& cache = astCacheState();
        Locker locker { cache.lock };
        if (m_ast) {
            ++m_astHandleCount;
            if (m_entryPointInformation.isEmpty()) {
                m_astEvicted = false;
                cache.insertMRULocked(*this);
                cache.evictToBudgetLocked(maxRetainedASTBytes, evictedASTs);
            }
            return { *this, *m_ast, ASTHandle::AdoptIncrementedCount };
        }
    }

    // The source passed this exact check when the module was created, so it passes again.
    auto checkResult = WGSL::staticCheck(m_source.isolatedCopy(), std::nullopt, *m_configuration);
    auto* successfulCheck = std::get_if<WGSL::SuccessfulCheck>(&checkResult);
    if (!successfulCheck) {
        ASSERT_NOT_REACHED();
        return { };
    }
    auto rebuiltAST = successfulCheck->ast.moveToUniquePtr();
    size_t footprint = rebuiltAST->memoryFootprint();

    ASTHandle handle;
    {
        auto& cache = astCacheState();
        Locker locker { cache.lock };
        if (!m_ast) {
            cache.unlinkLocked(*this);
            m_ast = WTF::move(rebuiltAST);
            m_astFootprint = footprint;
        }
        ++m_astHandleCount;
        if (m_entryPointInformation.isEmpty()) {
            m_astEvicted = false;
            cache.insertMRULocked(*this);
            cache.evictToBudgetLocked(maxRetainedASTBytes, evictedASTs);
        }
        handle = ASTHandle { *this, *m_ast, ASTHandle::AdoptIncrementedCount };
    }
    return handle;
}

const PipelineLayout* ShaderModule::pipelineLayoutHint(const String& name) const
{
    auto iterator = m_pipelineLayoutHints.find(name);
    if (iterator == m_pipelineLayoutHints.end())
        return nullptr;
    return iterator->value.ptr();
}

const WGSL::Reflection::EntryPointInformation* ShaderModule::entryPointInformation(const String& name) const
{
    auto iterator = m_entryPointInformation.find(name);
    if (iterator != m_entryPointInformation.end())
        return &iterator->value;

    return nullptr;
}

SUPPRESS_NODELETE const String& ShaderModule::defaultVertexEntryPoint() const
{
    ensureCompiled();
    return m_defaultVertexEntryPoint;
}

SUPPRESS_NODELETE const String& ShaderModule::defaultFragmentEntryPoint() const
{
    ensureCompiled();
    return m_defaultFragmentEntryPoint;
}

SUPPRESS_NODELETE const String& ShaderModule::defaultComputeEntryPoint() const
{
    ensureCompiled();
    return m_defaultComputeEntryPoint;
}

} // namespace WebGPU::Metal
