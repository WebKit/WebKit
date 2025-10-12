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

inline void expectTypeError(const String& wgsl, const String& errorMessage)
{
    auto result = WGSL::staticCheck(wgsl, std::nullopt, { 8 });
    EXPECT_TRUE(std::holds_alternative<WGSL::FailedCheck>(result));
    auto error = std::get<WGSL::FailedCheck>(result);
    EXPECT_EQ(error.errors.size(), 1u);
    EXPECT_EQ(error.errors[0].message(), errorMessage);
}

inline void expectNoError(const String& wgsl)
{
    auto result = WGSL::staticCheck(wgsl, std::nullopt, { 8 });
    EXPECT_TRUE(std::holds_alternative<WGSL::SuccessfulCheck>(result));
}


TEST(WGSLTypeCheckingTests, Array)
{
    expectTypeError("var<private> a:array;"_s, "'array' requires at least 1 template argument"_s);
    expectTypeError("@group(0) @binding(0) var<storage, read_write> b: array<u32, (1<<32)>;"_s, "value 4294967296 cannot be represented as 'i32'"_s);

    expectTypeError(fn("let x = array<i32, 0>();"_s), "array count must be greater than 0"_s);
    expectTypeError(fn("let x = array<i32, -1>();"_s), "array count must be greater than 0"_s);

    expectTypeError(fn("let x = array<i32, 2>(0);"_s), "array constructor has too few elements: expected 2, found 1"_s);
    expectTypeError(fn("let x = array<i32, 1>(0, 0);"_s), "array constructor has too many elements: expected 1, found 2"_s);

    expectTypeError(fn("let x = array<i32, 65536>();"_s), "array count (65536) must be less than 65536"_s);

    expectTypeError(fn("let x = array<i32>(0);"_s), "cannot construct a runtime-sized array"_s);

    expectTypeError(fn("let x = array<i32, 1>(0.0);"_s), "'<AbstractFloat>' cannot be used to construct an array of 'i32'"_s);

    expectTypeError(fn("let x = array();"_s), "cannot infer array element type from constructor"_s);

    expectTypeError(fn("let x = array(0, 0.0, 0u);"_s), "cannot infer common array element type from constructor arguments"_s);

    expectTypeError(fn("let x = array<i2, 1>(0.0);"_s), "unresolved type 'i2'"_s);

    expectTypeError("override elementCount = 4;"
        "fn testOverrideElementCount() {"
            "let xl = array<i32, elementCount>(0.0);"
        "}"_s, "array must have constant size in order to be constructed"_s);
}

TEST(WGSLTypeCheckingTests, Attributes)
{
    expectTypeError("@id(0) override z: array<i32, 1>;"_s, "'array<i32, 1>' cannot be used as the type of an 'override'"_s);
    expectTypeError("@id(u32(dpdx(0))) override y: i32;"_s, "cannot call function from constant context"_s);

    expectTypeError("struct S { @align(4.0) x: i32, }"_s, "@align must be an i32 or u32 value"_s);
    expectTypeError("struct S { @location(0.0) y: i32, }"_s, "@location must be an i32 or u32 value"_s);
    expectTypeError("struct S { @size(4.0) z: i32, }"_s, "@size must be an i32 or u32 value"_s);
    expectTypeError("struct S { @size(2) x: i32, }"_s, "@size value must be at least the byte-size of the type of the member"_s);
    expectTypeError("struct S { @size(-1) y: i32, }"_s, "@size value must be non-negative"_s);
    expectTypeError("struct S { @align(-1) z: i32, }"_s, "@align value must be positive"_s);
    expectTypeError("struct S { @align(3) w: i32, }"_s, "@align value must be a power of two"_s);

    // Type checking @group, @binding and @workgroup_size
    expectTypeError("@group(0.0) @binding(0) var x : i32;"_s, "@group must be an i32 or u32 value"_s);
    expectTypeError("@group(0) @binding(0.0) var y : i32;"_s, "@binding must be an i32 or u32 value"_s);
    expectTypeError("@id(0.0) override z : i32;"_s, "@id must be an i32 or u32 value"_s);
    expectTypeError("@compute @workgroup_size(1.0) fn f1() { }"_s, "@workgroup_size x dimension must be an i32 or u32 value"_s);
    expectTypeError("@compute @workgroup_size(1, 1.0) fn f2() { }"_s, "@workgroup_size y dimension must be an i32 or u32 value"_s);
    expectTypeError("@compute @workgroup_size(1, 1, 1.0) fn f3() { }"_s, "@workgroup_size z dimension must be an i32 or u32 value"_s);
    expectTypeError("@compute @workgroup_size(1i, 1u) fn f4() { }"_s, "@workgroup_size arguments must be of the same type, either i32 or u32"_s);
    expectTypeError("@compute @workgroup_size(1, 1i, 1u) fn f5() { }"_s, "@workgroup_size arguments must be of the same type, either i32 or u32"_s);
    expectTypeError("@compute @workgroup_size(1, 1u, 1i) fn f6() { }"_s, "@workgroup_size arguments must be of the same type, either i32 or u32"_s);


    // Attribute validation
    expectTypeError("@group(0) var<private> x: i32;"_s, "@group attribute must only be applied to resource variables"_s);
    expectTypeError("@group(-1) var<uniform> x: i32;"_s, "@group value must be non-negative"_s);
    expectTypeError("@binding(0) var<private> x: i32;"_s, "@binding attribute must only be applied to resource variables"_s);
    expectTypeError("@binding(-1) var<uniform> x: i32;"_s, "@binding value must be non-negative"_s);
    expectTypeError("@id(-1) var<private> y: i32;"_s, "@id attribute must only be applied to override variables"_s);
    expectTypeError("@id(-1) override z: i32;"_s, "@id value must be non-negative"_s);
    expectTypeError("@must_use fn mustUseWithoutReturnType() { }"_s, "@must_use can only be applied to functions that return a value"_s);
    expectTypeError("fn f1() -> @builtin(position) i32 { return 0; }"_s, "@builtin is not valid for non-entry point function types"_s);
    expectTypeError("fn f2() -> @location(0) i32 { return 0; }"_s, "@location is not valid for non-entry point function types"_s);
    expectTypeError("@fragment fn f3() -> @location(-1) i32 { return 0; }"_s, "@location value must be non-negative"_s);
    expectTypeError("@compute fn f4() -> @location(0) i32 { return 0; }"_s, "@location may not be used in the compute shader stage"_s);
    expectTypeError("@fragment fn f5() -> @location(0) bool { return false; }"_s, "@location must only be applied to declarations of numeric scalar or numeric vector type"_s);
    expectTypeError("fn f6() -> @interpolate(flat) i32 { return 0; }"_s, "@interpolate is only allowed on declarations that have a @location attribute"_s);
    expectTypeError("fn f7() -> @invariant i32 { return 0; }"_s, "@invariant is only allowed on declarations that have a @builtin(position) attribute"_s);
    expectTypeError("@workgroup_size(1) fn f8() { }"_s, "@workgroup_size must only be applied to compute shader entry point function"_s);
    expectTypeError("@workgroup_size(-1) @compute fn f9() { }"_s, "@workgroup_size argument must be at least 1"_s);

    // check that we don't crash by trying to read the size of S2, which won't have been computed
    expectNoError(
        "struct S1 { @size(16) x : f32 };"
        "struct S2 { @size(32) x: array<S1, 2>, };"_s);
}

