/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef WKSharedAPICast_h
#define WKSharedAPICast_h

#include "WKBase.h"
#include "WKEvent.h"
#include "WebEvent.h"
#include "WebNumber.h"
#include "WebString.h"
#include "WebURL.h"
#include <wtf/TypeTraits.h>

namespace WebKit {

class ImmutableArray;
class ImmutableDictionary;
class MutableArray;
class MutableDictionary;
class WebCertificateInfo;
class WebData;
class WebError;
class WebSerializedScriptValue;
class WebURLRequest;
class WebURLResponse;

template<typename APIType> struct APITypeInfo { };
template<typename ImplType> struct ImplTypeInfo { };

#define WK_ADD_API_MAPPING(TheAPIType, TheImplType) \
    template<> struct APITypeInfo<TheAPIType> { typedef TheImplType* ImplType; }; \
    template<> struct ImplTypeInfo<TheImplType*> { typedef TheAPIType APIType; };

WK_ADD_API_MAPPING(WKArrayRef, ImmutableArray)
WK_ADD_API_MAPPING(WKBooleanRef, WebBoolean)
WK_ADD_API_MAPPING(WKCertificateInfoRef, WebCertificateInfo)
WK_ADD_API_MAPPING(WKDataRef, WebData)
WK_ADD_API_MAPPING(WKDictionaryRef, ImmutableDictionary)
WK_ADD_API_MAPPING(WKDoubleRef, WebDouble)
WK_ADD_API_MAPPING(WKErrorRef, WebError)
WK_ADD_API_MAPPING(WKMutableArrayRef, MutableArray)
WK_ADD_API_MAPPING(WKMutableDictionaryRef, MutableDictionary)
WK_ADD_API_MAPPING(WKSerializedScriptValueRef, WebSerializedScriptValue)
WK_ADD_API_MAPPING(WKStringRef, WebString)
WK_ADD_API_MAPPING(WKTypeRef, APIObject)
WK_ADD_API_MAPPING(WKUInt64Ref, WebUInt64)
WK_ADD_API_MAPPING(WKURLRef, WebURL)
WK_ADD_API_MAPPING(WKURLRequestRef, WebURLRequest)
WK_ADD_API_MAPPING(WKURLResponseRef, WebURLResponse)

template<typename ImplType, typename APIType = typename ImplTypeInfo<ImplType*>::APIType>
class ProxyingRefPtr {
public:
    ProxyingRefPtr(PassRefPtr<ImplType> impl)
        : m_impl(impl)
    {
    }

    operator APIType() { return toRef(m_impl.get()); }

private:
    RefPtr<ImplType> m_impl;
};

/* Opaque typing convenience methods */

template<typename T>
inline typename APITypeInfo<T>::ImplType toWK(T t)
{
    // An example of the conversions that take place:
    // const struct OpaqueWKArray* -> const struct OpaqueWKArray -> struct OpaqueWKArray -> struct OpaqueWKArray* -> ImmutableArray*
    
    typedef typename WTF::RemovePointer<T>::Type PotentiallyConstValueType;
    typedef typename WTF::RemoveConst<PotentiallyConstValueType>::Type NonConstValueType;

    return reinterpret_cast<typename APITypeInfo<T>::ImplType>(const_cast<NonConstValueType*>(t));
}

template<typename T>
inline typename ImplTypeInfo<T>::APIType toRef(T t)
{
    return reinterpret_cast<typename ImplTypeInfo<T>::APIType>(t);
}

/* Special cases. */

inline ProxyingRefPtr<WebString> toRef(StringImpl* string)
{
    StringImpl* impl = string ? string : StringImpl::empty();
    return ProxyingRefPtr<WebString>(WebString::create(String(impl)));
}

inline ProxyingRefPtr<WebURL> toURLRef(StringImpl* string)
{
    StringImpl* impl = string ? string : StringImpl::empty();
    return ProxyingRefPtr<WebURL>(WebURL::create(String(impl)));
}

inline WKStringRef toCopiedRef(const String& string)
{
    StringImpl* impl = string.impl() ? string.impl() : StringImpl::empty();
    RefPtr<WebString> webString = WebString::create(String(impl));
    return toRef(webString.release().releaseRef());
}

inline WKURLRef toCopiedURLRef(const String& string)
{
    StringImpl* impl = string.impl() ? string.impl() : StringImpl::empty();
    RefPtr<WebURL> webURL = WebURL::create(String(impl));
    return toRef(webURL.release().releaseRef());
}

inline String toWTFString(WKStringRef stringRef)
{
    if (!stringRef)
        return String();
    return toWK(stringRef)->string();
}

inline String toWTFString(WKURLRef urlRef)
{
    if (!urlRef)
        return String();
    return toWK(urlRef)->string();
}

/* Enum conversions */

inline WKTypeID toRef(APIObject::Type type)
{
    return static_cast<WKTypeID>(type);
}

inline WKEventModifiers toRef(WebEvent::Modifiers modifiers)
{
    WKEventModifiers wkModifiers = 0;
    if (modifiers & WebEvent::ShiftKey)
        wkModifiers |= kWKEventModifiersShiftKey;
    if (modifiers & WebEvent::ControlKey)
        wkModifiers |= kWKEventModifiersControlKey;
    if (modifiers & WebEvent::AltKey)
        wkModifiers |= kWKEventModifiersAltKey;
    if (modifiers & WebEvent::MetaKey)
        wkModifiers |= kWKEventModifiersMetaKey;
    return wkModifiers;
}

inline WKEventMouseButton toRef(WebMouseEvent::Button mouseButton)
{
    WKEventMouseButton wkMouseButton = kWKEventMouseButtonNoButton;

    switch (mouseButton) {
    case WebMouseEvent::NoButton:
        wkMouseButton = kWKEventMouseButtonNoButton;
        break;
    case WebMouseEvent::LeftButton:
        wkMouseButton = kWKEventMouseButtonLeftButton;
        break;
    case WebMouseEvent::MiddleButton:
        wkMouseButton = kWKEventMouseButtonMiddleButton;
        break;
    case WebMouseEvent::RightButton:
        wkMouseButton = kWKEventMouseButtonRightButton;
        break;
    }

    return wkMouseButton;
}

} // namespace WebKit

#endif // WKSharedAPICast_h
