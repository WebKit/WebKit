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

#include "BindingSecurity.h"
#include "Document.h"
#include "V8BindingMacros.h"
#include "V8DOMConfiguration.h"
#include "V8DOMWindowShell.h"
#include "V8DOMWrapper.h"
#include "V8HiddenPropertyName.h"
#include "V8ObjectConstructor.h"
#include "V8PerIsolateData.h"
#include "V8Proxy.h"
#include "V8ThrowException.h"
#include "V8ValueCache.h"
#include <wtf/Noncopyable.h>
#include <wtf/text/AtomicString.h>
#include <v8.h>

namespace WebCore {

    class DOMStringList;

    // Schedule a DOM exception to be thrown, if the exception code is different
    // from zero.
    v8::Handle<v8::Value> setDOMException(int, v8::Isolate*);

    // Schedule a JavaScript error to be thrown.
    v8::Handle<v8::Value> throwError(ErrorType, const char*, v8::Isolate* = 0);

    // Schedule a JavaScript error to be thrown.
    v8::Handle<v8::Value> throwError(v8::Local<v8::Value>, v8::Isolate* = 0);

    // A helper for throwing JavaScript TypeError.
    v8::Handle<v8::Value> throwTypeError(const char* = 0, v8::Isolate* = 0);

    // A helper for throwing JavaScript TypeError for not enough arguments.
    v8::Handle<v8::Value> throwNotEnoughArgumentsError(v8::Isolate*);

    // Since v8::Null(isolate) crashes if we pass a null isolate,
    // we need to use v8NullWithCheck(isolate) if an isolate can be null.
    //
    // FIXME: Remove all null isolates from V8 bindings, and remove v8NullWithCheck(isolate).
    inline v8::Handle<v8::Value> v8NullWithCheck(v8::Isolate* isolate)
    {
        return isolate ? v8::Null(isolate) : v8::Null();
    }

    enum ExternalMode {
        Externalize,
        DoNotExternalize
    };

    template <typename StringType>
    StringType v8StringToWebCoreString(v8::Handle<v8::String>, ExternalMode);

    // Convert v8 types to a WTF::String. If the V8 string is not already
    // an external string then it is transformed into an external string at this
    // point to avoid repeated conversions.
    String toWebCoreString(v8::Handle<v8::Value>);

    // Convert a V8 value to a WTF::AtomicString.
    AtomicString toWebCoreAtomicString(v8::Handle<v8::Value>);

    // Return a V8 external string that shares the underlying buffer with the given
    // WebCore string. The reference counting mechanism is used to keep the
    // underlying buffer alive while the string is still live in the V8 engine.
    inline v8::Local<v8::String> v8ExternalString(const String& string, v8::Isolate* isolate = 0)
    {
        StringImpl* stringImpl = string.impl();
        if (!stringImpl)
            return isolate ? v8::String::Empty(isolate) : v8::String::Empty();

        V8PerIsolateData* data = V8PerIsolateData::current(isolate);
        return data->stringCache()->v8ExternalString(stringImpl, isolate);
    }

    // Convert a string to a V8 string.
    inline v8::Handle<v8::String> v8String(const String& string, v8::Isolate* isolate = 0)
    {
        return v8ExternalString(string, isolate);
    }

    inline v8::Handle<v8::Integer> v8Integer(int value, v8::Isolate* isolate = 0)
    {
        V8PerIsolateData* data = V8PerIsolateData::current(isolate);
        return data->integerCache()->v8Integer(value);
    }

    inline v8::Handle<v8::Integer> v8UnsignedInteger(unsigned value, v8::Isolate* isolate = 0)
    {
        V8PerIsolateData* data = V8PerIsolateData::current(isolate);
        return data->integerCache()->v8UnsignedInteger(value);
    }

    inline v8::Handle<v8::Value> v8Undefined()
    {
        return v8::Handle<v8::Value>();
    }

    template <class T>
    struct V8ValueTraits {
        static inline v8::Handle<v8::Value> arrayV8Value(const T& value, v8::Isolate* isolate)
        {
            return toV8(WTF::getPtr(value), isolate);
        }
    };