TEST(WGSLTypeCheckingTests, AccessExpression)
{
    // Test index access
    expectTypeError(fn("_ = vec2(0)[1f];"_s), "index must be of type 'i32' or 'u32', found: 'f32'"_s);

    // Test swizzle access
    expectTypeError(fn("_ = vec2(0).z;"_s), "invalid vector swizzle member"_s);
    expectTypeError(fn("_ = vec2(0).w;"_s), "invalid vector swizzle member"_s);
    expectTypeError(fn("_ = vec2(0).b;"_s), "invalid vector swizzle member"_s);
    expectTypeError(fn("_ = vec2(0).a;"_s), "invalid vector swizzle member"_s);

    expectTypeError(fn("_ = vec3(0).w;"_s), "invalid vector swizzle member"_s);
    expectTypeError(fn("_ = vec3(0).a;"_s), "invalid vector swizzle member"_s);

    expectTypeError(fn("_ = vec2(0).rx;"_s), "invalid vector swizzle member"_s);

    expectTypeError(fn("_ = vec2(0).v;"_s), "invalid vector swizzle character"_s);

    expectTypeError(fn("var z: mat3x3<f32>; _ = z[1].e;"_s), "invalid vector swizzle character"_s);

    // The restriction below can be found in the spec for the following types:
    // - Vectors: https://www.w3.org/TR/WGSL/#vector-single-component
    // - Matrices: https://www.w3.org/TR/WGSL/#matrix-access-expr
    // - Arrays: https://www.w3.org/TR/WGSL/#array-access-expr
    //
    // NOTE: When an abstract vector value e is indexed by an expression that is not
    // a const-expression, then the vector is concretized before the index is applied.
    expectNoError(fn("let x: f32 = vec2(0)[0];"_s));
    expectTypeError(fn("let i = 0; let x: u32 = vec2(0)[i];"_s), "cannot initialize var of type 'u32' with value of type 'i32'"_s);
    expectTypeError(enableF16(fn("let i = 0; let x: vec2<f16> = mat2x2(0, 0, 0, 0)[i];"_s)), "cannot initialize var of type 'vec2<f16>' with value of type 'vec2<f32>'"_s);

    expectNoError(fn("let x: f32 = array(0)[0];"_s));
    expectTypeError(fn("let i = 0; let x: u32 = array(0)[i];"_s), "cannot initialize var of type 'u32' with value of type 'i32'"_s);

    // Test cannot write to multiple vector locations
    expectTypeError(fn("var v = vec2(0); v.xy = vec2(1);"_s), "cannot assign to a value of type 'vec2<i32>'"_s);
}

TEST(WGSLTypeCheckingTests, Atomics)
{
    expectTypeError("@group(0) @binding(0) var<storage, read_write> x : atomic<i32, i32>;"_s, "'atomic' requires 1 template argument"_s);
    expectTypeError("@group(0) @binding(1) var<storage, read_write> y : atomic<f32>;"_s, "atomic only supports i32 or u32 types"_s);
}

TEST(WGSLTypeCheckingTests, Bitcast)
{
    // Wrong number of arguments
    expectTypeError(fn("_ = bitcast<i32>(0, 0);"_s), "bitcast expects a single argument, found 2"_s);
    expectTypeError(fn("_ = bitcast<i32>();"_s), "bitcast expects a single argument, found 0"_s);

    // Wrong number of template arguments
    expectTypeError(fn("_ = bitcast<i32, i32>(0);"_s), "bitcast expects a single template argument, found 2"_s);
    expectTypeError(fn("_ = bitcast(0);"_s), "bitcast expects a single template argument, found 0"_s);

    // Invalid conversion
    expectTypeError(fn("_ = bitcast<vec2<i32>>(0);"_s), "cannot bitcast from 'i32' to 'vec2<i32>'"_s);
    expectTypeError(fn("_ = bitcast<i32>(vec2(0));"_s), "cannot bitcast from 'vec2<i32>' to 'i32'"_s);

    // I32 overflow
    expectTypeError(fn("const x: f32 = bitcast<f32>(4294967295);"_s), "value 4294967295 cannot be represented as 'i32'"_s);

    // Function as a value
    expectTypeError("fn f() { } fn g() { const x: f32 = bitcast<f32>(f); }"_s, "cannot use function 'f' as value"_s);

    // Types that cannot be concretized
    expectTypeError(
        "@group(0) @binding(1) var s: sampler; fn test() { _ = bitcast<i32>(s); }"_s,
        "cannot bitcast from 'sampler' to 'i32'"_s);
    expectTypeError(
        "@group(0) @binding(2) var t: texture_depth_2d; fn test() { _ = bitcast<i32>(t); }"_s,
        "cannot bitcast from 'texture_depth_2d' to 'i32'"_s);
}

TEST(WGSLTypeCheckingTests, Break)
{
    expectTypeError(fn("if true { break; }"_s), "break statement must be in a loop or switch case"_s);
}

TEST(WGSLTypeCheckingTests, Comments)
{
    expectTypeError("/*"_s, "Trying to parse a GlobalDecl, expected 'const', 'fn', 'override', 'struct' or 'var'."_s);
}

TEST(WGSLTypeCheckingTests, CompoundAssignment)
{
    expectTypeError(fn("var x = 1; x += 1f;"_s), "no matching overload for operator +(ref<function, i32, read_write>, f32)"_s);
    expectTypeError(fn("let x = 1; x += 1;"_s), "cannot assign to a value of type 'i32'"_s);
}

