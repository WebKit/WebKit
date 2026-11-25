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
static void testCompilation(StringImpl* wgsl, Checks&&... checks)
{
    auto staticCheckResult = staticCheck(wgsl);
    EXPECT_TRUE(std::holds_alternative<WGSL::SuccessfulCheck>(staticCheckResult));
    auto& successfulCheck = std::get<WGSL::SuccessfulCheck>(staticCheckResult);

    auto maybePrepareResult = prepare(successfulCheck);
    EXPECT_TRUE(std::holds_alternative<WGSL::PrepareResult>(maybePrepareResult));
    auto& prepareResult = std::get<WGSL::PrepareResult>(maybePrepareResult);

    auto generationResult = generate(successfulCheck, prepareResult);
    EXPECT_TRUE(std::holds_alternative<RefPtr<StringImpl>>(generationResult));
    auto msl = std::get<RefPtr<StringImpl>>(generationResult);

    auto metalCompilationResult = metalCompile(msl);
    EXPECT_TRUE(std::holds_alternative<id<MTLLibrary>>(metalCompilationResult));

    auto library = std::get<id<MTLLibrary>>(metalCompilationResult);
    EXPECT_TRUE(library != nil);

    performChecks(std::get<RefPtr<StringImpl>>(generationResult), std::forward<Checks>(checks)...);
}

template<typename... Checks>
static void testCompilationForAppleGPUFamily(unsigned appleGPUFamily, StringImpl* wgsl, Checks&&... checks)
{
    auto staticCheckResult = staticCheck(wgsl);
    EXPECT_TRUE(std::holds_alternative<WGSL::SuccessfulCheck>(staticCheckResult));
    auto& successfulCheck = std::get<WGSL::SuccessfulCheck>(staticCheckResult);

    auto maybePrepareResult = prepare(successfulCheck);
    EXPECT_TRUE(std::holds_alternative<WGSL::PrepareResult>(maybePrepareResult));
    auto& prepareResult = std::get<WGSL::PrepareResult>(maybePrepareResult);

    auto generationResult = generate(successfulCheck, prepareResult, WGSL::DeviceState { .appleGPUFamily = appleGPUFamily });
    EXPECT_TRUE(std::holds_alternative<RefPtr<StringImpl>>(generationResult));
    auto msl = std::get<RefPtr<StringImpl>>(generationResult);

    auto metalCompilationResult = metalCompile(msl, appleGPUFamily);
    EXPECT_TRUE(std::holds_alternative<id<MTLLibrary>>(metalCompilationResult));

    auto library = std::get<id<MTLLibrary>>(metalCompilationResult);
    EXPECT_TRUE(library != nil);

    performChecks(std::get<RefPtr<StringImpl>>(generationResult), std::forward<Checks>(checks)...);
}

static void expectPrepareError(StringImpl* wgsl, StringImpl* errorMessage)
{
    auto staticCheckResult = staticCheck(wgsl);
    EXPECT_TRUE(std::holds_alternative<WGSL::SuccessfulCheck>(staticCheckResult));
    auto& successfulCheck = std::get<WGSL::SuccessfulCheck>(staticCheckResult);

    auto maybePrepareResult = prepare(successfulCheck);
    EXPECT_TRUE(std::holds_alternative<WGSL::Error>(maybePrepareResult));
    auto error = std::get<WGSL::Error>(maybePrepareResult);
    EXPECT_EQ(error.message(), errorMessage);
}

static void expectGenerateError(StringImpl* wgsl, StringImpl* errorMessage)
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

static void expectNoGenerateError(StringImpl* wgsl)
{
    auto staticCheckResult = staticCheck(wgsl);
    EXPECT_TRUE(std::holds_alternative<WGSL::SuccessfulCheck>(staticCheckResult));
    auto& successfulCheck = std::get<WGSL::SuccessfulCheck>(staticCheckResult);

    auto maybePrepareResult = prepare(successfulCheck);
    EXPECT_TRUE(std::holds_alternative<WGSL::PrepareResult>(maybePrepareResult));
    auto& prepareResult = std::get<WGSL::PrepareResult>(maybePrepareResult);

    auto generationResult = generate(successfulCheck, prepareResult);
    EXPECT_TRUE(std::holds_alternative<RefPtr<StringImpl>>(generationResult));
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
    testCompilation(""_si);
}

TEST_F(WGSLMetalCompilationTests, ReturnTypePromotion)
{
    testCompilation(
        StringImpl::create("fn testScalarPromotion() -> f32"
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
        "}"_s).ptr());
}

TEST_F(WGSLMetalCompilationTests, ConstantFunctionNotInOutput)
{
    testCompilation(
        StringImpl::create("@compute @workgroup_size(1)"
        "fn main()"
        "{"
        "  const a = pow(2, 2);"
        "  _ = a;"
        "}"_s).ptr(),
        checkNot("pow\\(.*\\)"_si),
        checkNot("float .* = \\d"_si),
        checkLiteral("(void)(4.)"_si));

}

TEST_F(WGSLMetalCompilationTests, BuiltinAliases)
{
    testCompilation(file("aliases.wgsl"_si));
}

TEST_F(WGSLMetalCompilationTests, AccessExpression)
{
    testCompilation(file("access-expression.wgsl"_si));
}

TEST_F(WGSLMetalCompilationTests, ArrayAliasConstructor)
{
    testCompilation(file("array-alias-constructor.wgsl"_si));
}

TEST_F(WGSLMetalCompilationTests, ArrayCountExpression)
{
    testCompilation(file("array-count-expression.wgsl"_si));
}

TEST_F(WGSLMetalCompilationTests, ArrayLengthPointer)
{
    testCompilation(file("array-length-pointer.wgsl"_si));
}

TEST_F(WGSLMetalCompilationTests, ArrayLengthPointer2)
{
    testCompilation(file("array-length-pointer2.wgsl"_si));
}

TEST_F(WGSLMetalCompilationTests, ArrayLengthUnorderd)
{
    testCompilation(file("array-length-unordered.wgsl"_si));
}

TEST_F(WGSLMetalCompilationTests, ArrayLength)
{
    testCompilation(file("array-length.wgsl"_si));
}

TEST_F(WGSLMetalCompilationTests, ArrayPrimitiveStruct)
{
    testCompilation(file("array-primitive-struct.wgsl"_si));
}

TEST_F(WGSLMetalCompilationTests, ArrayVec3)
{
    testCompilation(file("array-vec3.wgsl"_si));
}

TEST_F(WGSLMetalCompilationTests, Atomics)
{
    testCompilation(file("atomics.wgsl"_si));
}

TEST_F(WGSLMetalCompilationTests, Attributes)
{
    testCompilation(file("attributes.wgsl"_si));

    testCompilation(StringImpl::create("struct S { @builtin(position) @invariant value : vec4f };"
        "@vertex fn main() -> S { return S(vec4<f32>()); }"_s).ptr(),
        checkLiteral("[[invariant]]"_si));

    testCompilation("@vertex fn main() -> @invariant @builtin(position) vec4<f32> { return vec4<f32>(); }"_si,
        checkLiteral("[[invariant]]"_si));
}

