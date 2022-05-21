//-------------------------------------------------------------------------------------------------------------------------------------------------------------
//
// QuartzCore/CAPrivate.hpp
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

#include "CADefines.hpp"

#include <objc/runtime.h>

//-------------------------------------------------------------------------------------------------------------------------------------------------------------

#define _CA_PRIVATE_CLS(symbol) (Private::Class::s_k##symbol)
#define _CA_PRIVATE_SEL(accessor) (Private::Selector::s_k##accessor)

//-------------------------------------------------------------------------------------------------------------------------------------------------------------

#if defined(CA_PRIVATE_IMPLEMENTATION)

#define _CA_PRIVATE_VISIBILITY __attribute__((visibility("hidden")))
#define _CA_PRIVATE_IMPORT __attribute__((weak_import))

#if __OBJC__
#define _CA_PRIVATE_OBJC_LOOKUP_CLASS(symbol) ((__bridge void*)objc_lookUpClass(#symbol))
#else
#define _CA_PRIVATE_OBJC_LOOKUP_CLASS(symbol) objc_lookUpClass(#symbol)
#endif // __OBJC__

#define _CA_PRIVATE_DEF_CLS(symbol) void* s_k##symbol _CA_PRIVATE_VISIBILITY = _CA_PRIVATE_OBJC_LOOKUP_CLASS(symbol);
#define _CA_PRIVATE_DEF_PRO(symbol)
#define _CA_PRIVATE_DEF_SEL(accessor, symbol) SEL s_k##accessor _CA_PRIVATE_VISIBILITY = sel_registerName(symbol);
#define _CA_PRIVATE_DEF_STR(type, symbol)                \
    _CA_EXTERN type const CA##symbol _CA_PRIVATE_IMPORT; \
    type const                       CA::symbol = (nullptr != &CA##symbol) ? CA##symbol : nullptr;

#else

#define _CA_PRIVATE_DEF_CLS(symbol) extern void* s_k##symbol;
#define _CA_PRIVATE_DEF_PRO(symbol)
#define _CA_PRIVATE_DEF_SEL(accessor, symbol) extern SEL s_k##accessor;
#define _CA_PRIVATE_DEF_STR(type, symbol)

#endif // CA_PRIVATE_IMPLEMENTATION

//-------------------------------------------------------------------------------------------------------------------------------------------------------------

namespace CA
{
namespace Private
{
    namespace Class
    {

    } // Class
} // Private
} // CA

//-------------------------------------------------------------------------------------------------------------------------------------------------------------

namespace CA
{
namespace Private
{
    namespace Protocol
    {

        _CA_PRIVATE_DEF_PRO(CAMetalDrawable);

    } // Protocol
} // Private
} // CA

//-------------------------------------------------------------------------------------------------------------------------------------------------------------

namespace CA
{
namespace Private
{
    namespace Selector
    {

        _CA_PRIVATE_DEF_SEL(layer,
            "layer");
        _CA_PRIVATE_DEF_SEL(texture,
            "texture");

    } // Class
} // Private
} // CA

//-------------------------------------------------------------------------------------------------------------------------------------------------------------