TEST(WGSLTypeCheckingTests, ConstAssert)
{
    expectTypeError("fn f() { } const_assert(f);"_s, "cannot use function 'f' as value"_s);

    // Top level
    expectTypeError("const_assert(1);"_s, "const assertion condition must be a bool, got '<AbstractInt>'"_s);
    expectTypeError("const_assert(undefined);"_s, "unresolved identifier 'undefined'"_s);
    expectTypeError("const_assert(vec2);"_s, "cannot use type 'vec2' as value"_s);
    expectTypeError("const_assert(i32);"_s, "cannot use type 'i32' as value"_s);
    expectTypeError("const_assert(sqrt);"_s, "unresolved identifier 'sqrt'"_s);
    expectTypeError("const_assert(1 > 2);"_s, "const assertion failed"_s);

    expectTypeError(
        "const x = 1;"
        "const y = 2;"
        "const_assert(x > y);"_s,
        "const assertion failed"_s);


    expectTypeError(
        "const x = 1;"
        "var<private> z = 3;"
        "const_assert(x > z);"_s,
        "cannot use runtime value in constant expression"_s);

    // Function level
    expectTypeError(fn("const_assert(1);"_s), "const assertion condition must be a bool, got '<AbstractInt>'"_s);
    expectTypeError(fn("const_assert(undefined);"_s), "unresolved identifier 'undefined'"_s);
    expectTypeError(fn("const_assert(vec2);"_s), "cannot use type 'vec2' as value"_s);
    expectTypeError(fn("const_assert(i32);"_s), "cannot use type 'i32' as value"_s);
    expectTypeError(fn("const_assert(sqrt);"_s), "unresolved identifier 'sqrt'"_s);
    expectTypeError(fn("const_assert(1 > 2);"_s), "const assertion failed"_s);

    expectTypeError(fn(
        "const x = 1;"
        "const y = 2;"
        "const_assert(x > y);"_s),
        "const assertion failed"_s);

    expectTypeError(fn(
        "const x = 1;"
        "let z = 3;"
        "const_assert(x > z);"_s),
        "cannot use runtime value in constant expression"_s);
}

TEST(WGSLTypeCheckingTests, Constants)
{
    // Test out of bounds
    expectTypeError(fn("const x = array(0); _ = x[-1];"_s), "index -1 is out of bounds [0..0]"_s);
    expectNoError(fn("const x = array(0); _ = x[0];"_s));
    expectTypeError(fn("const x = array(0); _ = x[1];"_s), "index 1 is out of bounds [0..0]"_s);

    expectTypeError(fn("const x = mat2x2(0, 0, 0, 0); _ = x[-1];"_s), "index -1 is out of bounds [0..1]"_s);
    expectNoError(fn("const x = mat2x2(0, 0, 0, 0); _ = x[0];"_s));
    expectNoError(fn("const x = mat2x2(0, 0, 0, 0); _ = x[1];"_s));
    expectTypeError(fn("const x = mat2x2(0, 0, 0, 0); _ = x[2];"_s), "index 2 is out of bounds [0..1]"_s);

    expectTypeError(fn("const x = vec2(0); _ = x[-1];"_s), "index -1 is out of bounds [0..1]"_s);
    expectNoError(fn("const x = vec2(0); _ = x[0];"_s));
    expectNoError(fn("const x = vec2(0);_ = x[1];"_s));
    expectTypeError(fn("const x = vec2(0);_ = x[2];"_s), "index 2 is out of bounds [0..1]"_s);

    // Test invalid explicit u32 conversion
    expectTypeError(fn("let x = u32(37359285590000);"_s), "value 37359285590000 cannot be represented as 'u32'"_s);
}

TEST(WGSLTypeCheckingTests, Continue)
{
    expectTypeError(fn(
        "switch 1 {"
        "default: {"
        "   continue;"
        "}"
        "}"_s), "continue statement must be in a loop"_s);
}

