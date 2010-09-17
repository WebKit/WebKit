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

#include "PlatformBridge.h"
#include "ScriptSourceCode.h" // for WebCore::ScriptSourceCode
#include "SecurityOrigin.h" // for WebCore::SecurityOrigin
#include "SharedPersistent.h"
#include "V8AbstractEventListener.h"
#include "V8DOMWindowShell.h"
#include "V8DOMWrapper.h"
#include "V8GCController.h"
#include "V8Utilities.h"
#include "WrapperTypeInfo.h"
#include <v8.h>
#include <wtf/Forward.h>
#include <wtf/PassRefPtr.h> // so generated bindings don't have to
#include <wtf/Vector.h>

#if defined(ENABLE_DOM_STATS_COUNTERS) && PLATFORM(CHROMIUM)
#define INC_STATS(name) PlatformBridge::incrementStatsCounter(name)
#else
#define INC_STATS(name)
#endif

namespace WebCore {

    class CachedScript;
    class DOMWindow;
    class Frame;
    class Node;
    class SVGElement;
    class ScriptExecutionContext;
    class V8EventListener;
    class V8IsolatedContext;
    class WorldContextHandle;

    // FIXME: use standard logging facilities in WebCore.
    void logInfo(Frame*, const String& message, const String& url);

    // The following Batch structs and methods are used for setting multiple
    // properties on an ObjectTemplate, used from the generated bindings
    // initialization (ConfigureXXXTemplate). This greatly reduces the binary
    // size by moving from code driven setup to data table driven setup.

    // BatchedAttribute translates into calls to SetAccessor() on either the
    // instance or the prototype ObjectTemplate, based on |onProto|.
    struct BatchedAttribute {
        const char* const name;
        v8::AccessorGetter getter;
        v8::AccessorSetter setter;
        WrapperTypeInfo* data;
        v8::AccessControl settings;
        v8::PropertyAttribute attribute;
        bool onProto;
    };

    void batchConfigureAttributes(v8::Handle<v8::ObjectTemplate>, v8::Handle<v8::ObjectTemplate>, const BatchedAttribute*, size_t attributeCount);

    inline void configureAttribute(v8::Handle<v8::ObjectTemplate> instance, v8::Handle<v8::ObjectTemplate> proto, const BatchedAttribute& attribute)
    {
        (attribute.onProto ? proto : instance)->SetAccessor(v8::String::New(attribute.name),
            attribute.getter,
            attribute.setter,
            v8::External::Wrap(attribute.data),
            attribute.settings,
            attribute.attribute);
    }

    // BatchedConstant translates into calls to Set() for setting up an object's
    // constants. It sets the constant on both the FunctionTemplate and the
    // ObjectTemplate. PropertyAttributes is always ReadOnly.
    struct BatchedConstant {
        const char* const name;
        int value;
    };

    void batchConfigureConstants(v8::Handle<v8::FunctionTemplate>, v8::Handle<v8::ObjectTemplate>, const BatchedConstant*, size_t constantCount);

    struct BatchedCallback {
        const char* const name;
        v8::InvocationCallback callback;
    };

    void batchConfigureCallbacks(v8::Handle<v8::ObjectTemplate>, 
                                 v8::Handle<v8::Signature>,
                                 v8::PropertyAttribute,
                                 const BatchedCallback*, 
                                 size_t callbackCount);

    const int kMaxRecursionDepth = 20;

    typedef WTF::Vector<v8::Extension*> V8Extensions;

    class V8Proxy {
    public:
        // The types of javascript errors that can be thrown.
        enum ErrorType {
            RangeError,
            ReferenceError,
            SyntaxError,
            TypeError,
            GeneralError
        };

        // When to report errors.
        enum DelayReporting {
            ReportLater,
            ReportNow
        };

        explicit V8Proxy(Frame*);

        ~V8Proxy();

        Frame* frame() { return m_frame; }

        void clearForNavigation();
        void clearForClose();

        // FIXME: Need comment. User Gesture related.
        bool inlineCode() const { return m_inlineCode; }
        void setInlineCode(bool value) { m_inlineCode = value; }