TEST_F(WGSLMetalCompilationTests, BindingUIntMax)
{
    testCompilation(file("binding-uint-max.wgsl"_si));
}
TEST_F(WGSLMetalCompilationTests, Concretization)
{
    // Test variable inialization
    testCompilation(fn(
        StringImpl::create("let x1: u32 = 0;"
        "let x2: vec2<u32> = vec2(0, 0);"
        "let x3: f32 = 0;"
        "let x4: f32 = 0.0;"
        "var v1 = vec2(0.0);"
        "v1 = vec2f(0);"_s).ptr()));

    // Test concretization of arguments
    testCompilation(fn("let x = 0u + vec2(0, 0);"_si), checkLiteral("= vec<unsigned, 2>(0u, 0u);"_si));
    testCompilation(fn("let x = 0i + vec2(0, 0);"_si), checkLiteral("= vec<int, 2>(0, 0);"_si));
    testCompilation(fn("let x = 0f + vec2(0, 0);"_si), checkLiteral("= vec<float, 2>(0., 0.);"_si));

    // Test array concretization
    testCompilation(fn("let x = array<vec2<i32>, 1>(vec2(0, 0));"_si), checkLiteral("vec<int, 2>(0, 0)"_si));
    testCompilation(fn("let x = array<vec2<f32>, 1>(vec2(0, 0));"_si), checkLiteral("vec<float, 2>(0., 0.)"_si));
    testCompilation(fn("let x = array(vec2(0, 0), vec2(0.0, 0.0));"_si),
        checkLiteral("vec<float, 2>(0., 0.),"_si),
        checkLiteral("vec<float, 2>(0., 0.),"_si));
    testCompilation(fn("let x = array(vec2(0, 0), vec2(0u, 0u));"_si),
        checkLiteral("vec<unsigned, 2>(0u, 0u),"_si),
        checkLiteral("vec<unsigned, 2>(0u, 0u),"_si));

    // Test initializer concretization
    testCompilation(fn("let x = vec2(0, 0);"_si), checkLiteral(" = vec<int, 2>(0, 0);"_si));
    testCompilation(fn("let x : vec2<u32> = vec2(0, 0);"_si), checkLiteral(" = vec<unsigned, 2>(0u, 0u);"_si));

    // Test atomic materialization
    testCompilation(
        StringImpl::create("@group(0) @binding(0) var<storage, read_write> a: array<atomic<i32>>;"
        "@compute @workgroup_size(1)"
        "fn testArrayAccessMaterialization()"
        "{"
        "    let i = 0;"
        "    let x = atomicLoad(&a[i]);"
        "}"_s).ptr());
}

TEST_F(WGSLMetalCompilationTests, ConstAssert)
{
    testCompilation(file("const-assert.wgsl"_si));
}

TEST_F(WGSLMetalCompilationTests, ConstantMatrix)
{
    testCompilation(fn(
        StringImpl::create("let x = determinant(mat4x4("
        "    15,2,34,4,"
        "    18,2,3,4,"
        "    1,72,32,4,"
        "    17,2,3,4,"
        "));"_s).ptr()),
        checkLiteral("8680"_si));

    testCompilation(fn(
        StringImpl::create("let y = determinant(mat4x4("
        "    vec4(15,2,34,4),"
        "    vec4(18,2,3,4),"
        "    vec4(1,72,32,4),"
        "    vec4(17,2,3,4),"
        "));"_s).ptr()),
        checkLiteral("8680"_si));


    testCompilation(fn(
        StringImpl::create("const m2 = mat2x2("
        "  1,2,"
        "  3,4,"
        ");"
        "let tm2 = transpose(m2);"_s).ptr()),
        checkLiteral("1., 3., 2., 4."_si));

    testCompilation(fn(
        StringImpl::create("let tm3 = transpose(mat3x3("
        "    1,2,3,"
        "    4,5,6,"
        "    7,8,9,"
        "));"_s).ptr()),
        checkLiteral("1., 4., 7., 2., 5., 8., 3., 6., 9."_si));

    testCompilation(fn(
        StringImpl::create("const m2 = mat2x2("
        "  1,2,"
        "  3,4,"
        ");"
        "let x0 = m2 * 2;"_s).ptr()),
        checkLiteral("2., 4., 6., 8."_si));

    testCompilation(fn(
        StringImpl::create("const m2x3 = mat2x3("
        "    1,2,3,"
        "    4,5,6"
        ");"
        "let x1 = transpose(m2x3);"_s).ptr()),
        checkLiteral("1., 4., 2., 5., 3., 6."_si));

    testCompilation(fn(
        StringImpl::create("const m2x3 = mat2x3("
        "    1,2,3,"
        "    4,5,6"
        ");"
        "let x2 = m2x3 * vec2(2);"_s).ptr()),
        checkLiteral("10., 14., 18."_si));

    testCompilation(fn(
        StringImpl::create("const m2x3 = mat2x3("
        "    1,2,3,"
        "    4,5,6"
        ");"
        "let x3 = vec3(2) * m2x3;"_s).ptr()),
        checkLiteral("12., 30."_si));

    testCompilation(fn(
        "let x4 = dot(vec3(1,2,3), vec3(4,5,6));"_si),
        checkLiteral("32"_si));

    testCompilation(fn(
        "let x5 = mat3x2(1,2,3,4,5,6) * mat4x3(12,11,10,9,8,7,6,5,4,3,2,1);"_si),
        checkLiteral("95., 128., 68., 92., 41., 56., 14., 20."_si));
}

TEST_F(WGSLMetalCompilationTests, ConstantsUtf16)
{
    testCompilation(file("constants-utf16.wgsl"_si));
}

TEST_F(WGSLMetalCompilationTests, ConstantsUtf8)
{
    testCompilation(file("constants-utf8.wgsl"_si));
}

TEST_F(WGSLMetalCompilationTests, Division)
{
    testCompilation(file("division.wgsl"_si));
}

TEST_F(WGSLMetalCompilationTests, For)
{
    testCompilation(file("for.wgsl"_si));
}

TEST_F(WGSLMetalCompilationTests, FragmentOutput)
{
    testCompilation(file("fragment-output.wgsl"_si));
}

TEST_F(WGSLMetalCompilationTests, FuzzerTests)
{
    testCompilation(file("fuzz-127229681.wgsl"_si));
    testCompilation(file("fuzz-128785160.wgsl"_si));
    testCompilation(file("fuzz-130082002.wgsl"_si));
    testCompilation(file("fuzz-130088292.wgsl"_si));
    testCompilation(file("fuzz-130092499.wgsl"_si));
    testCompilation(file("fuzz-166715941.wgsl"_si));
}

TEST_F(WGSLMetalCompilationTests, GlobalConstantVector)
{
    testCompilation(file("global-constant-vector.wgsl"_si),
        check("vec<float, 4> local0 = vec<float, 4>\\(0\\., 0\\., 0\\., 0\\.\\)"_si),
        check("\\(void\\)\\(local\\d+\\[__wgslMin\\(unsigned\\(0\\), \\(4u - 1u\\)\\)\\]"_si));
}

TEST_F(WGSLMetalCompilationTests, GlobalOrdering)
{
    testCompilation(file("global-ordering.wgsl"_si));
}

TEST_F(WGSLMetalCompilationTests, GlobalSameBinding)
{
    testCompilation(file("global-same-binding.wgsl"_si));
}

TEST_F(WGSLMetalCompilationTests, InlineLargeAggregateReturns)
{
    auto shader = makeString(
        "struct Big { v : array<vec4<u32>, 8> }"
        "struct Small { v : array<vec4<u32>, 2> }"
        "@group(0) @binding(0) var<storage, read> input : array<Big>;"
        "@group(0) @binding(1) var<storage, read_write> output : array<Big>;"
        "fn combineBig(a : Big, b : Big) -> Big { var r = a; for (var i = 0u; i < 8u; i++) { r.v[i] += b.v[i]; } return r; }"
        "fn combineSmall(a : Small, b : Small) -> Small { var r = a; for (var i = 0u; i < 2u; i++) { r.v[i] += b.v[i]; } return r; }"
        "fn sumBig(a : Big) -> u32 { var r = 0u; for (var i = 0u; i < 8u; i++) { r += a.v[i].x; } return r; }"_s,
        fn(StringImpl::create("var acc = input[0];"
            "var small = Small(array<vec4<u32>, 2>(input[0].v[0], input[0].v[1]));"
            "for (var i = 1u; i < 4u; i++) { acc = combineBig(acc, input[i]); small = combineSmall(small, small); }"
            "acc.v[0].x += sumBig(acc) + small.v[0].x;"
            "output[0] = acc;"_s).ptr()));

    testCompilationForAppleGPUFamily(8, shader,
        checkNotLiteral("__attribute__((always_inline)) type0 function0"_si));

    testCompilationForAppleGPUFamily(9, shader,
        checkLiteral("__attribute__((always_inline)) type0 function0"_si),
        checkNotLiteral("__attribute__((always_inline)) type1 function1"_si),
        checkNotLiteral("__attribute__((always_inline)) unsigned function2"_si));
}

