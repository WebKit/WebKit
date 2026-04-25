/*
 * Copyright (C) 2018-2024 Apple Inc. All rights reserved.

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <JavaScriptCore/JSCJSValue.h>
#include <JavaScriptCore/SlotVisitor.h>
#include <JavaScriptCore/Weak.h>
#include <wtf/WeakPtr.h>

namespace JSC {
class JSGlobalObject;
class ThrowScope;
}

namespace WebCore {

class DOMWrapperWorld;
class JSDOMObject;

// This class includes a lot of subtle GC related things, and changing this class can easily cause GC crashes.
// Any changes to this class must be reviewed by JavaScriptCore reviewers too.
class JSValueInWrappedObject {
    // This must be neither copyable nor movable. Changing this will break concurrent GC.
    WTF_MAKE_NONCOPYABLE(JSValueInWrappedObject);
    WTF_MAKE_NONMOVABLE(JSValueInWrappedObject);
public:
    JSValueInWrappedObject() = default;
    inline JSValueInWrappedObject(JSC::JSGlobalObject&, JSC::JSValue); // Defined in JSValueInWrappedObjectInlines.h
    inline JSValueInWrappedObject(RefPtr<DOMWrapperWorld>&&, JSC::JSValue); // Defined in JSValueInWrappedObjectInlines.h

    inline explicit operator bool() const; // Defined in JSValueInWrappedObjectInlines.h
    template<typename Visitor> inline void visitInGCThread(Visitor&) const; // Defined in JSValueInWrappedObjectInlines.h

    inline void clear(); // Defined in JSValueInWrappedObjectInlines.h

    // If you expect the value you store to be returned by getValue and not cleared under you, you *MUST* use set not setWeakly.
    // The owner parameter is typically the wrapper of the DOM node this class is embedded into but can be any GCed object that
    // will visit this JSValueInWrappedObject via visitAdditionalChildrenInGCThread/isReachableFromOpaqueRoots.
    inline void set(JSC::JSGlobalObject&, const JSC::JSCell* owner, JSC::JSValue); // Defined in JSValueInWrappedObjectInlines.h
    inline void set(RefPtr<DOMWrapperWorld>&&, JSC::VM&, const JSC::JSCell* owner, JSC::JSValue); // Defined in JSValueInWrappedObjectInlines.h
    // Only use this if you actually expect this value to be weakly held. If you call visitInGCThread on this value *DONT* set using setWeakly
    // use set instead. The GC might or might not keep your value around in that case.
    inline void setWeakly(JSC::JSGlobalObject&, JSC::JSValue); // Defined in JSValueInWrappedObjectInlines.h
    inline void setWeakly(RefPtr<DOMWrapperWorld>&&, JSC::JSValue); // Defined in JSValueInWrappedObjectInlines.h
    inline JSC::JSValue getValue(JSC::JSValue nullValue = JSC::jsUndefined()) const; // Defined in JSValueInWrappedObjectInlines.h

    inline RefPtr<DOMWrapperWorld> world() const; // Defined in JSValueInWrappedObjectInlines.h
    inline bool isWorldCompatible(JSC::JSGlobalObject& lexicalGlobalObject) const; // Defined in JSValueInWrappedObjectInlines.h

private:
    inline void setValueInternal(JSC::JSValue); // Defined in JSValueInWrappedObjectInlines.h
    inline void setWorld(JSC::JSGlobalObject&); // Defined in JSValueInWrappedObjectInlines.h
    inline void setWorld(RefPtr<DOMWrapperWorld>&&); // Defined in JSValueInWrappedObjectInlines.h

    // Keep in mind that all of these fields are accessed concurrently without lock from concurrent GC thread.
    JSC::JSValue m_nonCell { };
    JSC::Weak<JSC::JSCell> m_cell { };
    SingleThreadWeakPtr<DOMWrapperWorld> m_world;
};

inline JSC::JSValue cachedPropertyValue(JSC::ThrowScope&, JSC::JSGlobalObject&, const JSDOMObject& owner, JSValueInWrappedObject& cacheSlot, const auto&);

} // namespace WebCore
