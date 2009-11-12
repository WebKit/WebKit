/*
* Copyright (C) 2009 Google Inc. All rights reserved.
* 
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
* 
*     * Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above
* copyright notice, this list of conditions and the following disclaimer
* in the documentation and/or other materials provided with the
* distribution.
*     * Neither the name of Google Inc. nor the names of its
* contributors may be used to endorse or promote products derived from
* this software without specific prior written permission.
* 
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
* A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
* OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef V8Binding_h
#define V8Binding_h

#include "AtomicString.h"
#include "MathExtras.h"
#include "PlatformString.h"
#include "V8DOMWrapper.h"
#include "V8Index.h"

#include <v8.h>

namespace WebCore {
    
    class EventListener;
    class EventTarget;
    
    // A helper function extract native object pointer from a DOM wrapper
    // and cast to the specified type.
    void* v8DOMWrapperToNative(v8::Handle<v8::Object>);
    
    template <class C>
    C* v8DOMWrapperTo(v8::Handle<v8::Object> object)
    {
        ASSERT(V8DOMWrapper::maybeDOMWrapper(object));
        return reinterpret_cast<C*>(v8DOMWrapperToNative(object));
    }
    template <class C>
    C* v8DOMWrapperToNode(v8::Handle<v8::Object> object)
    {
        ASSERT(V8DOMWrapper::maybeDOMWrapper(object));
        ASSERT(V8DOMWrapper::domWrapperType(object) == V8ClassIndex::NODE);
        return reinterpret_cast<C*>(v8DOMWrapperToNative(object));
    }
    
    void* v8DOMWrapperToNative(const v8::AccessorInfo&);
    
    template <class C>
    C* v8DOMWrapperTo(const v8::AccessorInfo& info) {
        ASSERT(V8DOMWrapper::maybeDOMWrapper(info.Holder()));
        return reinterpret_cast<C*>(v8DOMWrapperToNative(info));
    }
    template <class C>
    C* v8DOMWrapperToNode(const v8::AccessorInfo& info) {
        ASSERT(V8DOMWrapper::domWrapperType(info.Holder()) == V8ClassIndex::NODE);
        return reinterpret_cast<C*>(v8DOMWrapperToNative(info));
    }
    
    template <class C>
    C* v8DOMWrapperTo(V8ClassIndex::V8WrapperType type, v8::Handle<v8::Object> object)
    {
        // Native event listener is per frame, it cannot be handled by this generic function.
        ASSERT(type != V8ClassIndex::EVENTLISTENER);
        ASSERT(type != V8ClassIndex::EVENTTARGET);
        
        ASSERT(V8DOMWrapper::maybeDOMWrapper(object));
        
#ifndef NDEBUG
        const bool typeIsValid =
#define MAKE_CASE(TYPE, NAME) (type != V8ClassIndex::TYPE) &&
        DOM_NODE_TYPES(MAKE_CASE)
#if ENABLE(SVG)
        SVG_NODE_TYPES(MAKE_CASE)
#endif
#undef MAKE_CASE
        true;
        ASSERT(typeIsValid);
#endif
        
        return v8DOMWrapperTo<C>(object);
    }
    
    template <class C>
    C* v8DOMWrapperTo(V8ClassIndex::V8WrapperType type, const v8::AccessorInfo& info)
    {
#ifndef NDEBUG
        return v8DOMWrapperTo<C>(type, info.Holder());
#else
        return reinterpret_cast<C*>(v8DOMWrapperToNative(info));
#endif
    }

    
    enum ExternalMode {
        Externalize,
        DoNotExternalize
    };
    
    template <typename StringType>
    StringType v8StringToWebCoreString(v8::Handle<v8::String> v8String, ExternalMode external);

    // Convert v8 types to a WebCore::String. If the V8 string is not already
    // an external string then it is transformed into an external string at this
    // point to avoid repeated conversions.
    inline String v8StringToWebCoreString(v8::Handle<v8::String> v8String)
    {
        return v8StringToWebCoreString<String>(v8String, Externalize);
    }
    String v8NonStringValueToWebCoreString(v8::Handle<v8::Value>);
    String v8ValueToWebCoreString(v8::Handle<v8::Value> value);

    // Convert v8 types to a WebCore::AtomicString.
    inline AtomicString v8StringToAtomicWebCoreString(v8::Handle<v8::String> v8String)
    {
        return v8StringToWebCoreString<AtomicString>(v8String, Externalize);
    }
    AtomicString v8NonStringValueToAtomicWebCoreString(v8::Handle<v8::Value>);
    AtomicString v8ValueToAtomicWebCoreString(v8::Handle<v8::Value> value);

    // Return a V8 external string that shares the underlying buffer with the given
    // WebCore string. The reference counting mechanism is used to keep the
    // underlying buffer alive while the string is still live in the V8 engine.
    v8::Local<v8::String> v8ExternalString(const String&);

    // Convert a string to a V8 string.
    inline v8::Handle<v8::String> v8String(const String& string)
    {
        return v8ExternalString(string);
    }

    // Enables caching v8 wrappers created for WebCore::StringImpl.  Currently this cache requires
    // all the calls (both to convert WebCore::String to v8::String and to GC the handle)
    // to be performed on the main thread.
    void enableStringImplCache();

    // Convert a value to a 32-bit integer.  The conversion fails if the
    // value cannot be converted to an integer or converts to nan or to an infinity.
    int toInt32(v8::Handle<v8::Value> value, bool& ok);

    // Convert a value to a 32-bit integer assuming the conversion cannot fail.
    inline int toInt32(v8::Handle<v8::Value> value)
    {
        bool ok;
        return toInt32(value, ok);
    }

    inline float toFloat(v8::Local<v8::Value> value)
    {
        return static_cast<float>(value->NumberValue());
    }

    // FIXME: Drop this in favor of the type specific v8ValueToWebCoreString when we rework the code generation.
    inline String toWebCoreString(v8::Handle<v8::Value> object)
    {
        return v8ValueToWebCoreString(object);
    }
    
    String toWebCoreString(const v8::Arguments&, int index);

    // The string returned by this function is still owned by the argument
    // and will be deallocated when the argument is deallocated.
    inline const uint16_t* fromWebCoreString(const String& str)
    {
        return reinterpret_cast<const uint16_t*>(str.characters());
    }

    bool isUndefinedOrNull(v8::Handle<v8::Value> value);

    v8::Handle<v8::Boolean> v8Boolean(bool value);

    String toWebCoreStringWithNullCheck(v8::Handle<v8::Value> value);

    AtomicString toAtomicWebCoreStringWithNullCheck(v8::Handle<v8::Value> value);

    String toWebCoreStringWithNullOrUndefinedCheck(v8::Handle<v8::Value> value);
 
    v8::Handle<v8::String> v8UndetectableString(const String& str);

    v8::Handle<v8::Value> v8StringOrNull(const String& str);

    v8::Handle<v8::Value> v8StringOrUndefined(const String& str);

    v8::Handle<v8::Value> v8StringOrFalse(const String& str);
    
    v8::Persistent<v8::FunctionTemplate> createRawTemplate();

} // namespace WebCore

#endif // V8Binding_h