TEST_F(WGSLMetalCompilationTests, GlobalUsedByCallee)
{
    testCompilation(makeString(
        "var<private> x: i32;"_s,
        fn("_ = x;"_si)),
        check("int global\\d"_si));

    testCompilation(makeString(
        "var<private> y: i32;"
        "fn f() -> i32 { _ = y; return 0; }"_s,
        fn("_ = f();"_si)),
        check("int function\\d\\(thread int& global\\d\\)"_si),
        check("global\\d\\)"_si),
        check("return 0;"_si),
        check("function\\d\\(global\\d\\)"_si));

    testCompilation(makeString(
        "var<private> y: i32;"
        "fn g() -> i32 { let y = 42; return y; }"_s,
        fn("_ = g();"_si)),
        check("int function\\d\\(\\)"_si),
        checkNot("global\\d\\)"_si),
        check("return local\\d;"_si),
        check("function\\d\\(\\)"_si));

    testCompilation(makeString(
        "var<private> y: i32;"
        "override z = 42;"
        "fn h() -> i32 { _ = y; _ = z; return 0; }"_s,
        fn("_ = h();"_si)),
        check("int function\\d\\(thread int& global\\d, int global\\d\\)"_si),
        check("global\\d\\)"_si),
        check("return 0;"_si),
        check("function\\d\\(global\\d, \\(42\\)\\)"_si));

    testCompilation(makeString(
        "var<private> y: i32;"
        "fn f() -> i32 { _ = y; return 0; }"
        "fn i() -> i32 { _ = f(); return 0; }"_s,
        fn("_ = i();"_si)),
        check("int function\\d\\(thread int& global\\d\\)"_si),
        check("global\\d\\)"_si),
        check("int function\\d\\(thread int& global\\d\\)"_si),
        check("function\\d\\(global\\d\\)"_si),
        check("return 0;"_si),
        check("function\\d\\(global\\d\\)"_si));

    testCompilation(makeString(
        "var<private> y: i32;"
        "fn f() -> i32 { _ = y; return 0; }"
        "fn j(x: f32) -> f32 { _ = f(); return x; }"_s,
        fn("_ = j(j(42));"_si)),
        check("int function\\d\\(thread int& global\\d\\)"_si),
        check("global\\d\\)"_si),
        check("float function\\d\\(float parameter\\d, thread int& global\\d\\)"_si),
        check("function\\d\\(global\\d\\)"_si),
        check("return parameter\\d;"_si),
        check("function\\d\\(function\\d\\(42., global\\d\\), global\\d\\)"_si));
}

TEST_F(WGSLMetalCompilationTests, HexDoubleLChar)
{
    testCompilation(file("hex-double-lchar.wgsl"_si),
        checkLiteral("1.1754943508222875e-38"_si),
        checkLiteral("3.4028234663852886e+38"_si));
}

TEST_F(WGSLMetalCompilationTests, HexDoubleUChar)
{
    testCompilation(file("hex-double-uchar.wgsl"_si),
        checkLiteral("1.1754943508222875e-38"_si),
        checkLiteral("3.4028234663852886e+38"_si));
}

TEST_F(WGSLMetalCompilationTests, If)
{
    testCompilation(file("if.wgsl"_si));
}

TEST_F(WGSLMetalCompilationTests, LargeStruct)
{
    expectPrepareError(file("large-struct.wgsl"_si), "The combined byte size of all variables in this function exceeds 8192 bytes"_si);
}

TEST_F(WGSLMetalCompilationTests, Limits)
{
    expectPrepareError(file("limits-function-vars.wgsl"_si), "The combined byte size of all variables in this function exceeds 8192 bytes"_si);

    // These can depend on overrides, so they are only checked at generation time
    expectGenerateError(file("limits-private-vars.wgsl"_si), "The combined byte size of all variables in the private address space exceeds 8192 bytes"_si);
    expectGenerateError(file("limits-workgroup-vars.wgsl"_si), "The combined byte size of all variables in the workgroup address space exceeds 16384 bytes"_si);
}

TEST_F(WGSLMetalCompilationTests, LocalConstantVector)
{
    testCompilation("@fragment fn main() { const a = vec2(0); _ = a; }"_si, checkLiteral("(void)(vec<int, 2>(0, 0))"_si));
}

TEST_F(WGSLMetalCompilationTests, Loop)
{
    testCompilation(file("loop.wgsl"_si));
}

TEST_F(WGSLMetalCompilationTests, MinusMinusAmbiguity)
{
    testCompilation(file("minus-minus-ambiguity.wgsl"_si));
}

TEST_F(WGSLMetalCompilationTests, NameMangling)
{
    testCompilation(file("name-mangling.wgsl"_si),
        checkNotLiteral("MyStruct1"_si),
        checkNotLiteral("myStructField1"_si),
        checkNotLiteral("MyStruct2"_si),
        checkNotLiteral("myStructField2"_si),
        checkNotLiteral("myGlobal1"_si),
        checkNotLiteral("myGlobal2"_si),
        checkNotLiteral("myHelperFunction"_si),
        checkNotLiteral("myComputeEntrypoint"_si));
}

TEST_F(WGSLMetalCompilationTests, Overload)
{
    testCompilation(file("overload.wgsl"_si));
}

TEST_F(WGSLMetalCompilationTests, Override)
{
    expectGenerateError(file("override.wgsl"_si), "array count must be greater than 0"_si);
}

TEST_F(WGSLMetalCompilationTests, OverrideComplexExpression)
{
    testCompilation(file("override-complex-expression.wgsl"_si));
}

TEST_F(WGSLMetalCompilationTests, OverrideShortCircuit)
{
    auto shader = [](ASCIILiteral condition, ASCIILiteral operation) {
        return makeString(
            "override cond = "_s, condition, ";"
            "override shift = 32u;"
            "@compute @workgroup_size(1) "
            "fn main(@builtin(local_invocation_index) tid: u32) {"
            "    var r = 0u;"
            "    if (cond "_s, operation, " ((tid << shift) == 0u)) { r = 1u; }"
            "    _ = r;"
            "}"_s);
    };
    auto shiftError = "shift left value must be less than the bit width of the shifted value, which is 32"_s;

    expectGenerateError(shader("true"_s, "&&"_s), StringImpl::create(shiftError).ptr());
    expectGenerateError(shader("false"_s, "||"_s), StringImpl::create(shiftError).ptr());

    expectNoGenerateError(shader("false"_s, "&&"_s));
    expectNoGenerateError(shader("true"_s, "||"_s));

    expectNoGenerateError(
        StringImpl::create("override shift = 32u;"
        "@compute @workgroup_size(1) "
        "fn main(@builtin(local_invocation_index) tid: u32) {"
        "    var r = 0u;"
        "    if (false && ((tid << shift) == 0u)) { r = 1u; }"
        "    _ = r;"
        "}"_s).ptr());

    expectGenerateError(
        StringImpl::create("override cond = true;"
        "override shift = 32u;"
        "@compute @workgroup_size(1) "
        "fn main(@builtin(local_invocation_index) tid: u32) {"
        "    var r = 0u;"
        "    if (cond && (cond && ((tid << shift) == 0u))) { r = 1u; }"
        "    _ = r;"
        "}"_s).ptr(), StringImpl::create(shiftError).ptr());
}

