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

#include "ChromiumBridge.h"
#include "Node.h"
#include "NodeFilter.h"
#include "PlatformString.h" // for WebCore::String
#include "ScriptSourceCode.h" // for WebCore::ScriptSourceCode
#include "SecurityOrigin.h" // for WebCore::SecurityOrigin
#include "V8CustomBinding.h"
#include "V8DOMMap.h"
#include "V8DOMWrapper.h"
#include "V8EventListenerList.h"
#include "V8GCController.h"
#include "V8Index.h"
#include "V8Utilities.h"
#include <v8.h>
#include <wtf/Assertions.h>
#include <wtf/PassRefPtr.h> // so generated bindings don't have to
#include <wtf/Vector.h>

#include <iterator>
#include <list>

#ifdef ENABLE_DOM_STATS_COUNTERS
#define INC_STATS(name) ChromiumBridge::incrementStatsCounter(name)
#else
#define INC_STATS(name)
#endif

namespace WebCore {

    class CSSRule;
    class CSSRuleList;
    class CSSStyleDeclaration;
    class CSSValue;
    class CSSValueList;
    class ClientRectList;
    class DOMImplementation;
    class DOMWindow;
    class Document;
    class Element;
    class Event;
    class EventListener;
    class EventTarget;
    class Frame;
    class HTMLCollection;
    class HTMLDocument;
    class HTMLElement;
    class HTMLOptionsCollection;
    class MediaList;
    class MimeType;
    class MimeTypeArray;
    class NamedNodeMap;
    class Navigator;
    class Node;
    class NodeFilter;
    class NodeList;
    class Plugin;
    class PluginArray;
    class SVGElement;
#if ENABLE(SVG)
    class SVGElementInstance;
#endif
    class Screen;
    class ScriptExecutionContext;
#if ENABLE(DOM_STORAGE)
    class Storage;
    class StorageEvent;
#endif
    class String;
    class StyleSheet;
    class StyleSheetList;
    class V8EventListener;
    class V8ObjectEventListener;

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
        V8ClassIndex::V8WrapperType data;
        v8::AccessControl settings;
        v8::PropertyAttribute attribute;
        bool onProto;
    };

    void batchConfigureAttributes(v8::Handle<v8::ObjectTemplate>, v8::Handle<v8::ObjectTemplate>, const BatchedAttribute*, size_t attributeCount);

    // BatchedConstant translates into calls to Set() for setting up an object's
    // constants. It sets the constant on both the FunctionTemplate and the
    // ObjectTemplate. PropertyAttributes is always ReadOnly.
    struct BatchedConstant {
        const char* const name;
        int value;
    };

    void batchConfigureConstants(v8::Handle<v8::FunctionTemplate>, v8::Handle<v8::ObjectTemplate>, const BatchedConstant*, size_t constantCount);

    const int kMaxRecursionDepth = 20;

    // Information about an extension that is registered for use with V8. If scheme
    // is non-empty, it contains the URL scheme the extension should be used with.
    // Otherwise, the extension is used with all schemes.
    struct V8ExtensionInfo {
        String scheme;
        v8::Extension* extension;
    };
    typedef std::list<V8ExtensionInfo> V8ExtensionList;

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

        explicit V8Proxy(Frame* frame) : m_frame(frame), m_domNodeMap(getDOMNodeMap()), m_inlineCode(false), m_timerCallback(false), m_recursion(0) { }

        ~V8Proxy();

        Frame* frame() { return m_frame; }

        // Clear page-specific data, but keep the global object identify.
        void clearForNavigation();

        // Clear page-specific data before shutting down the proxy object.
        void clearForClose();

        // Update document object of the frame.
        void updateDocument();

        // Update the security origin of a document
        // (e.g., after setting docoument.domain).
        void updateSecurityOrigin();

        // Destroy the global object.
        void destroyGlobal();

        // FIXME: Need comment. User Gesture related.
        bool inlineCode() const { return m_inlineCode; }
        void setInlineCode(bool value) { m_inlineCode = value; }

        bool timerCallback() const { return m_timerCallback; }
        void setTimerCallback(bool value) { m_timerCallback = value; }

        // Has the context for this proxy been initialized?
        bool isContextInitialized();

        // Disconnects the proxy from its owner frame,
        // and clears all timeouts on the DOM window.
        void disconnectFrame();

        bool isEnabled();

        // Find/Create/Remove event listener wrappers.
        PassRefPtr<V8EventListener> findV8EventListener(v8::Local<v8::Value> listener, bool isHtml);
        PassRefPtr<V8EventListener> findOrCreateV8EventListener(v8::Local<v8::Value> listener, bool isHtml);

        PassRefPtr<V8EventListener> findObjectEventListener(v8::Local<v8::Value> listener, bool isHtml);
        PassRefPtr<V8EventListener> findOrCreateObjectEventListener(v8::Local<v8::Value> listener, bool isHtml);

        void removeV8EventListener(V8EventListener*);
        void removeObjectEventListener(V8ObjectEventListener*);

