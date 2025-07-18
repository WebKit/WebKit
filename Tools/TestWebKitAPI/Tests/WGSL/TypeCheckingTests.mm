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

TEST(WGSLTypeCheckingTests, AttributeValidation)
{
    expectTypeError("@id(0) override z: array<i32, 1>;"_s, "'array<i32, 1>' cannot be used as the type of an 'override'"_s);
    expectTypeError("@id(u32(dpdx(0))) override y: i32;"_s, "cannot call function from constant context"_s);
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

TEST(WGSLTypeCheckingTests, CompoundAssignment)
{
    expectTypeError(fn("var x = 1; x += 1f;"_s), "no matching overload for operator +(ref<function, i32, read_write>, f32)"_s);
    expectTypeError(fn("let x = 1; x += 1;"_s), "cannot assign to a value of type 'i32'"_s);
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

TEST(WGSLTypeCheckingTests, PhonyAssignment)
{
    expectTypeError(
        "fn f() { } fn main() { _ = f(); }"_s,
        "function 'f' does not return a value"_s);
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

TEST(WGSLTypeCheckingTests, Return)
{
    expectTypeError("fn noReturn() { return 0i; }"_s, "return statement type does not match its function return type, returned 'i32', expected 'void'"_s);
    expectTypeError("fn typeMisMatch() -> f32 { return 0i; }"_s, "return statement type does not match its function return type, returned 'i32', expected 'f32'"_s);
    expectTypeError("fn conversionFailure() -> i32 { return 500000000000; }"_s, "value 500000000000 cannot be represented as 'i32'"_s);
}

TEST(WGSLTypeCheckingTests, ParameterScope)
{
    expectTypeError("fn f(x: f32) { let x: i32 = 1; }"_s, "redeclaration of 'x'"_s);
}

TEST(WGSLTypeCheckingTests, StructWithDuplicateMember)
{
    expectTypeError("struct S { x: f32, x: T, y: i32 }"_s, "duplicate member 'x' in struct 'S'"_s);
}

}