TEST_F(WGSLMetalCompilationTests, PackUnpack)
{
    testCompilation(file("pack-unpack.wgsl"_si),
        check("1., 0.50\\d*, -0.49\\d*, -1."_si),
        check("1., 0.50\\d*, 0.25\\d*, 0."_si),
        check("-128, 127, -128, 127"_si),
        check("0u, 255u, 128u, 64u"_si),
        check("127, 127, -128, -128"_si),
        check("255u, 255u, 128u, 64u"_si),
        check("0.50\\d*, -0.49\\d*"_si),
        check("0.50\\d*, 0."_si),
        check("0.5, -0.5"_si));
}

TEST_F(WGSLMetalCompilationTests, Packing)
{
    const auto prologue = [&](StringImpl* wgsl) {
        return makeString(
            "struct Unpacked {"
            "  x: i32,"
            "}"
            "struct T {"
            "    v2f: vec2<f32>,"
            "    v3f: vec3<f32>,"
            "    v4f: vec4<f32>,"
            "    v2u: vec2<u32>,"
            "    v3u: vec3<u32>,"
            "    v4u: vec4<u32>,"
            "    f: f32,"
            "    u: u32,"
            "    unpacked: Unpacked,"
            "}"
            "struct U {"
            "    ts: array<T>,"
            "}"
            "var<private> t: T;"
            "var<private> m2: mat2x2<f32>;"
            "var<private> m3: mat3x3<f32>;"
            "@group(0) @binding(0) var<storage, read_write> t1: T;"
            "@group(0) @binding(1) var<storage, read_write> t2: T;"
            "@group(0) @binding(2) var<storage, read_write> v1: vec3<f32>;"
            "@group(0) @binding(3) var<storage, read_write> v2: vec3<f32>;"
            "@group(0) @binding(4) var<storage, read_write> at1: array<T, 2>;"
            "@group(0) @binding(5) var<storage, read_write> at2: array<T, 2>;"
            "@group(0) @binding(6) var<storage, read_write> av1: array<vec3f, 2>;"
            "@group(0) @binding(7) var<storage, read_write> av2: array<vec3f, 2>;"
            "@group(0) @binding(8) var<storage, read_write> u1: U;"_s,
            wgsl);
    };

    testCompilation(prologue("struct U2 { ts: array<T> }"_si), check("array<__PackedType<type\\d>, 1> field\\d"_si));

    testCompilation(prologue(fn(
        StringImpl::create("{ let x = t.v3f * m3; }"
        "{ let x = m3 * t.v3f; }"
        "{ let x = t1.v2f * m2; }"
        "{ let x = m2 * t1.v2f; }"
        "{ let x = u1.ts[0].v3f * m3; }"
        "{ let x = m3 * u1.ts[0].v3f; }"
        "{ let x = av1[0] * m3; }"_s).ptr())));

    // Test packed struct assignment
    testCompilation(prologue(fn(
        StringImpl::create("var t = t1;"
        "t = t1;"
        "t1 = t2;"
        "t2 = t;"_s).ptr())),
        check("local\\d+ = __unpack\\(global\\d+\\);"_si),
        check("local\\d+ = __unpack\\(global\\d+\\);"_si),
        check("global\\d+ = global\\d+;"_si),
        check("global\\d+ = __pack\\(local\\d+\\);"_si));

    // Test array of packed structs assignment
    testCompilation(prologue(fn(
        StringImpl::create("var at = at1;"
        "at = at1;"
        "at1 = at2;"
        "at2 = at;"_s).ptr())),
        check("local\\d+ = __unpack\\(global\\d+\\);"_si),
        check("local\\d+ = __unpack\\(global\\d+\\);"_si),
        check("global\\d+ = global\\d+;"_si),
        check("global\\d+ = __pack\\(local\\d+\\);"_si));

    // Test array of vec3 assignment
    testCompilation(prologue(fn(
        StringImpl::create("var av = av1;"
        "av = av1;"
        "av1 = av2;"
        "av2 = av;"_s).ptr())),
        check("local\\d+ = __unpack\\(global\\d+\\);"_si),
        check("local\\d+ = __unpack\\(global\\d+\\);"_si),
        check("global\\d+ = global\\d+;"_si),
        check("global\\d+ = __pack\\(local\\d+\\);"_si));


    // Test field access
    testCompilation(prologue(fn("t.v2f.x = t1.v2f.x;"_si)), check("global\\d+\\.field\\d\\.x = global\\d+\\.field\\d\\.x;"_si));
    testCompilation(prologue(fn("t.v3f.x = t1.v3f.x;"_si)), check("global\\d+\\.field\\d\\.x = global\\d+\\.field\\d\\.x;"_si));
    testCompilation(prologue(fn("t.v4f.x = t1.v4f.x;"_si)), check("global\\d+\\.field\\d\\.x = global\\d+\\.field\\d\\.x;"_si));
    testCompilation(prologue(fn("t.v2u.x = t1.v2u.x;"_si)), check("global\\d+\\.field\\d\\.x = global\\d+\\.field\\d\\.x;"_si));
    testCompilation(prologue(fn("t.v3u.x = t1.v3u.x;"_si)), check("global\\d+\\.field\\d\\.x = global\\d+\\.field\\d\\.x;"_si));
    testCompilation(prologue(fn("t.v4u.x = t1.v4u.x;"_si)), check("global\\d+\\.field\\d\\.x = global\\d+\\.field\\d\\.x;"_si));
    testCompilation(prologue(fn("t.v2f   = t1.v2f;"_si)), check("global\\d+\\.field\\d = global\\d+\\.field\\d;"_si));
    testCompilation(prologue(fn("t.v3f   = t1.v3f;"_si)), check("global\\d+\\.field\\d = __unpack\\(global\\d+\\.field\\d\\);"_si));
    testCompilation(prologue(fn("t.v4f   = t1.v4f;"_si)), check("global\\d+\\.field\\d = global\\d+\\.field\\d;"_si));
    testCompilation(prologue(fn("t.v2u   = t1.v2u;"_si)), check("global\\d+\\.field\\d = global\\d+\\.field\\d;"_si));
    testCompilation(prologue(fn("t.v3u   = t1.v3u;"_si)), check("global\\d+\\.field\\d = __unpack\\(global\\d+\\.field\\d\\);"_si));
    testCompilation(prologue(fn("t.v4u   = t1.v4u;"_si)), check("global\\d+\\.field\\d = global\\d+\\.field\\d;"_si));
    testCompilation(prologue(fn("t.f     = t1.f;"_si)), check("global\\d+\\.field\\d = global\\d+\\.field\\d;"_si));
    testCompilation(prologue(fn("t.u     = t1.u;"_si)), check("global\\d+\\.field\\d = global\\d+\\.field\\d;"_si));

    testCompilation(prologue(fn("t1.v2f.x = t2.v2f.x;"_si)), check("global\\d+\\.field\\d\\.x = global\\d+\\.field\\d\\.x;"_si));
    testCompilation(prologue(fn("t1.v3f.x = t2.v3f.x;"_si)), check("global\\d+\\.field\\d\\.x = global\\d+\\.field\\d\\.x;"_si));
    testCompilation(prologue(fn("t1.v4f.x = t2.v4f.x;"_si)), check("global\\d+\\.field\\d\\.x = global\\d+\\.field\\d\\.x;"_si));
    testCompilation(prologue(fn("t1.v2u.x = t2.v2u.x;"_si)), check("global\\d+\\.field\\d\\.x = global\\d+\\.field\\d\\.x;"_si));
    testCompilation(prologue(fn("t1.v3u.x = t2.v3u.x;"_si)), check("global\\d+\\.field\\d\\.x = global\\d+\\.field\\d\\.x;"_si));
    testCompilation(prologue(fn("t1.v4u.x = t2.v4u.x;"_si)), check("global\\d+\\.field\\d\\.x = global\\d+\\.field\\d\\.x;"_si));
    testCompilation(prologue(fn("t1.v2f   = t2.v2f;"_si)), check("global\\d+\\.field\\d = global\\d+\\.field\\d;"_si));
    testCompilation(prologue(fn("t1.v3f   = t2.v3f;"_si)), check("global\\d+\\.field\\d = global\\d+\\.field\\d;"_si));
    testCompilation(prologue(fn("t1.v4f   = t2.v4f;"_si)), check("global\\d+\\.field\\d = global\\d+\\.field\\d;"_si));
    testCompilation(prologue(fn("t1.v2u   = t2.v2u;"_si)), check("global\\d+\\.field\\d = global\\d+\\.field\\d;"_si));
    testCompilation(prologue(fn("t1.v3u   = t2.v3u;"_si)), check("global\\d+\\.field\\d = global\\d+\\.field\\d;"_si));
    testCompilation(prologue(fn("t1.v4u   = t2.v4u;"_si)), check("global\\d+\\.field\\d = global\\d+\\.field\\d;"_si));
    testCompilation(prologue(fn("t1.f     = t2.f;"_si)), check("global\\d+\\.field\\d = global\\d+\\.field\\d;"_si));
    testCompilation(prologue(fn("t1.u     = t2.u;"_si)), check("global\\d+\\.field\\d = global\\d+\\.field\\d;"_si));

    testCompilation(prologue(fn("t2.v2f.x = t.v2f.x;"_si)), check("global\\d+\\.field\\d\\.x = global\\d+\\.field\\d\\.x;"_si));
    testCompilation(prologue(fn("t2.v3f.x = t.v3f.x;"_si)), check("global\\d+\\.field\\d\\.x = global\\d+\\.field\\d\\.x;"_si));
    testCompilation(prologue(fn("t2.v4f.x = t.v4f.x;"_si)), check("global\\d+\\.field\\d\\.x = global\\d+\\.field\\d\\.x;"_si));
    testCompilation(prologue(fn("t2.v2u.x = t.v2u.x;"_si)), check("global\\d+\\.field\\d\\.x = global\\d+\\.field\\d\\.x;"_si));
    testCompilation(prologue(fn("t2.v3u.x = t.v3u.x;"_si)), check("global\\d+\\.field\\d\\.x = global\\d+\\.field\\d\\.x;"_si));
    testCompilation(prologue(fn("t2.v4u.x = t.v4u.x;"_si)), check("global\\d+\\.field\\d\\.x = global\\d+\\.field\\d\\.x;"_si));
    testCompilation(prologue(fn("t2.v2f   = t.v2f;"_si)), check("global\\d+\\.field\\d = global\\d+\\.field\\d;"_si));
    testCompilation(prologue(fn("t2.v3f   = t.v3f;"_si)), check("global\\d+\\.field\\d = global\\d+\\.field\\d;"_si));
    testCompilation(prologue(fn("t2.v4f   = t.v4f;"_si)), check("global\\d+\\.field\\d = global\\d+\\.field\\d;"_si));
    testCompilation(prologue(fn("t2.v2u   = t.v2u;"_si)), check("global\\d+\\.field\\d = global\\d+\\.field\\d;"_si));
    testCompilation(prologue(fn("t2.v3u   = t.v3u;"_si)), check("global\\d+\\.field\\d = global\\d+\\.field\\d;"_si));
    testCompilation(prologue(fn("t2.v4u   = t.v4u;"_si)), check("global\\d+\\.field\\d = global\\d+\\.field\\d;"_si));
    testCompilation(prologue(fn("t2.f     = t.f;"_si)), check("global\\d+\\.field\\d = global\\d+\\.field\\d"_si));
    testCompilation(prologue(fn("t2.u     = t.u;"_si)), check("global\\d+\\.field\\d = global\\d+\\.field\\d"_si));


    // Test index access
    testCompilation(prologue(makeString(
        "@group(0) @binding(9) var<storage, read_write> index: u32;"_s,
        fn(
            StringImpl::create("var at = at1;"
            "at[0] = at1[0];"
            "at1[0] = at2[0];"
            "at2[0] = at[0];"
            "at2[index] = at[index];"_s).ptr()))),
        check("local\\d+ = __unpack\\(global\\d+\\);"_si),
        check("local\\d+\\[0\\] = __unpack\\(global\\d+\\[0\\]\\);"_si),
        check("global\\d+\\[0\\] = global\\d+\\[0\\];"_si),
        check("global\\d+\\[0\\] = __pack\\(local\\d+\\[0\\]\\);"_si),
        check("global\\d+\\[__wgslMin\\(unsigned\\(global\\d+\\), \\(2u - 1u\\)\\)\\] = __pack\\(local\\d+\\[__wgslMin\\(unsigned\\(global\\d+\\), \\(2u - 1u\\)\\)\\]\\);"_si));

    // Indexing a packed matrix (rows == 3) yields a PackedVec3 column, which
    // must be unpacked before it can be swizzled or used as a vec3.
    testCompilation(prologue(makeString(
        "@group(0) @binding(9) var<storage, read_write> m43: mat4x3<f32>;"_s,
        fn(
            StringImpl::create("t.v3f = m43[1].xyz;"
            "t.v3f = m43[1];"
            "t.v3f.x = m43[1].x;"_s).ptr()))),
        check("__unpack\\(global\\d+\\.columns\\[__wgslMin\\(unsigned\\(1\\), \\(4u - 1u\\)\\)\\]\\)\\.xyz"_si),
        check("__unpack\\(global\\d+\\.columns\\[__wgslMin\\(unsigned\\(1\\), \\(4u - 1u\\)\\)\\]\\)"_si));

    // Test binary operations
    testCompilation(prologue(fn("t.v2f.x = 2 * t1.v2f.x;"_si)), check("global\\d+\\.field\\d\\.x = \\(2. \\* global\\d+\\.field\\d\\.x\\);"_si));
    testCompilation(prologue(fn("t.v3f.x = 2 * t1.v3f.x;"_si)), check("global\\d+\\.field\\d\\.x = \\(2. \\* global\\d+\\.field\\d\\.x\\);"_si));
    testCompilation(prologue(fn("t.v4f.x = 2 * t1.v4f.x;"_si)), check("global\\d+\\.field\\d\\.x = \\(2. \\* global\\d+\\.field\\d\\.x\\);"_si));
    testCompilation(prologue(fn("t.v2u.x = 2 * t1.v2u.x;"_si)), check("global\\d+\\.field\\d\\.x = \\(2u \\* global\\d+\\.field\\d\\.x\\);"_si));
    testCompilation(prologue(fn("t.v3u.x = 2 * t1.v3u.x;"_si)), check("global\\d+\\.field\\d\\.x = \\(2u \\* global\\d+\\.field\\d\\.x\\);"_si));
    testCompilation(prologue(fn("t.v4u.x = 2 * t1.v4u.x;"_si)), check("global\\d+\\.field\\d\\.x = \\(2u \\* global\\d+\\.field\\d\\.x\\);"_si));
    testCompilation(prologue(fn("t.v2f   = 2 * t1.v2f;"_si)), check("global\\d+\\.field\\d = \\(2. \\* global\\d+\\.field\\d\\);"_si));
    testCompilation(prologue(fn("t.v3f   = 2 * t1.v3f;"_si)), check("global\\d+\\.field\\d = \\(2. \\* __unpack\\(global\\d+\\.field\\d\\)\\);"_si));
    testCompilation(prologue(fn("t.v4f   = 2 * t1.v4f;"_si)), check("global\\d+\\.field\\d = \\(2. \\* global\\d+\\.field\\d\\);"_si));
    testCompilation(prologue(fn("t.v2u   = 2 * t1.v2u;"_si)), check("global\\d+\\.field\\d = \\(2u \\* global\\d+\\.field\\d\\);"_si));
    testCompilation(prologue(fn("t.v3u   = 2 * t1.v3u;"_si)), check("global\\d+\\.field\\d = \\(2u \\* __unpack\\(global\\d+\\.field\\d\\)\\);"_si));
    testCompilation(prologue(fn("t.v4u   = 2 * t1.v4u;"_si)), check("global\\d+\\.field\\d = \\(2u \\* global\\d+\\.field\\d\\);"_si));
    testCompilation(prologue(fn("t.f     = 2 * t1.f;"_si)), check("global\\d+\\.field\\d = \\(2. \\* global\\d+\\.field\\d\\);"_si));
    testCompilation(prologue(fn("t.u     = 2 * t1.u;"_si)), check("global\\d+\\.field\\d = \\(2u \\* global\\d+\\.field\\d\\);"_si));

    testCompilation(prologue(fn("t1.v2f.x = 2 * t2.v2f.x;"_si)), check("global\\d+\\.field\\d\\.x = \\(2. \\* global\\d+\\.field\\d\\.x\\);"_si));
    testCompilation(prologue(fn("t1.v3f.x = 2 * t2.v3f.x;"_si)), check("global\\d+\\.field\\d\\.x = \\(2. \\* global\\d+\\.field\\d\\.x\\);"_si));
    testCompilation(prologue(fn("t1.v4f.x = 2 * t2.v4f.x;"_si)), check("global\\d+\\.field\\d\\.x = \\(2. \\* global\\d+\\.field\\d\\.x\\);"_si));
    testCompilation(prologue(fn("t1.v2u.x = 2 * t2.v2u.x;"_si)), check("global\\d+\\.field\\d\\.x = \\(2u \\* global\\d+\\.field\\d\\.x\\);"_si));
    testCompilation(prologue(fn("t1.v3u.x = 2 * t2.v3u.x;"_si)), check("global\\d+\\.field\\d\\.x = \\(2u \\* global\\d+\\.field\\d\\.x\\);"_si));
    testCompilation(prologue(fn("t1.v4u.x = 2 * t2.v4u.x;"_si)), check("global\\d+\\.field\\d\\.x = \\(2u \\* global\\d+\\.field\\d\\.x\\);"_si));
    testCompilation(prologue(fn("t1.v2f   = 2 * t2.v2f;"_si)), check("global\\d+\\.field\\d = \\(2. \\* global\\d+\\.field\\d\\);"_si));
    testCompilation(prologue(fn("t1.v3f   = 2 * t2.v3f;"_si)), check("global\\d+\\.field\\d = \\(2. \\* __unpack\\(global\\d+\\.field\\d\\)\\);"_si));
    testCompilation(prologue(fn("t1.v4f   = 2 * t2.v4f;"_si)), check("global\\d+\\.field\\d = \\(2. \\* global\\d+\\.field\\d\\);"_si));
    testCompilation(prologue(fn("t1.v2u   = 2 * t2.v2u;"_si)), check("global\\d+\\.field\\d = \\(2u \\* global\\d+\\.field\\d\\);"_si));
    testCompilation(prologue(fn("t1.v3u   = 2 * t2.v3u;"_si)), check("global\\d+\\.field\\d = \\(2u \\* __unpack\\(global\\d+\\.field\\d\\)\\);"_si));
    testCompilation(prologue(fn("t1.v4u   = 2 * t2.v4u;"_si)), check("global\\d+\\.field\\d = \\(2u \\* global\\d+\\.field\\d\\);"_si));
    testCompilation(prologue(fn("t1.f     = 2 * t2.f;"_si)), check("global\\d+\\.field\\d = \\(2. \\* global\\d+\\.field\\d\\);"_si));
    testCompilation(prologue(fn("t1.u     = 2 * t2.u;"_si)), check("global\\d+\\.field\\d = \\(2u \\* global\\d+\\.field\\d\\);"_si));

    testCompilation(prologue(fn("t2.v2f.x = 2 * t.v2f.x;"_si)), check("global\\d+\\.field\\d\\.x = \\(2. \\* global\\d+\\.field\\d\\.x\\);"_si));
    testCompilation(prologue(fn("t2.v3f.x = 2 * t.v3f.x;"_si)), check("global\\d+\\.field\\d\\.x = \\(2. \\* global\\d+\\.field\\d\\.x\\);"_si));
    testCompilation(prologue(fn("t2.v4f.x = 2 * t.v4f.x;"_si)), check("global\\d+\\.field\\d\\.x = \\(2. \\* global\\d+\\.field\\d\\.x\\);"_si));
    testCompilation(prologue(fn("t2.v2u.x = 2 * t.v2u.x;"_si)), check("global\\d+\\.field\\d\\.x = \\(2u \\* global\\d+\\.field\\d\\.x\\);"_si));
    testCompilation(prologue(fn("t2.v3u.x = 2 * t.v3u.x;"_si)), check("global\\d+\\.field\\d\\.x = \\(2u \\* global\\d+\\.field\\d\\.x\\);"_si));
    testCompilation(prologue(fn("t2.v4u.x = 2 * t.v4u.x;"_si)), check("global\\d+\\.field\\d\\.x = \\(2u \\* global\\d+\\.field\\d\\.x\\);"_si));
    testCompilation(prologue(fn("t2.v2f   = 2 * t.v2f;"_si)), check("global\\d+\\.field\\d = \\(2. \\* global\\d+\\.field\\d\\);"_si));
    testCompilation(prologue(fn("t2.v3f   = 2 * t.v3f;"_si)), check("global\\d+\\.field\\d = \\(2. \\* global\\d+\\.field\\d\\);"_si));
    testCompilation(prologue(fn("t2.v4f   = 2 * t.v4f;"_si)), check("global\\d+\\.field\\d = \\(2. \\* global\\d+\\.field\\d\\);"_si));
    testCompilation(prologue(fn("t2.v2u   = 2 * t.v2u;"_si)), check("global\\d+\\.field\\d = \\(2u \\* global\\d+\\.field\\d\\);"_si));
    testCompilation(prologue(fn("t2.v3u   = 2 * t.v3u;"_si)), check("global\\d+\\.field\\d = \\(2u \\* global\\d+\\.field\\d\\);"_si));
    testCompilation(prologue(fn("t2.v4u   = 2 * t.v4u;"_si)), check("global\\d+\\.field\\d = \\(2u \\* global\\d+\\.field\\d\\);"_si));
    testCompilation(prologue(fn("t2.f     = 2 * t.f;"_si)), check("global\\d+\\.field\\d = \\(2. \\* global\\d+\\.field\\d\\);"_si));
    testCompilation(prologue(fn("t2.u     = 2 * t.u;"_si)), check("global\\d+\\.field\\d = \\(2u \\* global\\d+\\.field\\d\\);"_si));

    // Test unary operations

    testCompilation(prologue(fn("t.v2f.x = -t1.v2f.x;"_si)), check("global\\d+\\.field\\d\\.x = \\(-global\\d+\\.field\\d\\.x\\);"_si));
    testCompilation(prologue(fn("t.v3f.x = -t1.v3f.x;"_si)), check("global\\d+\\.field\\d\\.x = \\(-global\\d+\\.field\\d\\.x\\);"_si));
    testCompilation(prologue(fn("t.v4f.x = -t1.v4f.x;"_si)), check("global\\d+\\.field\\d\\.x = \\(-global\\d+\\.field\\d\\.x\\);"_si));
    testCompilation(prologue(fn("t.v2f   = -t1.v2f;"_si)), check("global\\d+\\.field\\d = \\(-global\\d+\\.field\\d\\);"_si));
    testCompilation(prologue(fn("t.v3f   = -t1.v3f;"_si)), check("global\\d+\\.field\\d = \\(-__unpack\\(global\\d+\\.field\\d\\)\\);"_si));
    testCompilation(prologue(fn("t.v4f   = -t1.v4f;"_si)), check("global\\d+\\.field\\d = \\(-global\\d+\\.field\\d\\);"_si));
    testCompilation(prologue(fn("t.f     = -t1.f;"_si)), check("global\\d+\\.field\\d = \\(-global\\d+\\.field\\d\\);"_si));

    testCompilation(prologue(fn("t1.v2f.x = -t2.v2f.x;"_si)), check("global\\d+\\.field\\d\\.x = \\(-global\\d+\\.field\\d\\.x\\);"_si));
    testCompilation(prologue(fn("t1.v3f.x = -t2.v3f.x;"_si)), check("global\\d+\\.field\\d\\.x = \\(-global\\d+\\.field\\d\\.x\\);"_si));
    testCompilation(prologue(fn("t1.v4f.x = -t2.v4f.x;"_si)), check("global\\d+\\.field\\d\\.x = \\(-global\\d+\\.field\\d\\.x\\);"_si));
    testCompilation(prologue(fn("t1.v2f   = -t2.v2f;"_si)), check("global\\d+\\.field\\d = \\(-global\\d+\\.field\\d\\);"_si));
    testCompilation(prologue(fn("t1.v3f   = -t2.v3f;"_si)), check("global\\d+\\.field\\d = \\(-__unpack\\(global\\d+\\.field\\d\\)\\);"_si));
    testCompilation(prologue(fn("t1.v4f   = -t2.v4f;"_si)), check("global\\d+\\.field\\d = \\(-global\\d+\\.field\\d\\);"_si));
    testCompilation(prologue(fn("t1.f     = -t2.f;"_si)), check("global\\d+\\.field\\d = \\(-global\\d+\\.field\\d\\);"_si));

    testCompilation(prologue(fn("t2.v2f.x = -t.v2f.x;"_si)), check("global\\d+\\.field\\d\\.x = \\(-global\\d+\\.field\\d\\.x\\);"_si));
    testCompilation(prologue(fn("t2.v3f.x = -t.v3f.x;"_si)), check("global\\d+\\.field\\d\\.x = \\(-global\\d+\\.field\\d\\.x\\);"_si));
    testCompilation(prologue(fn("t2.v4f.x = -t.v4f.x;"_si)), check("global\\d+\\.field\\d\\.x = \\(-global\\d+\\.field\\d\\.x\\);"_si));
    testCompilation(prologue(fn("t2.v2f   = -t.v2f;"_si)), check("global\\d+\\.field\\d = \\(-global\\d+\\.field\\d\\);"_si));
    testCompilation(prologue(fn("t2.v3f   = -t.v3f;"_si)), check("global\\d+\\.field\\d = \\(-global\\d+\\.field\\d\\);"_si));
    testCompilation(prologue(fn("t2.v4f   = -t.v4f;"_si)), check("global\\d+\\.field\\d = \\(-global\\d+\\.field\\d\\);"_si));
    testCompilation(prologue(fn("t2.f     = -t.f;"_si)), check("global\\d+\\.field\\d = \\(-global\\d+\\.field\\d\\);"_si));

    // Test function calls
    testCompilation(prologue(fn("t.v2f.x = abs(t1.v2f.x);"_si)), check("global\\d+\\.field\\d\\.x = abs\\(global\\d+\\.field\\d\\.x\\);"_si));
    testCompilation(prologue(fn("t.v3f.x = abs(t1.v3f.x);"_si)), check("global\\d+\\.field\\d\\.x = abs\\(global\\d+\\.field\\d\\.x\\);"_si));
    testCompilation(prologue(fn("t.v4f.x = abs(t1.v4f.x);"_si)), check("global\\d+\\.field\\d\\.x = abs\\(global\\d+\\.field\\d\\.x\\);"_si));
    testCompilation(prologue(fn("t.v2u.x = abs(t1.v2u.x);"_si)), check("global\\d+\\.field\\d\\.x = abs\\(global\\d+\\.field\\d\\.x\\);"_si));
    testCompilation(prologue(fn("t.v3u.x = abs(t1.v3u.x);"_si)), check("global\\d+\\.field\\d\\.x = abs\\(global\\d+\\.field\\d\\.x\\);"_si));
    testCompilation(prologue(fn("t.v4u.x = abs(t1.v4u.x);"_si)), check("global\\d+\\.field\\d\\.x = abs\\(global\\d+\\.field\\d\\.x\\);"_si));
    testCompilation(prologue(fn("t.v2f   = abs(t1.v2f);"_si)), check("global\\d+\\.field\\d = abs\\(global\\d+\\.field\\d\\);"_si));
    testCompilation(prologue(fn("t.v3f   = abs(t1.v3f);"_si)), check("global\\d+\\.field\\d = abs\\(__unpack\\(global\\d+\\.field\\d\\)\\);"_si));
    testCompilation(prologue(fn("t.v4f   = abs(t1.v4f);"_si)), check("global\\d+\\.field\\d = abs\\(global\\d+\\.field\\d\\);"_si));
    testCompilation(prologue(fn("t.v2u   = abs(t1.v2u);"_si)), check("global\\d+\\.field\\d = abs\\(global\\d+\\.field\\d\\);"_si));
    testCompilation(prologue(fn("t.v3u   = abs(t1.v3u);"_si)), check("global\\d+\\.field\\d = abs\\(__unpack\\(global\\d+\\.field\\d\\)\\);"_si));
    testCompilation(prologue(fn("t.v4u   = abs(t1.v4u);"_si)), check("global\\d+\\.field\\d = abs\\(global\\d+\\.field\\d\\);"_si));
    testCompilation(prologue(fn("t.f     = abs(t1.f);"_si)), check("global\\d+\\.field\\d = abs\\(global\\d+\\.field\\d\\);"_si));
    testCompilation(prologue(fn("t.u     = abs(t1.u);"_si)), check("global\\d+\\.field\\d = abs\\(global\\d+\\.field\\d\\);"_si));

    testCompilation(prologue(fn("t1.v2f.x = abs(t2.v2f.x);"_si)), check("global\\d+\\.field\\d\\.x = abs\\(global\\d+\\.field\\d\\.x\\);"_si));
    testCompilation(prologue(fn("t1.v3f.x = abs(t2.v3f.x);"_si)), check("global\\d+\\.field\\d\\.x = abs\\(global\\d+\\.field\\d\\.x\\);"_si));
    testCompilation(prologue(fn("t1.v4f.x = abs(t2.v4f.x);"_si)), check("global\\d+\\.field\\d\\.x = abs\\(global\\d+\\.field\\d\\.x\\);"_si));
    testCompilation(prologue(fn("t1.v2u.x = abs(t2.v2u.x);"_si)), check("global\\d+\\.field\\d\\.x = abs\\(global\\d+\\.field\\d\\.x\\);"_si));
    testCompilation(prologue(fn("t1.v3u.x = abs(t2.v3u.x);"_si)), check("global\\d+\\.field\\d\\.x = abs\\(global\\d+\\.field\\d\\.x\\);"_si));
    testCompilation(prologue(fn("t1.v4u.x = abs(t2.v4u.x);"_si)), check("global\\d+\\.field\\d\\.x = abs\\(global\\d+\\.field\\d\\.x\\);"_si));
    testCompilation(prologue(fn("t1.v2f   = abs(t2.v2f);"_si)), check("global\\d+\\.field\\d = abs\\(global\\d+\\.field\\d\\);"_si));
    testCompilation(prologue(fn("t1.v3f   = abs(t2.v3f);"_si)), check("global\\d+\\.field\\d = abs\\(__unpack\\(global\\d+\\.field\\d\\)\\);"_si));
    testCompilation(prologue(fn("t1.v4f   = abs(t2.v4f);"_si)), check("global\\d+\\.field\\d = abs\\(global\\d+\\.field\\d\\);"_si));
    testCompilation(prologue(fn("t1.v2u   = abs(t2.v2u);"_si)), check("global\\d+\\.field\\d = abs\\(global\\d+\\.field\\d\\);"_si));
    testCompilation(prologue(fn("t1.v3u   = abs(t2.v3u);"_si)), check("global\\d+\\.field\\d = abs\\(__unpack\\(global\\d+\\.field\\d\\)\\);"_si));
    testCompilation(prologue(fn("t1.v4u   = abs(t2.v4u);"_si)), check("global\\d+\\.field\\d = abs\\(global\\d+\\.field\\d\\);"_si));
    testCompilation(prologue(fn("t1.f     = abs(t2.f);"_si)), check("global\\d+\\.field\\d = abs\\(global\\d+\\.field\\d\\);"_si));
    testCompilation(prologue(fn("t1.u     = abs(t2.u);"_si)), check("global\\d+\\.field\\d = abs\\(global\\d+\\.field\\d\\);"_si));

    testCompilation(prologue(fn("t2.v2f.x = abs(t.v2f.x);"_si)), check("global\\d+\\.field\\d\\.x = abs\\(global\\d+\\.field\\d\\.x\\);"_si));
    testCompilation(prologue(fn("t2.v3f.x = abs(t.v3f.x);"_si)), check("global\\d+\\.field\\d\\.x = abs\\(global\\d+\\.field\\d\\.x\\);"_si));
    testCompilation(prologue(fn("t2.v4f.x = abs(t.v4f.x);"_si)), check("global\\d+\\.field\\d\\.x = abs\\(global\\d+\\.field\\d\\.x\\);"_si));
    testCompilation(prologue(fn("t2.v2u.x = abs(t.v2u.x);"_si)), check("global\\d+\\.field\\d\\.x = abs\\(global\\d+\\.field\\d\\.x\\);"_si));
    testCompilation(prologue(fn("t2.v3u.x = abs(t.v3u.x);"_si)), check("global\\d+\\.field\\d\\.x = abs\\(global\\d+\\.field\\d\\.x\\);"_si));
    testCompilation(prologue(fn("t2.v4u.x = abs(t.v4u.x);"_si)), check("global\\d+\\.field\\d\\.x = abs\\(global\\d+\\.field\\d\\.x\\);"_si));
    testCompilation(prologue(fn("t2.v2f   = abs(t.v2f);"_si)), check("global\\d+\\.field\\d = abs\\(global\\d+\\.field\\d\\);"_si));
    testCompilation(prologue(fn("t2.v3f   = abs(t.v3f);"_si)), check("global\\d+\\.field\\d = abs\\(global\\d+\\.field\\d\\);"_si));
    testCompilation(prologue(fn("t2.v4f   = abs(t.v4f);"_si)), check("global\\d+\\.field\\d = abs\\(global\\d+\\.field\\d\\);"_si));
    testCompilation(prologue(fn("t2.v2u   = abs(t.v2u);"_si)), check("global\\d+\\.field\\d = abs\\(global\\d+\\.field\\d\\);"_si));
    testCompilation(prologue(fn("t2.v3u   = abs(t.v3u);"_si)), check("global\\d+\\.field\\d = abs\\(global\\d+\\.field\\d\\);"_si));
    testCompilation(prologue(fn("t2.v4u   = abs(t.v4u);"_si)), check("global\\d+\\.field\\d = abs\\(global\\d+\\.field\\d\\);"_si));
    testCompilation(prologue(fn("t2.f     = abs(t.f);"_si)), check("global\\d+\\.field\\d = abs\\(global\\d+\\.field\\d\\);"_si));
    testCompilation(prologue(fn("t2.u     = abs(t.u);"_si)), check("global\\d+\\.field\\d = abs\\(global\\d+\\.field\\d\\);"_si));

    // Test runtime array
    testCompilation(prologue(makeString(
        "struct S {"
        "    @size(16) x: i32,"
        "    @align(32) y: array<i32>,"
        "}"
        "@group(0) @binding(10) var<storage, read_write> s: S;"_s,
        fn("s.x = 0; s.y[0] = 0;"_si))));

    // Test packed vec3 compound assignment
    testCompilation(prologue(makeString(
        "@group(0) @binding(11) var<storage,read_write> D: array<vec3f>;"_s,
        fn("D[0] += vec3f(1);"_si))));

    // Test inconstructible struct
    testCompilation(prologue(makeString(
        "struct InconstructibleS {"
        "    x: vec3f,"
        "    y: atomic<u32>"
        "}"
        "struct InconstructibleT {"
        "    x: InconstructibleS,"
        "}"
        "@group(0) @binding(12) var<storage, read_write> global: InconstructibleT;"_s,
        fn("var x = global.x.x;"_si))));
}