        bool timerCallback() const { return m_timerCallback; }
        void setTimerCallback(bool value) { m_timerCallback = value; }

        // Disconnects the proxy from its owner frame,
        // and clears all timeouts on the DOM window.
        void disconnectFrame();

#if ENABLE(SVG)
        static void setSVGContext(void*, SVGElement*);
        static SVGElement* svgContext(void*);

        // These helper functions are required in case we are given a PassRefPtr
        // to a (possibly) newly created object and must prevent its reference
        // count from dropping to zero as would happen in code like
        //
        //   V8Proxy::setSVGContext(imp->getNewlyCreatedObject().get(), context);
        //   foo(imp->getNewlyCreatedObject().get());
        //
        // In the above two lines each time getNewlyCreatedObject() is called it
        // creates a new object because we don't ref() it. (So our attemts to
        // associate a context with it fail.) Such code should be rewritten to
        //
        //   foo(V8Proxy::withSVGContext(imp->getNewlyCreatedObject(), context).get());
        //
        // where PassRefPtr::~PassRefPtr() is invoked only after foo() is
        // called.
        template <typename T>
        static PassRefPtr<T> withSVGContext(PassRefPtr<T> object, SVGElement* context)
        {
            setSVGContext(object.get(), context);
            return object;
        }

        template <typename T>
        static T* withSVGContext(T* object, SVGElement* context)
        {
            setSVGContext(object, context);
            return object;
        }
#endif

        void finishedWithEvent(Event*) { }

        // Evaluate JavaScript in a new isolated world. The script gets its own
        // global scope, its own prototypes for intrinsic JavaScript objects (String,
        // Array, and so-on), and its own wrappers for all DOM nodes and DOM
        // constructors.
        void evaluateInIsolatedWorld(int worldId, const Vector<ScriptSourceCode>& sources, int extensionGroup);

        // Returns true if the proxy is currently executing a script in V8.
        bool executingScript() const;

        // Evaluate a script file in the current execution environment.
        // The caller must hold an execution context.
        // If cannot evalute the script, it returns an error.
        v8::Local<v8::Value> evaluate(const ScriptSourceCode&, Node*);

        // Run an already compiled script.
        v8::Local<v8::Value> runScript(v8::Handle<v8::Script>, bool isInlineCode);

        // Call the function with the given receiver and arguments.
        v8::Local<v8::Value> callFunction(v8::Handle<v8::Function>, v8::Handle<v8::Object>, int argc, v8::Handle<v8::Value> argv[]);

        // Call the function with the given receiver and arguments.
        static v8::Local<v8::Value> callFunctionWithoutFrame(v8::Handle<v8::Function>, v8::Handle<v8::Object>, int argc, v8::Handle<v8::Value> argv[]);

        // Call the function as constructor with the given arguments.
        v8::Local<v8::Value> newInstance(v8::Handle<v8::Function>, int argc, v8::Handle<v8::Value> argv[]);

        // Returns the window object associated with a context.
        static DOMWindow* retrieveWindow(v8::Handle<v8::Context>);
        // Returns V8Proxy object of the currently executing context.
        static V8Proxy* retrieve();
        // Returns V8Proxy object associated with a frame.
        static V8Proxy* retrieve(Frame*);
        // Returns V8Proxy object associated with a script execution context.
        static V8Proxy* retrieve(ScriptExecutionContext*);

        // Returns the frame object of the window object associated with
        // a context.
        static Frame* retrieveFrame(v8::Handle<v8::Context>);


        // The three functions below retrieve WebFrame instances relating the
        // currently executing JavaScript. Since JavaScript can make function calls
        // across frames, though, we need to be more precise.
        //
        // For example, imagine that a JS function in frame A calls a function in
        // frame B, which calls native code, which wants to know what the 'active'
        // frame is.
        //
        // The 'entered context' is the context where execution first entered the
        // script engine; the context that is at the bottom of the JS function stack.
        // RetrieveFrameForEnteredContext() would return Frame A in our example.
        // This frame is often referred to as the "dynamic global object."
        //
        // The 'current context' is the context the JS engine is currently inside of;
        // the context that is at the top of the JS function stack.
        // RetrieveFrameForCurrentContext() would return Frame B in our example.
        // This frame is often referred to as the "lexical global object."
        //
        // Finally, the 'calling context' is the context one below the current
        // context on the JS function stack. For example, if function f calls
        // function g, then the calling context will be the context associated with
        // f. This context is commonly used by DOM security checks because they want
        // to know who called them.
        //
        // If you are unsure which of these functions to use, ask abarth.
        //
        // NOTE: These cannot be declared as inline function, because VS complains at
        // linking time.
        static Frame* retrieveFrameForEnteredContext();
        static Frame* retrieveFrameForCurrentContext();
        static Frame* retrieveFrameForCallingContext();

