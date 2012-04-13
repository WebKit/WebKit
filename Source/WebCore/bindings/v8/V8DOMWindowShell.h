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

#include "V8BindingPerContextData.h"
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
class V8DOMWindowShell : public RefCounted<V8DOMWindowShell> {
public:
    static PassRefPtr<V8DOMWindowShell> create(Frame*);

    v8::Handle<v8::Context> context() const { return m_context; }

    // Update document object of the frame.
    void updateDocument();

    void namedItemAdded(HTMLDocument*, const AtomicString&);
    void namedItemRemoved(HTMLDocument*, const AtomicString&);

    // Update the security origin of a document
    // (e.g., after setting docoument.domain).
    void updateSecurityOrigin();

    bool isContextInitialized();

    v8::Persistent<v8::Context> createNewContext(v8::Handle<v8::Object> global, int extensionGroup, int worldId);
    void setContext(v8::Handle<v8::Context>);
    static bool installDOMWindow(v8::Handle<v8::Context> context, DOMWindow*);

    bool initContextIfNeeded();
    void updateDocumentWrapper(v8::Handle<v8::Object> wrapper);

    void clearForNavigation();
    void clearForClose();

    void destroyGlobal();

    V8BindingPerContextData* perContextData() { return m_perContextData.get(); }

private:
    V8DOMWindowShell(Frame*);

    void disposeContextHandles();

    void setSecurityToken();
    void clearDocumentWrapper();

    // The JavaScript wrapper for the document object is cached on the global
    // object for fast access. UpdateDocumentWrapperCache sets the wrapper
    // for the current document on the global object. ClearDocumentWrapperCache
    // deletes the document wrapper from the global object.
    void updateDocumentWrapperCache();
    void clearDocumentWrapperCache();

    Frame* m_frame;

    OwnPtr<V8BindingPerContextData> m_perContextData;

    v8::Persistent<v8::Context> m_context;
    v8::Persistent<v8::Object> m_global;
    v8::Persistent<v8::Object> m_document;
};

} // namespace WebCore

#endif // V8DOMWindowShell_h
