/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#ifdef __cplusplus

#include <WebKit/WKType.h>

namespace detail {
template<typename> struct TypeCheckHelper;
}
#define TYPE_CHECKER(type, checker) \
extern "C" WK_EXPORT WKTypeID checker(void); \
namespace detail { \
template<> struct TypeCheckHelper<type> { \
    static WKTypeID typeID() { return checker(); } \
};\
} // namespace detail

TYPE_CHECKER(WKArrayRef, WKArrayGetTypeID);
TYPE_CHECKER(WKBooleanRef, WKBooleanGetTypeID);
TYPE_CHECKER(WKContextMenuItemRef, WKContextMenuItemGetTypeID);
TYPE_CHECKER(WKDataRef, WKDataGetTypeID);
TYPE_CHECKER(WKDictionaryRef, WKDictionaryGetTypeID);
TYPE_CHECKER(WKDoubleRef, WKDoubleGetTypeID);
TYPE_CHECKER(WKJSHandleRef, WKJSHandleGetTypeID);
TYPE_CHECKER(WKStringRef, WKStringGetTypeID);
TYPE_CHECKER(WKUInt64Ref, WKUInt64GetTypeID);
TYPE_CHECKER(WKURLRef, WKURLGetTypeID);

namespace WebKit {

template<typename T, typename U> inline T dynamic_wk_cast(U* object)
{
    if (!object || WKGetTypeID(object) != detail::TypeCheckHelper<T>::typeID())
        return nullptr;
    return reinterpret_cast<T>(object);
}

template<typename> class WKRetainPtr;
template<typename T, typename U> inline WKRetainPtr<T> dynamic_wk_cast(RetainPtr<U> object)
{
    return dynamic_wk_cast<T>(object.get());
}

} // namespace WebKit

using WebKit::dynamic_wk_cast;

#endif
