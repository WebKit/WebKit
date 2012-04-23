/*
 * Copyright (C) 2006, 2007, 2008, 2009 Google Inc. All rights reserved.
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

#include "config.h"
#include "V8Binding.h"

#include "DOMStringList.h"
#include "DOMWrapperVisitor.h"
#include "Element.h"
#include "PlatformString.h"
#include "QualifiedName.h"
#include "V8DOMStringList.h"
#include "V8Element.h"
#include "V8Proxy.h"

#include <wtf/MathExtras.h>
#include <wtf/MainThread.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Threading.h>
#include <wtf/text/AtomicString.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuffer.h>
#include <wtf/text/StringHash.h>

namespace WebCore {


V8BindingPerIsolateData::V8BindingPerIsolateData(v8::Isolate* isolate)
    : m_domDataStore(0)
    , m_constructorMode(ConstructorMode::CreateNewObject)
    , m_recursionLevel(0)
#ifndef NDEBUG
    , m_internalScriptRecursionLevel(0)
#endif
{
}

V8BindingPerIsolateData::~V8BindingPerIsolateData()
{
}

V8BindingPerIsolateData* V8BindingPerIsolateData::create(v8::Isolate* isolate)
{
    ASSERT(isolate);
    ASSERT(!isolate->GetData());
    V8BindingPerIsolateData* data = new V8BindingPerIsolateData(isolate);
    isolate->SetData(data);
    return data;
}

void V8BindingPerIsolateData::ensureInitialized(v8::Isolate* isolate) 
{
    ASSERT(isolate);
    if (!isolate->GetData()) 
        create(isolate);
}

void V8BindingPerIsolateData::dispose(v8::Isolate* isolate)
{
    void* data = isolate->GetData();
    delete static_cast<V8BindingPerIsolateData*>(data);
    isolate->SetData(0);
}



// WebCoreStringResource is a helper class for v8ExternalString. It is used
// to manage the life-cycle of the underlying buffer of the external string.
class WebCoreStringResource : public v8::String::ExternalStringResource {
public:
    explicit WebCoreStringResource(const String& string)
        : m_plainString(string)
    {
#ifndef NDEBUG
        m_threadId = WTF::currentThread();
#endif
        ASSERT(!string.isNull());
        v8::V8::AdjustAmountOfExternalAllocatedMemory(2 * string.length());
    }

    explicit WebCoreStringResource(const AtomicString& string)
        : m_plainString(string.string())
        , m_atomicString(string)
    {
#ifndef NDEBUG
        m_threadId = WTF::currentThread();
#endif
        ASSERT(!string.isNull());
        v8::V8::AdjustAmountOfExternalAllocatedMemory(2 * string.length());
    }

    virtual ~WebCoreStringResource()
    {
#ifndef NDEBUG
        ASSERT(m_threadId == WTF::currentThread());
#endif
        int reducedExternalMemory = -2 * m_plainString.length();
        if (m_plainString.impl() != m_atomicString.impl() && !m_atomicString.isNull())
            reducedExternalMemory *= 2;
        v8::V8::AdjustAmountOfExternalAllocatedMemory(reducedExternalMemory);
    }

    virtual const uint16_t* data() const
    {
        return reinterpret_cast<const uint16_t*>(m_plainString.impl()->characters());
    }

    virtual size_t length() const { return m_plainString.impl()->length(); }

    String webcoreString() { return m_plainString; }

    AtomicString atomicString()
    {
#ifndef NDEBUG
        ASSERT(m_threadId == WTF::currentThread());
#endif
        if (m_atomicString.isNull()) {
            m_atomicString = AtomicString(m_plainString);
            ASSERT(!m_atomicString.isNull());
            if (m_plainString.impl() != m_atomicString.impl())
                v8::V8::AdjustAmountOfExternalAllocatedMemory(2 * m_atomicString.length());
        }
        return m_atomicString;
    }

    void visitStrings(DOMWrapperVisitor* visitor)
    {
        visitor->visitJSExternalString(m_plainString.impl());
        if (m_plainString.impl() != m_atomicString.impl() && !m_atomicString.isNull())
            visitor->visitJSExternalString(m_atomicString.impl());
    }

    static WebCoreStringResource* toStringResource(v8::Handle<v8::String> v8String)
    {
        return static_cast<WebCoreStringResource*>(v8String->GetExternalStringResource());
    }

private:
    // A shallow copy of the string. Keeps the string buffer alive until the V8 engine garbage collects it.
    String m_plainString;
    // If this string is atomic or has been made atomic earlier the
    // atomic string is held here. In the case where the string starts
    // off non-atomic and becomes atomic later it is necessary to keep
    // the original string alive because v8 may keep derived pointers
    // into that string.
    AtomicString m_atomicString;

#ifndef NDEBUG
    WTF::ThreadIdentifier m_threadId;
#endif
};

#if ENABLE(INSPECTOR)
void V8BindingPerIsolateData::visitJSExternalStrings(DOMWrapperVisitor* visitor)
{
    v8::HandleScope handleScope;
    class VisitorImpl : public v8::ExternalResourceVisitor {
    public:
        VisitorImpl(DOMWrapperVisitor* visitor) : m_visitor(visitor) { }
        virtual ~VisitorImpl() { }
        virtual void VisitExternalString(v8::Handle<v8::String> string)
        {
            WebCoreStringResource* resource = static_cast<WebCoreStringResource*>(string->GetExternalStringResource());
            if (resource)
                resource->visitStrings(m_visitor);
        }
    private:
        DOMWrapperVisitor* m_visitor;
    } v8Visitor(visitor);
    v8::V8::VisitExternalResources(&v8Visitor);
}
#endif

String v8ValueToWebCoreString(v8::Handle<v8::Value> value)
{
    if (value->IsString())
        return v8StringToWebCoreString(v8::Handle<v8::String>::Cast(value));
    return v8NonStringValueToWebCoreString(value);
}

AtomicString v8ValueToAtomicWebCoreString(v8::Handle<v8::Value> value)
{
    if (value->IsString())
        return v8StringToAtomicWebCoreString(v8::Handle<v8::String>::Cast(value));
    return v8NonStringValueToAtomicWebCoreString(value);
}

int toInt32(v8::Handle<v8::Value> value, bool& ok)
{
    ok = true;
    
    // Fast case. The value is already a 32-bit integer.
    if (value->IsInt32())
        return value->Int32Value();
    
    // Can the value be converted to a number?
    v8::Local<v8::Number> numberObject = value->ToNumber();
    if (numberObject.IsEmpty()) {
        ok = false;
        return 0;
    }
    
    // Does the value convert to nan or to an infinity?
    double numberValue = numberObject->Value();
    if (isnan(numberValue) || isinf(numberValue)) {
        ok = false;
        return 0;
    }
    
    // Can the value be converted to a 32-bit integer?
    v8::Local<v8::Int32> intValue = value->ToInt32();
    if (intValue.IsEmpty()) {
        ok = false;
        return 0;
    }
    
    // Return the result of the int32 conversion.
    return intValue->Value();
}
    
uint32_t toUInt32(v8::Handle<v8::Value> value, bool& ok)
{
    ok = true;

    // Fast case. The value is already a 32-bit unsigned integer.
    if (value->IsUint32())
        return value->Uint32Value();

    if (value->IsInt32()) {
        int32_t result = value->Int32Value();
        if (result >= 0)
            return result;
    }

    // Can the value be converted to a number?
    v8::Local<v8::Number> numberObject = value->ToNumber();
    if (numberObject.IsEmpty()) {
        ok = false;
        return 0;
    }

    // Does the value convert to nan or to an infinity?
    double numberValue = numberObject->Value();
    if (isnan(numberValue) || isinf(numberValue)) {
        ok = false;
        return 0;
    }

    // Can the value be converted to a 32-bit unsigned integer?
    v8::Local<v8::Uint32> uintValue = value->ToUint32();
    if (uintValue.IsEmpty()) {
        ok = false;
        return 0;
    }

    return uintValue->Value();
}

template <class S> struct StringTraits
{
    static S fromStringResource(WebCoreStringResource* resource);

    static S fromV8String(v8::Handle<v8::String> v8String, int length);
};

template<>
struct StringTraits<String>
{
    static String fromStringResource(WebCoreStringResource* resource)
    {
        return resource->webcoreString();
    }

    static String fromV8String(v8::Handle<v8::String> v8String, int length)
    {
        ASSERT(v8String->Length() == length);
        // NOTE: as of now, String(const UChar*, int) performs String::createUninitialized
        // anyway, so no need to optimize like we do for AtomicString below.
        UChar* buffer;
        String result = String::createUninitialized(length, buffer);
        v8String->Write(reinterpret_cast<uint16_t*>(buffer), 0, length);
        return result;
    }
};

template<>
struct StringTraits<AtomicString>
{
    static AtomicString fromStringResource(WebCoreStringResource* resource)
    {
        return resource->atomicString();
    }

    static AtomicString fromV8String(v8::Handle<v8::String> v8String, int length)
    {
        ASSERT(v8String->Length() == length);
        static const int inlineBufferSize = 16;
        if (length <= inlineBufferSize) {
            UChar inlineBuffer[inlineBufferSize];
            v8String->Write(reinterpret_cast<uint16_t*>(inlineBuffer), 0, length);
            return AtomicString(inlineBuffer, length);
        }
        UChar* buffer;
        String tmp = String::createUninitialized(length, buffer);
        v8String->Write(reinterpret_cast<uint16_t*>(buffer), 0, length);
        return AtomicString(tmp);
    }
};

template <typename StringType>
StringType v8StringToWebCoreString(v8::Handle<v8::String> v8String, ExternalMode external)
{
    WebCoreStringResource* stringResource = WebCoreStringResource::toStringResource(v8String);
    if (stringResource)
        return StringTraits<StringType>::fromStringResource(stringResource);

    int length = v8String->Length();
    if (!length) {
        // Avoid trying to morph empty strings, as they do not have enough room to contain the external reference.
        return StringImpl::empty();
    }

    StringType result(StringTraits<StringType>::fromV8String(v8String, length));

    if (external == Externalize && v8String->CanMakeExternal()) {
        stringResource = new WebCoreStringResource(result);
        if (!v8String->MakeExternal(stringResource)) {
            // In case of a failure delete the external resource as it was not used.
            delete stringResource;
        }
    }
    return result;
}
    
// Explicitly instantiate the above template with the expected parameterizations,
// to ensure the compiler generates the code; otherwise link errors can result in GCC 4.4.
template String v8StringToWebCoreString<String>(v8::Handle<v8::String>, ExternalMode);
template AtomicString v8StringToWebCoreString<AtomicString>(v8::Handle<v8::String>, ExternalMode);

// Fast but non thread-safe version.
String int32ToWebCoreStringFast(int value)
{
    // Caching of small strings below is not thread safe: newly constructed AtomicString
    // are not safely published.
    ASSERT(isMainThread());

    // Most numbers used are <= 100. Even if they aren't used there's very little cost in using the space.
    const int kLowNumbers = 100;
    DEFINE_STATIC_LOCAL(Vector<AtomicString>, lowNumbers, (kLowNumbers + 1));
    String webCoreString;
    if (0 <= value && value <= kLowNumbers) {
        webCoreString = lowNumbers[value];
        if (!webCoreString) {
            AtomicString valueString = AtomicString(String::number(value));
            lowNumbers[value] = valueString;
            webCoreString = valueString;
        }
    } else
        webCoreString = String::number(value);
    return webCoreString;
}

String int32ToWebCoreString(int value)
{
    // If we are on the main thread (this should always true for non-workers), call the faster one.
    if (isMainThread())
        return int32ToWebCoreStringFast(value);
    return String::number(value);
}

String v8NonStringValueToWebCoreString(v8::Handle<v8::Value> object)
{
    ASSERT(!object->IsString());
    if (object->IsInt32())
        return int32ToWebCoreString(object->Int32Value());

    v8::TryCatch block;
    v8::Handle<v8::String> v8String = object->ToString();
    // Handle the case where an exception is thrown as part of invoking toString on the object.
    if (block.HasCaught()) {
        throwError(block.Exception());
        return StringImpl::empty();
    }
    // This path is unexpected.  However there is hypothesis that it
    // might be combination of v8 and v8 bindings bugs.  For now
    // just bailout as we'll crash if attempt to convert empty handle into a string.
    if (v8String.IsEmpty()) {
        ASSERT_NOT_REACHED();
        return StringImpl::empty();
    }
    return v8StringToWebCoreString<String>(v8String, DoNotExternalize);
}

AtomicString v8NonStringValueToAtomicWebCoreString(v8::Handle<v8::Value> object)
{
    ASSERT(!object->IsString());
    return AtomicString(v8NonStringValueToWebCoreString(object));
}

static v8::Local<v8::String> makeExternalString(const String& string)
{
    WebCoreStringResource* stringResource = new WebCoreStringResource(string);
    v8::Local<v8::String> newString = v8::String::NewExternal(stringResource);
    if (newString.IsEmpty())
        delete stringResource;

    return newString;
}

static void cachedStringCallback(v8::Persistent<v8::Value> wrapper, void* parameter)
{
    StringImpl* stringImpl = static_cast<StringImpl*>(parameter);
    V8BindingPerIsolateData::current()->stringCache()->remove(stringImpl);
    wrapper.Dispose();
    stringImpl->deref();
}

void StringCache::remove(StringImpl* stringImpl) 
{
    ASSERT(m_stringCache.contains(stringImpl));
    m_stringCache.remove(stringImpl);
}

v8::Local<v8::String> StringCache::v8ExternalStringSlow(StringImpl* stringImpl)
{
    if (!stringImpl->length())
        return v8::String::Empty();

    v8::String* cachedV8String = m_stringCache.get(stringImpl);
    if (cachedV8String) {
        v8::Persistent<v8::String> handle(cachedV8String);
        if (!handle.IsNearDeath() && !handle.IsEmpty()) {
            m_lastStringImpl = stringImpl;
            m_lastV8String = handle;
            return v8::Local<v8::String>::New(handle);
        }
    }

    v8::Local<v8::String> newString = makeExternalString(String(stringImpl));
    if (newString.IsEmpty())
        return newString;

    v8::Persistent<v8::String> wrapper = v8::Persistent<v8::String>::New(newString);
    if (wrapper.IsEmpty())
        return newString;

    stringImpl->ref();
    wrapper.MakeWeak(stringImpl, cachedStringCallback);
    m_stringCache.set(stringImpl, *wrapper);

    m_lastStringImpl = stringImpl;
    m_lastV8String = wrapper;

    return newString;
}
    
v8::Persistent<v8::FunctionTemplate> createRawTemplate()
{
    v8::HandleScope scope;
    v8::Local<v8::FunctionTemplate> result = v8::FunctionTemplate::New(V8Proxy::checkNewLegal);
    return v8::Persistent<v8::FunctionTemplate>::New(result);
}        

v8::Local<v8::Signature> configureTemplate(v8::Persistent<v8::FunctionTemplate> desc,
                                           const char *interfaceName,
                                           v8::Persistent<v8::FunctionTemplate> parentClass,
                                           int fieldCount,
                                           const BatchedAttribute* attributes, 
                                           size_t attributeCount,
                                           const BatchedCallback* callbacks,
                                           size_t callbackCount)
{
    desc->SetClassName(v8::String::New(interfaceName));
    v8::Local<v8::ObjectTemplate> instance = desc->InstanceTemplate();
    instance->SetInternalFieldCount(fieldCount);
    if (!parentClass.IsEmpty())
        desc->Inherit(parentClass);
    if (attributeCount)
        batchConfigureAttributes(instance, desc->PrototypeTemplate(),
                                 attributes, attributeCount);
    v8::Local<v8::Signature> defaultSignature = v8::Signature::New(desc);
    if (callbackCount)
        batchConfigureCallbacks(desc->PrototypeTemplate(),
                                defaultSignature,
                                static_cast<v8::PropertyAttribute>(v8::DontDelete),
                                callbacks, callbackCount);
    return defaultSignature;
}

v8::Persistent<v8::String> getToStringName()
{
    v8::Persistent<v8::String>& toStringName = V8BindingPerIsolateData::current()->toStringName();
    if (toStringName.IsEmpty())
        toStringName = v8::Persistent<v8::String>::New(v8::String::New("toString"));
    return *toStringName;

}

static v8::Handle<v8::Value> constructorToString(const v8::Arguments& args)
{
    // The DOM constructors' toString functions grab the current toString
    // for Functions by taking the toString function of itself and then
    // calling it with the constructor as its receiver. This means that
    // changes to the Function prototype chain or toString function are
    // reflected when printing DOM constructors. The only wart is that
    // changes to a DOM constructor's toString's toString will cause the
    // toString of the DOM constructor itself to change. This is extremely
    // obscure and unlikely to be a problem.
    v8::Handle<v8::Value> value = args.Callee()->Get(getToStringName());
    if (!value->IsFunction()) 
        return v8::String::New("");
    return v8::Handle<v8::Function>::Cast(value)->Call(args.This(), 0, 0);
}

v8::Persistent<v8::FunctionTemplate> getToStringTemplate()
{
    v8::Persistent<v8::FunctionTemplate>& toStringTemplate = V8BindingPerIsolateData::current()->toStringTemplate();
    if (toStringTemplate.IsEmpty())
        toStringTemplate = v8::Persistent<v8::FunctionTemplate>::New(v8::FunctionTemplate::New(constructorToString));
    return toStringTemplate;
}
    
v8::Handle<v8::Value> getElementStringAttr(const v8::AccessorInfo& info,
                                           const QualifiedName& name) 
{
    Element* imp = V8Element::toNative(info.Holder());
    return v8ExternalString(imp->getAttribute(name), info.GetIsolate());
}

void setElementStringAttr(const v8::AccessorInfo& info,
                          const QualifiedName& name,
                          v8::Local<v8::Value> value)
{
    Element* imp = V8Element::toNative(info.Holder());
    AtomicString v = toAtomicWebCoreStringWithNullCheck(value);
    imp->setAttribute(name, v);
}

PassRefPtr<DOMStringList> v8ValueToWebCoreDOMStringList(v8::Handle<v8::Value> value)
{
    v8::Local<v8::Value> v8Value(v8::Local<v8::Value>::New(value));

    if (V8DOMStringList::HasInstance(v8Value)) {
        RefPtr<DOMStringList> ret = V8DOMStringList::toNative(v8::Handle<v8::Object>::Cast(v8Value));
        return ret.release();
    }

    if (!v8Value->IsArray())
        return 0;

    RefPtr<DOMStringList> ret = DOMStringList::create();
    v8::Local<v8::Array> v8Array = v8::Local<v8::Array>::Cast(v8Value);
    for (size_t i = 0; i < v8Array->Length(); ++i) {
        v8::Local<v8::Value> indexedValue = v8Array->Get(v8::Integer::New(i));
        ret->append(v8ValueToWebCoreString(indexedValue));
    }
    return ret.release();
}

} // namespace WebCore
