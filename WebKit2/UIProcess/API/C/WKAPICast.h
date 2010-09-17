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

#ifndef WKAPICast_h
#define WKAPICast_h

#include "WKBase.h"
#include "WKPage.h"
#include "WebEvent.h"
#include "WebNumber.h"
#include "WebString.h"
#include "WebURL.h"
#include <WebCore/FrameLoaderTypes.h>
#include <wtf/TypeTraits.h>

#if defined(WIN32) || defined(_WIN32)
#include "WKAPICastWin.h"
#endif

namespace WebKit {

class ImmutableArray;
class ImmutableDictionary;
class MutableArray;
class MutableDictionary;
class WebBackForwardList;
class WebBackForwardListItem;
class WebContext;
class WebData;
class WebError;
class WebFormSubmissionListenerProxy;
class WebFramePolicyListenerProxy;
class WebFrameProxy;
class WebNavigationData;
class WebPageNamespace;
class WebPageProxy;
class WebPreferences;
class WebSerializedScriptValue;
class WebURLRequest;
class WebURLResponse;

template<typename APIType> struct APITypeInfo { };
template<> struct APITypeInfo<WKArrayRef>                       { typedef ImmutableArray* ImplType; };
template<> struct APITypeInfo<WKBackForwardListItemRef>         { typedef WebBackForwardListItem* ImplType; };
template<> struct APITypeInfo<WKBackForwardListRef>             { typedef WebBackForwardList* ImplType; };
template<> struct APITypeInfo<WKBooleanRef>                     { typedef WebBoolean* ImplType; };
template<> struct APITypeInfo<WKContextRef>                     { typedef WebContext* ImplType; };
template<> struct APITypeInfo<WKDataRef>                        { typedef WebData* ImplType; };
template<> struct APITypeInfo<WKDictionaryRef>                  { typedef ImmutableDictionary* ImplType; };
template<> struct APITypeInfo<WKDoubleRef>                      { typedef WebDouble* ImplType; };
template<> struct APITypeInfo<WKFormSubmissionListenerRef>      { typedef WebFormSubmissionListenerProxy* ImplType; };
template<> struct APITypeInfo<WKFramePolicyListenerRef>         { typedef WebFramePolicyListenerProxy* ImplType; };
template<> struct APITypeInfo<WKFrameRef>                       { typedef WebFrameProxy* ImplType; };
template<> struct APITypeInfo<WKMutableArrayRef>                { typedef MutableArray* ImplType; };
template<> struct APITypeInfo<WKMutableDictionaryRef>           { typedef MutableDictionary* ImplType; };
template<> struct APITypeInfo<WKNavigationDataRef>              { typedef WebNavigationData* ImplType; };
template<> struct APITypeInfo<WKPageNamespaceRef>               { typedef WebPageNamespace* ImplType; };
template<> struct APITypeInfo<WKPageRef>                        { typedef WebPageProxy* ImplType; };
template<> struct APITypeInfo<WKPreferencesRef>                 { typedef WebPreferences* ImplType; };
template<> struct APITypeInfo<WKSerializedScriptValueRef>       { typedef WebSerializedScriptValue* ImplType; };
template<> struct APITypeInfo<WKStringRef>                      { typedef WebString* ImplType; };
template<> struct APITypeInfo<WKTypeRef>                        { typedef APIObject* ImplType; };
template<> struct APITypeInfo<WKUInt64Ref>                      { typedef WebUInt64* ImplType; };
template<> struct APITypeInfo<WKURLRef>                         { typedef WebURL* ImplType; };
template<> struct APITypeInfo<WKURLRequestRef>                  { typedef WebURLRequest* ImplType; };
template<> struct APITypeInfo<WKURLResponseRef>                 { typedef WebURLResponse* ImplType; };


template<typename ImplType> struct ImplTypeInfo { };
template<> struct ImplTypeInfo<APIObject*>                      { typedef WKTypeRef APIType; };
template<> struct ImplTypeInfo<ImmutableArray*>                 { typedef WKArrayRef APIType; };
template<> struct ImplTypeInfo<ImmutableDictionary*>            { typedef WKDictionaryRef APIType; };
template<> struct ImplTypeInfo<MutableArray*>                   { typedef WKMutableArrayRef APIType; };
template<> struct ImplTypeInfo<MutableDictionary*>              { typedef WKMutableDictionaryRef APIType; };
template<> struct ImplTypeInfo<WebBackForwardList*>             { typedef WKBackForwardListRef APIType; };
template<> struct ImplTypeInfo<WebBackForwardListItem*>         { typedef WKBackForwardListItemRef APIType; };
template<> struct ImplTypeInfo<WebBoolean*>                     { typedef WKBooleanRef APIType; };
template<> struct ImplTypeInfo<WebContext*>                     { typedef WKContextRef APIType; };
template<> struct ImplTypeInfo<WebData*>                        { typedef WKDataRef APIType; };
template<> struct ImplTypeInfo<WebDouble*>                      { typedef WKDoubleRef APIType; };
template<> struct ImplTypeInfo<WebError*>                       { typedef WKErrorRef APIType; };
template<> struct ImplTypeInfo<WebFormSubmissionListenerProxy*> { typedef WKFormSubmissionListenerRef APIType; };
template<> struct ImplTypeInfo<WebFramePolicyListenerProxy*>    { typedef WKFramePolicyListenerRef APIType; };
template<> struct ImplTypeInfo<WebFrameProxy*>                  { typedef WKFrameRef APIType; };
template<> struct ImplTypeInfo<WebNavigationData*>              { typedef WKNavigationDataRef APIType; };
template<> struct ImplTypeInfo<WebPageNamespace*>               { typedef WKPageNamespaceRef APIType; };
template<> struct ImplTypeInfo<WebPageProxy*>                   { typedef WKPageRef APIType; };
template<> struct ImplTypeInfo<WebPreferences*>                 { typedef WKPreferencesRef APIType; };
template<> struct ImplTypeInfo<WebSerializedScriptValue*>       { typedef WKSerializedScriptValueRef APIType; };
template<> struct ImplTypeInfo<WebString*>                      { typedef WKStringRef APIType; };
template<> struct ImplTypeInfo<WebUInt64*>                      { typedef WKUInt64Ref APIType; };
template<> struct ImplTypeInfo<WebURL*>                         { typedef WKURLRef APIType; };
template<> struct ImplTypeInfo<WebURLRequest*>                  { typedef WKURLRequestRef APIType; };
template<> struct ImplTypeInfo<WebURLResponse*>                 { typedef WKURLResponseRef APIType; };


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

inline ProxyingRefPtr<WebString> toRef(WTF::StringImpl* string)
{
    WTF::StringImpl* impl = string ? string : WTF::StringImpl::empty();
    return ProxyingRefPtr<WebString>(WebString::create(WTF::String(impl)));
}

inline ProxyingRefPtr<WebURL> toURLRef(WTF::StringImpl* string)
{
    WTF::StringImpl* impl = string ? string : WTF::StringImpl::empty();
    return ProxyingRefPtr<WebURL>(WebURL::create(WTF::String(impl)));
}

inline WKStringRef toCopiedRef(const WTF::String& string)
{
    WTF::StringImpl* impl = string.impl() ? string.impl() : WTF::StringImpl::empty();
    RefPtr<WebString> webString = WebString::create(WTF::String(impl));
    return toRef(webString.release().releaseRef());
}

inline WKURLRef toCopiedURLRef(const WTF::String& string)
{
    WTF::StringImpl* impl = string.impl() ? string.impl() : WTF::StringImpl::empty();
    RefPtr<WebURL> webURL = WebURL::create(WTF::String(impl));
    return toRef(webURL.release().releaseRef());
}

inline WTF::String toWTFString(WKStringRef stringRef)
{
    if (!stringRef)
        return WTF::String();
    return toWK(stringRef)->string();
}

inline WTF::String toWTFString(WKURLRef urlRef)
{
    if (!urlRef)
        return WTF::String();
    return toWK(urlRef)->string();
}

/* Enum conversions */

inline WKTypeID toRef(APIObject::Type type)
{
    return static_cast<WKTypeID>(type);
}

inline WKFrameNavigationType toRef(WebCore::NavigationType type)
{
    WKFrameNavigationType wkType = kWKFrameNavigationTypeOther;

    switch (type) {
    case WebCore::NavigationTypeLinkClicked:
        wkType = kWKFrameNavigationTypeLinkClicked;
        break;
    case WebCore::NavigationTypeFormSubmitted:
        wkType = kWKFrameNavigationTypeFormSubmitted;
        break;
    case WebCore::NavigationTypeBackForward:
        wkType = kWKFrameNavigationTypeBackForward;
        break;
    case WebCore::NavigationTypeReload:
        wkType = kWKFrameNavigationTypeReload;
        break;
    case WebCore::NavigationTypeFormResubmitted:
        wkType = kWKFrameNavigationTypeFormResubmitted;
        break;
    case WebCore::NavigationTypeOther:
        wkType = kWKFrameNavigationTypeOther;
        break;
    }
    
    return wkType;
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

} // namespace WebKit

#endif // WKAPICast_h