TEST(WGSLTypeCheckingTests, Division)
{
    // Test AbstractInt
    expectTypeError(fn("_ = 42 / 0;"_s), "invalid division by zero"_s);
    expectTypeError(fn("_ = 42 / vec2(1, 0);"_s), "invalid division by zero"_s);
    expectTypeError(fn("_ = vec2(42) / 0;"_s), "invalid division by zero"_s);
    expectTypeError(fn("_ = vec2(42) / vec2(1, 0);"_s), "invalid division by zero"_s);

    expectTypeError(fn("let x = 42; _ = x / 0;"_s), "invalid division by zero"_s);
    expectTypeError(fn("let x = 42; _ = x / vec2(1, 0);"_s), "invalid division by zero"_s);
    expectTypeError(fn("let x = 42; _ = vec2(x) / 0;"_s), "invalid division by zero"_s);
    expectTypeError(fn("let x = 42; _ = vec2(x) / vec2(1, 0);"_s), "invalid division by zero"_s);

    expectNoError(fn("_ = (-2147483647 - 1) / -1;"_s));
    expectNoError(fn("_ = vec2(-2147483647 - 1, 1) / vec2(-1, 1);"_s));

    expectTypeError(fn("_ = (-9223372036854775807 - 1) / -1;"_s), "invalid division overflow"_s);
    expectTypeError(fn("_ = vec2(-9223372036854775807 - 1, 1) / vec2(-1, 1);"_s), "invalid division overflow"_s);

    // Test i32
    expectTypeError(fn("_ = 42i / 0i;"_s), "invalid division by zero"_s);
    expectTypeError(fn("_ = 42i / vec2(1i, 0i);"_s), "invalid division by zero"_s);
    expectTypeError(fn("_ = vec2(42i) / 0i;"_s), "invalid division by zero"_s);
    expectTypeError(fn("_ = vec2(42i) / vec2(1i, 0i);"_s), "invalid division by zero"_s);

    expectTypeError(fn("let x = 42i; _ = x / 0i;"_s), "invalid division by zero"_s);
    expectTypeError(fn("let x = 42i; _ = x / vec2(1i, 0i);"_s), "invalid division by zero"_s);
    expectTypeError(fn("let x = 42i; _ = vec2(x) / 0i;"_s), "invalid division by zero"_s);
    expectTypeError(fn("let x = 42i; _ = vec2(x) / vec2(1i, 0i);"_s), "invalid division by zero"_s);

    expectTypeError(fn("_ = (-2147483647i - 1i) / -1i;"_s), "invalid division overflow"_s);
    expectTypeError(fn("_ = vec2(-2147483647i - 1i, 1i) / vec2(-1i, 1i);"_s), "invalid division overflow"_s);

    // Test u32
    expectTypeError(fn("_ = 42u / 0u;"_s), "invalid division by zero"_s);
    expectTypeError(fn("_ = 42u / vec2(1u, 0u);"_s), "invalid division by zero"_s);
    expectTypeError(fn("_ = vec2(42u) / 0u;"_s), "invalid division by zero"_s);
    expectTypeError(fn("_ = vec2(42u) / vec2(1u, 0u);"_s), "invalid division by zero"_s);

    expectTypeError(fn("let x = 42u; _ = x / 0u;"_s), "invalid division by zero"_s);
    expectTypeError(fn("let x = 42u; _ = x / vec2(1u, 0u);"_s), "invalid division by zero"_s);
    expectTypeError(fn("let x = 42u; _ = vec2(x) / 0u;"_s), "invalid division by zero"_s);
    expectTypeError(fn("let x = 42u; _ = vec2(x) / vec2(1u, 0u);"_s), "invalid division by zero"_s);

    // Test AbstractFloat
    expectTypeError(fn("_ = 42.0 / 0.0;"_s), "value Infinity cannot be represented as '<AbstractFloat>'"_s);
    expectTypeError(fn("_ = 42.0 / vec2(1.0, 0.0);"_s), "value vec2(42, Infinity) cannot be represented as 'vec2<<AbstractFloat>>'"_s);
    expectTypeError(fn("_ = vec2(42.0) / 0.0;"_s), "value vec2(Infinity, Infinity) cannot be represented as 'vec2<<AbstractFloat>>'"_s);
    expectTypeError(fn("_ = vec2(42.0) / vec2(1.0, 0.0);"_s), "value vec2(42, Infinity) cannot be represented as 'vec2<<AbstractFloat>>'"_s);

    expectNoError(fn("let x = 42.0; _ = x / 0.0;"_s));
    expectNoError(fn("let x = 42.0; _ = x / vec2(1.0, 0.0);"_s));
    expectNoError(fn("let x = 42.0; _ = vec2(x) / 0.0;"_s));
    expectNoError(fn("let x = 42.0; _ = vec2(x) / vec2(1.0, 0.0);"_s));

    expectNoError(fn("_ = (-2147483647.0 - 1.0) / -1.0;"_s));
    expectNoError(fn("_ = vec2(-2147483647.0 - 1.0, 1.0) / vec2(-1.0, 1.0);"_s));

    expectNoError(fn("_ = (-9223372036854775807.0 - 1.0) / -1.0;"_s));
    expectNoError(fn("_ = vec2(-9223372036854775807.0 - 1.0, 1.0) / vec2(-1.0, 1.0);"_s));

    expectNoError(fn("_ = -340282346638528859811704183484516925440.0 - 1.0 / -1.0;"_s));
    expectNoError(fn("_ = vec2(-340282346638528859811704183484516925440.0 - 1.0, 1.0) / vec2(-1.0, 1.0);"_s));

    // Test f32
    expectTypeError(fn("_ = 42f / 0f;"_s), "value Infinity cannot be represented as 'f32'"_s);
    expectTypeError(fn("_ = 42f / vec2(1f, 0f);"_s), "value vec2(42f, Infinity) cannot be represented as 'vec2<f32>'"_s);
    expectTypeError(fn("_ = vec2(42f) / 0f;"_s), "value vec2(Infinity, Infinity) cannot be represented as 'vec2<f32>'"_s);
    expectTypeError(fn("_ = vec2(42f) / vec2(1f, 0f);"_s), "value vec2(42f, Infinity) cannot be represented as 'vec2<f32>'"_s);

    expectNoError(fn("let x = 42f; _ = x / 0f;"_s));
    expectNoError(fn("let x = 42f; _ = x / vec2(1f, 0f);"_s));
    expectNoError(fn("let x = 42f; _ = vec2(x) / 0f;"_s));
    expectNoError(fn("let x = 42f; _ = vec2(x) / vec2(1f, 0f);"_s));

    expectNoError(fn("_ = (-2147483647f - 1f) / -1f;"_s));
    expectNoError(fn("_ = vec2(-2147483647f - 1f, 1f) / vec2(-1f, 1f);"_s));

    expectNoError(fn("_ = (-9223372036854775807.f - 1.f) / -1.f;"_s));
    expectNoError(fn("_ = vec2(-9223372036854775807.f - 1.f, 1.f) / vec2(-1.f, 1.f);"_s));

    expectNoError(fn("_ = (-340282346638528859811704183484516925439.f - 1f) / -1f;"_s));
    expectNoError(fn("_ = vec2(-340282346638528859811704183484516925439.f - 1f, 1f) / vec2(-1f, 1f);"_s));

    // Test i32 compound
    expectTypeError(fn("var y: vec2<i32>; y /= 0;"_s), "invalid division by zero"_s);
    expectTypeError(fn("var y: vec2<i32>; y /= vec2(1, 0);"_s), "invalid division by zero"_s);
    expectTypeError(fn("var y: vec2<i32>; y[0] /= 0;"_s), "invalid division by zero"_s);
    expectTypeError(fn("var y: vec2<i32>; y[0] /= vec2(1, 0);"_s), "invalid division by zero"_s);

    // Test u32 compound
    expectTypeError(fn("var y: vec2<u32>; y /= 0;"_s), "invalid division by zero"_s);
    expectTypeError(fn("var y: vec2<u32>; y /= vec2(1, 0);"_s), "invalid division by zero"_s);
    expectTypeError(fn("var y: vec2<u32>; y[0] /= 0;"_s), "invalid division by zero"_s);
    expectTypeError(fn("var y: vec2<u32>; y[0] /= vec2(1, 0);"_s), "invalid division by zero"_s);

    // Test f32 compound
    expectNoError(fn("var y: vec2<f32>; y /= 0;"_s));
    expectNoError(fn("var y: vec2<f32>; y /= vec2(1, 0);"_s));
    expectNoError(fn("var y: vec2<f32>; y[0] /= 0;"_s));
    expectNoError(fn("var y: vec2<f32>; y[0] /= vec2(1, 0).y;"_s));

    // Test divisor overflow
    expectTypeError(fn("let x = 1; _ = x / 8144182087775404419;"_s), "value 8144182087775404419 cannot be represented as 'i32'"_s);
}

