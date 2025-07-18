/*
 * Copyright (c) 2022 Apple Inc. All rights reserved.
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

#pragma once

#include "WGSLShaderModule.h"
#include <JavaScriptCore/RegularExpression.h>
#include <wtf/FileSystem.h>
#include <wtf/text/MakeString.h>

#ifdef __OBJC__
#import <Metal/Metal.h>
#endif

#define EXPECT_SHADER(shader) \
    do { \
        if (!shader.has_value()) { \
            ::TestWGSLAPI::logCompilationError(shader.error()); \
            return; \
        } \
    } while (false)

namespace WGSL {
struct FailedCheck;
}

namespace TestWGSLAPI {

void logCompilationError(WGSL::FailedCheck& error);

inline String fn(const String& test)
{
    return makeString("@compute @workgroup_size(1) fn test() {\n"_s, test, "\n}"_s);
}

inline String enableF16(const String& test)
{
    return makeString("enable f16;\n\n"_s, test);
}

#ifdef __OBJC__
inline String file(const String& filePath)
{
    static NSString *shadersPath = nil;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [&] {
        shadersPath = [NSBundle.mainBundle.resourcePath stringByAppendingPathComponent:@"shaders"];
    });

    auto cfFilePath = filePath.createCFString();
    EXPECT_TRUE(cfFilePath);
    auto path = [shadersPath stringByAppendingPathComponent:(__bridge NSString *)cfFilePath.get()];
    auto readResult = FileSystem::readEntireFile(path);
    EXPECT_TRUE(readResult.has_value());
    return String::fromUTF8WithLatin1Fallback(readResult->span());
}
#endif

using Check = Function<unsigned(const String&, unsigned offset)>;

inline Check checkLiteral(const String& pattern)
{
    return [&](const String& msl, unsigned offset) -> unsigned {
        auto result = msl.find(pattern, offset);
        EXPECT_TRUE(result != notFound);
        return result;
    };
}

inline Check checkNot(const String& pattern)
{
    return [&](const String& msl, unsigned offset) -> unsigned {
        JSC::Yarr::RegularExpression test(pattern);
        auto result = test.match(msl, offset);
        EXPECT_EQ(result, -1);
        return offset;
    };
}

inline Check check(const String& pattern)
{
    return [&](const String& msl, unsigned offset) -> unsigned {
        JSC::Yarr::RegularExpression test(pattern);
        auto result = test.match(msl, offset);
        EXPECT_NE(result, -1);
        return result;
    };
}

inline Variant<WGSL::SuccessfulCheck, WGSL::FailedCheck> staticCheck(const String& wgsl)
{
    return WGSL::staticCheck(wgsl, std::nullopt, { 8 });
}

inline Variant<WGSL::PrepareResult, WGSL::Error> prepare(const WGSL::SuccessfulCheck& staticCheckResult)
{
    auto& shaderModule = staticCheckResult.ast;
    HashMap<String, WGSL::PipelineLayout*> pipelineLayouts;
    for (auto& entryPoint : shaderModule->callGraph().entrypoints())
        pipelineLayouts.add(entryPoint.originalName, nullptr);
    return WGSL::prepare(shaderModule, pipelineLayouts);
}

inline Variant<String, WGSL::Error> generate(const WGSL::SuccessfulCheck& staticCheckResult, WGSL::PrepareResult& prepareResult)
{
    auto& shaderModule = staticCheckResult.ast;
    HashMap<String, WGSL::ConstantValue> constantValues;
    for (auto& entryPoint : shaderModule->callGraph().entrypoints()) {
        const auto& entryPointInformation = prepareResult.entryPoints.get(entryPoint.originalName);
        for (const auto& [originalName, constant] : entryPointInformation.specializationConstants) {
            EXPECT_TRUE(constant.defaultValue);
            auto defaultValue = WGSL::evaluate(*constant.defaultValue, constantValues);
            EXPECT_TRUE(defaultValue.has_value());
            constantValues.add(constant.mangledName, *defaultValue);
        }
    }

    return WGSL::generate(shaderModule, prepareResult, constantValues, { });
}

#ifdef __OBJC__
inline Variant<id<MTLLibrary>, NSError *> metalCompile(const String& msl)
{
    auto device = MTLCreateSystemDefaultDevice();
    auto options = [MTLCompileOptions new];
    options.preprocessorMacros = @{ @"__wgslMetalAppleGPUFamily" : [NSString stringWithFormat:@"%u", 8] };
    NSError *error = nil;
    id<MTLLibrary> library = [device newLibraryWithSource:msl.createNSString().get() options:options error:&error];
    if (error != nil)
        return { error };
    return { library };
}
#endif

template<typename... Checks>
inline void performChecks(const String& input, Checks&&... checks)
{
    unsigned offset = 0;
    for (const auto& check : std::initializer_list<Check> { std::forward<Checks>(checks)... })
        offset = check(input, offset);
}

} // namespace TestWGSLAPI
