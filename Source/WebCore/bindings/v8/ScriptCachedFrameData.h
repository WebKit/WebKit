/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef ScriptCachedFrameData_h
#define ScriptCachedFrameData_h

#if PLATFORM(CHROMIUM)
// We don't use WebKit's page caching, so this implementation is just a stub.

namespace WebCore {

class Frame;
class DOMWindow;

class ScriptCachedFrameData  {
public:
    ScriptCachedFrameData(Frame*) { }
    ~ScriptCachedFrameData() { }

    void restore(Frame*) { }
    void clear() { }
    DOMWindow* domWindow() const { return 0; }
};

} // namespace WebCore

#elif PLATFORM(ANDROID) || PLATFORM(QT)
// FIXME: the right guard should be ENABLE(PAGE_CACHE). Replace with the right guard, once
// https://bugs.webkit.org/show_bug.cgi?id=35061 is fixed.
//
// On Android we do use WebKit's page cache. For now we don't support isolated worlds
// so our implementation does not take them into account.

#include "OwnHandle.h"
#include <v8.h>
#include <wtf/Noncopyable.h>

namespace WebCore {

class Frame;
class DOMWindow;

class ScriptCachedFrameData  {
    WTF_MAKE_NONCOPYABLE(ScriptCachedFrameData);
public:
    ScriptCachedFrameData(Frame*);
    ~ScriptCachedFrameData() { }

    void restore(Frame*);
    void clear();
    DOMWindow* domWindow() const;

private:
    OwnHandle<v8::Object> m_global;
    OwnHandle<v8::Context> m_context;
    DOMWindow* m_domWindow;
};

} // namespace WebCore

#else
#error You need to consider whether you want Page Cache and either add a stub or a real implementation.
#endif // PLATFORM(CHROMIUM)

#endif // ScriptCachedFrameData_h
