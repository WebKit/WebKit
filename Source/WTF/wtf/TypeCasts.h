/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#include <type_traits>

namespace WTF {

template <typename ExpectedType, typename ArgType, bool isBaseType = std::is_base_of_v<ExpectedType, ArgType>>
struct TypeCastTraits {
    static bool isOfType(ArgType&)
    {
        // If you're hitting this assertion, it is likely because you used
        // is<>() or downcast<>() with a type that doesn't have the needed
        // TypeCastTraits specialization. Please use the following macro
        // to add that specialization:
        // SPECIALIZE_TYPE_TRAITS_BEGIN() / SPECIALIZE_TYPE_TRAITS_END()
        static_assert(std::is_void_v<ExpectedType>, "Missing TypeCastTraits specialization");
        return false;
    }
};

// Template specialization for the case where ExpectedType is a base of ArgType,
// so we can return return true unconditionally.
template <typename ExpectedType, typename ArgType>
struct TypeCastTraits<ExpectedType, ArgType, true /* isBaseType */> {
    static bool isOfType(ArgType&) { return true; }
};

// Type checking function, to use before casting with downcast<>().
template <typename ExpectedType, typename ArgType>
inline bool is(const ArgType& source)
{
    static_assert(std::is_base_of_v<ArgType, ExpectedType>, "Unnecessary type check");
    return TypeCastTraits<const ExpectedType, const ArgType>::isOfType(source);
}

template <typename ExpectedType, typename ArgType>
inline bool is(ArgType* source)
{
    static_assert(std::is_base_of_v<ArgType, ExpectedType>, "Unnecessary type check");
    return source && TypeCastTraits<const ExpectedType, const ArgType>::isOfType(*source);
}

// Update T's constness to match Reference's.
template <typename Reference, typename T>
using match_constness_t =
    typename std::conditional_t<std::is_const_v<Reference>, typename std::add_const_t<T>, typename std::remove_const_t<T>>;

// Safe downcasting functions.
template<typename Target, typename Source>
inline match_constness_t<Source, Target>& checkedDowncast(Source& source)
{
    static_assert(!std::is_same_v<Source, Target>, "Unnecessary cast to same type");
    static_assert(std::is_base_of_v<Source, Target>, "Should be a downcast");
    RELEASE_ASSERT(is<Target>(source));
    return static_cast<match_constness_t<Source, Target>&>(source);
}

template<typename Target, typename Source>
inline match_constness_t<Source, Target>* checkedDowncast(Source* source)
{
    static_assert(!std::is_same_v<Source, Target>, "Unnecessary cast to same type");
    static_assert(std::is_base_of_v<Source, Target>, "Should be a downcast");
    RELEASE_ASSERT(!source || is<Target>(*source));
    return static_cast<match_constness_t<Source, Target>*>(source);
}

template<typename Target, typename Source>
inline match_constness_t<Source, Target>& uncheckedDowncast(Source& source)
{
    static_assert(!std::is_same_v<Source, Target>, "Unnecessary cast to same type");
    static_assert(std::is_base_of_v<Source, Target>, "Should be a downcast");
    ASSERT_WITH_SECURITY_IMPLICATION(is<Target>(source));
    return static_cast<match_constness_t<Source, Target>&>(source);
}

template<typename Target, typename Source>
inline match_constness_t<Source, Target>* uncheckedDowncast(Source* source)
{
    static_assert(!std::is_same_v<Source, Target>, "Unnecessary cast to same type");
    static_assert(std::is_base_of_v<Source, Target>, "Should be a downcast");
    ASSERT_WITH_SECURITY_IMPLICATION(!source || is<Target>(*source));
    return static_cast<match_constness_t<Source, Target>*>(source);
}

template<typename Target, typename Source>
inline match_constness_t<Source, Target>& downcast(Source& source)
{
    static_assert(!std::is_same_v<Source, Target>, "Unnecessary cast to same type");
    static_assert(std::is_base_of_v<Source, Target>, "Should be a downcast");
    // FIXME: This is too expensive to enable on x86 for now but we should try and
    // enable the RELEASE_ASSERT() on all architectures.
#if CPU(ARM64)
    RELEASE_ASSERT(is<Target>(source));
#else
    ASSERT_WITH_SECURITY_IMPLICATION(is<Target>(source));
#endif
    return static_cast<match_constness_t<Source, Target>&>(source);
}

template<typename Target, typename Source>
inline match_constness_t<Source, Target>* downcast(Source* source)
{
    static_assert(!std::is_same_v<Source, Target>, "Unnecessary cast to same type");
    static_assert(std::is_base_of_v<Source, Target>, "Should be a downcast");
    // FIXME: This is too expensive to enable on x86 for now but we should try and
    // enable the RELEASE_ASSERT() on all architectures.
#if CPU(ARM64)
    RELEASE_ASSERT(!source || is<Target>(*source));
#else
    ASSERT_WITH_SECURITY_IMPLICATION(!source || is<Target>(*source));
#endif
    return static_cast<match_constness_t<Source, Target>*>(source);
}

template<typename Target, typename Source>
inline match_constness_t<Source, Target>* dynamicDowncast(Source& source)
{
    static_assert(!std::is_same_v<Source, Target>, "Unnecessary cast to same type");
    static_assert(std::is_base_of_v<Source, Target>, "Should be a downcast");
    return is<Target>(source) ? &static_cast<match_constness_t<Source, Target>&>(source) : nullptr;
}

template<typename Target, typename Source>
inline match_constness_t<Source, Target>* dynamicDowncast(Source* source)
{
    static_assert(!std::is_same_v<Source, Target>, "Unnecessary cast to same type");
    static_assert(std::is_base_of_v<Source, Target>, "Should be a downcast");
    return is<Target>(source) ? static_cast<match_constness_t<Source, Target>*>(source) : nullptr;
}

// Add support for type checking / casting using is<>() / downcast<>() helpers for a specific class.
#define SPECIALIZE_TYPE_TRAITS_BEGIN(ClassName) \
namespace WTF { \
template <typename ArgType> \
class TypeCastTraits<const ClassName, ArgType, false /* isBaseType */> { \
public: \
    static bool isOfType(ArgType& source) { return isType(source); } \
private:

#define SPECIALIZE_TYPE_TRAITS_END() \
}; \
}

// Explicit specialization for C++ standard library types.

template<typename ExpectedType, typename ArgType, typename Deleter>
inline bool is(std::unique_ptr<ArgType, Deleter>& source)
{
    return is<ExpectedType>(source.get());
}

template<typename ExpectedType, typename ArgType, typename Deleter>
inline bool is(const std::unique_ptr<ArgType, Deleter>& source)
{
    return is<ExpectedType>(source.get());
}

} // namespace WTF

using WTF::TypeCastTraits;
using WTF::is;
using WTF::checkedDowncast;
using WTF::downcast;
using WTF::dynamicDowncast;
using WTF::uncheckedDowncast;