    template<>
    struct V8ValueTraits<String> {
        static inline v8::Handle<v8::Value> arrayV8Value(const String& value, v8::Isolate* isolate)
        {
            return v8String(value, isolate);
        }
    };

    template<>
    struct V8ValueTraits<unsigned long> {
        static inline v8::Handle<v8::Value> arrayV8Value(const unsigned long& value, v8::Isolate* isolate)
        {
            return v8UnsignedInteger(value, isolate);
        }
    };

    template<>
    struct V8ValueTraits<float> {
        static inline v8::Handle<v8::Value> arrayV8Value(const float& value, v8::Isolate*)
        {
            return v8::Number::New(value);
        }
    };

    template<>
    struct V8ValueTraits<double> {
        static inline v8::Handle<v8::Value> arrayV8Value(const double& value, v8::Isolate*)
        {
            return v8::Number::New(value);
        }
    };

    template<typename T>
    v8::Handle<v8::Value> v8Array(const Vector<T>& iterator, v8::Isolate* isolate)
    {
        v8::Local<v8::Array> result = v8::Array::New(iterator.size());
        int index = 0;
        typename Vector<T>::const_iterator end = iterator.end();
        typedef V8ValueTraits<T> TraitsType;
        for (typename Vector<T>::const_iterator iter = iterator.begin(); iter != end; ++iter)
            result->Set(v8Integer(index++, isolate), TraitsType::arrayV8Value(*iter, isolate));
        return result;
    }

    v8::Handle<v8::Value> v8Array(PassRefPtr<DOMStringList>, v8::Isolate*);

    template<class T> struct NativeValueTraits;

    template<>
    struct NativeValueTraits<String> {
        static inline String arrayNativeValue(const v8::Local<v8::Array>& array, size_t i)
        {
            return toWebCoreString(array->Get(i));
        }
    };

    template<>
    struct NativeValueTraits<float> {
        static inline float arrayNativeValue(const v8::Local<v8::Array>& array, size_t i)
        {
            return static_cast<float>(array->Get(v8Integer(i))->NumberValue());
        }
    };

    template<>
    struct NativeValueTraits<double> {
        static inline double arrayNativeValue(const v8::Local<v8::Array>& array, size_t i)
        {
            return static_cast<double>(array->Get(v8Integer(i))->NumberValue());
        }
    };

    template <class T>
    Vector<T> toNativeArray(v8::Handle<v8::Value> value)
    {
        if (!value->IsArray())
            return Vector<T>();

        Vector<T> result;
        typedef NativeValueTraits<T> TraitsType;
        v8::Local<v8::Value> v8Value(v8::Local<v8::Value>::New(value));
        v8::Local<v8::Array> array = v8::Local<v8::Array>::Cast(v8Value);
        size_t length = array->Length();
        for (size_t i = 0; i < length; ++i) {
            result.append(TraitsType::arrayNativeValue(array, i));
        }
        return result;
    }

    // Validates that the passed object is a sequence type per WebIDL spec
    // http://www.w3.org/TR/2012/WD-WebIDL-20120207/#es-sequence
    inline v8::Handle<v8::Value> toV8Sequence(v8::Handle<v8::Value> value, uint32_t& length)
    {
        if (!value->IsObject()) {
            throwTypeError();
            return v8Undefined();
        }

        v8::Local<v8::Value> v8Value(v8::Local<v8::Value>::New(value));
        v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(v8Value);

        EXCEPTION_BLOCK(v8::Local<v8::Value>, lengthValue, object->Get(v8::String::New("length")));

        if (lengthValue->IsUndefined() || lengthValue->IsNull()) {
            throwTypeError();
            return v8Undefined();
        }

        EXCEPTION_BLOCK(uint32_t, sequenceLength, lengthValue->Int32Value());
        length = sequenceLength;

        return v8Value;
    }

    // Convert a value to a 32-bit integer.  The conversion fails if the
    // value cannot be converted to an integer or converts to nan or to an infinity.
    int toInt32(v8::Handle<v8::Value> value, bool& ok);

