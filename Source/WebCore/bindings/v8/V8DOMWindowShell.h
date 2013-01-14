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

#ifndef V8DOMWindowShell_h
#define V8DOMWindowShell_h

#include "DOMWrapperWorld.h"
#include "ScopedPersistent.h"
#include "SecurityOrigin.h"
#include "V8PerContextData.h"
#include "WrapperTypeInfo.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/text/AtomicString.h>

namespace WebCore {

class DOMWindow;
class Frame;
class HTMLDocument;

// V8WindowShell represents all the per-global object state for a Frame that
// persist between navigations.
class V8DOMWindowShell {
public:
    static PassOwnPtr<V8DOMWindowShell> create(Frame*, PassRefPtr<DOMWrapperWorld>, v8::Isolate*);

    v8::Persistent<v8::Context> context() const { return m_context.get(); }

    // Update document object of the frame.
    void updateDocument();

    void namedItemAdded(HTMLDocument*, const AtomicString&);
    void namedItemRemoved(HTMLDocument*, const AtomicString&);

    // Update the security origin of a document
    // (e.g., after setting docoument.domain).
    void updateSecurityOrigin();

    bool isContextInitialized() { return !m_context.isEmpty(); }
    bool isGlobalInitialized() { return !m_global.isEmpty(); }

    bool initializeIfNeeded();
    void updateDocumentWrapper(v8::Handle<v8::Object> wrapper);

    void clearForNavigation();
    void clearForClose(bool destroyGlobal);

    DOMWrapperWorld* world() { return m_world.get(); }

    void destroyIsolatedShell();

private:
    V8DOMWindowShell(Frame*, PassRefPtr<DOMWrapperWorld>, v8::Isolate*);

    void disposeContext();

    void setSecurityToken();

    // The JavaScript wrapper for the document object is cached on the global
    // object for fast access. UpdateDocumentProperty sets the wrapper
    // for the current document on the global object. ClearDocumentProperty
    // deletes the document wrapper from the global object.
    void updateDocumentProperty();
    void clearDocumentProperty();

    void createContext();
    bool installDOMWindow();

    static V8DOMWindowShell* enteredIsolatedWorldContext();

    Frame* m_frame;
    RefPtr<DOMWrapperWorld> m_world;
    v8::Isolate* m_isolate;

    OwnPtr<V8PerContextData> m_perContextData;

    ScopedPersistent<v8::Context> m_context;
    ScopedPersistent<v8::Object> m_global;
    ScopedPersistent<v8::Object> m_document;
};

} // namespace WebCore

#endif // V8DOMWindowShell_h