TEST(WGSLTypeCheckingTests, EmptyStruct)
{
    expectTypeError("struct S { }"_s, "structures must have at least one member"_s);
}

TEST(WGSLTypeCheckingTests, F16)
{
    expectTypeError(fn("_ = 1h"_s), "f16 literal used without f16 extension enabled"_s);

    expectTypeError("fn f() -> f16 { }"_s, "f16 type used without f16 extension enabled"_s);
    expectTypeError(fn("var x: f16;"_s), "f16 type used without f16 extension enabled"_s);
    expectTypeError(fn("_ = vec2h().x;"_s), "f16 type used without f16 extension enabled"_s);
    expectTypeError(fn("_ = vec3h().x;"_s), "f16 type used without f16 extension enabled"_s);
    expectTypeError(fn("_ = vec4h().x;"_s), "f16 type used without f16 extension enabled"_s);
    expectTypeError(fn("_ = mat2x2h()[0].x;"_s), "f16 type used without f16 extension enabled"_s);
}

TEST(WGSLTypeCheckingTests, For)
{
    expectTypeError(fn("for (var i = 0; i; i++) { }"_s), "for-loop condition must be bool, got ref<function, i32, read_write>"_s);
}

TEST(WGSLTypeCheckingTests, FunctionCall)
{
    expectTypeError("fn f1(x: f32) -> f32 { }"_s, "missing return at end of function"_s);
    expectTypeError(fn("_ = f0();"_s), "unresolved call target 'f0'"_s);
    expectTypeError("fn f1(x: f32) { } fn f() { f1(0i); }"_s, "type in function call does not match parameter type: expected 'f32', found 'i32'"_s);
    expectTypeError("fn f3() { } fn f() { let f3: i32 = 0; f3(); }"_s, "cannot call value of type 'i32'"_s);
    expectTypeError("fn f1(x: f32) -> f32 { return x; } fn f() { let x: i32 = f1(0); }"_s, "cannot initialize var of type 'i32' with value of type 'f32'"_s);
    expectTypeError("fn f1(x: f32) { } fn f() { f1(); }"_s, "funtion call has too few arguments: expected 1, found 0"_s);
    expectTypeError("fn f1(x: f32) { } fn f() { f1(0, 0); }"_s, "funtion call has too many arguments: expected 1, found 2"_s);
    expectTypeError("fn f3() { } fn f() { let x = f3(); }"_s, "function 'f3' does not return a value"_s);
    expectTypeError(
        "@group(0) @binding(0) var<storage, read_write> x: array<f32>;"
        "fn f() {"
        "let arrayLength = 1; _ = arrayLength(&x);"
        "}"_s, "cannot call value of type 'i32'"_s);
    expectTypeError(fn("let array = 1f; _ = array<i32, 1>(1);"_s), "cannot call value of type 'f32'"_s);
    expectTypeError(fn("let vec2 = 1u; _ = vec2<i32>(1);"_s), "cannot call value of type 'u32'"_s);
    expectTypeError(enableF16(fn("let bitcast = 1h; _ = bitcast<i32>(1);"_s)), "cannot call value of type 'f16'"_s);
}

TEST(WGSLTypeCheckingTests, Fuzz_133788509)
{
    expectTypeError(file("fuzz-133788509.wgsl"_s), "maximum parser recursive depth reached"_s);
}

TEST(WGSLTypeCheckingTests, If)
{
    expectTypeError(fn("if array<f32, 9>() { return; }"_s), "expected 'bool', found 'array<f32, 9>'"_s);
}

TEST(WGSLTypeCheckingTests, Limits)
{
    expectTypeError(file("limits-brace-enclosed.wgsl"_s), "maximum parser recursive depth reached"_s);
    expectTypeError(file("limits-composite-type.wgsl"_s), "composite type may not be nested more than 15 levels"_s);
    expectTypeError(file("limits-const-array.wgsl"_s), "constant array cannot have more than 2047 elements"_s);
    expectTypeError(file("limits-function-parameters.wgsl"_s), "function cannot have more than 255 parameters"_s);
    expectTypeError(file("limits-struct-members.wgsl"_s), "struct cannot have more than 1023 members"_s);
    expectTypeError(file("limits-switch-case.wgsl"_s), "switch statement cannot have more than 1023 case selector values"_s);
}