    // Convert a value to a 32-bit integer assuming the conversion cannot fail.
    inline int toInt32(v8::Handle<v8::Value> value)
    {
        bool ok;
        return toInt32(value, ok);
    }

    // Convert a value to a 32-bit unsigned integer.  The conversion fails if the
    // value cannot be converted to an unsigned integer or converts to nan or to an infinity.
    uint32_t toUInt32(v8::Handle<v8::Value> value, bool& ok);

    // Convert a value to a 32-bit unsigned integer assuming the conversion cannot fail.
    inline uint32_t toUInt32(v8::Handle<v8::Value> value)
    {
        bool ok;
        return toUInt32(value, ok);
    }

    inline float toFloat(v8::Local<v8::Value> value)
    {
        return static_cast<float>(value->NumberValue());
    }

    inline long long toInt64(v8::Local<v8::Value> value)
    {
        return static_cast<long long>(value->IntegerValue());
    }

    // The string returned by this function is still owned by the argument
    // and will be deallocated when the argument is deallocated.
    inline const uint16_t* fromWebCoreString(const String& str)
    {
        return reinterpret_cast<const uint16_t*>(str.characters());
    }

    inline bool isUndefinedOrNull(v8::Handle<v8::Value> value)
    {
        return value->IsNull() || value->IsUndefined();
    }

    // Returns true if the provided object is to be considered a 'host object', as used in the
    // HTML5 structured clone algorithm.
    inline bool isHostObject(v8::Handle<v8::Object> object)
    {
        // If the object has any internal fields, then we won't be able to serialize or deserialize
        // them; conveniently, this is also a quick way to detect DOM wrapper objects, because
        // the mechanism for these relies on data stored in these fields. We should
        // catch external array data as a special case.
        return object->InternalFieldCount() || object->HasIndexedPropertiesInExternalArrayData();
    }

    inline v8::Handle<v8::Boolean> v8Boolean(bool value)
    {
        return value ? v8::True() : v8::False();
    }

    inline v8::Handle<v8::Boolean> v8Boolean(bool value, v8::Isolate* isolate)
    {
        return value ? v8::True(isolate) : v8::False(isolate);
    }

    // Since v8Boolean(value, isolate) crashes if we pass a null isolate,
    // we need to use v8BooleanWithCheck(value, isolate) if an isolate can be null.
    //
    // FIXME: Remove all null isolates from V8 bindings, and remove v8BooleanWithCheck(value, isolate).
    inline v8::Handle<v8::Boolean> v8BooleanWithCheck(bool value, v8::Isolate* isolate)
    {
        return isolate ? v8Boolean(value, isolate) : v8Boolean(value);
    }

    inline String toWebCoreStringWithNullCheck(v8::Handle<v8::Value> value)
    {
        return value->IsNull() ? String() : toWebCoreString(value);
    }

    inline String toWebCoreStringWithNullOrUndefinedCheck(v8::Handle<v8::Value> value)
    {
        return (value->IsNull() || value->IsUndefined()) ? String() : toWebCoreString(value);
    }

    inline AtomicString toWebCoreAtomicStringWithNullCheck(v8::Handle<v8::Value> value)
    {
        return value->IsNull() ? AtomicString() : toWebCoreAtomicString(value);
    }

    inline v8::Handle<v8::Value> v8StringOrNull(const String& str, v8::Isolate* isolate = 0)
    {
        return str.isNull() ? v8::Handle<v8::Value>(v8::Null()) : v8::Handle<v8::Value>(v8String(str, isolate));
    }

    inline v8::Handle<v8::Value> v8StringOrUndefined(const String& str, v8::Isolate* isolate = 0)
    {
        return str.isNull() ? v8::Handle<v8::Value>(v8::Undefined()) : v8::Handle<v8::Value>(v8String(str, isolate));
    }

    inline double toWebCoreDate(v8::Handle<v8::Value> object)
    {
        return (object->IsDate() || object->IsNumber()) ? object->NumberValue() : std::numeric_limits<double>::quiet_NaN();
    }

