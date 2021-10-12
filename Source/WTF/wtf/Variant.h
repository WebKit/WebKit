// Copyright (c) 2015, Just Software Solutions Ltd
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or
// without modification, are permitted provided that the
// following conditions are met:
//
// 1. Redistributions of source code must retain the above
// copyright notice, this list of conditions and the following
// disclaimer.
//
// 2. Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following
// disclaimer in the documentation and/or other materials
// provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of
// its contributors may be used to endorse or promote products
// derived from this software without specific prior written
// permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
// CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
// INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
// NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
// EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Copied from https://bitbucket.org/anthonyw/variant/src (5bce47fa788648f79e5ea1d77b0eef2e8f0b2999)

// Modified to make it compile with exceptions disabled.

#pragma once

#include <functional>
#include <limits.h>
#include <new>
#include <stddef.h>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <wtf/Compiler.h>
#include <wtf/StdLibExtras.h>

namespace WTF {

#if COMPILER_SUPPORTS(EXCEPTIONS)
#define __THROW_EXCEPTION(__exception) throw __exception;
#define __NOEXCEPT noexcept
#define __NOEXCEPT_(__exception) noexcept(__exception)
#else
#define __THROW_EXCEPTION(__exception) do { (void)__exception; CRASH(); } while (0);
#define __NOEXCEPT
#define __NOEXCEPT_(...)
#endif

using Monostate = std::monostate;
template<typename... Types>
using Variant = std::variant<Types...>;

template<typename _Type,typename ... _Types>
constexpr bool holds_alternative(Variant<_Types...> const& __v) __NOEXCEPT
{
    return std::holds_alternative<_Type>(__v);
}

template<typename _Type,typename ... _Types>
constexpr _Type& get(Variant<_Types...>& __v)
{
    return std::get<_Type>(__v);
}

template<typename _Type,typename ... _Types>
constexpr _Type&& get(Variant<_Types...>&& __v)
{
    return std::get<_Type>(WTFMove(__v));
}

template<typename _Type,typename ... _Types>
constexpr _Type const& get(Variant<_Types...> const& __v)
{
    return std::get<_Type>(__v);
}

template<typename _Type,typename ... _Types>
constexpr const _Type&& get(Variant<_Types...> const&& __v)
{
    return std::get<_Type>(WTFMove(__v));
}

template <typename Visitor, typename... Variants>
constexpr auto visit(Visitor&& visitor, Variants&&... variants) -> decltype(std::visit(std::forward<Visitor>(visitor), std::forward<Variants>(variants)...))
{
    return std::visit(std::forward<Visitor>(visitor), std::forward<Variants>(variants)...);
}

template<typename _Type,typename ... _Types>
constexpr std::add_pointer_t<_Type> get_if(Variant<_Types...>* variant)
{
    return std::get_if<_Type>(variant);
}

template<typename _Type,typename ... _Types>
constexpr std::add_pointer_t<_Type const> get_if(Variant<_Types...> const* variant)
{
    return std::get_if<_Type>(variant);
}

template<ptrdiff_t _Index,typename ... _Types>
constexpr typename std::add_pointer_t<std::variant_alternative_t<_Index, std::variant<_Types...>>>::__type get_if(Variant<_Types...>* variant)
{
    return std::get_if<_Index>(variant);
}

template<ptrdiff_t _Index,typename ... _Types>
constexpr typename std::add_pointer_t<std::variant_alternative_t<_Index, std::variant<_Types...>>>::__type const get_if(
    Variant<_Types...> const* variant)
{
    return std::get_if<_Index>(variant);
}

// -- WebKit Additions --

template<class V, class... F>
auto switchOn(V&& v, F&&... f) -> decltype(WTF::visit(makeVisitor(std::forward<F>(f)...), std::forward<V>(v)))
{
    return WTF::visit(makeVisitor(std::forward<F>(f)...), std::forward<V>(v));
}

} // namespace WTF

using WTF::Monostate;
using WTF::Variant;