TEST(WGSLTypeCheckingTests, Modulo)
{
    // Test AbstractInt
    expectTypeError(fn("_ = 42 % 0;"_s), "invalid modulo by zero"_s);
    expectTypeError(fn("_ = 42 % vec2(1, 0);"_s), "invalid modulo by zero"_s);
    expectTypeError(fn("_ = vec2(42) % 0;"_s), "invalid modulo by zero"_s);
    expectTypeError(fn("_ = vec2(42) % vec2(1, 0);"_s), "invalid modulo by zero"_s);

    expectTypeError(fn("let x = 42; _ = x % 0;"_s), "invalid modulo by zero"_s);
    expectTypeError(fn("let x = 42; _ = x % vec2(1, 0);"_s), "invalid modulo by zero"_s);
    expectTypeError(fn("let x = 42; _ = vec2(x) % 0;"_s), "invalid modulo by zero"_s);
    expectTypeError(fn("let x = 42; _ = vec2(x) % vec2(1, 0);"_s), "invalid modulo by zero"_s);

    expectNoError(fn("_ = (-2147483647 - 1) % -1;"_s));
    expectNoError(fn("_ = vec2(-2147483647 - 1, 1) % vec2(-1, 1);"_s));

    expectTypeError(fn("_ = (-9223372036854775807 - 1) % -1;"_s), "invalid modulo overflow"_s);
    expectTypeError(fn("_ = vec2(-9223372036854775807 - 1, 1) % vec2(-1, 1);"_s), "invalid modulo overflow"_s);

    // Test i32
    expectTypeError(fn("_ = 42i % 0i;"_s), "invalid modulo by zero"_s);
    expectTypeError(fn("_ = 42i % vec2(1i, 0i);"_s), "invalid modulo by zero"_s);
    expectTypeError(fn("_ = vec2(42i) % 0i;"_s), "invalid modulo by zero"_s);
    expectTypeError(fn("_ = vec2(42i) % vec2(1i, 0i);"_s), "invalid modulo by zero"_s);

    expectTypeError(fn("let x = 42i; _ = x % 0i;"_s), "invalid modulo by zero"_s);
    expectTypeError(fn("let x = 42i; _ = x % vec2(1i, 0i);"_s), "invalid modulo by zero"_s);
    expectTypeError(fn("let x = 42i; _ = vec2(x) % 0i;"_s), "invalid modulo by zero"_s);
    expectTypeError(fn("let x = 42i; _ = vec2(x) % vec2(1i, 0i);"_s), "invalid modulo by zero"_s);

    expectTypeError(fn("_ = (-2147483647i - 1i) % -1i;"_s), "invalid modulo overflow"_s);
    expectTypeError(fn("_ = vec2(-2147483647i - 1i, 1i) % vec2(-1i, 1i);"_s), "invalid modulo overflow"_s);

    // Test u32
    expectTypeError(fn("_ = 42u % 0u;"_s), "invalid modulo by zero"_s);
    expectTypeError(fn("_ = 42u % vec2(1u, 0u);"_s), "invalid modulo by zero"_s);
    expectTypeError(fn("_ = vec2(42u) % 0u;"_s), "invalid modulo by zero"_s);
    expectTypeError(fn("_ = vec2(42u) % vec2(1u, 0u);"_s), "invalid modulo by zero"_s);

    expectTypeError(fn("let x = 42u; _ = x % 0u;"_s), "invalid modulo by zero"_s);
    expectTypeError(fn("let x = 42u; _ = x % vec2(1u, 0u);"_s), "invalid modulo by zero"_s);
    expectTypeError(fn("let x = 42u; _ = vec2(x) % 0u;"_s), "invalid modulo by zero"_s);
    expectTypeError(fn("let x = 42u; _ = vec2(x) % vec2(1u, 0u);"_s), "invalid modulo by zero"_s);

    // Test AbstractFloat
    expectTypeError(fn("_ = 42.0 % 0.0;"_s), "value NaN cannot be represented as '<AbstractFloat>'"_s);
    expectTypeError(fn("_ = 42.0 % vec2(1.0, 0.0);"_s), "value vec2(0, NaN) cannot be represented as 'vec2<<AbstractFloat>>'"_s);
    expectTypeError(fn("_ = vec2(42.0) % 0.0;"_s), "value vec2(NaN, NaN) cannot be represented as 'vec2<<AbstractFloat>>'"_s);
    expectTypeError(fn("_ = vec2(42.0) % vec2(1.0, 0.0);"_s), "value vec2(0, NaN) cannot be represented as 'vec2<<AbstractFloat>>'"_s);

    expectNoError(fn("let x = 42.0; _ = x % 0.0;"_s));
    expectNoError(fn("let x = 42.0; _ = x % vec2(1.0, 0.0);"_s));
    expectNoError(fn("let x = 42.0; _ = vec2(x) % 0.0;"_s));
    expectNoError(fn("let x = 42.0; _ = vec2(x) % vec2(1.0, 0.0);"_s));

    expectNoError(fn("_ = (-2147483647.0 - 1.0) % -1.0;"_s));
    expectNoError(fn("_ = vec2(-2147483647.0 - 1.0, 1.0) % vec2(-1.0, 1.0);"_s));

    expectNoError(fn("_ = (-9223372036854775807.0 - 1.0) % -1.0;"_s));
    expectNoError(fn("_ = vec2(-9223372036854775807.0 - 1.0, 1.0) % vec2(-1.0, 1.0);"_s));

    expectNoError(fn("_ = -340282346638528859811704183484516925440.0 - 1.0 % -1.0;"_s));
    expectNoError(fn("_ = vec2(-340282346638528859811704183484516925440.0 - 1.0, 1.0) % vec2(-1.0, 1.0);"_s));

    // Test f32
    expectTypeError(fn("_ = 42f % 0f;"_s), "value NaN cannot be represented as 'f32'"_s);
    expectTypeError(fn("_ = 42f % vec2(1f, 0f);"_s), "value vec2(0f, NaN) cannot be represented as 'vec2<f32>'"_s);
    expectTypeError(fn("_ = vec2(42f) % 0f;"_s), "value vec2(NaN, NaN) cannot be represented as 'vec2<f32>'"_s);
    expectTypeError(fn("_ = vec2(42f) % vec2(1f, 0f);"_s), "value vec2(0f, NaN) cannot be represented as 'vec2<f32>'"_s);

    expectNoError(fn("let x = 42f; _ = x % 0f;"_s));
    expectNoError(fn("let x = 42f; _ = x % vec2(1f, 0f);"_s));
    expectNoError(fn("let x = 42f; _ = vec2(x) % 0f;"_s));
    expectNoError(fn("let x = 42f; _ = vec2(x) % vec2(1f, 0f);"_s));

    expectNoError(fn("_ = (-2147483647f - 1f) % -1f;"_s));
    expectNoError(fn("_ = vec2(-2147483647f - 1f, 1f) % vec2(-1f, 1f);"_s));

    expectNoError(fn("_ = (-9223372036854775807.f - 1.f) % -1.f;"_s));
    expectNoError(fn("_ = vec2(-9223372036854775807.f - 1.f, 1.f) % vec2(-1.f, 1.f);"_s));

    expectNoError(fn("_ = (-340282346638528859811704183484516925439.f - 1f) % -1f;"_s));
    expectNoError(fn("_ = vec2(-340282346638528859811704183484516925439.f - 1f, 1f) % vec2(-1f, 1f);"_s));

    // Test i32 compound
    expectTypeError(fn("var y: vec2<i32>; y %= 0;"_s), "invalid modulo by zero"_s);
    expectTypeError(fn("var y: vec2<i32>; y %= vec2(1, 0);"_s), "invalid modulo by zero"_s);
    expectTypeError(fn("var y: vec2<i32>; y[0] %= 0;"_s), "invalid modulo by zero"_s);
    expectTypeError(fn("var y: vec2<i32>; y[0] %= vec2(1, 0);"_s), "invalid modulo by zero"_s);

    // Test u32 compound
    expectTypeError(fn("var y: vec2<u32>; y %= 0;"_s), "invalid modulo by zero"_s);
    expectTypeError(fn("var y: vec2<u32>; y %= vec2(1, 0);"_s), "invalid modulo by zero"_s);
    expectTypeError(fn("var y: vec2<u32>; y[0] %= 0;"_s), "invalid modulo by zero"_s);
    expectTypeError(fn("var y: vec2<u32>; y[0] %= vec2(1, 0);"_s), "invalid modulo by zero"_s);

    // Test f32 compound
    expectNoError(fn("var y: vec2<f32>; y %= 0;"_s));
    expectNoError(fn("var y: vec2<f32>; y %= vec2(1, 0);"_s));
    expectNoError(fn("var y: vec2<f32>; y[0] %= 0;"_s));
    expectNoError(fn("var y: vec2<f32>; y[0] %= vec2(1, 0).y;"_s));
}

