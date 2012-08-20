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

#ifndef V8Proxy_h
#define V8Proxy_h

#include "PlatformSupport.h"
#include "SharedPersistent.h"
#include "StatsCounter.h"
#include "V8AbstractEventListener.h"
#include "V8DOMWrapper.h"
#include "V8GCController.h"
#include "V8Utilities.h"
#include "WrapperTypeInfo.h"
#include <v8.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/PassRefPtr.h> // so generated bindings don't have to
#include <wtf/Vector.h>
#include <wtf/text/TextPosition.h>

#if defined(ENABLE_DOM_STATS_COUNTERS) && PLATFORM(CHROMIUM)
#define INC_STATS(name) StatsCounter::incrementStatsCounter(name)
#else
#define INC_STATS(name)
#endif

namespace WebCore {

    class CachedScript;
    class DOMWindow;
    class Frame;
    class Node;
    class ScriptExecutionContext;
    class ScriptSourceCode;
    class SecurityOrigin;
    class V8DOMWindowShell;
    class V8EventListener;
    class V8IsolatedContext;
    class V8PerContextData;
    class WorldContextHandle;

    const int kMaxRecursionDepth = 22;

    // Note: although the pointer is raw, the instance is kept alive by a strong
    // reference to the v8 context it contains, which is not made weak until we
    // call world->destroy().
    //
    // FIXME: We want to eventually be holding window shells instead of the
    //        IsolatedContext directly.
    typedef HashMap<int, V8IsolatedContext*> IsolatedWorldMap;

    typedef HashMap<int, RefPtr<SecurityOrigin> > IsolatedWorldSecurityOriginMap;

    class V8Proxy {
    public:
        explicit V8Proxy(Frame*);

        ~V8Proxy();

        Frame* frame() const { return m_frame; }

        void clearForNavigation();
        void clearForClose();

        void finishedWithEvent(Event*) { }

        // Evaluate a script file in the current execution environment.
        // The caller must hold an execution context.
        // If cannot evalute the script, it returns an error.
        v8::Local<v8::Value> evaluate(const ScriptSourceCode&, Node*);

        // Run an already compiled script.
        v8::Local<v8::Value> runScript(v8::Handle<v8::Script>);

        // call the function with the given receiver and arguments and report times to DevTools.
        static v8::Local<v8::Value> instrumentedCallFunction(Frame*, v8::Handle<v8::Function>, v8::Handle<v8::Object> receiver, int argc, v8::Handle<v8::Value> args[]);

        // Call the function as constructor with the given arguments.
        v8::Local<v8::Value> newInstance(v8::Handle<v8::Function>, int argc, v8::Handle<v8::Value> argv[]);

        // Returns the window object associated with a context.
        static DOMWindow* retrieveWindow(v8::Handle<v8::Context>);

        // Returns the frame object of the window object associated with
        // a context.
        static Frame* retrieveFrame(v8::Handle<v8::Context>);

        static V8PerContextData* retrievePerContextData(Frame*);

        // Returns V8 Context of a frame. If none exists, creates
        // a new context. It is potentially slow and consumes memory.
        static v8::Local<v8::Context> context(Frame*);
        static v8::Local<v8::Context> mainWorldContext(Frame*);

        // If the current context causes out of memory, JavaScript setting
        // is disabled and it returns true.
        static bool handleOutOfMemory();

        static v8::Handle<v8::Script> compileScript(v8::Handle<v8::String> code, const String& fileName, const TextPosition& scriptStartPosition, v8::ScriptData* = 0);

        v8::Local<v8::Context> context();
        v8::Local<v8::Context> mainWorldContext();
        v8::Local<v8::Context> isolatedWorldContext(int worldId);
        bool matchesCurrentContext();

        // FIXME: This should eventually take DOMWrapperWorld argument!
        // FIXME: This method will be soon removed, as all methods that access windowShell()
        // will be moved to ScriptController.
        V8DOMWindowShell* windowShell() const;

        bool setContextDebugId(int id);
        static int contextDebugId(v8::Handle<v8::Context>);

        static void reportUnsafeAccessTo(Document* targetDocument);

        // FIXME: Move m_isolatedWorlds to ScriptController and remove this getter.
        IsolatedWorldMap& isolatedWorlds() { return m_isolatedWorlds; }

        // FIXME: Move m_isolatedWorldSecurityOrigins to ScriptController and remove this getter.
        IsolatedWorldSecurityOriginMap& isolatedWorldSecurityOrigins() { return m_isolatedWorldSecurityOrigins; }

    private:
        void resetIsolatedWorlds();

        PassOwnPtr<v8::ScriptData> precompileScript(v8::Handle<v8::String>, CachedScript*);

        Frame* m_frame;

        // The isolated worlds we are tracking for this frame. We hold them alive
        // here so that they can be used again by future calls to
        // evaluateInIsolatedWorld().
        IsolatedWorldMap m_isolatedWorlds;
        
        IsolatedWorldSecurityOriginMap m_isolatedWorldSecurityOrigins;
    };

    v8::Local<v8::Context> toV8Context(ScriptExecutionContext*, const WorldContextHandle& worldContext);
}

#endif // V8Proxy_h
