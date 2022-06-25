//-------------------------------------------------------------------------------------------------------------------------------------------------------------
//
// Metal/MTLPrivate.hpp
//
// Copyright 2020-2021 Apple Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//-------------------------------------------------------------------------------------------------------------------------------------------------------------

#pragma once

//-------------------------------------------------------------------------------------------------------------------------------------------------------------

#include "MTLDefines.hpp"

#include <objc/runtime.h>

//-------------------------------------------------------------------------------------------------------------------------------------------------------------

#define _MTL_PRIVATE_CLS(symbol) (Private::Class::s_k##symbol)
#define _MTL_PRIVATE_SEL(accessor) (Private::Selector::s_k##accessor)

//-------------------------------------------------------------------------------------------------------------------------------------------------------------

#if defined(MTL_PRIVATE_IMPLEMENTATION)

#define _MTL_PRIVATE_VISIBILITY __attribute__((visibility("hidden")))
#define _MTL_PRIVATE_IMPORT __attribute__((weak_import))

#if __OBJC__
#define _MTL_PRIVATE_OBJC_LOOKUP_CLASS(symbol) ((__bridge void*)objc_lookUpClass(#symbol))
#else
#define _MTL_PRIVATE_OBJC_LOOKUP_CLASS(symbol) objc_lookUpClass(#symbol)
#endif // __OBJC__

#define _MTL_PRIVATE_DEF_CLS(symbol) void* s_k##symbol _MTL_PRIVATE_VISIBILITY = _MTL_PRIVATE_OBJC_LOOKUP_CLASS(symbol);
#define _MTL_PRIVATE_DEF_PRO(symbol)
#define _MTL_PRIVATE_DEF_SEL(accessor, symbol) SEL s_k##accessor _MTL_PRIVATE_VISIBILITY = sel_registerName(symbol);

#if defined(__MAC_10_16) || defined(__MAC_11_0) || defined(__MAC_12_0) || defined(__IPHONE_14_0) || defined(__IPHONE_15_0) || defined(__TVOS_14_0) || defined(__TVOS_15_0)

#define _MTL_PRIVATE_DEF_STR(type, symbol)                  \
    _MTL_EXTERN type const MTL##symbol _MTL_PRIVATE_IMPORT; \
    type const                         MTL::symbol = (nullptr != &MTL##symbol) ? MTL##symbol : nullptr;

#else

#include <dlfcn.h>

namespace MTL
{
namespace Private
{

    template <typename _Type>
    inline _Type const LoadSymbol(const char* pSymbol)
    {
        const _Type* pAddress = static_cast<_Type*>(dlsym(RTLD_DEFAULT, pSymbol));

        return pAddress ? *pAddress : nullptr;
    }

} // Private
} // MTL

#define _MTL_PRIVATE_DEF_STR(type, symbol) \
    _MTL_EXTERN type const MTL##symbol;    \
    type const             MTL::symbol = Private::LoadSymbol<type>("MTL" #symbol);

#endif // defined(__MAC_10_16) || defined(__MAC_11_0) || defined(__MAC_12_0) || defined(__IPHONE_14_0) || defined(__IPHONE_15_0) || defined(__TVOS_14_0) || defined(__TVOS_15_0)

#else

#define _MTL_PRIVATE_DEF_CLS(symbol) extern void* s_k##symbol;
#define _MTL_PRIVATE_DEF_PRO(symbol)
#define _MTL_PRIVATE_DEF_SEL(accessor, symbol) extern SEL s_k##accessor;
#define _MTL_PRIVATE_DEF_STR(type, symbol)

#endif // MTL_PRIVATE_IMPLEMENTATION

//-------------------------------------------------------------------------------------------------------------------------------------------------------------

namespace MTL
{
namespace Private
{
    namespace Class
    {

    } // Class
} // Private
} // MTL

//-------------------------------------------------------------------------------------------------------------------------------------------------------------

namespace MTL
{
namespace Private
{
    namespace Protocol
    {

    } // Protocol
} // Private
} // MTL

//-------------------------------------------------------------------------------------------------------------------------------------------------------------

namespace MTL
{
namespace Private
{
    namespace Selector
    {

        _MTL_PRIVATE_DEF_SEL(beginScope,
            "beginScope");
        _MTL_PRIVATE_DEF_SEL(endScope,
            "endScope");
    } // Class
} // Private
} // MTL

//-------------------------------------------------------------------------------------------------------------------------------------------------------------