#if ENABLE(SVG)
        static void setSVGContext(void*, SVGElement*);
        static SVGElement* svgContext(void*);
#endif

        void setEventHandlerLineNumber(int lineNumber) { m_handlerLineNumber = lineNumber; }
        void finishedWithEvent(Event*) { }

        // Evaluate JavaScript in a new isolated world. The script gets its own
        // global scope, its own prototypes for intrinsic JavaScript objects (String,
        // Array, and so-on), and its own wrappers for all DOM nodes and DOM
        // constructors.
        void evaluateInNewWorld(const Vector<ScriptSourceCode>& sources);

        // Evaluate JavaScript in a new context. The script gets its own global scope
        // and its own prototypes for intrinsic JavaScript objects (String, Array,
        // and so-on). It shares the wrappers for all DOM nodes and DOM constructors.
        void evaluateInNewContext(const Vector<ScriptSourceCode>&);

        // Evaluate a script file in the current execution environment.
        // The caller must hold an execution context.
        // If cannot evalute the script, it returns an error.
        v8::Local<v8::Value> evaluate(const ScriptSourceCode&, Node*);

        // Run an already compiled script.
        v8::Local<v8::Value> runScript(v8::Handle<v8::Script>, bool isInlineCode);

        // Call the function with the given receiver and arguments.
        v8::Local<v8::Value> callFunction(v8::Handle<v8::Function>, v8::Handle<v8::Object>, int argc, v8::Handle<v8::Value> argv[]);

        // Call the function as constructor with the given arguments.
        v8::Local<v8::Value> newInstance(v8::Handle<v8::Function>, int argc, v8::Handle<v8::Value> argv[]);

        // Returns the dom constructor function for the given node type.
        v8::Local<v8::Function> getConstructor(V8ClassIndex::V8WrapperType);

        // To create JS Wrapper objects, we create a cache of a 'boiler plate'
        // object, and then simply Clone that object each time we need a new one.
        // This is faster than going through the full object creation process.
        v8::Local<v8::Object> createWrapperFromCache(V8ClassIndex::V8WrapperType);

        // Returns the window object of the currently executing context.
        static DOMWindow* retrieveWindow();
        // Returns the window object associated with a context.
        static DOMWindow* retrieveWindow(v8::Handle<v8::Context>);
        // Returns V8Proxy object of the currently executing context.
        static V8Proxy* retrieve();
        // Returns V8Proxy object associated with a frame.
        static V8Proxy* retrieve(Frame*);
        // Returns V8Proxy object associated with a script execution context.
        static V8Proxy* retrieve(ScriptExecutionContext*);

        // Returns the frame object of the window object associated
        // with the currently executing context.
        static Frame* retrieveFrame();
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
        static v8::Local<v8::Context> currentContext();

        // If the current context causes out of memory, JavaScript setting
        // is disabled and it returns true.
        static bool handleOutOfMemory();

        // Check if the active execution context can access the target frame.
        static bool canAccessFrame(Frame*, bool reportError);

        // Check if it is safe to access the given node from the
        // current security context.
        static bool checkNodeSecurity(Node*);

        static v8::Handle<v8::Value> checkNewLegal(const v8::Arguments&);

        static v8::Handle<v8::Script> compileScript(v8::Handle<v8::String> code, const String& fileName, int baseLine);

        // If the exception code is different from zero, a DOM exception is
        // schedule to be thrown.
        static void setDOMException(int exceptionCode);

        // Schedule an error object to be thrown.
        static v8::Handle<v8::Value> throwError(ErrorType, const char* message);

        // Create an instance of a function descriptor and set to the global object
        // as a named property. Used by v8_test_shell.
        static void bindJsObjectToWindow(Frame*, const char* name, int type, v8::Handle<v8::FunctionTemplate>, void*);

        template <int tag, typename T>
        static v8::Handle<v8::Value> constructDOMObject(const v8::Arguments&);

        // Process any pending JavaScript console messages.
        static void processConsoleMessages();

        // Function for retrieving the line number and source name for the top
        // JavaScript stack frame.
        static int sourceLineNumber();
        static String sourceName();

        // Returns a local handle of the context.
        v8::Local<v8::Context> context()
        {
            return v8::Local<v8::Context>::New(m_context);
        }

        bool setContextDebugId(int id);
        static int contextDebugId(v8::Handle<v8::Context>);

        // Registers an extension to be available on webpages with a particular scheme
        // If the scheme argument is empty, the extension is available on all pages.
        // Will only affect v8 contexts initialized after this call. Takes ownership
        // of the v8::Extension object passed.
        static void registerExtension(v8::Extension*, const String& schemeRestriction);

        // FIXME: Separate these concerns from V8Proxy?
        v8::Persistent<v8::Context> createNewContext(v8::Handle<v8::Object> global);
        bool installDOMWindow(v8::Handle<v8::Context> context, DOMWindow* window);

        void initContextIfNeeded();
        DOMWrapperMap<Node>& domNodeMap() { return m_domNodeMap; }
        void updateDocumentWrapper(v8::Handle<v8::Value> wrapper);

    private:
        static const char* kContextDebugDataType;
        static const char* kContextDebugDataValue;

        void disconnectEventListeners();
        void setSecurityToken();
        void clearDocumentWrapper();

        // The JavaScript wrapper for the document object is cached on the global
        // object for fast access. UpdateDocumentWrapperCache sets the wrapper
        // for the current document on the global object. ClearDocumentWrapperCache
        // deletes the document wrapper from the global object.
        void updateDocumentWrapperCache();
        void clearDocumentWrapperCache();

        // Dispose global handles of m_contexts and friends.
        void disposeContextHandles();

        static bool canAccessPrivate(DOMWindow*);

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

        static void createUtilityContext();

        // Returns a local handle of the utility context.
        static v8::Local<v8::Context> utilityContext()
        {
            if (m_utilityContext.IsEmpty())
                createUtilityContext();
            return v8::Local<v8::Context>::New(m_utilityContext);
        }

        Frame* m_frame;

        v8::Persistent<v8::Context> m_context;
        // For each possible type of wrapper, we keep a boilerplate object.
        // The boilerplate is used to create additional wrappers of the same type.
        // We keep a single persistent handle to an array of the activated
        // boilerplates.
        v8::Persistent<v8::Array> m_wrapperBoilerplates;
        v8::Persistent<v8::Value> m_objectPrototype;

        v8::Persistent<v8::Object> m_global;
        v8::Persistent<v8::Value> m_document;

        DOMWrapperMap<Node>& m_domNodeMap;

        // Utility context holding JavaScript functions used internally.
        static v8::Persistent<v8::Context> m_utilityContext;

        int m_handlerLineNumber;

        // A list of event listeners created for this frame,
        // the list gets cleared when removing all timeouts.
        V8EventListenerList m_eventListeners;

        // A list of event listeners create for XMLHttpRequest object for this frame,
        // the list gets cleared when removing all timeouts.
        V8EventListenerList m_xhrListeners;

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

        // List of extensions registered with the context.
        static V8ExtensionList m_extensions;
    };

    template <int tag, typename T>
    v8::Handle<v8::Value> V8Proxy::constructDOMObject(const v8::Arguments& args)
    {
        if (!args.IsConstructCall())
            return throwError(V8Proxy::TypeError, "DOM object constructor cannot be called as a function.");

        // Note: it's OK to let this RefPtr go out of scope because we also call
        // SetDOMWrapper(), which effectively holds a reference to obj.
        RefPtr<T> obj = T::create();
        V8DOMWrapper::setDOMWrapper(args.Holder(), tag, obj.get());
        obj->ref();
        V8DOMWrapper::setJSWrapperForDOMObject(obj.get(), v8::Persistent<v8::Object>::New(args.Holder()));
        return args.Holder();
    }


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
        V8Proxy::throwError(type, message);
        return v8::Undefined();
    }

    inline v8::Handle<v8::Primitive> throwError(ExceptionCode ec)
    {
        V8Proxy::setDOMException(ec);
        return v8::Undefined();
    }

    inline v8::Handle<v8::Primitive> throwError(v8::Local<v8::Value> exception)
    {
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
