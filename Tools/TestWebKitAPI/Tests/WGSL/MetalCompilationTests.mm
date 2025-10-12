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

    testCompilation("struct S { @builtin(position) @invariant value : vec4f };"
        "@vertex fn main() -> S { return S(vec4<f32>()); }"_s,
        checkLiteral("[[invariant]]"_s));

    testCompilation("@vertex fn main() -> @invariant @builtin(position) vec4<f32> { return vec4<f32>(); }"_s,
        checkLiteral("[[invariant]]"_s));
}

TEST_F(WGSLMetalCompilationTests, BindingUIntMax)
{
    testCompilation(file("binding-uint-max.wgsl"_s));
}
TEST_F(WGSLMetalCompilationTests, Concretization)
{
    // Test variable inialization
    testCompilation(fn(
        "let x1: u32 = 0;"
        "let x2: vec2<u32> = vec2(0, 0);"
        "let x3: f32 = 0;"
        "let x4: f32 = 0.0;"
        "var v1 = vec2(0.0);"
        "v1 = vec2f(0);"_s));

    // Test concretization of arguments
    testCompilation(fn("let x = 0u + vec2(0, 0);"_s), checkLiteral("= vec<unsigned, 2>(0u, 0u);"_s));
    testCompilation(fn("let x = 0i + vec2(0, 0);"_s), checkLiteral("= vec<int, 2>(0, 0);"_s));
    testCompilation(fn("let x = 0f + vec2(0, 0);"_s), checkLiteral("= vec<float, 2>(0., 0.);"_s));

    // Test array concretization
    testCompilation(fn("let x = array<vec2<i32>, 1>(vec2(0, 0));"_s), checkLiteral("vec<int, 2>(0, 0)"_s));
    testCompilation(fn("let x = array<vec2<f32>, 1>(vec2(0, 0));"_s), checkLiteral("vec<float, 2>(0., 0.)"_s));
    testCompilation(fn("let x = array(vec2(0, 0), vec2(0.0, 0.0));"_s),
        checkLiteral("vec<float, 2>(0., 0.),"_s),
        checkLiteral("vec<float, 2>(0., 0.),"_s));
    testCompilation(fn("let x = array(vec2(0, 0), vec2(0u, 0u));"_s),
        checkLiteral("vec<unsigned, 2>(0u, 0u),"_s),
        checkLiteral("vec<unsigned, 2>(0u, 0u),"_s));

    // Test initializer concretization
    testCompilation(fn("let x = vec2(0, 0);"_s), checkLiteral(" = vec<int, 2>(0, 0);"_s));
    testCompilation(fn("let x : vec2<u32> = vec2(0, 0);"_s), checkLiteral(" = vec<unsigned, 2>(0u, 0u);"_s));

    // Test atomic materialization
    testCompilation(
        "@group(0) @binding(0) var<storage, read_write> a: array<atomic<i32>>;"
        "@compute @workgroup_size(1)"
        "fn testArrayAccessMaterialization()"
        "{"
        "    let i = 0;"
        "    let x = atomicLoad(&a[i]);"
        "}"_s);
}

TEST_F(WGSLMetalCompilationTests, ConstAssert)
{
    testCompilation(file("const-assert.wgsl"_s));
}