TEST(WGSLTypeCheckingTests, Overload)
{
    // Test explicit type arguments
    expectTypeError(fn("let v0 = vec2<i32>(0.0, 0.0);"_s), "no matching overload for initializer vec2<i32>(<AbstractFloat>, <AbstractFloat>)"_s);

    // test constraints
    expectTypeError(
        "struct S { x: i32, };"
        "fn f() { let x = S(0); let x2 = x + x; }"_s,
        "no matching overload for operator +(S, S)"_s);
    expectTypeError(
        "struct S { x: i32, };"
        "fn f() { let x = S(0); let x3 = vec2(x, x); }"_s,
        "no matching overload for initializer vec2(S, S)"_s);
    expectTypeError(
        "struct S { x: i32, };"
        "fn f() { let x = S(0); let x4 = mat2x2(0u, 0u, 0u, 0u); }"_s,
        "no matching overload for initializer mat2x2(u32, u32, u32, u32)"_s);
    expectTypeError(
        "struct S { x: i32, };"
        "fn f() { let x = S(0); let x5 = vec2<S>(x, x); }"_s,
        "no matching overload for initializer vec2<S>(S, S)"_s);
    expectTypeError(
        "struct S { x: i32, };"
        "fn f() { let x6 = vec2<S>(vec2(0, 0)); }"_s,
        "no matching overload for initializer vec2<S>(vec2<<AbstractInt>>)"_s);
    expectTypeError(
        fn("let x7 = -1u;"_s),
        "no matching overload for operator -(u32)"_s);
    expectTypeError(
        fn("let x8 = -vec2(1u, 1u);"_s),
        "no matching overload for operator -(vec2<u32>)"_s);
    expectTypeError(
        fn("const x: u32 = 1i << 1;"_s),
        "cannot initialize var of type 'u32' with value of type 'i32'"_s);

    // test textureGather
    expectTypeError(fn("_ = textureGather();"_s), "no matching overload for initializer textureGather()"_s);

    // test compound assignment
    expectTypeError(fn("var f = 1f; f /= vec2f(1);"_s), "cannot assign 'vec2<f32>' to 'f32'"_s);
}

TEST(WGSLTypeCheckingTests, ParameterScope)
{
    expectTypeError("fn f(x: f32) { let x: i32 = 1; }"_s, "redeclaration of 'x'"_s);
}

TEST(WGSLTypeCheckingTests, PhonyAssignment)
{
    expectTypeError(
        "fn f() { } fn main() { _ = f(); }"_s,
        "function 'f' does not return a value"_s);
}


TEST(WGSLTypeCheckingTests, PointerAssignment)
{
    expectTypeError(fn("var x = 1; *&*&x=3 x=1;"_s), "Expected a ;, but got a Identifier"_s);
}

TEST(WGSLTypeCheckingTests, PointerAsConstant)
{
    expectTypeError(
        "var<storage> x: i32; @id(*&x) var<storage> y: i32;"_s,
        "cannot use runtime value in constant expression"_s);
}

TEST(WGSLTypeCheckingTests, PointerAccessMode)
{
    expectTypeError("fn f1(x: ptr<function, i32, read>) { }"_s, "only pointers in <storage> address space may specify an access mode"_s);
}

TEST(WGSLTypeCheckingTests, Redeclaration)
{
    // error during reordering of declarations
    expectTypeError(
        "struct f { x: i32 }"
        "override f = 1;"
        "fn f() { }"_s,
        "redeclaration of 'f'"_s);

    // error during type checking
    expectTypeError(fn(
        "let x = 1;"
        "var x = 2;"
        "const x = 2;"_s),
        "redeclaration of 'x'"_s);
}

TEST(WGSLTypeCheckingTests, References)
{
    // Test reference assignment
    expectTypeError("@group(0) @binding(0) var<storage, read> x: i32; fn f() { x = 0; }"_s, "cannot store into a read-only type 'ref<storage, i32, read>'"_s);
    expectTypeError("@group(0) @binding(1) var<storage, read_write> y: i32; fn f() { y = 0u; }"_s, "cannot assign value of type 'u32' to 'i32'"_s);
    expectTypeError(fn("let z: i32 = 0; z = 0;"_s), "cannot assign to a value of type 'i32'"_s);

    // Test decrement/increment
    expectTypeError(fn("let x = 0i; x++;"_s), "cannot modify a value of type 'i32'"_s);
    expectTypeError("@group(0) @binding(0) var<storage, read> x: i32; fn f() { x++; }"_s, "cannot modify read-only type 'ref<storage, i32, read>'"_s);
    expectTypeError(fn("var x = 0f; x++;"_s), "increment can only be applied to integers, found f32"_s);
}

TEST(WGSLTypeCheckingTests, Reordering)
{
    expectTypeError("const a = a * 2;"_s, "encountered a dependency cycle: a -> a"_s);
    expectTypeError("fn f() { f(); };"_s, "encountered a dependency cycle: f -> f"_s);
    expectTypeError("struct S { s: S }"_s, "encountered a dependency cycle: S -> S"_s);
    expectTypeError("const a = S(array(1)); struct S { x: array<i32, a.x[0]> };"_s, "encountered a dependency cycle: a -> S -> a"_s);
    expectTypeError(
        "const y = x * 2;"
        "const z = y;"
        "const w = z;"
        "const x = w;"_s,
        "encountered a dependency cycle: y -> x -> w -> z -> y"_s);
    expectTypeError(
        "const y = x * 2;"
        "const y = 2;"_s,
        "redeclaration of 'y'"_s);
}

