/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ASTExpression.h"

namespace WGSL::AST {

class BoolLiteral final : public Expression {
    WTF_MAKE_FAST_ALLOCATED;
public:
    BoolLiteral(SourceSpan span, bool value)
        : Expression(span)
        , m_value(value)
    { }

    Kind kind() const override;
    bool value() const { return m_value; }

private:
    bool m_value;
};

class Int32Literal final : public Expression {
    WTF_MAKE_FAST_ALLOCATED;
public:
    Int32Literal(SourceSpan span, int32_t value)
        : Expression(span)
        , m_value(value)
    { }

    Kind kind() const override;
    int32_t value() const { return m_value; }

private:
    int32_t m_value;
};

class Uint32Literal final : public Expression {
    WTF_MAKE_FAST_ALLOCATED;
public:
    Uint32Literal(SourceSpan span, uint32_t value)
        : Expression(span)
        , m_value(value)
    { }

    Kind kind() const override;
    uint32_t value() const { return m_value; }

private:
    uint32_t m_value;
};

class Float32Literal final : public Expression {
    WTF_MAKE_FAST_ALLOCATED;
public:
    Float32Literal(SourceSpan span, float value)
        : Expression(span)
        , m_value(value)
    { }

    Kind kind() const override;
    float value() const { return m_value; }

private:
    float m_value;
};

// Literal ints without size prefix; these ints are signed 64-bit numbers.
// https://gpuweb.github.io/gpuweb/wgsl/#abstractint
class AbstractIntLiteral final : public Expression {
    WTF_MAKE_FAST_ALLOCATED;
public:
    AbstractIntLiteral(SourceSpan span, int64_t value)
        : Expression(span)
        , m_value(value)
    { }

    Kind kind() const override;
    int64_t value() const { return m_value; }

private:
    int64_t m_value;
};

// Literal floats without size prefix; these floats are double-precision
// floating point numbers.
// https://gpuweb.github.io/gpuweb/wgsl/#abstractfloat
class AbstractFloatLiteral final : public Expression {
    WTF_MAKE_FAST_ALLOCATED;
public:
    AbstractFloatLiteral(SourceSpan span, double value)
        : Expression(span)
        , m_value(value)
    { }

    Kind kind() const override;
    double value() const { return m_value; }

private:
    double m_value;
};

} // namespace WGSL::AST

SPECIALIZE_TYPE_TRAITS_WGSL_AST(BoolLiteral)
SPECIALIZE_TYPE_TRAITS_WGSL_AST(Int32Literal)
SPECIALIZE_TYPE_TRAITS_WGSL_AST(Float32Literal)
SPECIALIZE_TYPE_TRAITS_WGSL_AST(Uint32Literal)
SPECIALIZE_TYPE_TRAITS_WGSL_AST(AbstractIntLiteral)
SPECIALIZE_TYPE_TRAITS_WGSL_AST(AbstractFloatLiteral)
