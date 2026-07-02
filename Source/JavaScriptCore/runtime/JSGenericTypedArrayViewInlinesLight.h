/*
 * Copyright (C) 2013-2026 Apple Inc. All rights reserved.
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

#include <JavaScriptCore/JSArrayBufferViewInlinesLight.h>
#include <JavaScriptCore/JSGenericTypedArrayView.h>

namespace JSC {

template<typename Adaptor>
RefPtr<typename Adaptor::ViewType> JSGenericTypedArrayView<Adaptor>::possiblySharedTypedImpl()
{
    return Adaptor::ViewType::tryCreate(possiblySharedBuffer(), byteOffsetRaw(), isAutoLength() ? std::nullopt : std::optional { lengthRaw() });
}

template<typename Adaptor>
RefPtr<typename Adaptor::ViewType> JSGenericTypedArrayView<Adaptor>::unsharedTypedImpl()
{
    return Adaptor::ViewType::tryCreate(unsharedBuffer(), byteOffsetRaw(), isAutoLength() ? std::nullopt : std::optional { lengthRaw() });
}

template<typename Adaptor> inline RefPtr<typename Adaptor::ViewType> toPossiblySharedNativeTypedView(VM&, JSValue value)
{
    auto* wrapper = dynamicDowncast<typename Adaptor::JSViewType>(value);
    if (!wrapper)
        return nullptr;
    return wrapper->possiblySharedTypedImpl();
}

template<typename Adaptor> inline RefPtr<typename Adaptor::ViewType> toUnsharedNativeTypedView(VM& vm, JSValue value)
{
    auto result = toPossiblySharedNativeTypedView<Adaptor>(vm, value);
    if (!result || result->isShared())
        return nullptr;
    return result;
}

template<typename Adaptor> RefPtr<typename Adaptor::ViewType> JSGenericTypedArrayView<Adaptor>::toWrapped(VM& vm, JSValue value)
{
    auto result = JSC::toUnsharedNativeTypedView<Adaptor>(vm, value);
    if (!result || result->isResizableOrGrowableShared())
        return nullptr;
    return result;
}

template<typename Adaptor> RefPtr<typename Adaptor::ViewType> JSGenericTypedArrayView<Adaptor>::toWrappedAllowResizable(VM& vm, JSValue value)
{
    return JSC::toUnsharedNativeTypedView<Adaptor>(vm, value);
}

template<typename Adaptor> RefPtr<typename Adaptor::ViewType> JSGenericTypedArrayView<Adaptor>::toWrappedAllowShared(VM& vm, JSValue value)
{
    auto result = JSC::toPossiblySharedNativeTypedView<Adaptor>(vm, value);
    if (!result || result->isResizableOrGrowableShared())
        return nullptr;
    return result;
}

template<typename Adaptor> RefPtr<typename Adaptor::ViewType> JSGenericTypedArrayView<Adaptor>::toWrappedAllowSharedAndResizable(VM& vm, JSValue value)
{
    return JSC::toPossiblySharedNativeTypedView<Adaptor>(vm, value);
}

template<typename Adaptor> inline const ClassInfo* JSGenericTypedArrayView<Adaptor>::info()
{
#define JSC_GET_CLASS_INFO(type) case Type##type: return get##type##ArrayClassInfo();
    switch (Adaptor::typeValue) {
        FOR_EACH_TYPED_ARRAY_TYPE_EXCLUDING_DATA_VIEW(JSC_GET_CLASS_INFO)
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return nullptr;
    }
#undef JSC_GET_CLASS_INFO
}

template<typename PassedAdaptor> inline const ClassInfo* JSGenericResizableOrGrowableSharedTypedArrayView<PassedAdaptor>::info()
{
    switch (Base::Adaptor::typeValue) {
#define JSC_GET_CLASS_INFO(type) \
    case Type##type: return getResizableOrGrowableShared##type##ArrayClassInfo();
        FOR_EACH_TYPED_ARRAY_TYPE_EXCLUDING_DATA_VIEW(JSC_GET_CLASS_INFO)
#undef JSC_GET_CLASS_INFO
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return nullptr;
    }
}

} // namespace JSC
