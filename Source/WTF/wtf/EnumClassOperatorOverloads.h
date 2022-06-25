/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include <type_traits>

#define OVERLOAD_OPERATOR_FOR_ENUM_CLASS_WHEN(enumName, op, enableExpression) \
    template<typename T> \
    constexpr auto operator op(enumName enumEntry, T value) -> std::enable_if_t<(enableExpression), T> \
    { \
        return static_cast<T>(enumEntry) op value; \
    } \
    \
    template<typename T> \
    constexpr auto operator op(T value, enumName enumEntry) -> std::enable_if_t<(enableExpression), T> \
    { \
        return value op static_cast<T>(enumEntry); \
    } \

#define OVERLOAD_MATH_OPERATORS_FOR_ENUM_CLASS_WHEN(enumName, enableExpression) \
    OVERLOAD_OPERATOR_FOR_ENUM_CLASS_WHEN(enumName, +, enableExpression) \
    OVERLOAD_OPERATOR_FOR_ENUM_CLASS_WHEN(enumName, -, enableExpression) \
    OVERLOAD_OPERATOR_FOR_ENUM_CLASS_WHEN(enumName, *, enableExpression) \
    OVERLOAD_OPERATOR_FOR_ENUM_CLASS_WHEN(enumName, /, enableExpression) \
    OVERLOAD_OPERATOR_FOR_ENUM_CLASS_WHEN(enumName, %, enableExpression) \

#define OVERLOAD_MATH_OPERATORS_FOR_ENUM_CLASS_WITH_INTEGRALS(enumName) OVERLOAD_MATH_OPERATORS_FOR_ENUM_CLASS_WHEN(enumName, std::is_integral_v<T>)

#define OVERLOAD_RELATIONAL_OPERATORS_FOR_ENUM_CLASS_WHEN(enumName, enableExpression) \
    OVERLOAD_OPERATOR_FOR_ENUM_CLASS_WHEN(enumName, ==, enableExpression) \
    OVERLOAD_OPERATOR_FOR_ENUM_CLASS_WHEN(enumName, !=, enableExpression) \
    OVERLOAD_OPERATOR_FOR_ENUM_CLASS_WHEN(enumName, <, enableExpression) \
    OVERLOAD_OPERATOR_FOR_ENUM_CLASS_WHEN(enumName, <=, enableExpression) \
    OVERLOAD_OPERATOR_FOR_ENUM_CLASS_WHEN(enumName, >, enableExpression) \
    OVERLOAD_OPERATOR_FOR_ENUM_CLASS_WHEN(enumName, >=, enableExpression) \

#define OVERLOAD_RELATIONAL_OPERATORS_FOR_ENUM_CLASS_WITH_INTEGRALS(enumName) OVERLOAD_RELATIONAL_OPERATORS_FOR_ENUM_CLASS_WHEN(enumName, std::is_integral_v<T>)

#define OVERLOAD_BITWISE_OPERATORS_FOR_ENUM_CLASS_WHEN(enumName, enableExpression) \
    OVERLOAD_OPERATOR_FOR_ENUM_CLASS_WHEN(enumName, |, enableExpression) \
    OVERLOAD_OPERATOR_FOR_ENUM_CLASS_WHEN(enumName, &, enableExpression) \
    OVERLOAD_OPERATOR_FOR_ENUM_CLASS_WHEN(enumName, ^, enableExpression) \

#define OVERLOAD_BITWISE_OPERATORS_FOR_ENUM_CLASS_WITH_INTERGRALS(enumName) OVERLOAD_BITWISE_OPERATORS_FOR_ENUM_CLASS_WHEN(enumName, std::is_integral_v<T>)