TEST_F(WGSLMetalCompilationTests, PackingNestedArray)
{
    testCompilation(file("packing-nested-array.wgsl"_si));
}

TEST_F(WGSLMetalCompilationTests, PackingPointerArguments)
{
    testCompilation(file("packing-pointer-arguments.wgsl"_si));
}

TEST_F(WGSLMetalCompilationTests, PointerRewritingConcreteType)
{
    testCompilation(fn("var h = vec2f(); _ = length(*&h);"_si),
        checkLiteral("length(local0)"_si));
}

TEST_F(WGSLMetalCompilationTests, Pointers)
{
    testCompilation(file("pointers.wgsl"_si));
}

TEST_F(WGSLMetalCompilationTests, References)
{
    testCompilation(file("references.wgsl"_si));
}

TEST_F(WGSLMetalCompilationTests, Reordering)
{
    testCompilation(file("references.wgsl"_si));
}

TEST_F(WGSLMetalCompilationTests, RuntimeSizedArray)
{
    testCompilation(file("runtime-sized-array-resource.wgsl"_si));
}

TEST_F(WGSLMetalCompilationTests, Scope)
{
    testCompilation(file("scope.wgsl"_si));
}

TEST_F(WGSLMetalCompilationTests, Shadowing)
{
    testCompilation(file("shadowing.wgsl"_si));
}

