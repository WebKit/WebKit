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

#ifndef WebKitSharedScriptRepository_h
#define WebKitSharedScriptRepository_h

#if ENABLE(SHARED_SCRIPT)

#include <wtf/PassRefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

typedef int ExceptionCode;

class Document;
class KURL;
class SharedScriptContext;
class String;
class WebKitSharedScript;

// Interface to a repository which manages references to the set of active SharedScriptContexts.
class WebKitSharedScriptRepository {
public:
    // Connects the passed WebKitSharedScript object with a SharedScriptContext, creating a new one if necessary.
    static void connect(PassRefPtr<WebKitSharedScript>, const KURL&, const String& name, ExceptionCode&);

    static void removeSharedScriptContext(SharedScriptContext*);
    static void documentDetached(Document*);

private:
    WebKitSharedScriptRepository() { }
    static WebKitSharedScriptRepository& instance();
    void connectToSharedScript(PassRefPtr<WebKitSharedScript>, const KURL&, const String& name, ExceptionCode&);
    PassRefPtr<SharedScriptContext> getSharedScriptContext(const String& name, const KURL&);

    // List of SharedScriptContexts.
    // Expectation is that there will be a limited number of SharedScriptContexts, and so tracking them in a Vector is more efficient than nested HashMaps.
    typedef Vector<SharedScriptContext*> SharedScriptContextList;
    SharedScriptContextList m_sharedScriptContexts;

};

} // namespace WebCore

#endif // ENABLE(SHARED_SCRIPT)

#endif // WebKitSharedScriptRepository_h

