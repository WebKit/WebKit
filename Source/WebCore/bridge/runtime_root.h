/*
 * Copyright (C) 2004 Apple Inc.  All rights reserved.
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

#ifndef RUNTIME_ROOT_H_
#define RUNTIME_ROOT_H_

#include <JavaScriptCore/Strong.h>
#include <JavaScriptCore/Weak.h>
#include <JavaScriptCore/WeakInlines.h>
#include <wtf/Forward.h>
#include <wtf/HashCountedSet.h>
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/RefCounted.h>

namespace JSC {

class Interpreter;
class JSGlobalObject;

namespace Bindings {

class RootObject;
class RuntimeObject;

typedef HashCountedSet<JSObject*> ProtectCountSet;

extern RootObject* findProtectingRootObject(JSObject*);
extern RootObject* findRootObject(JSGlobalObject*);

class RootObject : public RefCounted<RootObject>, private JSC::WeakHandleOwner {
    friend class JavaJSObject;

public:
    WEBCORE_EXPORT virtual ~RootObject();
    
    static Ref<RootObject> create(const void* nativeHandle, JSGlobalObject*);

    bool isValid() { return m_isValid; }
    void invalidate();
    
    void gcProtect(JSObject*);
    void gcUnprotect(JSObject*);
    bool gcIsProtected(JSObject*);

    const void* nativeHandle() const;
    WEBCORE_EXPORT JSGlobalObject* globalObject() const;
    void updateGlobalObject(JSGlobalObject*);

    void addRuntimeObject(VM&, RuntimeObject*);
    void removeRuntimeObject(RuntimeObject*);

    struct InvalidationCallback {
        virtual void operator()(RootObject*) = 0;
        virtual ~InvalidationCallback();
    };
    void addInvalidationCallback(InvalidationCallback* callback) { m_invalidationCallbacks.add(callback); }

private:
    RootObject(const void* nativeHandle, JSGlobalObject*);

    // WeakHandleOwner
    void finalize(JSC::Handle<JSC::Unknown>, void* context) override;

    bool m_isValid;
    
    const void* m_nativeHandle;
    Strong<JSGlobalObject> m_globalObject;

    ProtectCountSet m_protectCountSet;
    HashMap<RuntimeObject*, JSC::Weak<RuntimeObject>> m_runtimeObjects; // We use a map to implement a set.

    HashSet<InvalidationCallback*> m_invalidationCallbacks;
};

} // namespace Bindings

} // namespace JSC

#endif // RUNTIME_ROOT_H_