    inline v8::Handle<v8::Value> v8DateOrNull(double value, v8::Isolate* isolate = 0)
    {
        return isfinite(value) ? v8::Date::New(value) : v8NullWithCheck(isolate);
    }

    v8::Persistent<v8::FunctionTemplate> createRawTemplate();

    v8::Persistent<v8::String> getToStringName();
    v8::Persistent<v8::FunctionTemplate> getToStringTemplate();

    String int32ToWebCoreString(int value);

    PassRefPtr<DOMStringList> toDOMStringList(v8::Handle<v8::Value>);

    // Returns the window object associated with a context.
    DOMWindow* toDOMWindow(v8::Handle<v8::Context>);

    // Returns the frame object of the window object associated with
    // a context, if the window is currently being displayed in the Frame.
    Frame* toFrameIfNotDetached(v8::Handle<v8::Context>);

    // Returns the PerContextData associated with a frame for the current isolated world.
    V8PerContextData* perContextDataForCurrentWorld(Frame*);

    // If the current context causes out of memory, JavaScript setting
    // is disabled and it returns true.
    bool handleOutOfMemory();

    void crashIfV8IsDead();

    class V8ParameterBase {
    public:
        operator String() { return toString<String>(); }
        operator AtomicString() { return toString<AtomicString>(); }

    protected:
        V8ParameterBase(v8::Local<v8::Value> object) : m_v8Object(object), m_mode(Externalize), m_string() { }

        bool prepareBase()
        {
            if (m_v8Object.IsEmpty())
                return true;

            if (LIKELY(m_v8Object->IsString()))
                return true;

            if (LIKELY(m_v8Object->IsInt32())) {
                setString(int32ToWebCoreString(m_v8Object->Int32Value()));
                return true;
            }

            m_mode = DoNotExternalize;
            v8::TryCatch block;
            m_v8Object = m_v8Object->ToString();
            // Handle the case where an exception is thrown as part of invoking toString on the object.
            if (block.HasCaught()) {
                block.ReThrow();
                return false;
            }
            return true;
        }

        v8::Local<v8::Value> object() { return m_v8Object; }

        void setString(const String& string)
        {
            m_string = string;
            m_v8Object.Clear(); // To signal that String is ready.
        }

     private:
        v8::Local<v8::Value> m_v8Object;
        ExternalMode m_mode;
        String m_string;

        template <class StringType>
        StringType toString()
        {
            if (LIKELY(!m_v8Object.IsEmpty()))
                return v8StringToWebCoreString<StringType>(m_v8Object.As<v8::String>(), m_mode);

            return StringType(m_string);
        }
    };

    // V8Parameter is an adapter class that converts V8 values to Strings
    // or AtomicStrings as appropriate, using multiple typecast operators.
    enum V8ParameterMode {
        DefaultMode,
        WithNullCheck,
        WithUndefinedOrNullCheck
    };
    template <V8ParameterMode MODE = DefaultMode>
    class V8Parameter: public V8ParameterBase {
    public:
        V8Parameter(v8::Local<v8::Value> object) : V8ParameterBase(object) { }
        V8Parameter(v8::Local<v8::Value> object, bool) : V8ParameterBase(object) { prepare(); }

        bool prepare();
    };

    template<> inline bool V8Parameter<DefaultMode>::prepare()
    {
        return V8ParameterBase::prepareBase();
    }

    template<> inline bool V8Parameter<WithNullCheck>::prepare()
    {
        if (object().IsEmpty() || object()->IsNull()) {
            setString(String());
            return true;
        }

        return V8ParameterBase::prepareBase();
    }

    template<> inline bool V8Parameter<WithUndefinedOrNullCheck>::prepare()
    {
        if (object().IsEmpty() || object()->IsNull() || object()->IsUndefined()) {
            setString(String());
            return true;
        }

        return V8ParameterBase::prepareBase();
    }

} // namespace WebCore

#endif // V8Binding_h
