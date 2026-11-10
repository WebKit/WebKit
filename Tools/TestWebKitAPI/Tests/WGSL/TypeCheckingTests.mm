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

#include "config.h"
#include "AST.h"
#include "Lexer.h"
#include "Parser.h"
#include "TestWGSLAPI.h"
#include <wtf/Assertions.h>

namespace TestWGSLAPI {

inline void expectTypeError(StringImpl* wgsl, StringImpl* errorMessage)
{
    auto result = WGSL::staticCheck(wgsl, std::nullopt, {
        .maxBuffersPlusVertexBuffersForVertexStage = 8,
        .maxBuffersForFragmentStage = 8,
        .maxBuffersForComputeStage = 8,
        .maximumCombinedWorkgroupVariablesSize = 16384,
        .supportedFeatures = { "shader-f16"_s, "clip-distances"_s, "primitive-index"_s }
    });
    EXPECT_TRUE(std::holds_alternative<WGSL::FailedCheck>(result));
    if (!std::holds_alternative<WGSL::FailedCheck>(result))
        return;
    auto error = std::get<WGSL::FailedCheck>(result);
    EXPECT_EQ(error.errors.size(), 1u);
    EXPECT_EQ(error.errors[0].message(), errorMessage);
}

inline void expectNoError(StringImpl* wgsl)
{
    auto result = WGSL::staticCheck(wgsl, std::nullopt, { 8 });
    EXPECT_TRUE(std::holds_alternative<WGSL::SuccessfulCheck>(result));
}


TEST(WGSLTypeCheckingTests, Array)
{
    expectTypeError("var<private> a:array;"_si, "'array' requires at least 1 template argument"_si);
    expectTypeError("@group(0) @binding(0) var<storage, read_write> b: array<u32, (1<<32)>;"_si, "value 4294967296 cannot be represented as 'i32'"_si);

    expectTypeError(fn("let x = array<i32, 0>();"_si), "array count must be greater than 0"_si);
    expectTypeError(fn("let x = array<i32, -1>();"_si), "array count must be greater than 0"_si);

    expectTypeError(fn("let x = array<i32, 2>(0);"_si), "array constructor has too few elements: expected 2, found 1"_si);
    expectTypeError(fn("let x = array<i32, 1>(0, 0);"_si), "array constructor has too many elements: expected 1, found 2"_si);

    expectTypeError(fn("let x = array<i32, 65536>();"_si), "array count (65536) must be less than 65536"_si);

    expectTypeError(fn("let x = array<i32>(0);"_si), "cannot construct a runtime-sized array"_si);

    expectTypeError(fn("let x = array<i32, 1>(0.0);"_si), "'<AbstractFloat>' cannot be used to construct an array of 'i32'"_si);

    expectTypeError(fn("let x = array();"_si), "cannot infer array element type from constructor"_si);

    expectTypeError(fn("let x = array(0, 0.0, 0u);"_si), "cannot infer common array element type from constructor arguments"_si);

    expectTypeError(fn("let x = array<i2, 1>(0.0);"_si), "unresolved type 'i2'"_si);

    expectTypeError(StringImpl::create("override elementCount = 4;"
        "fn testOverrideElementCount() {"
            "let xl = array<i32, elementCount>(0.0);"
        "}"_s).ptr(), "array must have constant size in order to be constructed"_si);
}

TEST(WGSLTypeCheckingTests, Attributes)
{
    expectTypeError("@id(0) override z: array<i32, 1>;"_si, "'array<i32, 1>' cannot be used as the type of an 'override'"_si);
    expectTypeError("@id(u32(dpdx(0))) override y: i32;"_si, "cannot call function from constant context"_si);

    expectTypeError("struct S { @align(4.0) x: i32, }"_si, "@align must be an i32 or u32 value"_si);
    expectTypeError("struct S { @location(0.0) y: i32, }"_si, "@location must be an i32 or u32 value"_si);
    expectTypeError("struct S { @size(4.0) z: i32, }"_si, "@size must be an i32 or u32 value"_si);
    expectTypeError("struct S { @size(2) x: i32, }"_si, "@size value must be at least the byte-size of the type of the member"_si);
    expectTypeError("struct S { @size(-1) y: i32, }"_si, "@size value must be non-negative"_si);
    expectTypeError("struct S { @align(-1) z: i32, }"_si, "@align value must be positive"_si);
    expectTypeError("struct S { @align(3) w: i32, }"_si, "@align value must be a power of two"_si);

    // Type checking @group, @binding and @workgroup_size
    expectTypeError("@group(0.0) @binding(0) var x : i32;"_si, "@group must be an i32 or u32 value"_si);
    expectTypeError("@group(0) @binding(0.0) var y : i32;"_si, "@binding must be an i32 or u32 value"_si);
    expectTypeError("@id(0.0) override z : i32;"_si, "@id must be an i32 or u32 value"_si);
    expectTypeError("@compute @workgroup_size(1.0) fn f1() { }"_si, "@workgroup_size x dimension must be an i32 or u32 value"_si);
    expectTypeError("@compute @workgroup_size(1, 1.0) fn f2() { }"_si, "@workgroup_size y dimension must be an i32 or u32 value"_si);
    expectTypeError("@compute @workgroup_size(1, 1, 1.0) fn f3() { }"_si, "@workgroup_size z dimension must be an i32 or u32 value"_si);
    expectTypeError("@compute @workgroup_size(1i, 1u) fn f4() { }"_si, "@workgroup_size arguments must be of the same type, either i32 or u32"_si);
    expectTypeError("@compute @workgroup_size(1, 1i, 1u) fn f5() { }"_si, "@workgroup_size arguments must be of the same type, either i32 or u32"_si);
    expectTypeError("@compute @workgroup_size(1, 1u, 1i) fn f6() { }"_si, "@workgroup_size arguments must be of the same type, either i32 or u32"_si);


    // Attribute validation
    expectTypeError("@group(0) var<private> x: i32;"_si, "@group attribute must only be applied to resource variables"_si);
    expectTypeError("@group(-1) var<uniform> x: i32;"_si, "@group value must be non-negative"_si);
    expectTypeError("@binding(0) var<private> x: i32;"_si, "@binding attribute must only be applied to resource variables"_si);
    expectTypeError("@binding(-1) var<uniform> x: i32;"_si, "@binding value must be non-negative"_si);
    expectTypeError("@id(-1) var<private> y: i32;"_si, "@id attribute must only be applied to override variables"_si);
    expectTypeError("@id(-1) override z: i32;"_si, "@id value must be non-negative"_si);
    expectTypeError("@must_use fn mustUseWithoutReturnType() { }"_si, "@must_use can only be applied to functions that return a value"_si);
    expectTypeError("fn f1() -> @builtin(position) i32 { return 0; }"_si, "@builtin is not valid for non-entry point function types"_si);
    expectTypeError("fn f2() -> @location(0) i32 { return 0; }"_si, "@location is not valid for non-entry point function types"_si);
    expectTypeError("@fragment fn f3() -> @location(-1) i32 { return 0; }"_si, "@location value must be non-negative"_si);
    expectTypeError("@compute fn f4() -> @location(0) i32 { return 0; }"_si, "@location may not be used in the compute shader stage"_si);
    expectTypeError("@fragment fn f5() -> @location(0) bool { return false; }"_si, "@location must only be applied to declarations of numeric scalar or numeric vector type"_si);
    expectTypeError("fn f6() -> @interpolate(flat) i32 { return 0; }"_si, "@interpolate is only allowed on declarations that have a @location attribute"_si);
    expectTypeError("fn f7() -> @invariant i32 { return 0; }"_si, "@invariant is only allowed on declarations that have a @builtin(position) attribute"_si);
    expectTypeError("@workgroup_size(1) fn f8() { }"_si, "@workgroup_size must only be applied to compute shader entry point function"_si);
    expectTypeError("@workgroup_size(-1) @compute fn f9() { }"_si, "@workgroup_size argument must be at least 1"_si);

    // check that we don't crash by trying to read the size of S2, which won't have been computed
    expectNoError(
        StringImpl::create("struct S1 { @size(16) x : f32 };"
        "struct S2 { @size(32) x: array<S1, 2>, };"_s).ptr());
}

TEST(WGSLTypeCheckingTests, AccessExpression)
{
    // Test index access
    expectTypeError(fn("_ = vec2(0)[1f];"_si), "index must be of type 'i32' or 'u32', found: 'f32'"_si);

    // Test swizzle access
    expectTypeError(fn("_ = vec2(0).z;"_si), "invalid vector swizzle member"_si);
    expectTypeError(fn("_ = vec2(0).w;"_si), "invalid vector swizzle member"_si);
    expectTypeError(fn("_ = vec2(0).b;"_si), "invalid vector swizzle member"_si);
    expectTypeError(fn("_ = vec2(0).a;"_si), "invalid vector swizzle member"_si);

    expectTypeError(fn("_ = vec3(0).w;"_si), "invalid vector swizzle member"_si);
    expectTypeError(fn("_ = vec3(0).a;"_si), "invalid vector swizzle member"_si);

    expectTypeError(fn("_ = vec2(0).rx;"_si), "invalid vector swizzle member"_si);

    expectTypeError(fn("_ = vec2(0).v;"_si), "invalid vector swizzle character"_si);

    expectTypeError(fn("var z: mat3x3<f32>; _ = z[1].e;"_si), "invalid vector swizzle character"_si);

    // The restriction below can be found in the spec for the following types:
    // - Vectors: https://www.w3.org/TR/WGSL/#vector-single-component
    // - Matrices: https://www.w3.org/TR/WGSL/#matrix-access-expr
    // - Arrays: https://www.w3.org/TR/WGSL/#array-access-expr
    //
    // NOTE: When an abstract vector value e is indexed by an expression that is not
    // a const-expression, then the vector is concretized before the index is applied.
    expectNoError(fn("let x: f32 = vec2(0)[0];"_si));
    expectTypeError(fn("let i = 0; let x: u32 = vec2(0)[i];"_si), "cannot initialize var of type 'u32' with value of type 'i32'"_si);
    expectTypeError(enableF16(fn("let i = 0; let x: vec2<f16> = mat2x2(0, 0, 0, 0)[i];"_si)), "cannot initialize var of type 'vec2<f16>' with value of type 'vec2<f32>'"_si);

    expectNoError(fn("let x: f32 = array(0)[0];"_si));
    expectTypeError(fn("let i = 0; let x: u32 = array(0)[i];"_si), "cannot initialize var of type 'u32' with value of type 'i32'"_si);

    // Test cannot write to multiple vector locations
    expectTypeError(fn("var v = vec2(0); v.xy = vec2(1);"_si), "cannot assign to a value of type 'vec2<i32>'"_si);
}

TEST(WGSLTypeCheckingTests, Atomics)
{
    expectTypeError("@group(0) @binding(0) var<storage, read_write> x : atomic<i32, i32>;"_si, "'atomic' requires 1 template argument"_si);
    expectTypeError("@group(0) @binding(1) var<storage, read_write> y : atomic<f32>;"_si, "atomic only supports i32 or u32 types"_si);
}

TEST(WGSLTypeCheckingTests, Bitcast)
{
    // Wrong number of arguments
    expectTypeError(fn("_ = bitcast<i32>(0, 0);"_si), "bitcast expects a single argument, found 2"_si);
    expectTypeError(fn("_ = bitcast<i32>();"_si), "bitcast expects a single argument, found 0"_si);

    // Wrong number of template arguments
    expectTypeError(fn("_ = bitcast<i32, i32>(0);"_si), "bitcast expects a single template argument, found 2"_si);
    expectTypeError(fn("_ = bitcast(0);"_si), "bitcast expects a single template argument, found 0"_si);

    // Invalid conversion
    expectTypeError(fn("_ = bitcast<vec2<i32>>(0);"_si), "cannot bitcast from 'i32' to 'vec2<i32>'"_si);
    expectTypeError(fn("_ = bitcast<i32>(vec2(0));"_si), "cannot bitcast from 'vec2<i32>' to 'i32'"_si);

    // I32 overflow
    expectTypeError(fn("const x: f32 = bitcast<f32>(4294967295);"_si), "value 4294967295 cannot be represented as 'i32'"_si);

    // Function as a value
    expectTypeError("fn f() { } fn g() { const x: f32 = bitcast<f32>(f); }"_si, "cannot use function 'f' as value"_si);

    // Types that cannot be concretized
    expectTypeError(
        "@group(0) @binding(1) var s: sampler; fn test() { _ = bitcast<i32>(s); }"_si,
        "cannot bitcast from 'sampler' to 'i32'"_si);
    expectTypeError(
        "@group(0) @binding(2) var t: texture_depth_2d; fn test() { _ = bitcast<i32>(t); }"_si,
        "cannot bitcast from 'texture_depth_2d' to 'i32'"_si);
}

TEST(WGSLTypeCheckingTests, Break)
{
    expectTypeError(fn("if true { break; }"_si), "break statement must be in a loop or switch case"_si);
}

TEST(WGSLTypeCheckingTests, Comments)
{
    expectTypeError("/*"_si, "Trying to parse a GlobalDecl, expected 'const', 'fn', 'override', 'struct' or 'var'."_si);
}

TEST(WGSLTypeCheckingTests, CompoundAssignment)
{
    expectTypeError(fn("var x = 1; x += 1f;"_si), "no matching overload for operator +(ref<function, i32, read_write>, f32)"_si);
    expectTypeError(fn("let x = 1; x += 1;"_si), "cannot assign to a value of type 'i32'"_si);
}

TEST(WGSLTypeCheckingTests, ConstAssert)
{
    expectTypeError("fn f() { } const_assert(f);"_si, "cannot use function 'f' as value"_si);

    // Top level
    expectTypeError("const_assert(1);"_si, "const assertion condition must be a bool, got '<AbstractInt>'"_si);
    expectTypeError("const_assert(undefined);"_si, "unresolved identifier 'undefined'"_si);
    expectTypeError("const_assert(vec2);"_si, "cannot use type 'vec2' as value"_si);
    expectTypeError("const_assert(i32);"_si, "cannot use type 'i32' as value"_si);
    expectTypeError("const_assert(sqrt);"_si, "unresolved identifier 'sqrt'"_si);
    expectTypeError("const_assert(1 > 2);"_si, "const assertion failed"_si);

    expectTypeError(
        StringImpl::create("const x = 1;"
        "const y = 2;"
        "const_assert(x > y);"_s).ptr(),
        "const assertion failed"_si);


    expectTypeError(
        StringImpl::create("const x = 1;"
        "var<private> z = 3;"
        "const_assert(x > z);"_s).ptr(),
        "cannot use runtime value in constant expression"_si);

    // Function level
    expectTypeError(fn("const_assert(1);"_si), "const assertion condition must be a bool, got '<AbstractInt>'"_si);
    expectTypeError(fn("const_assert(undefined);"_si), "unresolved identifier 'undefined'"_si);
    expectTypeError(fn("const_assert(vec2);"_si), "cannot use type 'vec2' as value"_si);
    expectTypeError(fn("const_assert(i32);"_si), "cannot use type 'i32' as value"_si);
    expectTypeError(fn("const_assert(sqrt);"_si), "unresolved identifier 'sqrt'"_si);
    expectTypeError(fn("const_assert(1 > 2);"_si), "const assertion failed"_si);

    expectTypeError(fn(
        StringImpl::create("const x = 1;"
        "const y = 2;"
        "const_assert(x > y);"_s).ptr()),
        "const assertion failed"_si);

    expectTypeError(fn(
        StringImpl::create("const x = 1;"
        "let z = 3;"
        "const_assert(x > z);"_s).ptr()),
        "cannot use runtime value in constant expression"_si);
}

TEST(WGSLTypeCheckingTests, Constants)
{
    // Test out of bounds
    expectTypeError(fn("const x = array(0); _ = x[-1];"_si), "index -1 is out of bounds [0..0]"_si);
    expectNoError(fn("const x = array(0); _ = x[0];"_si));
    expectTypeError(fn("const x = array(0); _ = x[1];"_si), "index 1 is out of bounds [0..0]"_si);

    expectTypeError(fn("const x = mat2x2(0, 0, 0, 0); _ = x[-1];"_si), "index -1 is out of bounds [0..1]"_si);
    expectNoError(fn("const x = mat2x2(0, 0, 0, 0); _ = x[0];"_si));
    expectNoError(fn("const x = mat2x2(0, 0, 0, 0); _ = x[1];"_si));
    expectTypeError(fn("const x = mat2x2(0, 0, 0, 0); _ = x[2];"_si), "index 2 is out of bounds [0..1]"_si);

    expectTypeError(fn("const x = vec2(0); _ = x[-1];"_si), "index -1 is out of bounds [0..1]"_si);
    expectNoError(fn("const x = vec2(0); _ = x[0];"_si));
    expectNoError(fn("const x = vec2(0);_ = x[1];"_si));
    expectTypeError(fn("const x = vec2(0);_ = x[2];"_si), "index 2 is out of bounds [0..1]"_si);

    // Test invalid explicit u32 conversion
    expectTypeError(fn("let x = u32(37359285590000);"_si), "value 37359285590000 cannot be represented as 'u32'"_si);

    // Test nested arrays
    expectNoError(fn("const c = array<array<u32, 1>, 1>(array<u32, 1>(0));"_si));
}

TEST(WGSLTypeCheckingTests, Continue)
{
    expectTypeError(fn(
        StringImpl::create("switch 1 {"
        "default: {"
        "   continue;"
        "}"
        "}"_s).ptr()), "continue statement must be in a loop"_si);
}

TEST(WGSLTypeCheckingTests, Division)
{
    // Test AbstractInt
    expectTypeError(fn("_ = 42 / 0;"_si), "invalid division by zero"_si);
    expectTypeError(fn("_ = 42 / vec2(1, 0);"_si), "invalid division by zero"_si);
    expectTypeError(fn("_ = vec2(42) / 0;"_si), "invalid division by zero"_si);
    expectTypeError(fn("_ = vec2(42) / vec2(1, 0);"_si), "invalid division by zero"_si);

    expectTypeError(fn("let x = 42; _ = x / 0;"_si), "invalid division by zero"_si);
    expectTypeError(fn("let x = 42; _ = x / vec2(1, 0);"_si), "invalid division by zero"_si);
    expectTypeError(fn("let x = 42; _ = vec2(x) / 0;"_si), "invalid division by zero"_si);
    expectTypeError(fn("let x = 42; _ = vec2(x) / vec2(1, 0);"_si), "invalid division by zero"_si);

    expectNoError(fn("_ = (-2147483647 - 1) / -1;"_si));
    expectNoError(fn("_ = vec2(-2147483647 - 1, 1) / vec2(-1, 1);"_si));

    expectTypeError(fn("_ = (-9223372036854775807 - 1) / -1;"_si), "invalid division overflow"_si);
    expectTypeError(fn("_ = vec2(-9223372036854775807 - 1, 1) / vec2(-1, 1);"_si), "invalid division overflow"_si);

    // Test i32
    expectTypeError(fn("_ = 42i / 0i;"_si), "invalid division by zero"_si);
    expectTypeError(fn("_ = 42i / vec2(1i, 0i);"_si), "invalid division by zero"_si);
    expectTypeError(fn("_ = vec2(42i) / 0i;"_si), "invalid division by zero"_si);
    expectTypeError(fn("_ = vec2(42i) / vec2(1i, 0i);"_si), "invalid division by zero"_si);

    expectTypeError(fn("let x = 42i; _ = x / 0i;"_si), "invalid division by zero"_si);
    expectTypeError(fn("let x = 42i; _ = x / vec2(1i, 0i);"_si), "invalid division by zero"_si);
    expectTypeError(fn("let x = 42i; _ = vec2(x) / 0i;"_si), "invalid division by zero"_si);
    expectTypeError(fn("let x = 42i; _ = vec2(x) / vec2(1i, 0i);"_si), "invalid division by zero"_si);

    expectTypeError(fn("_ = (-2147483647i - 1i) / -1i;"_si), "invalid division overflow"_si);
    expectTypeError(fn("_ = vec2(-2147483647i - 1i, 1i) / vec2(-1i, 1i);"_si), "invalid division overflow"_si);

    // Test u32
    expectTypeError(fn("_ = 42u / 0u;"_si), "invalid division by zero"_si);
    expectTypeError(fn("_ = 42u / vec2(1u, 0u);"_si), "invalid division by zero"_si);
    expectTypeError(fn("_ = vec2(42u) / 0u;"_si), "invalid division by zero"_si);
    expectTypeError(fn("_ = vec2(42u) / vec2(1u, 0u);"_si), "invalid division by zero"_si);

    expectTypeError(fn("let x = 42u; _ = x / 0u;"_si), "invalid division by zero"_si);
    expectTypeError(fn("let x = 42u; _ = x / vec2(1u, 0u);"_si), "invalid division by zero"_si);
    expectTypeError(fn("let x = 42u; _ = vec2(x) / 0u;"_si), "invalid division by zero"_si);
    expectTypeError(fn("let x = 42u; _ = vec2(x) / vec2(1u, 0u);"_si), "invalid division by zero"_si);

    // Test AbstractFloat
    expectTypeError(fn("_ = 42.0 / 0.0;"_si), "value Infinity cannot be represented as '<AbstractFloat>'"_si);
    expectTypeError(fn("_ = 42.0 / vec2(1.0, 0.0);"_si), "value vec2(42, Infinity) cannot be represented as 'vec2<<AbstractFloat>>'"_si);
    expectTypeError(fn("_ = vec2(42.0) / 0.0;"_si), "value vec2(Infinity, Infinity) cannot be represented as 'vec2<<AbstractFloat>>'"_si);
    expectTypeError(fn("_ = vec2(42.0) / vec2(1.0, 0.0);"_si), "value vec2(42, Infinity) cannot be represented as 'vec2<<AbstractFloat>>'"_si);

    expectNoError(fn("let x = 42.0; _ = x / 0.0;"_si));
    expectNoError(fn("let x = 42.0; _ = x / vec2(1.0, 0.0);"_si));
    expectNoError(fn("let x = 42.0; _ = vec2(x) / 0.0;"_si));
    expectNoError(fn("let x = 42.0; _ = vec2(x) / vec2(1.0, 0.0);"_si));

    expectNoError(fn("_ = (-2147483647.0 - 1.0) / -1.0;"_si));
    expectNoError(fn("_ = vec2(-2147483647.0 - 1.0, 1.0) / vec2(-1.0, 1.0);"_si));

    expectNoError(fn("_ = (-9223372036854775807.0 - 1.0) / -1.0;"_si));
    expectNoError(fn("_ = vec2(-9223372036854775807.0 - 1.0, 1.0) / vec2(-1.0, 1.0);"_si));

    expectNoError(fn("_ = -340282346638528859811704183484516925440.0 - 1.0 / -1.0;"_si));
    expectNoError(fn("_ = vec2(-340282346638528859811704183484516925440.0 - 1.0, 1.0) / vec2(-1.0, 1.0);"_si));

    // Test f32
    expectTypeError(fn("_ = 42f / 0f;"_si), "value Infinity cannot be represented as 'f32'"_si);
    expectTypeError(fn("_ = 42f / vec2(1f, 0f);"_si), "value vec2(42f, Infinity) cannot be represented as 'vec2<f32>'"_si);
    expectTypeError(fn("_ = vec2(42f) / 0f;"_si), "value vec2(Infinity, Infinity) cannot be represented as 'vec2<f32>'"_si);
    expectTypeError(fn("_ = vec2(42f) / vec2(1f, 0f);"_si), "value vec2(42f, Infinity) cannot be represented as 'vec2<f32>'"_si);

    expectNoError(fn("let x = 42f; _ = x / 0f;"_si));
    expectNoError(fn("let x = 42f; _ = x / vec2(1f, 0f);"_si));
    expectNoError(fn("let x = 42f; _ = vec2(x) / 0f;"_si));
    expectNoError(fn("let x = 42f; _ = vec2(x) / vec2(1f, 0f);"_si));

    expectNoError(fn("_ = (-2147483647f - 1f) / -1f;"_si));
    expectNoError(fn("_ = vec2(-2147483647f - 1f, 1f) / vec2(-1f, 1f);"_si));

    expectNoError(fn("_ = (-9223372036854775807.f - 1.f) / -1.f;"_si));
    expectNoError(fn("_ = vec2(-9223372036854775807.f - 1.f, 1.f) / vec2(-1.f, 1.f);"_si));

    expectNoError(fn("_ = (-340282346638528859811704183484516925439.f - 1f) / -1f;"_si));
    expectNoError(fn("_ = vec2(-340282346638528859811704183484516925439.f - 1f, 1f) / vec2(-1f, 1f);"_si));

    // Test i32 compound
    expectTypeError(fn("var y: vec2<i32>; y /= 0;"_si), "invalid division by zero"_si);
    expectTypeError(fn("var y: vec2<i32>; y /= vec2(1, 0);"_si), "invalid division by zero"_si);
    expectTypeError(fn("var y: vec2<i32>; y[0] /= 0;"_si), "invalid division by zero"_si);
    expectTypeError(fn("var y: vec2<i32>; y[0] /= vec2(1, 0);"_si), "invalid division by zero"_si);

    // Test u32 compound
    expectTypeError(fn("var y: vec2<u32>; y /= 0;"_si), "invalid division by zero"_si);
    expectTypeError(fn("var y: vec2<u32>; y /= vec2(1, 0);"_si), "invalid division by zero"_si);
    expectTypeError(fn("var y: vec2<u32>; y[0] /= 0;"_si), "invalid division by zero"_si);
    expectTypeError(fn("var y: vec2<u32>; y[0] /= vec2(1, 0);"_si), "invalid division by zero"_si);

    // Test f32 compound
    expectNoError(fn("var y: vec2<f32>; y /= 0;"_si));
    expectNoError(fn("var y: vec2<f32>; y /= vec2(1, 0);"_si));
    expectNoError(fn("var y: vec2<f32>; y[0] /= 0;"_si));
    expectNoError(fn("var y: vec2<f32>; y[0] /= vec2(1, 0).y;"_si));

    // Test divisor overflow
    expectTypeError(fn("let x = 1; _ = x / 8144182087775404419;"_si), "value 8144182087775404419 cannot be represented as 'i32'"_si);
}

TEST(WGSLTypeCheckingTests, EmptyStruct)
{
    expectTypeError("struct S { }"_si, "structures must have at least one member"_si);
}

TEST(WGSLTypeCheckingTests, F16)
{
    expectTypeError(fn("_ = 1h"_si), "f16 literal used without f16 extension enabled"_si);

    expectTypeError("fn f() -> f16 { }"_si, "f16 type used without f16 extension enabled"_si);
    expectTypeError(fn("var x: f16;"_si), "f16 type used without f16 extension enabled"_si);
    expectTypeError(fn("_ = vec2h().x;"_si), "f16 type used without f16 extension enabled"_si);
    expectTypeError(fn("_ = vec3h().x;"_si), "f16 type used without f16 extension enabled"_si);
    expectTypeError(fn("_ = vec4h().x;"_si), "f16 type used without f16 extension enabled"_si);
    expectTypeError(fn("_ = mat2x2h()[0].x;"_si), "f16 type used without f16 extension enabled"_si);
}

TEST(WGSLTypeCheckingTests, For)
{
    expectTypeError(fn("for (var i = 0; i; i++) { }"_si), "for-loop condition must be bool, got ref<function, i32, read_write>"_si);
}

TEST(WGSLTypeCheckingTests, FunctionCall)
{
    expectTypeError("fn f1(x: f32) -> f32 { }"_si, "missing return at end of function"_si);
    expectTypeError(fn("_ = f0();"_si), "unresolved call target 'f0'"_si);
    expectTypeError("fn f1(x: f32) { } fn f() { f1(0i); }"_si, "type in function call does not match parameter type: expected 'f32', found 'i32'"_si);
    expectTypeError("fn f3() { } fn f() { let f3: i32 = 0; f3(); }"_si, "cannot call value of type 'i32'"_si);
    expectTypeError("fn f1(x: f32) -> f32 { return x; } fn f() { let x: i32 = f1(0); }"_si, "cannot initialize var of type 'i32' with value of type 'f32'"_si);
    expectTypeError("fn f1(x: f32) { } fn f() { f1(); }"_si, "funtion call has too few arguments: expected 1, found 0"_si);
    expectTypeError("fn f1(x: f32) { } fn f() { f1(0, 0); }"_si, "funtion call has too many arguments: expected 1, found 2"_si);
    expectTypeError("fn f3() { } fn f() { let x = f3(); }"_si, "function 'f3' does not return a value"_si);
    expectTypeError(
        StringImpl::create("@group(0) @binding(0) var<storage, read_write> x: array<f32>;"
        "fn f() {"
        "let arrayLength = 1; _ = arrayLength(&x);"
        "}"_s).ptr(), "cannot call value of type 'i32'"_si);
    expectTypeError(fn("let array = 1f; _ = array<i32, 1>(1);"_si), "cannot call value of type 'f32'"_si);
    expectTypeError(fn("let vec2 = 1u; _ = vec2<i32>(1);"_si), "cannot call value of type 'u32'"_si);
    expectTypeError(enableF16(fn("let bitcast = 1h; _ = bitcast<i32>(1);"_si)), "cannot call value of type 'f16'"_si);
}

TEST(WGSLTypeCheckingTests, Fuzz_133788509)
{
    expectTypeError(file("fuzz-133788509.wgsl"_si), "maximum parser recursive depth reached"_si);
}

TEST(WGSLTypeCheckingTests, If)
{
    expectTypeError(fn("if array<f32, 9>() { return; }"_si), "expected 'bool', found 'array<f32, 9>'"_si);
}

TEST(WGSLTypeCheckingTests, Limits)
{
    expectTypeError(file("limits-brace-enclosed.wgsl"_si), "maximum parser recursive depth reached"_si);
    expectTypeError(file("limits-composite-type.wgsl"_si), "composite type may not be nested more than 15 levels"_si);
    expectTypeError(file("limits-const-array.wgsl"_si), "constant array cannot have more than 2047 elements"_si);
    expectTypeError(file("limits-function-parameters.wgsl"_si), "function cannot have more than 255 parameters"_si);
    expectTypeError(file("limits-struct-members.wgsl"_si), "struct cannot have more than 1023 members"_si);
    expectTypeError(file("limits-switch-case.wgsl"_si), "switch statement cannot have more than 1023 case selector values"_si);
}

TEST(WGSLTypeCheckingTests, Modulo)
{
    // Test AbstractInt
    expectTypeError(fn("_ = 42 % 0;"_si), "invalid modulo by zero"_si);
    expectTypeError(fn("_ = 42 % vec2(1, 0);"_si), "invalid modulo by zero"_si);
    expectTypeError(fn("_ = vec2(42) % 0;"_si), "invalid modulo by zero"_si);
    expectTypeError(fn("_ = vec2(42) % vec2(1, 0);"_si), "invalid modulo by zero"_si);

    expectTypeError(fn("let x = 42; _ = x % 0;"_si), "invalid modulo by zero"_si);
    expectTypeError(fn("let x = 42; _ = x % vec2(1, 0);"_si), "invalid modulo by zero"_si);
    expectTypeError(fn("let x = 42; _ = vec2(x) % 0;"_si), "invalid modulo by zero"_si);
    expectTypeError(fn("let x = 42; _ = vec2(x) % vec2(1, 0);"_si), "invalid modulo by zero"_si);

    expectNoError(fn("_ = (-2147483647 - 1) % -1;"_si));
    expectNoError(fn("_ = vec2(-2147483647 - 1, 1) % vec2(-1, 1);"_si));

    expectTypeError(fn("_ = (-9223372036854775807 - 1) % -1;"_si), "invalid modulo overflow"_si);
    expectTypeError(fn("_ = vec2(-9223372036854775807 - 1, 1) % vec2(-1, 1);"_si), "invalid modulo overflow"_si);

    // Test i32
    expectTypeError(fn("_ = 42i % 0i;"_si), "invalid modulo by zero"_si);
    expectTypeError(fn("_ = 42i % vec2(1i, 0i);"_si), "invalid modulo by zero"_si);
    expectTypeError(fn("_ = vec2(42i) % 0i;"_si), "invalid modulo by zero"_si);
    expectTypeError(fn("_ = vec2(42i) % vec2(1i, 0i);"_si), "invalid modulo by zero"_si);

    expectTypeError(fn("let x = 42i; _ = x % 0i;"_si), "invalid modulo by zero"_si);
    expectTypeError(fn("let x = 42i; _ = x % vec2(1i, 0i);"_si), "invalid modulo by zero"_si);
    expectTypeError(fn("let x = 42i; _ = vec2(x) % 0i;"_si), "invalid modulo by zero"_si);
    expectTypeError(fn("let x = 42i; _ = vec2(x) % vec2(1i, 0i);"_si), "invalid modulo by zero"_si);

    expectTypeError(fn("_ = (-2147483647i - 1i) % -1i;"_si), "invalid modulo overflow"_si);
    expectTypeError(fn("_ = vec2(-2147483647i - 1i, 1i) % vec2(-1i, 1i);"_si), "invalid modulo overflow"_si);

    // Test u32
    expectTypeError(fn("_ = 42u % 0u;"_si), "invalid modulo by zero"_si);
    expectTypeError(fn("_ = 42u % vec2(1u, 0u);"_si), "invalid modulo by zero"_si);
    expectTypeError(fn("_ = vec2(42u) % 0u;"_si), "invalid modulo by zero"_si);
    expectTypeError(fn("_ = vec2(42u) % vec2(1u, 0u);"_si), "invalid modulo by zero"_si);

    expectTypeError(fn("let x = 42u; _ = x % 0u;"_si), "invalid modulo by zero"_si);
    expectTypeError(fn("let x = 42u; _ = x % vec2(1u, 0u);"_si), "invalid modulo by zero"_si);
    expectTypeError(fn("let x = 42u; _ = vec2(x) % 0u;"_si), "invalid modulo by zero"_si);
    expectTypeError(fn("let x = 42u; _ = vec2(x) % vec2(1u, 0u);"_si), "invalid modulo by zero"_si);

    // Test AbstractFloat
    expectTypeError(fn("_ = 42.0 % 0.0;"_si), "value NaN cannot be represented as '<AbstractFloat>'"_si);
    expectTypeError(fn("_ = 42.0 % vec2(1.0, 0.0);"_si), "value vec2(0, NaN) cannot be represented as 'vec2<<AbstractFloat>>'"_si);
    expectTypeError(fn("_ = vec2(42.0) % 0.0;"_si), "value vec2(NaN, NaN) cannot be represented as 'vec2<<AbstractFloat>>'"_si);
    expectTypeError(fn("_ = vec2(42.0) % vec2(1.0, 0.0);"_si), "value vec2(0, NaN) cannot be represented as 'vec2<<AbstractFloat>>'"_si);

    expectNoError(fn("let x = 42.0; _ = x % 0.0;"_si));
    expectNoError(fn("let x = 42.0; _ = x % vec2(1.0, 0.0);"_si));
    expectNoError(fn("let x = 42.0; _ = vec2(x) % 0.0;"_si));
    expectNoError(fn("let x = 42.0; _ = vec2(x) % vec2(1.0, 0.0);"_si));

    expectNoError(fn("_ = (-2147483647.0 - 1.0) % -1.0;"_si));
    expectNoError(fn("_ = vec2(-2147483647.0 - 1.0, 1.0) % vec2(-1.0, 1.0);"_si));

    expectNoError(fn("_ = (-9223372036854775807.0 - 1.0) % -1.0;"_si));
    expectNoError(fn("_ = vec2(-9223372036854775807.0 - 1.0, 1.0) % vec2(-1.0, 1.0);"_si));

    expectNoError(fn("_ = -340282346638528859811704183484516925440.0 - 1.0 % -1.0;"_si));
    expectNoError(fn("_ = vec2(-340282346638528859811704183484516925440.0 - 1.0, 1.0) % vec2(-1.0, 1.0);"_si));

    // Test f32
    expectTypeError(fn("_ = 42f % 0f;"_si), "value NaN cannot be represented as 'f32'"_si);
    expectTypeError(fn("_ = 42f % vec2(1f, 0f);"_si), "value vec2(0f, NaN) cannot be represented as 'vec2<f32>'"_si);
    expectTypeError(fn("_ = vec2(42f) % 0f;"_si), "value vec2(NaN, NaN) cannot be represented as 'vec2<f32>'"_si);
    expectTypeError(fn("_ = vec2(42f) % vec2(1f, 0f);"_si), "value vec2(0f, NaN) cannot be represented as 'vec2<f32>'"_si);

    expectNoError(fn("let x = 42f; _ = x % 0f;"_si));
    expectNoError(fn("let x = 42f; _ = x % vec2(1f, 0f);"_si));
    expectNoError(fn("let x = 42f; _ = vec2(x) % 0f;"_si));
    expectNoError(fn("let x = 42f; _ = vec2(x) % vec2(1f, 0f);"_si));

    expectNoError(fn("_ = (-2147483647f - 1f) % -1f;"_si));
    expectNoError(fn("_ = vec2(-2147483647f - 1f, 1f) % vec2(-1f, 1f);"_si));

    expectNoError(fn("_ = (-9223372036854775807.f - 1.f) % -1.f;"_si));
    expectNoError(fn("_ = vec2(-9223372036854775807.f - 1.f, 1.f) % vec2(-1.f, 1.f);"_si));

    expectNoError(fn("_ = (-340282346638528859811704183484516925439.f - 1f) % -1f;"_si));
    expectNoError(fn("_ = vec2(-340282346638528859811704183484516925439.f - 1f, 1f) % vec2(-1f, 1f);"_si));

    // Test i32 compound
    expectTypeError(fn("var y: vec2<i32>; y %= 0;"_si), "invalid modulo by zero"_si);
    expectTypeError(fn("var y: vec2<i32>; y %= vec2(1, 0);"_si), "invalid modulo by zero"_si);
    expectTypeError(fn("var y: vec2<i32>; y[0] %= 0;"_si), "invalid modulo by zero"_si);
    expectTypeError(fn("var y: vec2<i32>; y[0] %= vec2(1, 0);"_si), "invalid modulo by zero"_si);

    // Test u32 compound
    expectTypeError(fn("var y: vec2<u32>; y %= 0;"_si), "invalid modulo by zero"_si);
    expectTypeError(fn("var y: vec2<u32>; y %= vec2(1, 0);"_si), "invalid modulo by zero"_si);
    expectTypeError(fn("var y: vec2<u32>; y[0] %= 0;"_si), "invalid modulo by zero"_si);
    expectTypeError(fn("var y: vec2<u32>; y[0] %= vec2(1, 0);"_si), "invalid modulo by zero"_si);

    // Test f32 compound
    expectNoError(fn("var y: vec2<f32>; y %= 0;"_si));
    expectNoError(fn("var y: vec2<f32>; y %= vec2(1, 0);"_si));
    expectNoError(fn("var y: vec2<f32>; y[0] %= 0;"_si));
    expectNoError(fn("var y: vec2<f32>; y[0] %= vec2(1, 0).y;"_si));
}

TEST(WGSLTypeCheckingTests, Overload)
{
    // Test explicit type arguments
    expectTypeError(fn("let v0 = vec2<i32>(0.0, 0.0);"_si), "no matching overload for initializer vec2<i32>(<AbstractFloat>, <AbstractFloat>)"_si);

    // test constraints
    expectTypeError(
        StringImpl::create("struct S { x: i32, };"
        "fn f() { let x = S(0); let x2 = x + x; }"_s).ptr(),
        "no matching overload for operator +(S, S)"_si);
    expectTypeError(
        StringImpl::create("struct S { x: i32, };"
        "fn f() { let x = S(0); let x3 = vec2(x, x); }"_s).ptr(),
        "no matching overload for initializer vec2(S, S)"_si);
    expectTypeError(
        StringImpl::create("struct S { x: i32, };"
        "fn f() { let x = S(0); let x4 = mat2x2(0u, 0u, 0u, 0u); }"_s).ptr(),
        "no matching overload for initializer mat2x2(u32, u32, u32, u32)"_si);
    expectTypeError(
        StringImpl::create("struct S { x: i32, };"
        "fn f() { let x = S(0); let x5 = vec2<S>(x, x); }"_s).ptr(),
        "no matching overload for initializer vec2<S>(S, S)"_si);
    expectTypeError(
        StringImpl::create("struct S { x: i32, };"
        "fn f() { let x6 = vec2<S>(vec2(0, 0)); }"_s).ptr(),
        "no matching overload for initializer vec2<S>(vec2<<AbstractInt>>)"_si);
    expectTypeError(
        fn("let x7 = -1u;"_si),
        "no matching overload for operator -(u32)"_si);
    expectTypeError(
        fn("let x8 = -vec2(1u, 1u);"_si),
        "no matching overload for operator -(vec2<u32>)"_si);
    expectTypeError(
        fn("const x: u32 = 1i << 1;"_si),
        "cannot initialize var of type 'u32' with value of type 'i32'"_si);

    // test textureGather
    expectTypeError(fn("_ = textureGather();"_si), "no matching overload for initializer textureGather()"_si);

    // test compound assignment
    expectTypeError(fn("var f = 1f; f /= vec2f(1);"_si), "cannot assign 'vec2<f32>' to 'f32'"_si);
}

TEST(WGSLTypeCheckingTests, ParameterScope)
{
    expectTypeError("fn f(x: f32) { let x: i32 = 1; }"_si, "redeclaration of 'x'"_si);
}

TEST(WGSLTypeCheckingTests, PhonyAssignment)
{
    expectTypeError(
        "fn f() { } fn main() { _ = f(); }"_si,
        "function 'f' does not return a value"_si);
}


TEST(WGSLTypeCheckingTests, PointerAssignment)
{
    expectTypeError(fn("var x = 1; *&*&x=3 x=1;"_si), "Expected a ;, but got a Identifier"_si);
}

TEST(WGSLTypeCheckingTests, PointerAsConstant)
{
    expectTypeError(
        "var<storage> x: i32; @id(*&x) var<storage> y: i32;"_si,
        "cannot use runtime value in constant expression"_si);
}

TEST(WGSLTypeCheckingTests, PointerAccessMode)
{
    expectTypeError("fn f1(x: ptr<function, i32, read>) { }"_si, "only pointers in <storage> address space may specify an access mode"_si);
    expectTypeError("alias T = ptr<storage, u32, write>;"_si, "access mode 'write' is not valid for the <storage> address space"_si);
    expectNoError("alias T = ptr<storage, u32, read>;"_si);
    expectNoError("alias T = ptr<storage, u32, read_write>;"_si);
}

TEST(WGSLTypeCheckingTests, Redeclaration)
{
    // error during reordering of declarations
    expectTypeError(
        StringImpl::create("struct f { x: i32 }"
        "override f = 1;"
        "fn f() { }"_s).ptr(),
        "redeclaration of 'f'"_si);

    // error during type checking
    expectTypeError(fn(
        StringImpl::create("let x = 1;"
        "var x = 2;"
        "const x = 2;"_s).ptr()),
        "redeclaration of 'x'"_si);
}

TEST(WGSLTypeCheckingTests, References)
{
    // Test reference assignment
    expectTypeError("@group(0) @binding(0) var<storage, read> x: i32; fn f() { x = 0; }"_si, "cannot store into a read-only type 'ref<storage, i32, read>'"_si);
    expectTypeError("@group(0) @binding(1) var<storage, read_write> y: i32; fn f() { y = 0u; }"_si, "cannot assign value of type 'u32' to 'i32'"_si);
    expectTypeError(fn("let z: i32 = 0; z = 0;"_si), "cannot assign to a value of type 'i32'"_si);

    // Test decrement/increment
    expectTypeError(fn("let x = 0i; x++;"_si), "cannot modify a value of type 'i32'"_si);
    expectTypeError("@group(0) @binding(0) var<storage, read> x: i32; fn f() { x++; }"_si, "cannot modify read-only type 'ref<storage, i32, read>'"_si);
    expectTypeError(fn("var x = 0f; x++;"_si), "increment can only be applied to integers, found f32"_si);
}

TEST(WGSLTypeCheckingTests, Reordering)
{
    expectTypeError("const a = a * 2;"_si, "encountered a dependency cycle: a -> a"_si);
    expectTypeError("fn f() { f(); };"_si, "encountered a dependency cycle: f -> f"_si);
    expectTypeError("struct S { s: S }"_si, "encountered a dependency cycle: S -> S"_si);
    expectTypeError("const a = S(array(1)); struct S { x: array<i32, a.x[0]> };"_si, "encountered a dependency cycle: a -> S -> a"_si);
    expectTypeError(
        StringImpl::create("const y = x * 2;"
        "const z = y;"
        "const w = z;"
        "const x = w;"_s).ptr(),
        "encountered a dependency cycle: y -> x -> w -> z -> y"_si);
    expectTypeError(
        StringImpl::create("const y = x * 2;"
        "const y = 2;"_s).ptr(),
        "redeclaration of 'y'"_si);
}

TEST(WGSLTypeCheckingTests, RequiredAlignment)
{
    expectTypeError(
        StringImpl::create("struct S1 { y1: array<i32, 2> }"
        "@group(0) @binding(1) var<uniform> x1: S1;"_s).ptr(),
        "arrays in the uniform address space must have a stride multiple of 16 bytes, but has a stride of 4 bytes"_si);

    expectTypeError(
        StringImpl::create("struct S1 { x1: i32, @align(4) y1: array<vec4i, 2> }"
        "@group(0) @binding(1) var<uniform> x1: S1;"_s).ptr(),
        "@align attribute 4 of struct member is not a multiple of the type's alignment 16"_si);

    expectTypeError(
        StringImpl::create("struct S1 { x1: i32, y3: T }"
        "struct T { x: i32, y: i32, }"
        "@group(0) @binding(1) var<uniform> x1: S1;"_s).ptr(),
        "offset of struct member S1::y3 must be a multiple of 16 bytes, but its offset is 4 bytes"_si);

    expectTypeError(
        StringImpl::create("struct S1 { y3: T, z3: i32 }"
        "struct T { x: i32, y: i32, }"
        "@group(0) @binding(1) var<uniform> x1: S1;"_s).ptr(),
        "uniform address space requires that the number of bytes between S1::y3 and S1::z3 must be at least 16 bytes, but it is 8 bytes"_si);
}

inline void expectStaticCheckErrorContaining(StringImpl* wgsl, StringImpl* errorMessageSubstring)
{
    auto result = staticCheck(wgsl);
    EXPECT_TRUE(std::holds_alternative<WGSL::FailedCheck>(result));
    if (!std::holds_alternative<WGSL::FailedCheck>(result))
        return;
    auto error = std::get<WGSL::FailedCheck>(result);
    EXPECT_EQ(error.errors.size(), 1u);
    EXPECT_TRUE(contains(error.errors[0].message(), errorMessageSubstring));
}

TEST(WGSLTypeCheckingTests, ResourceBinding)
{
    expectStaticCheckErrorContaining(
        StringImpl::create("@group(0) @binding(0) var<storage, read_write> big: array<u32>;"
        "@group(0) @binding(0) var<storage, read_write> tiny: array<u32>;"
        "@compute @workgroup_size(1) fn main() {"
            "tiny[0] = 1u;"
            "var big: u32 = arrayLength(&big);"
            "tiny[1] = big;"
        "}"_s).ptr(),
        "both which use the same resource binding: @group(0) @binding(0)"_si);

    expectStaticCheckErrorContaining(
        StringImpl::create("@group(0) @binding(0) var<storage, read_write> big: array<u32>;"
        "@group(0) @binding(0) var<storage, read_write> tiny: array<u32>;"
        "@compute @workgroup_size(1) fn main() {"
            "tiny[0] = arrayLength(&big);"
        "}"_s).ptr(),
        "both which use the same resource binding: @group(0) @binding(0)"_si);

    expectNoError(
        StringImpl::create("@group(0) @binding(0) var<storage, read_write> big: array<u32>;"
        "@group(0) @binding(0) var<storage, read_write> tiny: array<u32>;"
        "@compute @workgroup_size(1) fn main() {"
            "tiny[0] = arrayLength(&tiny);"
        "}"_s).ptr());

    expectNoError(
        StringImpl::create("@group(0) @binding(0) var<storage, read_write> big: array<u32>;"
        "@group(0) @binding(0) var<storage, read_write> tiny: array<u32>;"
        "@compute @workgroup_size(1) fn main() {"
            "var big: u32 = 0u;"
            "big = big + 1u;"
            "tiny[0] = big;"
        "}"_s).ptr());
}

TEST(WGSLTypeCheckingTests, DeclarationScope)
{
    expectTypeError(
        StringImpl::create("var<workgroup> w: u32;"
        "@vertex fn main() -> @builtin(position) vec4<f32> {"
            "var w: u32 = w;"
            "return vec4<f32>(f32(w));"
        "}"_s).ptr(),
        "var with `workgroup` address space cannot be used by `vertex` pipeline stage"_si);

    expectNoError(
        StringImpl::create("fn f() -> u32 { var g: u32 = g(); return g; }"
        "fn g() -> u32 { return 7u; }"
        "@compute @workgroup_size(1) fn main() { _ = f(); }"_s).ptr());

    expectNoError(
        StringImpl::create("fn f() -> u32 { var g: u32 = g; return g; }"
        "var<private> g: u32 = 7u;"
        "@compute @workgroup_size(1) fn main() { _ = f(); }"_s).ptr());

    expectTypeError(
        StringImpl::create("fn f() -> u32 { var f: u32 = f(); return f; }"
        "@compute @workgroup_size(1) fn main() { _ = f(); }"_s).ptr(),
        "encountered a dependency cycle: f -> f"_si);
}

TEST(WGSLTypeCheckingTests, Return)
{
    expectTypeError("fn noReturn() { return 0i; }"_si, "return statement type does not match its function return type, returned 'i32', expected 'void'"_si);
    expectTypeError("fn typeMisMatch() -> f32 { return 0i; }"_si, "return statement type does not match its function return type, returned 'i32', expected 'f32'"_si);
    expectTypeError("fn conversionFailure() -> i32 { return 500000000000; }"_si, "value 500000000000 cannot be represented as 'i32'"_si);
}

TEST(WGSLTypeCheckingTests, Struct)
{
    expectTypeError("struct S { x: f32, x: T, y: i32 }"_si, "duplicate member 'x' in struct 'S'"_si);

    expectTypeError(
        StringImpl::create("struct T { x: u32, y: U, }"
        "struct U { x: u32, y: array<u32>, }"_s).ptr(),
        "a struct that contains a runtime array cannot be nested inside another struct"_si);

    expectTypeError(
        StringImpl::create("struct S { x: f32, y: i32, }"
        "fn f() { _ = S(0); }"_s).ptr(),
        "struct initializer has too few inputs: expected 2, found 1"_si);

    expectTypeError(
        StringImpl::create("struct S { x: f32, y: i32, }"
        "fn f() { _ = S(0, 0, 0); }"_s).ptr(),
        "struct initializer has too many inputs: expected 2, found 3"_si);

    expectTypeError(
        StringImpl::create("struct S { x: f32, y: i32, }"
        "fn f() { _ = S(0i, 0); }"_s).ptr(),
        "type in struct initializer does not match struct member type: expected 'f32', found 'i32'"_si);

    expectTypeError(
        StringImpl::create("struct S { x: f32, y: i32, }"
        "fn f() { _ = S(0f, 0f); }"_s).ptr(),
        "type in struct initializer does not match struct member type: expected 'i32', found 'f32'"_si);
}

TEST(WGSLTypeCheckingTests, Switch)
{
    expectTypeError(fn(
        StringImpl::create("let x: u32 = 42;"
        "switch 1u {"
        "    case 1i, default,: { }"
        "}"_s).ptr()),
        "the case selector values must have the same type as the selector expression: the selector expression has type 'u32' and case selector has type 'i32'"_si);

    expectTypeError(fn(
        StringImpl::create("switch false {"
        "    case default,: { }"
        "}"_s).ptr()),
        "switch selector must be of type i32 or u32"_si);
}

TEST(WGSLTypeCheckingTests, TextureGather)
{
    expectTypeError(
        StringImpl::create("@group(0) @binding(0) var t: texture_2d<f32>;"
        "@group(0) @binding(1) var s: sampler;"
        "@fragment fn f() {"
        "    _ = textureGather(4, t, s, vec2());"
        "}"_s).ptr(),
        "the component argument must be at least 0 and at most 3. component is 4"_si);

    expectTypeError(
        StringImpl::create("@group(0) @binding(0) var t: texture_2d<f32>;"
        "@group(0) @binding(1) var s: sampler;"
        "@fragment fn f() {"
        "    var c = 4;"
        "    _ = textureGather(c, t, s, vec2());"
        "}"_s).ptr(),
        "the component argument must be a const-expression"_si);

    expectTypeError(
        StringImpl::create("@group(0) @binding(0) var s: sampler;"
        "@group(0) @binding(23) var td2d: texture_depth_2d;"
        "@fragment fn f() {"
        "    var offset = vec2i(0);"
        "    _ = textureGather(td2d, s, vec2f(0), offset);"
        "}"_s).ptr(),
        "the offset argument must be a const-expression"_si);

    expectTypeError(
        StringImpl::create("@group(0) @binding(0) var s: sampler;"
        "@group(0) @binding(23) var td2d: texture_depth_2d;"
        "@fragment fn f() {"
        "    _ = textureGather(td2d, s, vec2f(0), vec2i(-9));"
        "}"_s).ptr(),
        "each component of the offset argument must be at least -8 and at most 7. offset component 0 is -9"_si);

    expectTypeError(
        StringImpl::create("@group(0) @binding(0) var s: sampler;"
        "@group(0) @binding(23) var td2d: texture_depth_2d;"
        "@fragment fn f() {"
        "    _ = textureGather(td2d, s, vec2f(0), vec2i(8));"
        "}"_s).ptr(),
        "each component of the offset argument must be at least -8 and at most 7. offset component 0 is 8"_si);
}

TEST(WGSLTypeCheckingTests, TypesVsValues)
{
    expectTypeError(
        StringImpl::create("struct S { x: i32 }"
        "var<private> s : S;"
        "fn f() -> s { return s; }"_s).ptr(),
        "cannot use value 's' as type"_si);

    expectTypeError(
        StringImpl::create("struct S { x: i32 }"
        "fn f() { let s = S(42); _ = s(42); }"_s).ptr(),
        "cannot call value of type 'S'"_si);

    expectTypeError(
        StringImpl::create("struct S { x: i32 }"
        "fn f() { _ = S; }"_s).ptr(),
        "cannot use type 'S' as value"_si);

    expectTypeError(fn("var x : undefined;"_si), "unresolved type 'undefined'"_si);
    expectTypeError(fn("_ = undefined;"_si), "unresolved identifier 'undefined'"_si);
}

TEST(WGSLTypeCheckingTests, Unicode)
{
    expectTypeError(StringImpl::fromUTF8("@group(5)@binding(xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx0߃"), "Expected a ), but got a EOF"_si);
}

TEST(WGSLTypeCheckingTests, Visibility)
{
    expectTypeError(
        StringImpl::create("fn f() { storageBarrier(); }"
        "@fragment fn main() { f(); }"_s).ptr(),
        "built-in cannot be used by fragment pipeline stage"_si);

    expectTypeError(
        StringImpl::create("fn f() { discard; }"
        "@compute @workgroup_size(1) fn main() { f(); }"_s).ptr(),
        "discard statement cannot be used by compute pipeline stage"_si);

    expectTypeError(
        StringImpl::create("fn f() { discard; }"
        "@vertex fn main() -> @builtin(position) vec4f { f(); return vec4f(1); }"_s).ptr(),
        "discard statement cannot be used by vertex pipeline stage"_si);
}

TEST(WGSLTypeCheckingTests, While)
{
    expectTypeError(fn("var i = 0; while (i) { }"_si), "while condition must be bool, got ref<function, i32, read_write>"_si);
}

}