        // Returns V8 Context of a frame. If none exists, creates
        // a new context. It is potentially slow and consumes memory.
        static v8::Local<v8::Context> context(Frame*);
        static v8::Local<v8::Context> mainWorldContext(Frame*);
        static v8::Local<v8::Context> currentContext();

        // If the current context causes out of memory, JavaScript setting
        // is disabled and it returns true.
        static bool handleOutOfMemory();

        static v8::Handle<v8::Value> checkNewLegal(const v8::Arguments&);

        static v8::Handle<v8::Script> compileScript(v8::Handle<v8::String> code, const String& fileName, int baseLine, v8::ScriptData* = 0);

        // If the exception code is different from zero, a DOM exception is
        // schedule to be thrown.
        static void setDOMException(int exceptionCode);

        // Schedule an error object to be thrown.
        static v8::Handle<v8::Value> throwError(ErrorType, const char* message);

        // Helpers for throwing syntax and type errors with predefined messages.
        static v8::Handle<v8::Value> throwTypeError();
        static v8::Handle<v8::Value> throwSyntaxError();

        template <typename T>
        static v8::Handle<v8::Value> constructDOMObject(const v8::Arguments&, WrapperTypeInfo*);

        template <typename T>
        static v8::Handle<v8::Value> constructDOMObjectWithScriptExecutionContext(const v8::Arguments&, WrapperTypeInfo*);

        // Process any pending JavaScript console messages.
        static void processConsoleMessages();

        v8::Local<v8::Context> context();
        v8::Local<v8::Context> mainWorldContext();

        // FIXME: This should eventually take DOMWrapperWorld argument!
        V8DOMWindowShell* windowShell() const { return m_windowShell.get(); }

        bool setContextDebugId(int id);
        static int contextDebugId(v8::Handle<v8::Context>);

        // Registers a v8 extension to be available on webpages. Will only
        // affect v8 contexts initialized after this call. Takes ownership of
        // the v8::Extension object passed.
        static void registerExtension(v8::Extension*);

        static void registerExtensionWithV8(v8::Extension*);
        static bool registeredExtensionWithV8(v8::Extension*);

        static const V8Extensions& extensions() { return m_extensions; }

        // Report an unsafe attempt to access the given frame on the console.
        static void reportUnsafeAccessTo(Frame* target, DelayReporting delay);

    private:
        void didLeaveScriptContext();

        void resetIsolatedWorlds();

        PassOwnPtr<v8::ScriptData> precompileScript(v8::Handle<v8::String>, CachedScript*);

        // Returns false when we're out of memory in V8.
        bool setInjectedScriptContextDebugId(v8::Handle<v8::Context> targetContext);

        static const char* rangeExceptionName(int exceptionCode);
        static const char* eventExceptionName(int exceptionCode);
        static const char* xmlHttpRequestExceptionName(int exceptionCode);
        static const char* domExceptionName(int exceptionCode);

#if ENABLE(XPATH)
        static const char* xpathExceptionName(int exceptionCode);
#endif

#if ENABLE(SVG)
        static const char* svgExceptionName(int exceptionCode);
#endif

#if ENABLE(DATABASE)
        static const char* sqlExceptionName(int exceptionCode);
#endif

        Frame* m_frame;

        // For the moment, we have one of these.  Soon we will have one per DOMWrapperWorld.
        RefPtr<V8DOMWindowShell> m_windowShell;

        // True for <a href="javascript:foo()"> and false for <script>foo()</script>.
        // Only valid during execution.
        bool m_inlineCode;

