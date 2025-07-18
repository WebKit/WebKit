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

#import "config.h"

#import "AST.h"
#import "Lexer.h"
#import "Parser.h"
#import "TestWGSLAPI.h"
#import <JavaScriptCore/InitializeThreading.h>
#import <wtf/Assertions.h>

namespace TestWGSLAPI {

template<typename... Checks>
static void testCompilation(const String& wgsl, Checks&&... checks)
{
    auto staticCheckResult = staticCheck(wgsl);
    EXPECT_TRUE(std::holds_alternative<WGSL::SuccessfulCheck>(staticCheckResult));
    auto& successfulCheck = std::get<WGSL::SuccessfulCheck>(staticCheckResult);

    auto maybePrepareResult = prepare(successfulCheck);
    EXPECT_TRUE(std::holds_alternative<WGSL::PrepareResult>(maybePrepareResult));
    auto& prepareResult = std::get<WGSL::PrepareResult>(maybePrepareResult);

    auto generationResult = generate(successfulCheck, prepareResult);
    EXPECT_TRUE(std::holds_alternative<String>(generationResult));
    auto msl = std::get<String>(generationResult);

    auto metalCompilationResult = metalCompile(msl);
    EXPECT_TRUE(std::holds_alternative<id<MTLLibrary>>(metalCompilationResult));

    auto library = std::get<id<MTLLibrary>>(metalCompilationResult);
    EXPECT_TRUE(library != nil);

    performChecks(std::get<String>(generationResult), std::forward<Checks>(checks)...);
}

static void expectPrepareError(const String& wgsl, const String& errorMessage)
{
    auto staticCheckResult = staticCheck(wgsl);
    EXPECT_TRUE(std::holds_alternative<WGSL::SuccessfulCheck>(staticCheckResult));
    auto& successfulCheck = std::get<WGSL::SuccessfulCheck>(staticCheckResult);

    auto maybePrepareResult = prepare(successfulCheck);
    EXPECT_TRUE(std::holds_alternative<WGSL::Error>(maybePrepareResult));
    auto error = std::get<WGSL::Error>(maybePrepareResult);
    EXPECT_EQ(error.message(), errorMessage);
}

static void expectGenerateError(const String& wgsl, const String& errorMessage)
{
    auto staticCheckResult = staticCheck(wgsl);
    EXPECT_TRUE(std::holds_alternative<WGSL::SuccessfulCheck>(staticCheckResult));
    auto& successfulCheck = std::get<WGSL::SuccessfulCheck>(staticCheckResult);

    auto maybePrepareResult = prepare(successfulCheck);
    EXPECT_TRUE(std::holds_alternative<WGSL::PrepareResult>(maybePrepareResult));
    auto& prepareResult = std::get<WGSL::PrepareResult>(maybePrepareResult);

    auto generationResult = generate(successfulCheck, prepareResult);
    EXPECT_TRUE(std::holds_alternative<WGSL::Error>(generationResult));

    auto error = std::get<WGSL::Error>(generationResult);
    EXPECT_EQ(error.message(), errorMessage);
}

class WGSLMetalCompilationTests : public testing::Test {
protected:
    static void SetUpTestSuite()
    {
        JSC::initialize();
    }
};

TEST_F(WGSLMetalCompilationTests, Empty)
{
    testCompilation(""_s);
}

TEST_F(WGSLMetalCompilationTests, ReturnTypePromotion)
{
    testCompilation(
        "fn testScalarPromotion() -> f32"
        "{"
        "    return 0;"
        "}"

        "fn testVectorPromotion() -> vec3f"
        "{"
        "    return vec3(0);"
        "}"

        "@compute"
        "@workgroup_size(1, 1, 1)"
        "fn main() {"
        "    _ = testScalarPromotion();"
        "    _ = testVectorPromotion();"
        "}"_s);
}

TEST_F(WGSLMetalCompilationTests, ConstantFunctionNotInOutput)
{
    testCompilation(
        "@compute @workgroup_size(1)"
        "fn main()"
        "{"
        "  const a = pow(2, 2);"
        "  _ = a;"
        "}"_s,
        checkNot("pow\\(.*\\)"_s),
        checkNot("float .* = \\d"_s),
        checkLiteral("(void)(4.)"_s));

}

TEST_F(WGSLMetalCompilationTests, BuiltinAliases)
{
    testCompilation(file("aliases.wgsl"_s));
}

TEST_F(WGSLMetalCompilationTests, AccessExpression)
{
    testCompilation(file("access-expression.wgsl"_s));
}

TEST_F(WGSLMetalCompilationTests, ArrayAliasConstructor)
{
    testCompilation(file("array-alias-constructor.wgsl"_s));
}

TEST_F(WGSLMetalCompilationTests, ArrayCountExpression)
{
    testCompilation(file("array-count-expression.wgsl"_s));
}

TEST_F(WGSLMetalCompilationTests, ArrayLengthPointer)
{
    testCompilation(file("array-length-pointer.wgsl"_s));
}

TEST_F(WGSLMetalCompilationTests, ArrayLengthPointer2)
{
    testCompilation(file("array-length-pointer2.wgsl"_s));
}

TEST_F(WGSLMetalCompilationTests, ArrayLengthUnorderd)
{
    testCompilation(file("array-length-unordered.wgsl"_s));
}

TEST_F(WGSLMetalCompilationTests, ArrayLength)
{
    testCompilation(file("array-length.wgsl"_s));
}

TEST_F(WGSLMetalCompilationTests, ArrayPrimitiveStruct)
{
    testCompilation(file("array-primitive-struct.wgsl"_s));
}

TEST_F(WGSLMetalCompilationTests, ArrayVec3)
{
    testCompilation(file("array-vec3.wgsl"_s));
}

TEST_F(WGSLMetalCompilationTests, Atomics)
{
    testCompilation(file("atomics.wgsl"_s));
}

TEST_F(WGSLMetalCompilationTests, Attributes)
{
    testCompilation(file("attributes.wgsl"_s));
}

TEST_F(WGSLMetalCompilationTests, BindingUIntMax)
{
    testCompilation(file("binding-uint-max.wgsl"_s));
}
TEST_F(WGSLMetalCompilationTests, Concretization)
{
    testCompilation(file("binding-uint-max.wgsl"_s));
}

TEST_F(WGSLMetalCompilationTests, ConstAssert)
{
    testCompilation(file("const-assert.wgsl"_s));
}

TEST_F(WGSLMetalCompilationTests, ConstantsUtf16)
{
    testCompilation(file("constants-utf16.wgsl"_s));
}

TEST_F(WGSLMetalCompilationTests, ConstantsUtf8)
{
    testCompilation(file("constants-utf8.wgsl"_s));
}

TEST_F(WGSLMetalCompilationTests, Division)
{
    testCompilation(file("division.wgsl"_s));
}

TEST_F(WGSLMetalCompilationTests, For)
{
    testCompilation(file("for.wgsl"_s));
}

TEST_F(WGSLMetalCompilationTests, FragmentOutput)
{
    testCompilation(file("fragment-output.wgsl"_s));
}

TEST_F(WGSLMetalCompilationTests, FuzzerTests)
{
    testCompilation(file("fuzz-127229681.wgsl"_s));
    testCompilation(file("fuzz-128785160.wgsl"_s));
    testCompilation(file("fuzz-130082002.wgsl"_s));
    testCompilation(file("fuzz-130088292.wgsl"_s));
    testCompilation(file("fuzz-130092499.wgsl"_s));
}

TEST_F(WGSLMetalCompilationTests, GlobalConstantVector)
{
    testCompilation(file("global-constant-vector.wgsl"_s),
        check("vec<float, 4> local0 = vec<float, 4>\\(0\\., 0\\., 0\\., 0\\.\\)"_s),
        check("\\(void\\)\\(local\\d+\\[__wgslMin\\(unsigned\\(0\\), \\(4u - 1u\\)\\)\\]"_s));
}

TEST_F(WGSLMetalCompilationTests, GlobalOrdering)
{
    testCompilation(file("global-ordering.wgsl"_s));
}

TEST_F(WGSLMetalCompilationTests, GlobalSameBinding)
{
    testCompilation(file("global-same-binding.wgsl"_s));
}

TEST_F(WGSLMetalCompilationTests, HexDoubleLChar)
{
    testCompilation(file("hex-double-lchar.wgsl"_s),
        checkLiteral("1.1754943508222875e-38"_s),
        checkLiteral("3.4028234663852886e+38"_s));
}

TEST_F(WGSLMetalCompilationTests, HexDoubleUChar)
{
    testCompilation(file("hex-double-uchar.wgsl"_s),
        checkLiteral("1.1754943508222875e-38"_s),
        checkLiteral("3.4028234663852886e+38"_s));
}

TEST_F(WGSLMetalCompilationTests, If)
{
    testCompilation(file("if.wgsl"_s));
}

TEST_F(WGSLMetalCompilationTests, LargeStruct)
{
    expectPrepareError(file("large-struct.wgsl"_s), "The combined byte size of all variables in this function exceeds 8192 bytes"_s);
}

TEST_F(WGSLMetalCompilationTests, Limits)
{
    expectPrepareError(file("limits-function-vars.wgsl"_s), "The combined byte size of all variables in this function exceeds 8192 bytes"_s);

    // These can depend on overrides, so they are only checked at generation time
    expectGenerateError(file("limits-private-vars.wgsl"_s), "The combined byte size of all variables in the private address space exceeds 8192 bytes"_s);
    expectGenerateError(file("limits-workgroup-vars.wgsl"_s), "The combined byte size of all variables in the workgroup address space exceeds 16384 bytes"_s);
}

TEST_F(WGSLMetalCompilationTests, LocalConstantVector)
{
    testCompilation("@fragment fn main() { const a = vec2(0); _ = a; }"_s, checkLiteral("(void)(vec<int, 2>(0, 0))"_s));
}

TEST_F(WGSLMetalCompilationTests, Loop)
{
    testCompilation(file("loop.wgsl"_s));
}

TEST_F(WGSLMetalCompilationTests, MinusMinusAmbiguity)
{
    testCompilation(file("minus-minus-ambiguity.wgsl"_s));
}

TEST_F(WGSLMetalCompilationTests, Overload)
{
    testCompilation(file("overload.wgsl"_s));
}

TEST_F(WGSLMetalCompilationTests, Override)
{
    expectGenerateError(file("override.wgsl"_s), "array count must be greater than 0"_s);
}

TEST_F(WGSLMetalCompilationTests, PackUnpack)
{
    testCompilation(file("pack-unpack.wgsl"_s),
        check("1., 0.50\\d*, -0.49\\d*, -1."_s),
        check("1., 0.50\\d*, 0.25\\d*, 0."_s),
        check("-128, 127, -128, 127"_s),
        check("0u, 255u, 128u, 64u"_s),
        check("127, 127, -128, -128"_s),
        check("255u, 255u, 128u, 64u"_s),
        check("0.50\\d*, -0.49\\d*"_s),
        check("0.50\\d*, 0."_s),
        check("0.5, -0.5"_s));
}

TEST_F(WGSLMetalCompilationTests, PackingNestedArray)
{
    testCompilation(file("packing-nested-array.wgsl"_s));
}

TEST_F(WGSLMetalCompilationTests, PackingPointerArguments)
{
    testCompilation(file("packing-pointer-arguments.wgsl"_s));
}

TEST_F(WGSLMetalCompilationTests, PointerRewritingConcreteType)
{
    testCompilation(fn("var h = vec2f(); _ = length(*&h);"_s),
        checkLiteral("length(local0)"_s));
}

TEST_F(WGSLMetalCompilationTests, Pointers)
{
    testCompilation(file("pointers.wgsl"_s));
}

TEST_F(WGSLMetalCompilationTests, References)
{
    testCompilation(file("references.wgsl"_s));
}

TEST_F(WGSLMetalCompilationTests, Reordering)
{
    testCompilation(file("references.wgsl"_s));
}

TEST_F(WGSLMetalCompilationTests, RuntimeSizedArray)
{
    testCompilation(file("runtime-sized-array-resource.wgsl"_s));
}

TEST_F(WGSLMetalCompilationTests, Scope)
{
    testCompilation(file("scope.wgsl"_s));
}

TEST_F(WGSLMetalCompilationTests, Shadowing)
{
    testCompilation(file("shadowing.wgsl"_s));
}

TEST_F(WGSLMetalCompilationTests, Struct)
{
    testCompilation(file("struct.wgsl"_s));
}

TEST_F(WGSLMetalCompilationTests, Switch)
{
    testCompilation(file("switch.wgsl"_s));
}

TEST_F(WGSLMetalCompilationTests, Swizzle)
{
    testCompilation(file("swizzle.wgsl"_s));
}

TEST_F(WGSLMetalCompilationTests, TextureExternal)
{
    testCompilation(file("texture-external.wgsl"_s));
}

TEST_F(WGSLMetalCompilationTests, TypePromotion)
{
    testCompilation(file("type-promotion.wgsl"_s));
}

TEST_F(WGSLMetalCompilationTests, While)
{
    testCompilation(file("while.wgsl"_s));
}

}
