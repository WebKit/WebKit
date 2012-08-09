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

#ifndef V8PerIsolateData_h
#define V8PerIsolateData_h

#include <v8.h>
#include <wtf/HashMap.h>
#include <wtf/OwnPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class DOMDataStore;
class GCEventData;
class IntegerCache;
class MemoryObjectInfo;
class StringCache;
class V8HiddenPropertyName;
struct WrapperTypeInfo;

#if ENABLE(INSPECTOR)
class ExternalStringVisitor;
#endif

typedef WTF::Vector<DOMDataStore*> DOMDataList;

#ifndef NDEBUG
class GlobalHandleInfo;
typedef HashMap<v8::Value*, GlobalHandleInfo*> GlobalHandleMap;
#endif

class V8PerIsolateData {
public:
    static V8PerIsolateData* create(v8::Isolate*);
    static void ensureInitialized(v8::Isolate*);
    static V8PerIsolateData* current(v8::Isolate* isolate = 0)
    {
        if (UNLIKELY(!isolate))
            isolate = v8::Isolate::GetCurrent();
        ASSERT(isolate->GetData());
        return static_cast<V8PerIsolateData*>(isolate->GetData()); 
    }
    static void dispose(v8::Isolate*);

    typedef HashMap<WrapperTypeInfo*, v8::Persistent<v8::FunctionTemplate> > TemplateMap;

    TemplateMap& rawTemplateMap() { return m_rawTemplates; }
    TemplateMap& templateMap() { return m_templates; }
    v8::Persistent<v8::String>& toStringName() { return m_toStringName; }
    v8::Persistent<v8::FunctionTemplate>& toStringTemplate() { return m_toStringTemplate; }

    v8::Persistent<v8::FunctionTemplate>& lazyEventListenerToStringTemplate()
    {
        return m_lazyEventListenerToStringTemplate;
    }

    StringCache* stringCache() { return m_stringCache.get(); }
    IntegerCache* integerCache() { return m_integerCache.get(); }

#if ENABLE(INSPECTOR)
    void visitExternalStrings(ExternalStringVisitor*);
#endif
    DOMDataList& allStores() { return m_domDataList; }

    V8HiddenPropertyName* hiddenPropertyName() { return m_hiddenPropertyName.get(); }
    v8::Persistent<v8::Context>& auxiliaryContext() { return m_auxiliaryContext; }

    void registerDOMDataStore(DOMDataStore* domDataStore) 
    {
        m_domDataList.append(domDataStore);
    }

    void unregisterDOMDataStore(DOMDataStore* domDataStore)
    {
        ASSERT(m_domDataList.find(domDataStore));
        m_domDataList.remove(m_domDataList.find(domDataStore));
    }

    // DOMDataStore is owned outside V8PerIsolateData.
    DOMDataStore* domDataStore() { return m_domDataStore; }
    void setDOMDataStore(DOMDataStore* store) { m_domDataStore = store; }

    int recursionLevel() const { return m_recursionLevel; }
    int incrementRecursionLevel() { return ++m_recursionLevel; }
    int decrementRecursionLevel() { return --m_recursionLevel; }

#ifndef NDEBUG
    GlobalHandleMap& globalHandleMap() { return m_globalHandleMap; }

    int internalScriptRecursionLevel() const { return m_internalScriptRecursionLevel; }
    int incrementInternalScriptRecursionLevel() { return ++m_internalScriptRecursionLevel; }
    int decrementInternalScriptRecursionLevel() { return --m_internalScriptRecursionLevel; }
#endif

    GCEventData* gcEventData() { return m_gcEventData.get(); }

    void reportMemoryUsage(MemoryObjectInfo*) const;

    // Gives the system a hint that we should request garbage collection
    // upon the next close or navigation event, because some expensive
    // objects have been allocated that we want to take every opportunity
    // to collect.
    void setShouldCollectGarbageSoon() { m_shouldCollectGarbageSoon = true; }
    void clearShouldCollectGarbageSoon() { m_shouldCollectGarbageSoon = false; }
    bool shouldCollectGarbageSoon() const { return m_shouldCollectGarbageSoon; }

private:
    explicit V8PerIsolateData(v8::Isolate*);
    ~V8PerIsolateData();

    TemplateMap m_rawTemplates;
    TemplateMap m_templates;
    v8::Persistent<v8::String> m_toStringName;
    v8::Persistent<v8::FunctionTemplate> m_toStringTemplate;
    v8::Persistent<v8::FunctionTemplate> m_lazyEventListenerToStringTemplate;
    OwnPtr<StringCache> m_stringCache;
    OwnPtr<IntegerCache> m_integerCache;

    DOMDataList m_domDataList;
    DOMDataStore* m_domDataStore;

    OwnPtr<V8HiddenPropertyName> m_hiddenPropertyName;
    v8::Persistent<v8::Context> m_auxiliaryContext;

    bool m_constructorMode;
    friend class ConstructorMode;

    int m_recursionLevel;

#ifndef NDEBUG
    GlobalHandleMap m_globalHandleMap;
    int m_internalScriptRecursionLevel;
#endif
    OwnPtr<GCEventData> m_gcEventData;
    bool m_shouldCollectGarbageSoon;
};

} // namespace WebCore

#endif // V8PerIsolateData_h
