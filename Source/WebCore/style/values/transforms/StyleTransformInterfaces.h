/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "FloatSize.h"
#include "StyleValueTypes.h"
#include "TransformFunctions.h"
#include "TransformationMatrix.h"

namespace WebCore {
namespace Style {

// MARK: - IsIdentityTransform

template<typename T> concept HasIsIdentityTransform = requires(T t) { t.isIdentity(); };
template<typename Op> struct IsIdentityTransform;

template<typename Op> bool isIdentity(const Op& op)
{
    if constexpr (HasIsIdentityTransform<Op>)
        return op.isIdentity();
    else
        return IsIdentityTransform<Op>{}(op);
}

// MARK: - IsAffectedByTransformOrigin

template<typename T> concept HasIsAffectedByTransformOrigin = requires(T t) { t.isAffectedByTransformOrigin(); };
template<typename Op> struct IsAffectedByTransformOrigin;
template<typename Op> bool isAffectedByTransformOrigin(const Op& op)
{
    if constexpr (HasIsAffectedByTransformOrigin<Op>)
        return op.isAffectedByTransformOrigin();
    else
        return IsAffectedByTransformOrigin<Op>{}(op);
}

// MARK: - IsRepresentableIn2D

template<typename T> concept HasIsRepresentableIn2D = requires(T t) { t.isRepresentableIn2D(); };
template<typename Op> struct IsRepresentableIn2D;
template<typename Op> bool isRepresentableIn2D(const Op& op)
{
    if constexpr (HasIsRepresentableIn2D<Op>)
        return op.isRepresentableIn2D();
    else
        return IsRepresentableIn2D<Op>{}(op);
}

// MARK: - IsWidthDependent

template<typename T> concept HasIsWidthDependent = requires(T t) { t.isWidthDependent(); };
template<typename Op> struct IsWidthDependent;
template<typename Op> bool isWidthDependent(const Op& op)
{
    if constexpr (HasIsWidthDependent<Op>)
        return op.isWidthDependent();
    else
        return IsWidthDependent<Op>{}(op);
}

// MARK: - IsHeightDependent

template<typename T> concept HasIsHeightDependent = requires(T t) { t.isHeightDependent(); };
template<typename Op> struct IsHeightDependent;
template<typename Op> bool isHeightDependent(const Op& op)
{
    if constexpr (HasIsHeightDependent<Op>)
        return op.isHeightDependent();
    else
        return IsHeightDependent<Op>{}(op);
}

// MARK: - IsSizeDependent

template<typename T> concept HasIsSizeDependent = requires(T t) { t.isSizeDependent(); };
template<typename Op> struct IsSizeDependent;
template<typename Op> bool isSizeDependent(const Op& op)
{
    if constexpr (HasIsSizeDependent<Op>)
        return op.isSizeDependent();
    else
        return IsSizeDependent<Op>{}(op);
}

// MARK: - ApplyTransform

template<typename T> concept HasApplyTransform = requires(T t) { t.applyTransform(std::declval<TransformationMatrix&>(), std::declval<const FloatSize&>()); };
template<typename Op> struct ApplyTransform;
template<typename Op> void applyTransform(const Op& op, TransformationMatrix& transform, const FloatSize& referenceBox)
{
    if constexpr (HasApplyTransform<Op>)
        op.applyTransform(transform, referenceBox);
    else
        ApplyTransform<Op>{}(op, transform, referenceBox);
}

// As a convenience for callers that don't already have a `TransformationMatrix`.
template<typename Op> TransformationMatrix computeTransform(const Op& op, const FloatSize& referenceBox)
{
    TransformationMatrix transform;
    applyTransform(op, transform, referenceBox);
    return transform;
}

// MARK: - ApplyUnroundedTransform

template<typename T> concept HasApplyUnroundedTransform = requires(T t) { t.applyUnroundedTransform(std::declval<TransformationMatrix&>(), std::declval<const FloatSize&>()); };
template<typename T> concept HasPlatformApplyUnroundedTransform = requires(T t) { t.applyUnroundedTransform(std::declval<TransformationMatrix&>()); };
template<typename Op> struct ApplyUnroundedTransform;
template<typename Op> void applyUnroundedTransform(const Op& op, TransformationMatrix& transform, const FloatSize& referenceBox)
{
    // Return true if the referenceBox was used in the computation, false otherwise.
    if constexpr (HasApplyUnroundedTransform<Op>)
        op.applyUnroundedTransform(transform, referenceBox);
    else
        ApplyUnroundedTransform<Op>{}(op, transform, referenceBox);
}

// MARK: - Identity

template<typename T> concept HasIdentity = requires { { T::identity() } -> std::convertible_to<T>; };
template<typename Op> struct Identity;
template<typename Op> Op identity(const Op& op)
{
    if constexpr (HasIdentity<Op>)
        return Op::identity();
    else
        return Identity<Op>{}(op);
}

// MARK: - ToPlatform

template<typename T> concept HasToPlatformWithReferenceBox = requires(T t) { t.toPlatform(std::declval<const FloatSize&>()); };
template<typename T> concept HasToPlatformWithoutReferenceBox = requires(T t) { t.toPlatform(); };
template<typename Op> struct ToPlatform;
template<typename Op> decltype(auto) toPlatform(const Op& op, const FloatSize& referenceBox)
{
    if constexpr (HasToPlatformWithReferenceBox<Op>)
        return op.toPlatform(referenceBox);
    else if constexpr (HasToPlatformWithoutReferenceBox<Op>)
        return op.toPlatform();
    else
        return ToPlatform<Op>{}(op, referenceBox);
}

// MARK: - IsIdentityTransform

// Specialization for `FunctionNotation`.
template<CSSValueID Name, typename T> struct IsIdentityTransform<FunctionNotation<Name, T>> {
    bool operator()(const FunctionNotation<Name, T>& function)
    {
        return isIdentity(*function);
    }
};

// Specialization for `std::variant`.
template<typename... Ts> struct IsIdentityTransform<std::variant<Ts...>> {
    bool operator()(const std::variant<Ts...>& value)
    {
        return WTF::switchOn(value, [&](const auto& alternative) { return isIdentity(alternative); });
    }
};

// Specialization for `VariantListProxy`.
template<typename... Ts> struct IsIdentityTransform<WTF::VariantListProxy<std::variant<Ts...>>> {
    bool operator()(const WTF::VariantListProxy<std::variant<Ts...>>& value)
    {
        return WTF::switchOn(value, [&](const auto& alternative) { return isIdentity(alternative); });
    }
};

// MARK: - IsAffectedByTransformOrigin

// Specialization for `FunctionNotation`.
template<CSSValueID Name, typename T> struct IsAffectedByTransformOrigin<FunctionNotation<Name, T>> {
    bool operator()(const FunctionNotation<Name, T>& function)
    {
        return isAffectedByTransformOrigin(*function);
    }
};

// Specialization for `std::variant`.
template<typename... Ts> struct IsAffectedByTransformOrigin<std::variant<Ts...>> {
    bool operator()(const std::variant<Ts...>& value)
    {
        return WTF::switchOn(value, [&](const auto& alternative) { return isAffectedByTransformOrigin(alternative); });
    }
};

// Specialization for `VariantListProxy`.
template<typename... Ts> struct IsAffectedByTransformOrigin<WTF::VariantListProxy<std::variant<Ts...>>> {
    bool operator()(const WTF::VariantListProxy<std::variant<Ts...>>& value)
    {
        return WTF::switchOn(value, [&](const auto& alternative) { return isAffectedByTransformOrigin(alternative); });
    }
};

// Specialization for `None`.
template<> struct IsAffectedByTransformOrigin<None> {
    constexpr bool operator()(const None&)
    {
        return false;
    }
};

// MARK: - IsRepresentableIn2D

// Specialization for `FunctionNotation`.
template<CSSValueID Name, typename T> struct IsRepresentableIn2D<FunctionNotation<Name, T>> {
    bool operator()(const FunctionNotation<Name, T>& function)
    {
        return isRepresentableIn2D(*function);
    }
};

// Specialization for `std::variant`.
template<typename... Ts> struct IsRepresentableIn2D<std::variant<Ts...>> {
    bool operator()(const std::variant<Ts...>& value)
    {
        return WTF::switchOn(value, [&](const auto& alternative) { return isRepresentableIn2D(alternative); });
    }
};

// Specialization for `VariantListProxy`.
template<typename... Ts> struct IsRepresentableIn2D<WTF::VariantListProxy<std::variant<Ts...>>> {
    bool operator()(const WTF::VariantListProxy<std::variant<Ts...>>& value)
    {
        return WTF::switchOn(value, [&](const auto& alternative) { return isRepresentableIn2D(alternative); });
    }
};

// Specialization for `None`.
template<> struct IsRepresentableIn2D<None> {
    constexpr bool operator()(const None&)
    {
        return true;
    }
};

// MARK: - IsWidthDependent

// The default implementation of `IsWidthDependent` returns `false`.
template<typename Op> struct IsWidthDependent {
    constexpr bool operator()(const Op&)
    {
        return false;
    }
};

// Specialization for `FunctionNotation`.
template<CSSValueID Name, typename T> struct IsWidthDependent<FunctionNotation<Name, T>> {
    bool operator()(const FunctionNotation<Name, T>& function)
    {
        return isWidthDependent(*function);
    }
};

// Specialization for `std::variant`.
template<typename... Ts> struct IsWidthDependent<std::variant<Ts...>> {
    bool operator()(const std::variant<Ts...>& value)
    {
        return WTF::switchOn(value, [&](const auto& alternative) { return isWidthDependent(alternative); });
    }
};

// Specialization for `VariantListProxy`.
template<typename... Ts> struct IsWidthDependent<WTF::VariantListProxy<std::variant<Ts...>>> {
    bool operator()(const WTF::VariantListProxy<std::variant<Ts...>>& value)
    {
        return WTF::switchOn(value, [&](const auto& alternative) { return isWidthDependent(alternative); });
    }
};

// Specialization for `None`.
template<> struct IsWidthDependent<None> {
    bool operator()(const None&)
    {
        return false;
    }
};

// MARK: - IsHeightDependent

// The default implementation of `IsHeightDependent` returns `false`.
template<typename Op> struct IsHeightDependent {
    constexpr bool operator()(const Op&)
    {
        return false;
    }
};

// Specialization for `FunctionNotation`.
template<CSSValueID Name, typename T> struct IsHeightDependent<FunctionNotation<Name, T>> {
    bool operator()(const FunctionNotation<Name, T>& function)
    {
        return isHeightDependent(*function);
    }
};

// Specialization for `std::variant`.
template<typename... Ts> struct IsHeightDependent<std::variant<Ts...>> {
    bool operator()(const std::variant<Ts...>& value)
    {
        return WTF::switchOn(value, [&](const auto& alternative) { return isHeightDependent(alternative); });
    }
};

// Specialization for `VariantListProxy`.
template<typename... Ts> struct IsHeightDependent<WTF::VariantListProxy<std::variant<Ts...>>> {
    bool operator()(const WTF::VariantListProxy<std::variant<Ts...>>& value)
    {
        return WTF::switchOn(value, [&](const auto& alternative) { return isHeightDependent(alternative); });
    }
};

// Specialization for `None`.
template<> struct IsHeightDependent<None> {
    bool operator()(const None&)
    {
        return false;
    }
};

// MARK: - IsSizeDependent

// The default implementation of `IsSizeDependent` forwards to `IsWidthDependent`/`IsHeightDependent`.
template<typename Op> struct IsSizeDependent {
    constexpr bool operator()(const Op& value)
    {
        return isWidthDependent(value) || isHeightDependent(value);
    }
};

// Specialization for `FunctionNotation`.
template<CSSValueID Name, typename T> struct IsSizeDependent<FunctionNotation<Name, T>> {
    bool operator()(const FunctionNotation<Name, T>& function)
    {
        return isSizeDependent(*function);
    }
};

// Specialization for `std::variant`.
template<typename... Ts> struct IsSizeDependent<std::variant<Ts...>> {
    bool operator()(const std::variant<Ts...>& value)
    {
        return WTF::switchOn(value, [&](const auto& alternative) { return isSizeDependent(alternative); });
    }
};

// Specialization for `VariantListProxy`.
template<typename... Ts> struct IsSizeDependent<WTF::VariantListProxy<std::variant<Ts...>>> {
    bool operator()(const WTF::VariantListProxy<std::variant<Ts...>>& value)
    {
        return WTF::switchOn(value, [&](const auto& alternative) { return isSizeDependent(alternative); });
    }
};

// MARK: - ApplyTransform

// The default implementation of `ApplyTransform` forwards to the platform implementation.
template<typename T> struct ApplyTransform {
    void operator()(const T& value, TransformationMatrix& transform, const FloatSize& referenceBox)
    {
        toPlatform(value, referenceBox).applyTransform(transform);
    }
};

// Specialization for `FunctionNotation`.
template<CSSValueID Name, typename T> struct ApplyTransform<FunctionNotation<Name, T>> {
    void operator()(const FunctionNotation<Name, T>& function, TransformationMatrix& transform, const FloatSize& referenceBox)
    {
        applyTransform(*function, transform, referenceBox);
    }
};

// Specialization for `std::variant`.
template<typename... Ts> struct ApplyTransform<std::variant<Ts...>> {
    void operator()(const std::variant<Ts...>& value, TransformationMatrix& transform, const FloatSize& referenceBox)
    {
        WTF::switchOn(value, [&](const auto& alternative) { applyTransform(alternative, transform, referenceBox); });
    }
};

// Specialization for `VariantListProxy`.
template<typename... Ts> struct ApplyTransform<WTF::VariantListProxy<std::variant<Ts...>>> {
    void operator()(const WTF::VariantListProxy<std::variant<Ts...>>& value, TransformationMatrix& transform, const FloatSize& referenceBox)
    {
        WTF::switchOn(value, [&](const auto& alternative) { applyTransform(alternative, transform, referenceBox); });
    }
};

// Specialization for `None`.
template<> struct ApplyTransform<None> {
    constexpr void operator()(const None&, TransformationMatrix&, const FloatSize&) { }
};

// MARK: - ApplyUnroundedTransform

// The default implementation of `ApplyUnroundedTransform` forwards to platform implementation if available, otherwise, to `applyTransform`.
template<typename Op> struct ApplyUnroundedTransform {
    void operator()(const Op& value, TransformationMatrix& transform, const FloatSize& referenceBox)
    {
        if constexpr (HasPlatformApplyUnroundedTransform<typename Op::Platform>)
            toPlatform(value, referenceBox).applyUnroundedTransform(transform);
        else
            applyTransform(value, transform, referenceBox);
    }
};

// Specialization for `FunctionNotation`.
template<CSSValueID Name, typename T> struct ApplyUnroundedTransform<FunctionNotation<Name, T>> {
    void operator()(const FunctionNotation<Name, T>& function, TransformationMatrix& transform, const FloatSize& referenceBox)
    {
        applyUnroundedTransform(*function, transform, referenceBox);
    }
};

// Specialization for `std::variant`.
template<typename... Ts> struct ApplyUnroundedTransform<std::variant<Ts...>> {
    void operator()(const std::variant<Ts...>& value, TransformationMatrix& transform, const FloatSize& referenceBox)
    {
        WTF::switchOn(value, [&](const auto& alternative) { applyUnroundedTransform(alternative, transform, referenceBox); });
    }
};

// Specialization for `VariantListProxy`.
template<typename... Ts> struct ApplyUnroundedTransform<WTF::VariantListProxy<std::variant<Ts...>>> {
    void operator()(const WTF::VariantListProxy<std::variant<Ts...>>& value, TransformationMatrix& transform, const FloatSize& referenceBox)
    {
        WTF::switchOn(value, [&](const auto& alternative) { applyUnroundedTransform(alternative, transform, referenceBox); });
    }
};

// Specialization for `None`.
template<> struct ApplyUnroundedTransform<None> {
    constexpr void operator()(const None&, TransformationMatrix&, const FloatSize&) { }
};

// MARK: - Identity

// Specialization for `FunctionNotation`.
template<CSSValueID Name, typename T> struct Identity<FunctionNotation<Name, T>> {
    FunctionNotation<Name, T> operator()(const FunctionNotation<Name, T>& function)
    {
        return { identity(*function) };
    }
};

// Specialization for `std::variant`.
template<typename... Ts> struct Identity<std::variant<Ts...>> {
    std::variant<Ts...> operator()(const std::variant<Ts...>& value)
    {
        return WTF::switchOn(value, [&](const auto& alternative) -> std::variant<Ts...> { return identity(alternative); });
    }
};

// Specialization for `VariantListProxy`.
template<typename... Ts> struct Identity<WTF::VariantListProxy<std::variant<Ts...>>> {
    std::variant<Ts...> operator()(const WTF::VariantListProxy<std::variant<Ts...>>& value)
    {
        return WTF::switchOn(value, [&](const auto& alternative) -> std::variant<Ts...> { return identity(alternative); });
    }
};

// MARK: - ToPlatform

// Specialization for `FunctionNotation`.
template<CSSValueID Name, typename T> struct ToPlatform<FunctionNotation<Name, T>> {
    decltype(auto) operator()(const FunctionNotation<Name, T>& function, const FloatSize& referenceBox)
    {
        return toPlatform(*function, referenceBox);
    }
};

} // namespace Style
} // namespace WebCore