TEST(WGSLTypeCheckingTests, RequiredAlignment)
{
    expectTypeError(
        "struct S1 { y1: array<i32, 2> }"
        "@group(0) @binding(1) var<uniform> x1: S1;"_s,
        "arrays in the uniform address space must have a stride multiple of 16 bytes, but has a stride of 4 bytes"_s);

    expectTypeError(
        "struct S1 { x1: i32, @align(4) y1: array<vec4i, 2> }"
        "@group(0) @binding(1) var<uniform> x1: S1;"_s,
        "@align attribute 4 of struct member is not a multiple of the type's alignment 16"_s);

    expectTypeError(
        "struct S1 { x1: i32, y3: T }"
        "struct T { x: i32, y: i32, }"
        "@group(0) @binding(1) var<uniform> x1: S1;"_s,
        "offset of struct member S1::y3 must be a multiple of 16 bytes, but its offset is 4 bytes"_s);

    expectTypeError(
        "struct S1 { y3: T, z3: i32 }"
        "struct T { x: i32, y: i32, }"
        "@group(0) @binding(1) var<uniform> x1: S1;"_s,
        "uniform address space requires that the number of bytes between S1::y3 and S1::z3 must be at least 16 bytes, but it is 8 bytes"_s);
}

TEST(WGSLTypeCheckingTests, Return)
{
    expectTypeError("fn noReturn() { return 0i; }"_s, "return statement type does not match its function return type, returned 'i32', expected 'void'"_s);
    expectTypeError("fn typeMisMatch() -> f32 { return 0i; }"_s, "return statement type does not match its function return type, returned 'i32', expected 'f32'"_s);
    expectTypeError("fn conversionFailure() -> i32 { return 500000000000; }"_s, "value 500000000000 cannot be represented as 'i32'"_s);
}

TEST(WGSLTypeCheckingTests, Struct)
{
    expectTypeError("struct S { x: f32, x: T, y: i32 }"_s, "duplicate member 'x' in struct 'S'"_s);

    expectTypeError(
        "struct T { x: u32, y: U, }"
        "struct U { x: u32, y: array<u32>, }"_s,
        "a struct that contains a runtime array cannot be nested inside another struct"_s);

    expectTypeError(
        "struct S { x: f32, y: i32, }"
        "fn f() { _ = S(0); }"_s,
        "struct initializer has too few inputs: expected 2, found 1"_s);

    expectTypeError(
        "struct S { x: f32, y: i32, }"
        "fn f() { _ = S(0, 0, 0); }"_s,
        "struct initializer has too many inputs: expected 2, found 3"_s);

    expectTypeError(
        "struct S { x: f32, y: i32, }"
        "fn f() { _ = S(0i, 0); }"_s,
        "type in struct initializer does not match struct member type: expected 'f32', found 'i32'"_s);

    expectTypeError(
        "struct S { x: f32, y: i32, }"
        "fn f() { _ = S(0f, 0f); }"_s,
        "type in struct initializer does not match struct member type: expected 'i32', found 'f32'"_s);
}

TEST(WGSLTypeCheckingTests, Switch)
{
    expectTypeError(fn(
        "let x: u32 = 42;"
        "switch 1u {"
        "    case 1i, default,: { }"
        "}"_s),
        "the case selector values must have the same type as the selector expression: the selector expression has type 'u32' and case selector has type 'i32'"_s);

    expectTypeError(fn(
        "switch false {"
        "    case default,: { }"
        "}"_s),
        "switch selector must be of type i32 or u32"_s);
}

TEST(WGSLTypeCheckingTests, TextureGather)
{
    expectTypeError(
        "@group(0) @binding(0) var t: texture_2d<f32>;"
        "@group(0) @binding(1) var s: sampler;"
        "@fragment fn f() {"
        "    _ = textureGather(4, t, s, vec2());"
        "}"_s,
        "the component argument must be at least 0 and at most 3. component is 4"_s);

    expectTypeError(
        "@group(0) @binding(0) var t: texture_2d<f32>;"
        "@group(0) @binding(1) var s: sampler;"
        "@fragment fn f() {"
        "    var c = 4;"
        "    _ = textureGather(c, t, s, vec2());"
        "}"_s,
        "the component argument must be a const-expression"_s);

    expectTypeError(
        "@group(0) @binding(0) var s: sampler;"
        "@group(0) @binding(23) var td2d: texture_depth_2d;"
        "@fragment fn f() {"
        "    var offset = vec2i(0);"
        "    _ = textureGather(td2d, s, vec2f(0), offset);"
        "}"_s,
        "the offset argument must be a const-expression"_s);

    expectTypeError(
        "@group(0) @binding(0) var s: sampler;"
        "@group(0) @binding(23) var td2d: texture_depth_2d;"
        "@fragment fn f() {"
        "    _ = textureGather(td2d, s, vec2f(0), vec2i(-9));"
        "}"_s,
        "each component of the offset argument must be at least -8 and at most 7. offset component 0 is -9"_s);

    expectTypeError(
        "@group(0) @binding(0) var s: sampler;"
        "@group(0) @binding(23) var td2d: texture_depth_2d;"
        "@fragment fn f() {"
        "    _ = textureGather(td2d, s, vec2f(0), vec2i(8));"
        "}"_s,
        "each component of the offset argument must be at least -8 and at most 7. offset component 0 is 8"_s);
}

TEST(WGSLTypeCheckingTests, TypesVsValues)
{
    expectTypeError(
        "struct S { x: i32 }"
        "var<private> s : S;"
        "fn f() -> s { return s; }"_s,
        "cannot use value 's' as type"_s);

    expectTypeError(
        "struct S { x: i32 }"
        "fn f() { let s = S(42); _ = s(42); }"_s,
        "cannot call value of type 'S'"_s);

    expectTypeError(
        "struct S { x: i32 }"
        "fn f() { _ = S; }"_s,
        "cannot use type 'S' as value"_s);

    expectTypeError(fn("var x : undefined;"_s), "unresolved type 'undefined'"_s);
    expectTypeError(fn("_ = undefined;"_s), "unresolved identifier 'undefined'"_s);
}

TEST(WGSLTypeCheckingTests, Unicode)
{
    expectTypeError(String::fromUTF8("@group(5)@binding(xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx0߃"), "Expected a ), but got a EOF"_s);
}

TEST(WGSLTypeCheckingTests, Visibility)
{
    expectTypeError(
        "fn f() { storageBarrier(); }"
        "@fragment fn main() { f(); }"_s,
        "built-in cannot be used by fragment pipeline stage"_s);
}

TEST(WGSLTypeCheckingTests, While)
{
    expectTypeError(fn("var i = 0; while (i) { }"_s), "while condition must be bool, got ref<function, i32, read_write>"_s);
}

}