        // True when executing from within a timer callback. Only valid during
        // execution.
        bool m_timerCallback;

        // Track the recursion depth to be able to avoid too deep recursion. The V8
        // engine allows much more recursion than KJS does so we need to guard against
        // excessive recursion in the binding layer.
        int m_recursion;

        // All of the extensions registered with the context.
        static V8Extensions m_extensions;

        // The isolated worlds we are tracking for this frame. We hold them alive
        // here so that they can be used again by future calls to
        // evaluateInIsolatedWorld().
        //
        // Note: although the pointer is raw, the instance is kept alive by a strong
        // reference to the v8 context it contains, which is not made weak until we
        // call world->destroy().
        //
        // FIXME: We want to eventually be holding window shells instead of the
        //        IsolatedContext directly.
        typedef HashMap<int, V8IsolatedContext*> IsolatedWorldMap;
        IsolatedWorldMap m_isolatedWorlds;
    };

    template <typename T>
    v8::Handle<v8::Value> V8Proxy::constructDOMObject(const v8::Arguments& args, WrapperTypeInfo* type)
    {
        if (!args.IsConstructCall())
            return throwError(V8Proxy::TypeError, "DOM object constructor cannot be called as a function.");

        // Note: it's OK to let this RefPtr go out of scope because we also call
        // SetDOMWrapper(), which effectively holds a reference to obj.
        RefPtr<T> obj = T::create();
        V8DOMWrapper::setDOMWrapper(args.Holder(), type, obj.get());
        obj->ref();
        V8DOMWrapper::setJSWrapperForDOMObject(obj.get(), v8::Persistent<v8::Object>::New(args.Holder()));
        return args.Holder();
    }

    template <typename T>
    v8::Handle<v8::Value> V8Proxy::constructDOMObjectWithScriptExecutionContext(const v8::Arguments& args, WrapperTypeInfo* type)
    {
        if (!args.IsConstructCall())
            return throwError(V8Proxy::TypeError, "");

        ScriptExecutionContext* context = getScriptExecutionContext();
        if (!context)
            return throwError(V8Proxy::ReferenceError, "");

        // Note: it's OK to let this RefPtr go out of scope because we also call
        // SetDOMWrapper(), which effectively holds a reference to obj.
        RefPtr<T> obj = T::create(context);
        V8DOMWrapper::setDOMWrapper(args.Holder(), type, obj.get());
        obj->ref();
        V8DOMWrapper::setJSWrapperForDOMObject(obj.get(), v8::Persistent<v8::Object>::New(args.Holder()));
        return args.Holder();
    }


    v8::Local<v8::Context> toV8Context(ScriptExecutionContext*, const WorldContextHandle& worldContext);

    // Used by an interceptor callback that it hasn't found anything to
    // intercept.
    inline static v8::Local<v8::Object> notHandledByInterceptor()
    {
        return v8::Local<v8::Object>();
    }

    inline static v8::Local<v8::Boolean> deletionNotHandledByInterceptor()
    {
        return v8::Local<v8::Boolean>();
    }
    inline v8::Handle<v8::Primitive> throwError(const char* message, V8Proxy::ErrorType type = V8Proxy::TypeError)
    {
        if (!v8::V8::IsExecutionTerminating())
            V8Proxy::throwError(type, message);
        return v8::Undefined();
    }

    inline v8::Handle<v8::Primitive> throwError(ExceptionCode ec)
    {
        if (!v8::V8::IsExecutionTerminating())
            V8Proxy::setDOMException(ec);
        return v8::Undefined();
    }

    inline v8::Handle<v8::Primitive> throwError(v8::Local<v8::Value> exception)
    {
        if (!v8::V8::IsExecutionTerminating())
            v8::ThrowException(exception);
        return v8::Undefined();
    }

    template <class T> inline v8::Handle<v8::Object> toV8(PassRefPtr<T> object, v8::Local<v8::Object> holder)
    {
        object->ref();
        V8DOMWrapper::setJSWrapperForDOMObject(object.get(), v8::Persistent<v8::Object>::New(holder));
        return holder;
    }

}

#endif // V8Proxy_h