TEST_F(WGSLMetalCompilationTests, ConstantMatrix)
{
    testCompilation(fn(
        "let x = determinant(mat4x4("
        "    15,2,34,4,"
        "    18,2,3,4,"
        "    1,72,32,4,"
        "    17,2,3,4,"
        "));"_s),
        checkLiteral("8680"_s));

    testCompilation(fn(
        "let y = determinant(mat4x4("
        "    vec4(15,2,34,4),"
        "    vec4(18,2,3,4),"
        "    vec4(1,72,32,4),"
        "    vec4(17,2,3,4),"
        "));"_s),
        checkLiteral("8680"_s));


    testCompilation(fn(
        "const m2 = mat2x2("
        "  1,2,"
        "  3,4,"
        ");"
        "let tm2 = transpose(m2);"_s),
        checkLiteral("1., 3., 2., 4."_s));

    testCompilation(fn(
        "let tm3 = transpose(mat3x3("
        "    1,2,3,"
        "    4,5,6,"
        "    7,8,9,"
        "));"_s),
        checkLiteral("1., 4., 7., 2., 5., 8., 3., 6., 9."_s));

    testCompilation(fn(
        "const m2 = mat2x2("
        "  1,2,"
        "  3,4,"
        ");"
        "let x0 = m2 * 2;"_s),
        checkLiteral("2., 4., 6., 8."_s));

    testCompilation(fn(
        "const m2x3 = mat2x3("
        "    1,2,3,"
        "    4,5,6"
        ");"
        "let x1 = transpose(m2x3);"_s),
        checkLiteral("1., 4., 2., 5., 3., 6."_s));

    testCompilation(fn(
        "const m2x3 = mat2x3("
        "    1,2,3,"
        "    4,5,6"
        ");"
        "let x2 = m2x3 * vec2(2);"_s),
        checkLiteral("10., 14., 18."_s));

    testCompilation(fn(
        "const m2x3 = mat2x3("
        "    1,2,3,"
        "    4,5,6"
        ");"
        "let x3 = vec3(2) * m2x3;"_s),
        checkLiteral("12., 30."_s));

    testCompilation(fn(
        "let x4 = dot(vec3(1,2,3), vec3(4,5,6));"_s),
        checkLiteral("32"_s));

    testCompilation(fn(
        "let x5 = mat3x2(1,2,3,4,5,6) * mat4x3(12,11,10,9,8,7,6,5,4,3,2,1);"_s),
        checkLiteral("95., 128., 68., 92., 41., 56., 14., 20."_s));
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

TEST_F(WGSLMetalCompilationTests, GlobalUsedByCallee)
{
    testCompilation(makeString(
        "var<private> x: i32;"_s,
        fn("_ = x;"_s)),
        check("int global\\d"_s));

    testCompilation(makeString(
        "var<private> y: i32;"
        "fn f() -> i32 { _ = y; return 0; }"_s,
        fn("_ = f();"_s)),
        check("int function\\d\\(thread int& global\\d\\)"_s),
        check("global\\d\\)"_s),
        check("return 0;"_s),
        check("function\\d\\(global\\d\\)"_s));

    testCompilation(makeString(
        "var<private> y: i32;"
        "fn g() -> i32 { let y = 42; return y; }"_s,
        fn("_ = g();"_s)),
        check("int function\\d\\(\\)"_s),
        checkNot("global\\d\\)"_s),
        check("return local\\d;"_s),
        check("function\\d\\(\\)"_s));

    testCompilation(makeString(
        "var<private> y: i32;"
        "override z = 42;"
        "fn h() -> i32 { _ = y; _ = z; return 0; }"_s,
        fn("_ = h();"_s)),
        check("int function\\d\\(thread int& global\\d, int global\\d\\)"_s),
        check("global\\d\\)"_s),
        check("return 0;"_s),
        check("function\\d\\(global\\d, \\(42\\)\\)"_s));

    testCompilation(makeString(
        "var<private> y: i32;"
        "fn f() -> i32 { _ = y; return 0; }"
        "fn i() -> i32 { _ = f(); return 0; }"_s,
        fn("_ = i();"_s)),
        check("int function\\d\\(thread int& global\\d\\)"_s),
        check("global\\d\\)"_s),
        check("int function\\d\\(thread int& global\\d\\)"_s),
        check("function\\d\\(global\\d\\)"_s),
        check("return 0;"_s),
        check("function\\d\\(global\\d\\)"_s));

    testCompilation(makeString(
        "var<private> y: i32;"
        "fn f() -> i32 { _ = y; return 0; }"
        "fn j(x: f32) -> f32 { _ = f(); return x; }"_s,
        fn("_ = j(j(42));"_s)),
        check("int function\\d\\(thread int& global\\d\\)"_s),
        check("global\\d\\)"_s),
        check("float function\\d\\(float parameter\\d, thread int& global\\d\\)"_s),
        check("function\\d\\(global\\d\\)"_s),
        check("return parameter\\d;"_s),
        check("function\\d\\(function\\d\\(42., global\\d\\), global\\d\\)"_s));
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

TEST_F(WGSLMetalCompilationTests, NameMangling)
{
    testCompilation(file("name-mangling.wgsl"_s),
        checkNotLiteral("MyStruct1"_s),
        checkNotLiteral("myStructField1"_s),
        checkNotLiteral("MyStruct2"_s),
        checkNotLiteral("myStructField2"_s),
        checkNotLiteral("myGlobal1"_s),
        checkNotLiteral("myGlobal2"_s),
        checkNotLiteral("myHelperFunction"_s),
        checkNotLiteral("myComputeEntrypoint"_s));
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

TEST_F(WGSLMetalCompilationTests, Packing)
{
    const auto prologue = [&](const String& wgsl) {
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

    testCompilation(prologue("struct U2 { ts: array<T> }"_s), check("array<__PackedType<type\\d>, 1> field\\d"_s));

    testCompilation(prologue(fn(
        "{ let x = t.v3f * m3; }"
        "{ let x = m3 * t.v3f; }"
        "{ let x = t1.v2f * m2; }"
        "{ let x = m2 * t1.v2f; }"
        "{ let x = u1.ts[0].v3f * m3; }"
        "{ let x = m3 * u1.ts[0].v3f; }"
        "{ let x = av1[0] * m3; }"_s)));

    // Test packed struct assignment
    testCompilation(prologue(fn(
        "var t = t1;"
        "t = t1;"
        "t1 = t2;"
        "t2 = t;"_s)),
        check("local\\d+ = __unpack\\(global\\d+\\);"_s),
        check("local\\d+ = __unpack\\(global\\d+\\);"_s),
        check("global\\d+ = global\\d+;"_s),
        check("global\\d+ = __pack\\(local\\d+\\);"_s));

    // Test array of packed structs assignment
    testCompilation(prologue(fn(
        "var at = at1;"
        "at = at1;"
        "at1 = at2;"
        "at2 = at;"_s)),
        check("local\\d+ = __unpack\\(global\\d+\\);"_s),
        check("local\\d+ = __unpack\\(global\\d+\\);"_s),
        check("global\\d+ = global\\d+;"_s),
        check("global\\d+ = __pack\\(local\\d+\\);"_s));

    // Test array of vec3 assignment
    testCompilation(prologue(fn(
        "var av = av1;"
        "av = av1;"
        "av1 = av2;"
        "av2 = av;"_s)),
        check("local\\d+ = __unpack\\(global\\d+\\);"_s),
        check("local\\d+ = __unpack\\(global\\d+\\);"_s),
        check("global\\d+ = global\\d+;"_s),
        check("global\\d+ = __pack\\(local\\d+\\);"_s));


    // Test field access
    testCompilation(prologue(fn("t.v2f.x = t1.v2f.x;"_s)), check("global\\d+\\.field\\d\\.x = global\\d+\\.field\\d\\.x;"_s));
    testCompilation(prologue(fn("t.v3f.x = t1.v3f.x;"_s)), check("global\\d+\\.field\\d\\.x = global\\d+\\.field\\d\\.x;"_s));
    testCompilation(prologue(fn("t.v4f.x = t1.v4f.x;"_s)), check("global\\d+\\.field\\d\\.x = global\\d+\\.field\\d\\.x;"_s));
    testCompilation(prologue(fn("t.v2u.x = t1.v2u.x;"_s)), check("global\\d+\\.field\\d\\.x = global\\d+\\.field\\d\\.x;"_s));
    testCompilation(prologue(fn("t.v3u.x = t1.v3u.x;"_s)), check("global\\d+\\.field\\d\\.x = global\\d+\\.field\\d\\.x;"_s));
    testCompilation(prologue(fn("t.v4u.x = t1.v4u.x;"_s)), check("global\\d+\\.field\\d\\.x = global\\d+\\.field\\d\\.x;"_s));
    testCompilation(prologue(fn("t.v2f   = t1.v2f;"_s)), check("global\\d+\\.field\\d = global\\d+\\.field\\d;"_s));
    testCompilation(prologue(fn("t.v3f   = t1.v3f;"_s)), check("global\\d+\\.field\\d = __unpack\\(global\\d+\\.field\\d\\);"_s));
    testCompilation(prologue(fn("t.v4f   = t1.v4f;"_s)), check("global\\d+\\.field\\d = global\\d+\\.field\\d;"_s));
    testCompilation(prologue(fn("t.v2u   = t1.v2u;"_s)), check("global\\d+\\.field\\d = global\\d+\\.field\\d;"_s));
    testCompilation(prologue(fn("t.v3u   = t1.v3u;"_s)), check("global\\d+\\.field\\d = __unpack\\(global\\d+\\.field\\d\\);"_s));
    testCompilation(prologue(fn("t.v4u   = t1.v4u;"_s)), check("global\\d+\\.field\\d = global\\d+\\.field\\d;"_s));
    testCompilation(prologue(fn("t.f     = t1.f;"_s)), check("global\\d+\\.field\\d = global\\d+\\.field\\d;"_s));
    testCompilation(prologue(fn("t.u     = t1.u;"_s)), check("global\\d+\\.field\\d = global\\d+\\.field\\d;"_s));

    testCompilation(prologue(fn("t1.v2f.x = t2.v2f.x;"_s)), check("global\\d+\\.field\\d\\.x = global\\d+\\.field\\d\\.x;"_s));
    testCompilation(prologue(fn("t1.v3f.x = t2.v3f.x;"_s)), check("global\\d+\\.field\\d\\.x = global\\d+\\.field\\d\\.x;"_s));
    testCompilation(prologue(fn("t1.v4f.x = t2.v4f.x;"_s)), check("global\\d+\\.field\\d\\.x = global\\d+\\.field\\d\\.x;"_s));
    testCompilation(prologue(fn("t1.v2u.x = t2.v2u.x;"_s)), check("global\\d+\\.field\\d\\.x = global\\d+\\.field\\d\\.x;"_s));
    testCompilation(prologue(fn("t1.v3u.x = t2.v3u.x;"_s)), check("global\\d+\\.field\\d\\.x = global\\d+\\.field\\d\\.x;"_s));
    testCompilation(prologue(fn("t1.v4u.x = t2.v4u.x;"_s)), check("global\\d+\\.field\\d\\.x = global\\d+\\.field\\d\\.x;"_s));
    testCompilation(prologue(fn("t1.v2f   = t2.v2f;"_s)), check("global\\d+\\.field\\d = global\\d+\\.field\\d;"_s));
    testCompilation(prologue(fn("t1.v3f   = t2.v3f;"_s)), check("global\\d+\\.field\\d = global\\d+\\.field\\d;"_s));
    testCompilation(prologue(fn("t1.v4f   = t2.v4f;"_s)), check("global\\d+\\.field\\d = global\\d+\\.field\\d;"_s));
    testCompilation(prologue(fn("t1.v2u   = t2.v2u;"_s)), check("global\\d+\\.field\\d = global\\d+\\.field\\d;"_s));
    testCompilation(prologue(fn("t1.v3u   = t2.v3u;"_s)), check("global\\d+\\.field\\d = global\\d+\\.field\\d;"_s));
    testCompilation(prologue(fn("t1.v4u   = t2.v4u;"_s)), check("global\\d+\\.field\\d = global\\d+\\.field\\d;"_s));
    testCompilation(prologue(fn("t1.f     = t2.f;"_s)), check("global\\d+\\.field\\d = global\\d+\\.field\\d;"_s));
    testCompilation(prologue(fn("t1.u     = t2.u;"_s)), check("global\\d+\\.field\\d = global\\d+\\.field\\d;"_s));

    testCompilation(prologue(fn("t2.v2f.x = t.v2f.x;"_s)), check("global\\d+\\.field\\d\\.x = global\\d+\\.field\\d\\.x;"_s));
    testCompilation(prologue(fn("t2.v3f.x = t.v3f.x;"_s)), check("global\\d+\\.field\\d\\.x = global\\d+\\.field\\d\\.x;"_s));
    testCompilation(prologue(fn("t2.v4f.x = t.v4f.x;"_s)), check("global\\d+\\.field\\d\\.x = global\\d+\\.field\\d\\.x;"_s));
    testCompilation(prologue(fn("t2.v2u.x = t.v2u.x;"_s)), check("global\\d+\\.field\\d\\.x = global\\d+\\.field\\d\\.x;"_s));
    testCompilation(prologue(fn("t2.v3u.x = t.v3u.x;"_s)), check("global\\d+\\.field\\d\\.x = global\\d+\\.field\\d\\.x;"_s));
    testCompilation(prologue(fn("t2.v4u.x = t.v4u.x;"_s)), check("global\\d+\\.field\\d\\.x = global\\d+\\.field\\d\\.x;"_s));
    testCompilation(prologue(fn("t2.v2f   = t.v2f;"_s)), check("global\\d+\\.field\\d = global\\d+\\.field\\d;"_s));
    testCompilation(prologue(fn("t2.v3f   = t.v3f;"_s)), check("global\\d+\\.field\\d = global\\d+\\.field\\d;"_s));
    testCompilation(prologue(fn("t2.v4f   = t.v4f;"_s)), check("global\\d+\\.field\\d = global\\d+\\.field\\d;"_s));
    testCompilation(prologue(fn("t2.v2u   = t.v2u;"_s)), check("global\\d+\\.field\\d = global\\d+\\.field\\d;"_s));
    testCompilation(prologue(fn("t2.v3u   = t.v3u;"_s)), check("global\\d+\\.field\\d = global\\d+\\.field\\d;"_s));
    testCompilation(prologue(fn("t2.v4u   = t.v4u;"_s)), check("global\\d+\\.field\\d = global\\d+\\.field\\d;"_s));
    testCompilation(prologue(fn("t2.f     = t.f;"_s)), check("global\\d+\\.field\\d = global\\d+\\.field\\d"_s));
    testCompilation(prologue(fn("t2.u     = t.u;"_s)), check("global\\d+\\.field\\d = global\\d+\\.field\\d"_s));


    // Test index access
    testCompilation(prologue(makeString(
        "@group(0) @binding(9) var<storage, read_write> index: u32;"_s,
        fn(
            "var at = at1;"
            "at[0] = at1[0];"
            "at1[0] = at2[0];"
            "at2[0] = at[0];"
            "at2[index] = at[index];"_s))),
        check("local\\d+ = __unpack\\(global\\d+\\);"_s),
        check("local\\d+\\[__wgslMin\\(unsigned\\(0\\), \\(2u - 1u\\)\\)\\] = __unpack\\(global\\d+\\[__wgslMin\\(unsigned\\(0\\), \\(2u - 1u\\)\\)\\]\\);"_s),
        check("global\\d+\\[__wgslMin\\(unsigned\\(0\\), \\(2u - 1u\\)\\)\\] = global\\d+\\[__wgslMin\\(unsigned\\(0\\), \\(2u - 1u\\)\\)\\];"_s),
        check("global\\d+\\[__wgslMin\\(unsigned\\(0\\), \\(2u - 1u\\)\\)\\] = __pack\\(local\\d+\\[__wgslMin\\(unsigned\\(0\\), \\(2u - 1u\\)\\)\\]\\);"_s),
        check("global\\d+\\[__wgslMin\\(unsigned\\(global\\d+\\), \\(2u - 1u\\)\\)\\] = __pack\\(local\\d+\\[__wgslMin\\(unsigned\\(global\\d+\\), \\(2u - 1u\\)\\)\\]\\);"_s));

    // Test binary operations
    testCompilation(prologue(fn("t.v2f.x = 2 * t1.v2f.x;"_s)), check("global\\d+\\.field\\d\\.x = \\(2. \\* global\\d+\\.field\\d\\.x\\);"_s));
    testCompilation(prologue(fn("t.v3f.x = 2 * t1.v3f.x;"_s)), check("global\\d+\\.field\\d\\.x = \\(2. \\* global\\d+\\.field\\d\\.x\\);"_s));
    testCompilation(prologue(fn("t.v4f.x = 2 * t1.v4f.x;"_s)), check("global\\d+\\.field\\d\\.x = \\(2. \\* global\\d+\\.field\\d\\.x\\);"_s));
    testCompilation(prologue(fn("t.v2u.x = 2 * t1.v2u.x;"_s)), check("global\\d+\\.field\\d\\.x = \\(2u \\* global\\d+\\.field\\d\\.x\\);"_s));
    testCompilation(prologue(fn("t.v3u.x = 2 * t1.v3u.x;"_s)), check("global\\d+\\.field\\d\\.x = \\(2u \\* global\\d+\\.field\\d\\.x\\);"_s));
    testCompilation(prologue(fn("t.v4u.x = 2 * t1.v4u.x;"_s)), check("global\\d+\\.field\\d\\.x = \\(2u \\* global\\d+\\.field\\d\\.x\\);"_s));
    testCompilation(prologue(fn("t.v2f   = 2 * t1.v2f;"_s)), check("global\\d+\\.field\\d = \\(2. \\* global\\d+\\.field\\d\\);"_s));
    testCompilation(prologue(fn("t.v3f   = 2 * t1.v3f;"_s)), check("global\\d+\\.field\\d = \\(2. \\* __unpack\\(global\\d+\\.field\\d\\)\\);"_s));
    testCompilation(prologue(fn("t.v4f   = 2 * t1.v4f;"_s)), check("global\\d+\\.field\\d = \\(2. \\* global\\d+\\.field\\d\\);"_s));
    testCompilation(prologue(fn("t.v2u   = 2 * t1.v2u;"_s)), check("global\\d+\\.field\\d = \\(2u \\* global\\d+\\.field\\d\\);"_s));
    testCompilation(prologue(fn("t.v3u   = 2 * t1.v3u;"_s)), check("global\\d+\\.field\\d = \\(2u \\* __unpack\\(global\\d+\\.field\\d\\)\\);"_s));
    testCompilation(prologue(fn("t.v4u   = 2 * t1.v4u;"_s)), check("global\\d+\\.field\\d = \\(2u \\* global\\d+\\.field\\d\\);"_s));
    testCompilation(prologue(fn("t.f     = 2 * t1.f;"_s)), check("global\\d+\\.field\\d = \\(2. \\* global\\d+\\.field\\d\\);"_s));
    testCompilation(prologue(fn("t.u     = 2 * t1.u;"_s)), check("global\\d+\\.field\\d = \\(2u \\* global\\d+\\.field\\d\\);"_s));

    testCompilation(prologue(fn("t1.v2f.x = 2 * t2.v2f.x;"_s)), check("global\\d+\\.field\\d\\.x = \\(2. \\* global\\d+\\.field\\d\\.x\\);"_s));
    testCompilation(prologue(fn("t1.v3f.x = 2 * t2.v3f.x;"_s)), check("global\\d+\\.field\\d\\.x = \\(2. \\* global\\d+\\.field\\d\\.x\\);"_s));
    testCompilation(prologue(fn("t1.v4f.x = 2 * t2.v4f.x;"_s)), check("global\\d+\\.field\\d\\.x = \\(2. \\* global\\d+\\.field\\d\\.x\\);"_s));
    testCompilation(prologue(fn("t1.v2u.x = 2 * t2.v2u.x;"_s)), check("global\\d+\\.field\\d\\.x = \\(2u \\* global\\d+\\.field\\d\\.x\\);"_s));
    testCompilation(prologue(fn("t1.v3u.x = 2 * t2.v3u.x;"_s)), check("global\\d+\\.field\\d\\.x = \\(2u \\* global\\d+\\.field\\d\\.x\\);"_s));
    testCompilation(prologue(fn("t1.v4u.x = 2 * t2.v4u.x;"_s)), check("global\\d+\\.field\\d\\.x = \\(2u \\* global\\d+\\.field\\d\\.x\\);"_s));
    testCompilation(prologue(fn("t1.v2f   = 2 * t2.v2f;"_s)), check("global\\d+\\.field\\d = \\(2. \\* global\\d+\\.field\\d\\);"_s));
    testCompilation(prologue(fn("t1.v3f   = 2 * t2.v3f;"_s)), check("global\\d+\\.field\\d = \\(2. \\* __unpack\\(global\\d+\\.field\\d\\)\\);"_s));
    testCompilation(prologue(fn("t1.v4f   = 2 * t2.v4f;"_s)), check("global\\d+\\.field\\d = \\(2. \\* global\\d+\\.field\\d\\);"_s));
    testCompilation(prologue(fn("t1.v2u   = 2 * t2.v2u;"_s)), check("global\\d+\\.field\\d = \\(2u \\* global\\d+\\.field\\d\\);"_s));
    testCompilation(prologue(fn("t1.v3u   = 2 * t2.v3u;"_s)), check("global\\d+\\.field\\d = \\(2u \\* __unpack\\(global\\d+\\.field\\d\\)\\);"_s));
    testCompilation(prologue(fn("t1.v4u   = 2 * t2.v4u;"_s)), check("global\\d+\\.field\\d = \\(2u \\* global\\d+\\.field\\d\\);"_s));
    testCompilation(prologue(fn("t1.f     = 2 * t2.f;"_s)), check("global\\d+\\.field\\d = \\(2. \\* global\\d+\\.field\\d\\);"_s));
    testCompilation(prologue(fn("t1.u     = 2 * t2.u;"_s)), check("global\\d+\\.field\\d = \\(2u \\* global\\d+\\.field\\d\\);"_s));

    testCompilation(prologue(fn("t2.v2f.x = 2 * t.v2f.x;"_s)), check("global\\d+\\.field\\d\\.x = \\(2. \\* global\\d+\\.field\\d\\.x\\);"_s));
    testCompilation(prologue(fn("t2.v3f.x = 2 * t.v3f.x;"_s)), check("global\\d+\\.field\\d\\.x = \\(2. \\* global\\d+\\.field\\d\\.x\\);"_s));
    testCompilation(prologue(fn("t2.v4f.x = 2 * t.v4f.x;"_s)), check("global\\d+\\.field\\d\\.x = \\(2. \\* global\\d+\\.field\\d\\.x\\);"_s));
    testCompilation(prologue(fn("t2.v2u.x = 2 * t.v2u.x;"_s)), check("global\\d+\\.field\\d\\.x = \\(2u \\* global\\d+\\.field\\d\\.x\\);"_s));
    testCompilation(prologue(fn("t2.v3u.x = 2 * t.v3u.x;"_s)), check("global\\d+\\.field\\d\\.x = \\(2u \\* global\\d+\\.field\\d\\.x\\);"_s));
    testCompilation(prologue(fn("t2.v4u.x = 2 * t.v4u.x;"_s)), check("global\\d+\\.field\\d\\.x = \\(2u \\* global\\d+\\.field\\d\\.x\\);"_s));
    testCompilation(prologue(fn("t2.v2f   = 2 * t.v2f;"_s)), check("global\\d+\\.field\\d = \\(2. \\* global\\d+\\.field\\d\\);"_s));
    testCompilation(prologue(fn("t2.v3f   = 2 * t.v3f;"_s)), check("global\\d+\\.field\\d = \\(2. \\* global\\d+\\.field\\d\\);"_s));
    testCompilation(prologue(fn("t2.v4f   = 2 * t.v4f;"_s)), check("global\\d+\\.field\\d = \\(2. \\* global\\d+\\.field\\d\\);"_s));
    testCompilation(prologue(fn("t2.v2u   = 2 * t.v2u;"_s)), check("global\\d+\\.field\\d = \\(2u \\* global\\d+\\.field\\d\\);"_s));
    testCompilation(prologue(fn("t2.v3u   = 2 * t.v3u;"_s)), check("global\\d+\\.field\\d = \\(2u \\* global\\d+\\.field\\d\\);"_s));
    testCompilation(prologue(fn("t2.v4u   = 2 * t.v4u;"_s)), check("global\\d+\\.field\\d = \\(2u \\* global\\d+\\.field\\d\\);"_s));
    testCompilation(prologue(fn("t2.f     = 2 * t.f;"_s)), check("global\\d+\\.field\\d = \\(2. \\* global\\d+\\.field\\d\\);"_s));
    testCompilation(prologue(fn("t2.u     = 2 * t.u;"_s)), check("global\\d+\\.field\\d = \\(2u \\* global\\d+\\.field\\d\\);"_s));

    // Test unary operations

    testCompilation(prologue(fn("t.v2f.x = -t1.v2f.x;"_s)), check("global\\d+\\.field\\d\\.x = \\(-global\\d+\\.field\\d\\.x\\);"_s));
    testCompilation(prologue(fn("t.v3f.x = -t1.v3f.x;"_s)), check("global\\d+\\.field\\d\\.x = \\(-global\\d+\\.field\\d\\.x\\);"_s));
    testCompilation(prologue(fn("t.v4f.x = -t1.v4f.x;"_s)), check("global\\d+\\.field\\d\\.x = \\(-global\\d+\\.field\\d\\.x\\);"_s));
    testCompilation(prologue(fn("t.v2f   = -t1.v2f;"_s)), check("global\\d+\\.field\\d = \\(-global\\d+\\.field\\d\\);"_s));
    testCompilation(prologue(fn("t.v3f   = -t1.v3f;"_s)), check("global\\d+\\.field\\d = \\(-__unpack\\(global\\d+\\.field\\d\\)\\);"_s));
    testCompilation(prologue(fn("t.v4f   = -t1.v4f;"_s)), check("global\\d+\\.field\\d = \\(-global\\d+\\.field\\d\\);"_s));
    testCompilation(prologue(fn("t.f     = -t1.f;"_s)), check("global\\d+\\.field\\d = \\(-global\\d+\\.field\\d\\);"_s));

    testCompilation(prologue(fn("t1.v2f.x = -t2.v2f.x;"_s)), check("global\\d+\\.field\\d\\.x = \\(-global\\d+\\.field\\d\\.x\\);"_s));
    testCompilation(prologue(fn("t1.v3f.x = -t2.v3f.x;"_s)), check("global\\d+\\.field\\d\\.x = \\(-global\\d+\\.field\\d\\.x\\);"_s));
    testCompilation(prologue(fn("t1.v4f.x = -t2.v4f.x;"_s)), check("global\\d+\\.field\\d\\.x = \\(-global\\d+\\.field\\d\\.x\\);"_s));
    testCompilation(prologue(fn("t1.v2f   = -t2.v2f;"_s)), check("global\\d+\\.field\\d = \\(-global\\d+\\.field\\d\\);"_s));
    testCompilation(prologue(fn("t1.v3f   = -t2.v3f;"_s)), check("global\\d+\\.field\\d = \\(-__unpack\\(global\\d+\\.field\\d\\)\\);"_s));
    testCompilation(prologue(fn("t1.v4f   = -t2.v4f;"_s)), check("global\\d+\\.field\\d = \\(-global\\d+\\.field\\d\\);"_s));
    testCompilation(prologue(fn("t1.f     = -t2.f;"_s)), check("global\\d+\\.field\\d = \\(-global\\d+\\.field\\d\\);"_s));

    testCompilation(prologue(fn("t2.v2f.x = -t.v2f.x;"_s)), check("global\\d+\\.field\\d\\.x = \\(-global\\d+\\.field\\d\\.x\\);"_s));
    testCompilation(prologue(fn("t2.v3f.x = -t.v3f.x;"_s)), check("global\\d+\\.field\\d\\.x = \\(-global\\d+\\.field\\d\\.x\\);"_s));
    testCompilation(prologue(fn("t2.v4f.x = -t.v4f.x;"_s)), check("global\\d+\\.field\\d\\.x = \\(-global\\d+\\.field\\d\\.x\\);"_s));
    testCompilation(prologue(fn("t2.v2f   = -t.v2f;"_s)), check("global\\d+\\.field\\d = \\(-global\\d+\\.field\\d\\);"_s));
    testCompilation(prologue(fn("t2.v3f   = -t.v3f;"_s)), check("global\\d+\\.field\\d = \\(-global\\d+\\.field\\d\\);"_s));
    testCompilation(prologue(fn("t2.v4f   = -t.v4f;"_s)), check("global\\d+\\.field\\d = \\(-global\\d+\\.field\\d\\);"_s));
    testCompilation(prologue(fn("t2.f     = -t.f;"_s)), check("global\\d+\\.field\\d = \\(-global\\d+\\.field\\d\\);"_s));

    // Test function calls
    testCompilation(prologue(fn("t.v2f.x = abs(t1.v2f.x);"_s)), check("global\\d+\\.field\\d\\.x = abs\\(global\\d+\\.field\\d\\.x\\);"_s));
    testCompilation(prologue(fn("t.v3f.x = abs(t1.v3f.x);"_s)), check("global\\d+\\.field\\d\\.x = abs\\(global\\d+\\.field\\d\\.x\\);"_s));
    testCompilation(prologue(fn("t.v4f.x = abs(t1.v4f.x);"_s)), check("global\\d+\\.field\\d\\.x = abs\\(global\\d+\\.field\\d\\.x\\);"_s));
    testCompilation(prologue(fn("t.v2u.x = abs(t1.v2u.x);"_s)), check("global\\d+\\.field\\d\\.x = abs\\(global\\d+\\.field\\d\\.x\\);"_s));
    testCompilation(prologue(fn("t.v3u.x = abs(t1.v3u.x);"_s)), check("global\\d+\\.field\\d\\.x = abs\\(global\\d+\\.field\\d\\.x\\);"_s));
    testCompilation(prologue(fn("t.v4u.x = abs(t1.v4u.x);"_s)), check("global\\d+\\.field\\d\\.x = abs\\(global\\d+\\.field\\d\\.x\\);"_s));
    testCompilation(prologue(fn("t.v2f   = abs(t1.v2f);"_s)), check("global\\d+\\.field\\d = abs\\(global\\d+\\.field\\d\\);"_s));
    testCompilation(prologue(fn("t.v3f   = abs(t1.v3f);"_s)), check("global\\d+\\.field\\d = abs\\(__unpack\\(global\\d+\\.field\\d\\)\\);"_s));
    testCompilation(prologue(fn("t.v4f   = abs(t1.v4f);"_s)), check("global\\d+\\.field\\d = abs\\(global\\d+\\.field\\d\\);"_s));
    testCompilation(prologue(fn("t.v2u   = abs(t1.v2u);"_s)), check("global\\d+\\.field\\d = abs\\(global\\d+\\.field\\d\\);"_s));
    testCompilation(prologue(fn("t.v3u   = abs(t1.v3u);"_s)), check("global\\d+\\.field\\d = abs\\(__unpack\\(global\\d+\\.field\\d\\)\\);"_s));
    testCompilation(prologue(fn("t.v4u   = abs(t1.v4u);"_s)), check("global\\d+\\.field\\d = abs\\(global\\d+\\.field\\d\\);"_s));
    testCompilation(prologue(fn("t.f     = abs(t1.f);"_s)), check("global\\d+\\.field\\d = abs\\(global\\d+\\.field\\d\\);"_s));
    testCompilation(prologue(fn("t.u     = abs(t1.u);"_s)), check("global\\d+\\.field\\d = abs\\(global\\d+\\.field\\d\\);"_s));

    testCompilation(prologue(fn("t1.v2f.x = abs(t2.v2f.x);"_s)), check("global\\d+\\.field\\d\\.x = abs\\(global\\d+\\.field\\d\\.x\\);"_s));
    testCompilation(prologue(fn("t1.v3f.x = abs(t2.v3f.x);"_s)), check("global\\d+\\.field\\d\\.x = abs\\(global\\d+\\.field\\d\\.x\\);"_s));
    testCompilation(prologue(fn("t1.v4f.x = abs(t2.v4f.x);"_s)), check("global\\d+\\.field\\d\\.x = abs\\(global\\d+\\.field\\d\\.x\\);"_s));
    testCompilation(prologue(fn("t1.v2u.x = abs(t2.v2u.x);"_s)), check("global\\d+\\.field\\d\\.x = abs\\(global\\d+\\.field\\d\\.x\\);"_s));
    testCompilation(prologue(fn("t1.v3u.x = abs(t2.v3u.x);"_s)), check("global\\d+\\.field\\d\\.x = abs\\(global\\d+\\.field\\d\\.x\\);"_s));
    testCompilation(prologue(fn("t1.v4u.x = abs(t2.v4u.x);"_s)), check("global\\d+\\.field\\d\\.x = abs\\(global\\d+\\.field\\d\\.x\\);"_s));
    testCompilation(prologue(fn("t1.v2f   = abs(t2.v2f);"_s)), check("global\\d+\\.field\\d = abs\\(global\\d+\\.field\\d\\);"_s));
    testCompilation(prologue(fn("t1.v3f   = abs(t2.v3f);"_s)), check("global\\d+\\.field\\d = abs\\(__unpack\\(global\\d+\\.field\\d\\)\\);"_s));
    testCompilation(prologue(fn("t1.v4f   = abs(t2.v4f);"_s)), check("global\\d+\\.field\\d = abs\\(global\\d+\\.field\\d\\);"_s));
    testCompilation(prologue(fn("t1.v2u   = abs(t2.v2u);"_s)), check("global\\d+\\.field\\d = abs\\(global\\d+\\.field\\d\\);"_s));
    testCompilation(prologue(fn("t1.v3u   = abs(t2.v3u);"_s)), check("global\\d+\\.field\\d = abs\\(__unpack\\(global\\d+\\.field\\d\\)\\);"_s));
    testCompilation(prologue(fn("t1.v4u   = abs(t2.v4u);"_s)), check("global\\d+\\.field\\d = abs\\(global\\d+\\.field\\d\\);"_s));
    testCompilation(prologue(fn("t1.f     = abs(t2.f);"_s)), check("global\\d+\\.field\\d = abs\\(global\\d+\\.field\\d\\);"_s));
    testCompilation(prologue(fn("t1.u     = abs(t2.u);"_s)), check("global\\d+\\.field\\d = abs\\(global\\d+\\.field\\d\\);"_s));

    testCompilation(prologue(fn("t2.v2f.x = abs(t.v2f.x);"_s)), check("global\\d+\\.field\\d\\.x = abs\\(global\\d+\\.field\\d\\.x\\);"_s));
    testCompilation(prologue(fn("t2.v3f.x = abs(t.v3f.x);"_s)), check("global\\d+\\.field\\d\\.x = abs\\(global\\d+\\.field\\d\\.x\\);"_s));
    testCompilation(prologue(fn("t2.v4f.x = abs(t.v4f.x);"_s)), check("global\\d+\\.field\\d\\.x = abs\\(global\\d+\\.field\\d\\.x\\);"_s));
    testCompilation(prologue(fn("t2.v2u.x = abs(t.v2u.x);"_s)), check("global\\d+\\.field\\d\\.x = abs\\(global\\d+\\.field\\d\\.x\\);"_s));
    testCompilation(prologue(fn("t2.v3u.x = abs(t.v3u.x);"_s)), check("global\\d+\\.field\\d\\.x = abs\\(global\\d+\\.field\\d\\.x\\);"_s));
    testCompilation(prologue(fn("t2.v4u.x = abs(t.v4u.x);"_s)), check("global\\d+\\.field\\d\\.x = abs\\(global\\d+\\.field\\d\\.x\\);"_s));
    testCompilation(prologue(fn("t2.v2f   = abs(t.v2f);"_s)), check("global\\d+\\.field\\d = abs\\(global\\d+\\.field\\d\\);"_s));
    testCompilation(prologue(fn("t2.v3f   = abs(t.v3f);"_s)), check("global\\d+\\.field\\d = abs\\(global\\d+\\.field\\d\\);"_s));
    testCompilation(prologue(fn("t2.v4f   = abs(t.v4f);"_s)), check("global\\d+\\.field\\d = abs\\(global\\d+\\.field\\d\\);"_s));
    testCompilation(prologue(fn("t2.v2u   = abs(t.v2u);"_s)), check("global\\d+\\.field\\d = abs\\(global\\d+\\.field\\d\\);"_s));
    testCompilation(prologue(fn("t2.v3u   = abs(t.v3u);"_s)), check("global\\d+\\.field\\d = abs\\(global\\d+\\.field\\d\\);"_s));
    testCompilation(prologue(fn("t2.v4u   = abs(t.v4u);"_s)), check("global\\d+\\.field\\d = abs\\(global\\d+\\.field\\d\\);"_s));
    testCompilation(prologue(fn("t2.f     = abs(t.f);"_s)), check("global\\d+\\.field\\d = abs\\(global\\d+\\.field\\d\\);"_s));
    testCompilation(prologue(fn("t2.u     = abs(t.u);"_s)), check("global\\d+\\.field\\d = abs\\(global\\d+\\.field\\d\\);"_s));

    // Test runtime array
    testCompilation(prologue(makeString(
        "struct S {"
        "    @size(16) x: i32,"
        "    @align(32) y: array<i32>,"
        "}"
        "@group(0) @binding(10) var<storage, read_write> s: S;"_s,
        fn("s.x = 0; s.y[0] = 0;"_s))));

    // Test packed vec3 compound assignment
    testCompilation(prologue(makeString(
        "@group(0) @binding(11) var<storage,read_write> D: array<vec3f>;"_s,
        fn("D[0] += vec3f(1);"_s))));

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
        fn("var x = global.x.x;"_s))));
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

TEST_F(WGSLMetalCompilationTests, VarInitialization)
{
    testCompilation(
        "fn f(x: f32) { }"
        "@compute @workgroup_size(1)"
        "fn main() {"
        "    var a = 1.0;"
        "    var b = a;"
        "    b = 0.0;"
        "    f(b);"
        "}"_s,
        check("float local\\d+ = 1"_s),
        check("float local\\d+ = local\\d"_s),
        check("local\\d+ = 0"_s),
        check("function\\d\\(local\\d+\\)"_s));
}

TEST_F(WGSLMetalCompilationTests, While)
{
    testCompilation(file("while.wgsl"_s));
}

}