TEST_F(WGSLMetalCompilationTests, Struct)
{
    testCompilation(file("struct.wgsl"_si));
}

TEST_F(WGSLMetalCompilationTests, Switch)
{
    testCompilation(file("switch.wgsl"_si));
}

TEST_F(WGSLMetalCompilationTests, Swizzle)
{
    testCompilation(file("swizzle.wgsl"_si));
}

TEST_F(WGSLMetalCompilationTests, TextureExternal)
{
    testCompilation(file("texture-external.wgsl"_si));
}

TEST_F(WGSLMetalCompilationTests, TypePromotion)
{
    testCompilation(file("type-promotion.wgsl"_si));
}

TEST_F(WGSLMetalCompilationTests, VarInitialization)
{
    testCompilation(
        StringImpl::create("fn f(x: f32) { }"
        "@compute @workgroup_size(1)"
        "fn main() {"
        "    var a = 1.0;"
        "    var b = a;"
        "    b = 0.0;"
        "    f(b);"
        "}"_s).ptr(),
        check("float local\\d+ = 1"_si),
        check("float local\\d+ = local\\d"_si),
        check("local\\d+ = 0"_si),
        check("function\\d\\(local\\d+\\)"_si));
}

TEST_F(WGSLMetalCompilationTests, While)
{
    testCompilation(file("while.wgsl"_si));
}

}
