/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "config.h"
#include "V8PerIsolateData.h"

#include "ScriptGCEvent.h"
#include "ScriptProfiler.h"
#include "V8Binding.h"
#include <wtf/MemoryInstrumentationHashMap.h>
#include <wtf/MemoryInstrumentationVector.h>

namespace WTF {

// WrapperTypeInfo are statically allocated, don't count them.
template<> struct SequenceMemoryInstrumentationTraits<WebCore::WrapperTypeInfo*> {
    template <typename I> static void reportMemoryUsage(I, I, MemoryClassInfo&) { }
};

}

namespace WebCore {

V8PerIsolateData::V8PerIsolateData(v8::Isolate* isolate)
    : m_stringCache(adoptPtr(new StringCache()))
    , m_integerCache(adoptPtr(new IntegerCache()))
    , m_domDataStore(0)
    , m_hiddenPropertyName(adoptPtr(new V8HiddenPropertyName()))
    , m_constructorMode(ConstructorMode::CreateNewObject)
    , m_recursionLevel(0)
#ifndef NDEBUG
    , m_internalScriptRecursionLevel(0)
#endif
    , m_gcEventData(adoptPtr(new GCEventData()))
    , m_shouldCollectGarbageSoon(false)
{
    m_v8Null.set(v8::Null(isolate));
}

V8PerIsolateData::~V8PerIsolateData()
{
}

V8PerIsolateData* V8PerIsolateData::create(v8::Isolate* isolate)
{
    ASSERT(isolate);
    ASSERT(!isolate->GetData());
    V8PerIsolateData* data = new V8PerIsolateData(isolate);
    isolate->SetData(data);
    return data;
}

void V8PerIsolateData::ensureInitialized(v8::Isolate* isolate) 
{
    ASSERT(isolate);
    if (!isolate->GetData()) 
        create(isolate);
}

v8::Persistent<v8::Value> V8PerIsolateData::ensureLiveRoot()
{
    if (m_liveRoot.isEmpty())
        m_liveRoot.set(v8::Null());
    return m_liveRoot.get();
}

void V8PerIsolateData::dispose(v8::Isolate* isolate)
{
    void* data = isolate->GetData();
    delete static_cast<V8PerIsolateData*>(data);
    isolate->SetData(0);
}

v8::Handle<v8::FunctionTemplate> V8PerIsolateData::toStringTemplate()
{
    if (m_toStringTemplate.isEmpty())
        m_toStringTemplate.set(v8::FunctionTemplate::New(constructorOfToString));
    return v8::Local<v8::FunctionTemplate>::New(m_toStringTemplate.get());
}

void V8PerIsolateData::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::Binding);
    info.addMember(m_rawTemplates);
    info.addMember(m_templates);
    info.addMember(m_stringCache);
    info.addMember(m_integerCache);
    info.addMember(m_domDataList);
    info.addMember(m_domDataStore);
    info.addMember(m_hiddenPropertyName);
    info.addMember(m_gcEventData);

    info.addPrivateBuffer(ScriptProfiler::profilerSnapshotsSize(), WebCoreMemoryTypes::InspectorProfilerAgent);

    info.ignoreMember(m_toStringTemplate);
    info.ignoreMember(m_lazyEventListenerToStringTemplate);
    info.ignoreMember(m_v8Null);
    info.ignoreMember(m_liveRoot);
    info.ignoreMember(m_auxiliaryContext);
}

#if ENABLE(INSPECTOR)
void V8PerIsolateData::visitExternalStrings(ExternalStringVisitor* visitor)
{
    v8::HandleScope handleScope;
    class VisitorImpl : public v8::ExternalResourceVisitor {
    public:
        VisitorImpl(ExternalStringVisitor* visitor) : m_visitor(visitor) { }
        virtual ~VisitorImpl() { }
        virtual void VisitExternalString(v8::Handle<v8::String> string)
        {
            WebCoreStringResourceBase* resource = WebCoreStringResourceBase::toWebCoreStringResourceBase(string);
            if (resource)
                resource->visitStrings(m_visitor);
        }
    private:
        ExternalStringVisitor* m_visitor;
    } v8Visitor(visitor);
    v8::V8::VisitExternalResources(&v8Visitor);
}
#endif

v8::Handle<v8::Value> V8PerIsolateData::constructorOfToString(const v8::Arguments& args)
{
    // The DOM constructors' toString functions grab the current toString
    // for Functions by taking the toString function of itself and then
    // calling it with the constructor as its receiver. This means that
    // changes to the Function prototype chain or toString function are
    // reflected when printing DOM constructors. The only wart is that
    // changes to a DOM constructor's toString's toString will cause the
    // toString of the DOM constructor itself to change. This is extremely
    // obscure and unlikely to be a problem.
    v8::Handle<v8::Value> value = args.Callee()->Get(v8::String::NewSymbol("toString"));
    if (!value->IsFunction()) 
        return v8::String::Empty(args.GetIsolate());
    return v8::Handle<v8::Function>::Cast(value)->Call(args.This(), 0, 0);
}

} // namespace WebCore
