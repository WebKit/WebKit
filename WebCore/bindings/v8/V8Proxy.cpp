/*
 * Copyright (C) 2008, 2009 Google Inc. All rights reserved.
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

#include "config.h"
#include "V8Proxy.h"

#include "ChromiumBridge.h"
#include "CSSMutableStyleDeclaration.h"
#include "DOMObjectsInclude.h"
#include "DocumentLoader.h"
#include "FrameLoaderClient.h"
#include "ScriptController.h"
#include "V8Binding.h"
#include "V8Collection.h"
#include "V8CustomBinding.h"
#include "V8DOMMap.h"
#include "V8DOMWindow.h"
#include "V8Index.h"
#include "V8IsolatedWorld.h"
#include "WorkerContextExecutionProxy.h"

#include <algorithm>
#include <utility>
#include <v8.h>
#include <v8-debug.h>
#include <wtf/Assertions.h>
#include <wtf/OwnArrayPtr.h>
#include <wtf/StdLibExtras.h>
#include <wtf/UnusedParam.h>

namespace WebCore {

v8::Persistent<v8::Context> V8Proxy::m_utilityContext;

// Static list of registered extensions
V8ExtensionList V8Proxy::m_extensions;

const char* V8Proxy::kContextDebugDataType = "type";
const char* V8Proxy::kContextDebugDataValue = "value";

void batchConfigureAttributes(v8::Handle<v8::ObjectTemplate> instance, v8::Handle<v8::ObjectTemplate> proto, const BatchedAttribute* attributes, size_t attributeCount)
{
    for (size_t i = 0; i < attributeCount; ++i) {
        const BatchedAttribute* attribute = &attributes[i];
        (attribute->onProto ? proto : instance)->SetAccessor(v8::String::New(attribute->name),
            attribute->getter,
            attribute->setter,
            attribute->data == V8ClassIndex::INVALID_CLASS_INDEX ? v8::Handle<v8::Value>() : v8::Integer::New(V8ClassIndex::ToInt(attribute->data)),
            attribute->settings,
            attribute->attribute);
    }
}

void batchConfigureConstants(v8::Handle<v8::FunctionTemplate> functionDescriptor, v8::Handle<v8::ObjectTemplate> proto, const BatchedConstant* constants, size_t constantCount)
{
    for (size_t i = 0; i < constantCount; ++i) {
        const BatchedConstant* constant = &constants[i];
        functionDescriptor->Set(v8::String::New(constant->name), v8::Integer::New(constant->value), v8::ReadOnly);
        proto->Set(v8::String::New(constant->name), v8::Integer::New(constant->value), v8::ReadOnly);
    }
}

typedef HashMap<Node*, v8::Object*> DOMNodeMap;
typedef HashMap<void*, v8::Object*> DOMObjectMap;

#if ENABLE(SVG)
v8::Handle<v8::Value> V8DOMWrapper::convertSVGElementInstanceToV8Object(SVGElementInstance* instance)
{
    if (!instance)
        return v8::Null();

    v8::Handle<v8::Object> existingInstance = getDOMSVGElementInstanceMap().get(instance);
    if (!existingInstance.IsEmpty())
        return existingInstance;

    instance->ref();

    // Instantiate the V8 object and remember it
    v8::Handle<v8::Object> result = instantiateV8Object(V8ClassIndex::SVGELEMENTINSTANCE, V8ClassIndex::SVGELEMENTINSTANCE, instance);
    if (!result.IsEmpty()) {
        // Only update the DOM SVG element map if the result is non-empty.
        getDOMSVGElementInstanceMap().set(instance, v8::Persistent<v8::Object>::New(result));
    }
    return result;
}

// Map of SVG objects with contexts to their contexts
static HashMap<void*, SVGElement*>& svgObjectToContextMap()
{
    typedef HashMap<void*, SVGElement*> SvgObjectToContextMap;
    DEFINE_STATIC_LOCAL(SvgObjectToContextMap, staticSvgObjectToContextMap, ());
    return staticSvgObjectToContextMap;
}

v8::Handle<v8::Value> V8DOMWrapper::convertSVGObjectWithContextToV8Object(V8ClassIndex::V8WrapperType type, void* object)
{
    if (!object)
        return v8::Null();

    v8::Persistent<v8::Object> result = getDOMSVGObjectWithContextMap().get(object);
    if (!result.IsEmpty())
        return result;

    // Special case: SVGPathSegs need to be downcast to their real type
    if (type == V8ClassIndex::SVGPATHSEG)
        type = V8Custom::DowncastSVGPathSeg(object);

    v8::Local<v8::Object> v8Object = instantiateV8Object(type, type, object);
    if (!v8Object.IsEmpty()) {
        result = v8::Persistent<v8::Object>::New(v8Object);
        switch (type) {
#define MAKE_CASE(TYPE, NAME)     \
        case V8ClassIndex::TYPE: static_cast<NAME*>(object)->ref(); break;
        SVG_OBJECT_TYPES(MAKE_CASE)
#undef MAKE_CASE
#define MAKE_CASE(TYPE, NAME)     \
        case V8ClassIndex::TYPE:    \
            static_cast<V8SVGPODTypeWrapper<NAME>*>(object)->ref(); break;
        SVG_POD_NATIVE_TYPES(MAKE_CASE)
#undef MAKE_CASE
        default:
            ASSERT_NOT_REACHED();
        }
        getDOMSVGObjectWithContextMap().set(object, result);
    }

    return result;
}

void V8Proxy::setSVGContext(void* object, SVGElement* context)
{
    if (!object)
        return;

    SVGElement* oldContext = svgObjectToContextMap().get(object);

    if (oldContext == context)
        return;

    if (oldContext)
        oldContext->deref();

    if (context)
        context->ref();

    svgObjectToContextMap().set(object, context);
}

SVGElement* V8Proxy::svgContext(void* object)
{
    return svgObjectToContextMap().get(object);
}

#endif

typedef HashMap<int, v8::FunctionTemplate*> FunctionTemplateMap;

bool AllowAllocation::m_current = false;

// JavaScriptConsoleMessages encapsulate everything needed to
// log messages originating from JavaScript to the Chrome console.
class JavaScriptConsoleMessage {
public:
    JavaScriptConsoleMessage(const String& string, const String& sourceID, unsigned lineNumber)
        : m_string(string)
        , m_sourceID(sourceID)
        , m_lineNumber(lineNumber)
        { 
        }

    void addToPage(Page*) const;

private:
    const String m_string;
    const String m_sourceID;
    const unsigned m_lineNumber;
};

void JavaScriptConsoleMessage::addToPage(Page* page) const
{
    ASSERT(page);
    Console* console = page->mainFrame()->domWindow()->console();
    console->addMessage(JSMessageSource, ErrorMessageLevel, m_string, m_lineNumber, m_sourceID);
}

// The ConsoleMessageManager handles all console messages that stem
// from JavaScript. It keeps a list of messages that have been delayed but
// it makes sure to add all messages to the console in the right order.
class ConsoleMessageManager {
public:
    // Add a message to the console. May end up calling JavaScript code
    // indirectly through the inspector so only call this function when
    // it is safe to do allocations.
    static void addMessage(Page*, const JavaScriptConsoleMessage&);

    // Add a message to the console but delay the reporting until it
    // is safe to do so: Either when we leave JavaScript execution or
    // when adding other console messages. The primary purpose of this
    // method is to avoid calling into V8 to handle console messages
    // when the VM is in a state that does not support GCs or allocations.
    // Delayed messages are always reported in the page corresponding
    // to the active context.
    static void addDelayedMessage(const JavaScriptConsoleMessage&);

    // Process any delayed messages. May end up calling JavaScript code
    // indirectly through the inspector so only call this function when
    // it is safe to do allocations.
    static void processDelayedMessages();

private:
    // All delayed messages are stored in this vector. If the vector
    // is NULL, there are no delayed messages.
    static Vector<JavaScriptConsoleMessage>* m_delayed;
};

Vector<JavaScriptConsoleMessage>* ConsoleMessageManager::m_delayed = 0;

void ConsoleMessageManager::addMessage(Page* page, const JavaScriptConsoleMessage& message)
{
    // Process any delayed messages to make sure that messages
    // appear in the right order in the console.
    processDelayedMessages();
    message.addToPage(page);
}

void ConsoleMessageManager::addDelayedMessage(const JavaScriptConsoleMessage& message)
{
    if (!m_delayed) {
        // Allocate a vector for the delayed messages. Will be
        // deallocated when the delayed messages are processed
        // in processDelayedMessages().
        m_delayed = new Vector<JavaScriptConsoleMessage>();
    }
    m_delayed->append(message);
}

void ConsoleMessageManager::processDelayedMessages()
{
    // If we have a delayed vector it cannot be empty.
    if (!m_delayed)
        return;
    ASSERT(!m_delayed->isEmpty());

    // Add the delayed messages to the page of the active
    // context. If that for some bizarre reason does not
    // exist, we clear the list of delayed messages to avoid
    // posting messages. We still deallocate the vector.
    Frame* frame = V8Proxy::retrieveFrameForEnteredContext();
    Page* page = 0;
    if (frame)
        page = frame->page();
    if (!page)
        m_delayed->clear();

    // Iterate through all the delayed messages and add them
    // to the console.
    const int size = m_delayed->size();
    for (int i = 0; i < size; i++)
        m_delayed->at(i).addToPage(page);

    // Deallocate the delayed vector.
    delete m_delayed;
    m_delayed = 0;
}

// Convenience class for ensuring that delayed messages in the
// ConsoleMessageManager are processed quickly.
class ConsoleMessageScope {
public:
    ConsoleMessageScope() { ConsoleMessageManager::processDelayedMessages(); }
    ~ConsoleMessageScope() { ConsoleMessageManager::processDelayedMessages(); }
};

void logInfo(Frame* frame, const String& message, const String& url)
{
    Page* page = frame->page();
    if (!page)
        return;
    JavaScriptConsoleMessage consoleMessage(message, url, 0);
    ConsoleMessageManager::addMessage(page, consoleMessage);
}

static void handleConsoleMessage(v8::Handle<v8::Message> message, v8::Handle<v8::Value> data)
{
    // Use the frame where JavaScript is called from.
    Frame* frame = V8Proxy::retrieveFrameForEnteredContext();
    if (!frame)
        return;

    Page* page = frame->page();
    if (!page)
        return;

    v8::Handle<v8::String> errorMessageString = message->Get();
    ASSERT(!errorMessageString.IsEmpty());
    String errorMessage = toWebCoreString(errorMessageString);

    v8::Handle<v8::Value> resourceName = message->GetScriptResourceName();
    bool useURL = resourceName.IsEmpty() || !resourceName->IsString();
    String resourceNameString = useURL ? frame->document()->url() : toWebCoreString(resourceName);
    JavaScriptConsoleMessage consoleMessage(errorMessage, resourceNameString, message->GetLineNumber());
    ConsoleMessageManager::addMessage(page, consoleMessage);
}

enum DelayReporting {
    ReportLater,
    ReportNow
};

static void reportUnsafeAccessTo(Frame* target, DelayReporting delay)
{
    ASSERT(target);
    Document* targetDocument = target->document();
    if (!targetDocument)
        return;

    Frame* source = V8Proxy::retrieveFrameForEnteredContext();
    if (!source || !source->document())
        return; // Ignore error if the source document is gone.

    Document* sourceDocument = source->document();

    // FIXME: This error message should contain more specifics of why the same
    // origin check has failed.
    String str = String::format("Unsafe JavaScript attempt to access frame "
                                "with URL %s from frame with URL %s. "
                                "Domains, protocols and ports must match.\n",
                                targetDocument->url().string().utf8().data(),
                                sourceDocument->url().string().utf8().data());

    // Build a console message with fake source ID and line number.
    const String kSourceID = "";
    const int kLineNumber = 1;
    JavaScriptConsoleMessage message(str, kSourceID, kLineNumber);

    if (delay == ReportNow) {
        // NOTE: Safari prints the message in the target page, but it seems like
        // it should be in the source page. Even for delayed messages, we put it in
        // the source page; see ConsoleMessageManager::processDelayedMessages().
        ConsoleMessageManager::addMessage(source->page(), message);

    } else {
        ASSERT(delay == ReportLater);
        // We cannot safely report the message eagerly, because this may cause
        // allocations and GCs internally in V8 and we cannot handle that at this
        // point. Therefore we delay the reporting.
        ConsoleMessageManager::addDelayedMessage(message);
    }
}

static void reportUnsafeJavaScriptAccess(v8::Local<v8::Object> host, v8::AccessType type, v8::Local<v8::Value> data)
{
    Frame* target = V8Custom::GetTargetFrame(host, data);
    if (target)
        reportUnsafeAccessTo(target, ReportLater);
}

static void handleFatalErrorInV8()
{
    // FIXME: We temporarily deal with V8 internal error situations
    // such as out-of-memory by crashing the renderer.
    CRASH();
}

static void reportFatalErrorInV8(const char* location, const char* message)
{
    // V8 is shutdown, we cannot use V8 api.
    // The only thing we can do is to disable JavaScript.
    // FIXME: clean up V8Proxy and disable JavaScript.
    printf("V8 error: %s (%s)\n", message, location);
    handleFatalErrorInV8();
}

V8Proxy::~V8Proxy()
{
    clearForClose();
    destroyGlobal();
}

void V8Proxy::destroyGlobal()
{
    if (!m_global.IsEmpty()) {
#ifndef NDEBUG
        V8GCController::unregisterGlobalHandle(this, m_global);
#endif
        m_global.Dispose();
        m_global.Clear();
    }
}


bool V8DOMWrapper::domObjectHasJSWrapper(void* object)
{
    return getDOMObjectMap().contains(object) || getActiveDOMObjectMap().contains(object);
}

// The caller must have increased obj's ref count.
void V8DOMWrapper::setJSWrapperForDOMObject(void* object, v8::Persistent<v8::Object> wrapper)
{
    ASSERT(V8DOMWrapper::maybeDOMWrapper(wrapper));
#ifndef NDEBUG
    V8ClassIndex::V8WrapperType type = V8DOMWrapper::domWrapperType(wrapper);
    switch (type) {
#define MAKE_CASE(TYPE, NAME) case V8ClassIndex::TYPE:
        ACTIVE_DOM_OBJECT_TYPES(MAKE_CASE)
        ASSERT_NOT_REACHED();
#undef MAKE_CASE
    default:
        break;
    }
#endif
    getDOMObjectMap().set(object, wrapper);
}

// The caller must have increased obj's ref count.
void V8DOMWrapper::setJSWrapperForActiveDOMObject(void* object, v8::Persistent<v8::Object> wrapper)
{
    ASSERT(V8DOMWrapper::maybeDOMWrapper(wrapper));
#ifndef NDEBUG
    V8ClassIndex::V8WrapperType type = V8DOMWrapper::domWrapperType(wrapper);
    switch (type) {
#define MAKE_CASE(TYPE, NAME) case V8ClassIndex::TYPE: break;
        ACTIVE_DOM_OBJECT_TYPES(MAKE_CASE)
    default: 
        ASSERT_NOT_REACHED();
#undef MAKE_CASE
    }
#endif
    getActiveDOMObjectMap().set(object, wrapper);
}

// The caller must have increased node's ref count.
void V8DOMWrapper::setJSWrapperForDOMNode(Node* node, v8::Persistent<v8::Object> wrapper)
{
    ASSERT(V8DOMWrapper::maybeDOMWrapper(wrapper));
    getDOMNodeMap().set(node, wrapper);
}

// Event listeners

static V8EventListener* findEventListenerInList(V8EventListenerList& list, v8::Local<v8::Value> listener, bool isInline)
{
    ASSERT(v8::Context::InContext());

    if (!listener->IsObject())
        return 0;

    return list.find(listener->ToObject(), isInline);
}

// Find an existing wrapper for a JS event listener in the map.
PassRefPtr<V8EventListener> V8Proxy::findV8EventListener(v8::Local<v8::Value> listener, bool isInline)
{
    return findEventListenerInList(m_eventListeners, listener, isInline);
}

PassRefPtr<V8EventListener> V8Proxy::findOrCreateV8EventListener(v8::Local<v8::Value> object, bool isInline)
{
    ASSERT(v8::Context::InContext());

    if (!object->IsObject())
        return 0;

    V8EventListener* wrapper = findEventListenerInList(m_eventListeners, object, isInline);
    if (wrapper)
        return wrapper;

    // Create a new one, and add to cache.
    RefPtr<V8EventListener> newListener = V8EventListener::create(m_frame, v8::Local<v8::Object>::Cast(object), isInline);
    m_eventListeners.add(newListener.get());

    return newListener;
}

// Object event listeners (such as XmlHttpRequest and MessagePort) are
// different from listeners on DOM nodes. An object event listener wrapper
// only holds a weak reference to the JS function. A strong reference can
// create a cycle.
//
// The lifetime of these objects is bounded by the life time of its JS
// wrapper. So we can create a hidden reference from the JS wrapper to
// to its JS function.
//
//                          (map)
//              XHR      <----------  JS_wrapper
//               |             (hidden) :  ^
//               V                      V  : (may reachable by closure)
//           V8_listener  --------> JS_function
//                         (weak)  <-- may create a cycle if it is strong
//
// The persistent reference is made weak in the constructor
// of V8ObjectEventListener.

PassRefPtr<V8EventListener> V8Proxy::findObjectEventListener( v8::Local<v8::Value> listener, bool isInline)
{
    return findEventListenerInList(m_xhrListeners, listener, isInline);
}

PassRefPtr<V8EventListener> V8Proxy::findOrCreateObjectEventListener(v8::Local<v8::Value> object, bool isInline)
{
    ASSERT(v8::Context::InContext());

    if (!object->IsObject())
        return 0;

    V8EventListener* wrapper = findEventListenerInList(m_xhrListeners, object, isInline);
    if (wrapper)
        return wrapper;

    // Create a new one, and add to cache.
    RefPtr<V8EventListener> newListener = V8ObjectEventListener::create(m_frame, v8::Local<v8::Object>::Cast(object), isInline);
    m_xhrListeners.add(newListener.get());

    return newListener.release();
}


static void removeEventListenerFromList(V8EventListenerList& list, V8EventListener* listener)
{
    list.remove(listener);
}

void V8Proxy::removeV8EventListener(V8EventListener* listener)
{
    removeEventListenerFromList(m_eventListeners, listener);
}


void V8Proxy::removeObjectEventListener(V8ObjectEventListener* listener)
{
    removeEventListenerFromList(m_xhrListeners, listener);
}

static void disconnectEventListenersInList(V8EventListenerList& list)
{
    V8EventListenerList::iterator it = list.begin();
    while (it != list.end()) {
        (*it)->disconnectFrame();
        ++it;
    }
    list.clear();
}


void V8Proxy::disconnectEventListeners()
{
    disconnectEventListenersInList(m_eventListeners);
    disconnectEventListenersInList(m_xhrListeners);
}


v8::Handle<v8::Script> V8Proxy::compileScript(v8::Handle<v8::String> code, const String& fileName, int baseLine)
{
    const uint16_t* fileNameString = fromWebCoreString(fileName);
    v8::Handle<v8::String> name = v8::String::New(fileNameString, fileName.length());
    v8::Handle<v8::Integer> line = v8::Integer::New(baseLine);
    v8::ScriptOrigin origin(name, line);
    v8::Handle<v8::Script> script = v8::Script::Compile(code, &origin);
    return script;
}

bool V8Proxy::handleOutOfMemory()
{
    v8::Local<v8::Context> context = v8::Context::GetCurrent();

    if (!context->HasOutOfMemoryException())
        return false;

    // Warning, error, disable JS for this frame?
    Frame* frame = V8Proxy::retrieveFrame(context);

    V8Proxy* proxy = V8Proxy::retrieve(frame);
    if (proxy) {
        // Clean m_context, and event handlers.
        proxy->clearForClose();

        proxy->destroyGlobal();
    }

    ChromiumBridge::notifyJSOutOfMemory(frame);

    // Disable JS.
    Settings* settings = frame->settings();
    ASSERT(settings);
    settings->setJavaScriptEnabled(false);

    return true;
}

void V8Proxy::evaluateInNewWorld(const Vector<ScriptSourceCode>& sources)
{
    initContextIfNeeded();
    V8IsolatedWorld::evaluate(sources, this);
}

void V8Proxy::evaluateInNewContext(const Vector<ScriptSourceCode>& sources)
{
    initContextIfNeeded();

    v8::HandleScope handleScope;

    // Set up the DOM window as the prototype of the new global object.
    v8::Handle<v8::Context> windowContext = m_context;
    v8::Handle<v8::Object> windowGlobal = windowContext->Global();
    v8::Handle<v8::Value> windowWrapper = V8DOMWrapper::lookupDOMWrapper(V8ClassIndex::DOMWINDOW, windowGlobal);

    ASSERT(V8DOMWrapper::convertDOMWrapperToNative<DOMWindow>(windowWrapper) == m_frame->domWindow());

    v8::Persistent<v8::Context> context = createNewContext(v8::Handle<v8::Object>());
    v8::Context::Scope contextScope(context);

    // Setup context id for JS debugger.
    v8::Handle<v8::Object> contextData = v8::Object::New();
    v8::Handle<v8::Value> windowContextData = windowContext->GetData();
    if (windowContextData->IsObject()) {
        v8::Handle<v8::String> propertyName = v8::String::New(kContextDebugDataValue);
        contextData->Set(propertyName, v8::Object::Cast(*windowContextData)->Get(propertyName));
    }
    contextData->Set(v8::String::New(kContextDebugDataType), v8::String::New("injected"));
    context->SetData(contextData);

    v8::Handle<v8::Object> global = context->Global();

    v8::Handle<v8::String> implicitProtoString = v8::String::New("__proto__");
    global->Set(implicitProtoString, windowWrapper);

    // Give the code running in the new context a way to get access to the
    // original context.
    global->Set(v8::String::New("contentWindow"), windowGlobal);

    // Run code in the new context.
    for (size_t i = 0; i < sources.size(); ++i)
        evaluate(sources[i], 0);

    // Using the default security token means that the canAccess is always
    // called, which is slow.
    // FIXME: Use tokens where possible. This will mean keeping track of all
    // created contexts so that they can all be updated when the document domain
    // changes.
    context->UseDefaultSecurityToken();
    context.Dispose();
}

v8::Local<v8::Value> V8Proxy::evaluate(const ScriptSourceCode& source, Node* node)
{
    ASSERT(v8::Context::InContext());

    // Compile the script.
    v8::Local<v8::String> code = v8ExternalString(source.source());
    ChromiumBridge::traceEventBegin("v8.compile", node, "");

    // NOTE: For compatibility with WebCore, ScriptSourceCode's line starts at
    // 1, whereas v8 starts at 0.
    v8::Handle<v8::Script> script = compileScript(code, source.url(), source.startLine() - 1);
    ChromiumBridge::traceEventEnd("v8.compile", node, "");

    ChromiumBridge::traceEventBegin("v8.run", node, "");
    v8::Local<v8::Value> result;
    {
        // Isolate exceptions that occur when executing the code. These
        // exceptions should not interfere with javascript code we might
        // evaluate from C++ when returning from here.
        v8::TryCatch tryCatch;
        tryCatch.SetVerbose(true);

        // Set inlineCode to true for <a href="javascript:doSomething()">
        // and false for <script>doSomething</script>. We make a rough guess at
        // this based on whether the script source has a URL.
        result = runScript(script, source.url().string().isNull());
    }
    ChromiumBridge::traceEventEnd("v8.run", node, "");
    return result;
}

v8::Local<v8::Value> V8Proxy::runScript(v8::Handle<v8::Script> script, bool isInlineCode)
{
    if (script.IsEmpty())
        return notHandledByInterceptor();

    // Compute the source string and prevent against infinite recursion.
    if (m_recursion >= kMaxRecursionDepth) {
        v8::Local<v8::String> code = v8ExternalString("throw RangeError('Recursion too deep')");
        // FIXME: Ideally, we should be able to re-use the origin of the
        // script passed to us as the argument instead of using an empty string
        // and 0 baseLine.
        script = compileScript(code, "", 0);
    }

    if (handleOutOfMemory())
        ASSERT(script.IsEmpty());

    if (script.IsEmpty())
        return notHandledByInterceptor();

    // Save the previous value of the inlineCode flag and update the flag for
    // the duration of the script invocation.
    bool previousInlineCode = inlineCode();
    setInlineCode(isInlineCode);

    // Run the script and keep track of the current recursion depth.
    v8::Local<v8::Value> result;
    {
        ConsoleMessageScope scope;
        m_recursion++;

        // See comment in V8Proxy::callFunction.
        m_frame->keepAlive();

        result = script->Run();
        m_recursion--;
    }

    if (handleOutOfMemory())
        ASSERT(result.IsEmpty());

    // Handle V8 internal error situation (Out-of-memory).
    if (result.IsEmpty())
        return notHandledByInterceptor();

    // Restore inlineCode flag.
    setInlineCode(previousInlineCode);

    if (v8::V8::IsDead())
        handleFatalErrorInV8();

    return result;
}

v8::Local<v8::Value> V8Proxy::callFunction(v8::Handle<v8::Function> function, v8::Handle<v8::Object> receiver, int argc, v8::Handle<v8::Value> args[])
{
    // For now, we don't put any artificial limitations on the depth
    // of recursion that stems from calling functions. This is in
    // contrast to the script evaluations.
    v8::Local<v8::Value> result;
    {
        ConsoleMessageScope scope;

        // Evaluating the JavaScript could cause the frame to be deallocated,
        // so we start the keep alive timer here.
        // Frame::keepAlive method adds the ref count of the frame and sets a
        // timer to decrease the ref count. It assumes that the current JavaScript
        // execution finishs before firing the timer.
        m_frame->keepAlive();

        result = function->Call(receiver, argc, args);
    }

    if (v8::V8::IsDead())
        handleFatalErrorInV8();

    return result;
}

v8::Local<v8::Value> V8Proxy::newInstance(v8::Handle<v8::Function> constructor, int argc, v8::Handle<v8::Value> args[])
{
    // No artificial limitations on the depth of recursion, see comment in
    // V8Proxy::callFunction.
    v8::Local<v8::Value> result;
    {
        ConsoleMessageScope scope;

        // See comment in V8Proxy::callFunction.
        m_frame->keepAlive();

        result = constructor->NewInstance(argc, args);
    }

    if (v8::V8::IsDead())
        handleFatalErrorInV8();

    return result;
}

v8::Local<v8::Function> V8Proxy::getConstructor(V8ClassIndex::V8WrapperType type)
{
    // A DOM constructor is a function instance created from a DOM constructor
    // template. There is one instance per context. A DOM constructor is
    // different from a normal function in two ways:
    //   1) it cannot be called as constructor (aka, used to create a DOM object)
    //   2) its __proto__ points to Object.prototype rather than
    //      Function.prototype.
    // The reason for 2) is that, in Safari, a DOM constructor is a normal JS
    // object, but not a function. Hotmail relies on the fact that, in Safari,
    // HTMLElement.__proto__ == Object.prototype.
    //
    // m_objectPrototype is a cache of the original Object.prototype.

    ASSERT(isContextInitialized());
    // Enter the context of the proxy to make sure that the
    // function is constructed in the context corresponding to
    // this proxy.
    v8::Context::Scope scope(m_context);
    v8::Handle<v8::FunctionTemplate> functionTemplate = V8DOMWrapper::getTemplate(type);
    // Getting the function might fail if we're running out of
    // stack or memory.
    v8::TryCatch tryCatch;
    v8::Local<v8::Function> value = functionTemplate->GetFunction();
    if (value.IsEmpty())
      return v8::Local<v8::Function>();
    // Hotmail fix, see comments above.
    value->Set(v8::String::New("__proto__"), m_objectPrototype);
    return value;
}

v8::Local<v8::Object> V8Proxy::createWrapperFromCache(V8ClassIndex::V8WrapperType type)
{
    int classIndex = V8ClassIndex::ToInt(type);
    v8::Local<v8::Object> clone(m_wrapperBoilerplates->CloneElementAt(classIndex));
    if (!clone.IsEmpty())
        return clone;

    // Not in cache.
    initContextIfNeeded();
    v8::Context::Scope scope(m_context);
    v8::Local<v8::Function> function = getConstructor(type);
    v8::Local<v8::Object> instance = SafeAllocation::newInstance(function);
    if (!instance.IsEmpty()) {
        m_wrapperBoilerplates->Set(v8::Integer::New(classIndex), instance);
        return instance->Clone();
    }
    return notHandledByInterceptor();
}

// Get the string 'toString'.
static v8::Persistent<v8::String> GetToStringName()
{
    DEFINE_STATIC_LOCAL(v8::Persistent<v8::String>, value, ());
    if (value.IsEmpty())
        value = v8::Persistent<v8::String>::New(v8::String::New("toString"));
    return value;
}

static v8::Handle<v8::Value> ConstructorToString(const v8::Arguments& args)
{
    // The DOM constructors' toString functions grab the current toString
    // for Functions by taking the toString function of itself and then
    // calling it with the constructor as its receiver. This means that
    // changes to the Function prototype chain or toString function are
    // reflected when printing DOM constructors. The only wart is that
    // changes to a DOM constructor's toString's toString will cause the
    // toString of the DOM constructor itself to change. This is extremely
    // obscure and unlikely to be a problem.
    v8::Handle<v8::Value> value = args.Callee()->Get(GetToStringName());
    if (!value->IsFunction()) 
        return v8::String::New("");
    return v8::Handle<v8::Function>::Cast(value)->Call(args.This(), 0, 0);
}

v8::Persistent<v8::FunctionTemplate> V8DOMWrapper::getTemplate(V8ClassIndex::V8WrapperType type)
{
    v8::Persistent<v8::FunctionTemplate>* cacheCell = V8ClassIndex::GetCache(type);
    if (!cacheCell->IsEmpty())
        return *cacheCell;

    // Not in the cache.
    FunctionTemplateFactory factory = V8ClassIndex::GetFactory(type);
    v8::Persistent<v8::FunctionTemplate> descriptor = factory();
    // DOM constructors are functions and should print themselves as such.
    // However, we will later replace their prototypes with Object
    // prototypes so we need to explicitly override toString on the
    // instance itself. If we later make DOM constructors full objects
    // we can give them class names instead and Object.prototype.toString
    // will work so we can remove this code.
    DEFINE_STATIC_LOCAL(v8::Persistent<v8::FunctionTemplate>, toStringTemplate, ());
    if (toStringTemplate.IsEmpty())
        toStringTemplate = v8::Persistent<v8::FunctionTemplate>::New(v8::FunctionTemplate::New(ConstructorToString));
    descriptor->Set(GetToStringName(), toStringTemplate);
    switch (type) {
    case V8ClassIndex::CSSSTYLEDECLARATION:
        // The named property handler for style declarations has a
        // setter. Therefore, the interceptor has to be on the object
        // itself and not on the prototype object.
        descriptor->InstanceTemplate()->SetNamedPropertyHandler( USE_NAMED_PROPERTY_GETTER(CSSStyleDeclaration), USE_NAMED_PROPERTY_SETTER(CSSStyleDeclaration));
        setCollectionStringOrNullIndexedGetter<CSSStyleDeclaration>(descriptor);
        break;
    case V8ClassIndex::CSSRULELIST:
        setCollectionIndexedGetter<CSSRuleList, CSSRule>(descriptor,  V8ClassIndex::CSSRULE);
        break;
    case V8ClassIndex::CSSVALUELIST:
        setCollectionIndexedGetter<CSSValueList, CSSValue>(descriptor, V8ClassIndex::CSSVALUE);
        break;
    case V8ClassIndex::CSSVARIABLESDECLARATION:
        setCollectionStringOrNullIndexedGetter<CSSVariablesDeclaration>(descriptor);
        break;
    case V8ClassIndex::WEBKITCSSTRANSFORMVALUE:
        setCollectionIndexedGetter<WebKitCSSTransformValue, CSSValue>(descriptor, V8ClassIndex::CSSVALUE);
        break;
    case V8ClassIndex::UNDETECTABLEHTMLCOLLECTION:
        descriptor->InstanceTemplate()->MarkAsUndetectable(); // fall through
    case V8ClassIndex::HTMLCOLLECTION:
        descriptor->InstanceTemplate()->SetNamedPropertyHandler(USE_NAMED_PROPERTY_GETTER(HTMLCollection));
        descriptor->InstanceTemplate()->SetCallAsFunctionHandler(USE_CALLBACK(HTMLCollectionCallAsFunction));
        setCollectionIndexedGetter<HTMLCollection, Node>(descriptor, V8ClassIndex::NODE);
        break;
    case V8ClassIndex::HTMLOPTIONSCOLLECTION:
        descriptor->InstanceTemplate()->SetNamedPropertyHandler(USE_NAMED_PROPERTY_GETTER(HTMLCollection));
        descriptor->InstanceTemplate()->SetIndexedPropertyHandler(USE_INDEXED_PROPERTY_GETTER(HTMLOptionsCollection), USE_INDEXED_PROPERTY_SETTER(HTMLOptionsCollection));
        descriptor->InstanceTemplate()->SetCallAsFunctionHandler(USE_CALLBACK(HTMLCollectionCallAsFunction));
        break;
    case V8ClassIndex::HTMLSELECTELEMENT:
        descriptor->InstanceTemplate()->SetNamedPropertyHandler(USE_NAMED_PROPERTY_GETTER(HTMLSelectElementCollection));
        descriptor->InstanceTemplate()->SetIndexedPropertyHandler(nodeCollectionIndexedPropertyGetter<HTMLSelectElement>, USE_INDEXED_PROPERTY_SETTER(HTMLSelectElementCollection),
            0, 0, nodeCollectionIndexedPropertyEnumerator<HTMLSelectElement>, v8::Integer::New(V8ClassIndex::NODE));
        break;
    case V8ClassIndex::HTMLDOCUMENT: {
        descriptor->InstanceTemplate()->SetNamedPropertyHandler(USE_NAMED_PROPERTY_GETTER(HTMLDocument), 0, 0, USE_NAMED_PROPERTY_DELETER(HTMLDocument));

        // We add an extra internal field to all Document wrappers for
        // storing a per document DOMImplementation wrapper.
        //
        // Additionally, we add two extra internal fields for
        // HTMLDocuments to implement temporary shadowing of
        // document.all. One field holds an object that is used as a
        // marker. The other field holds the marker object if
        // document.all is not shadowed and some other value if
        // document.all is shadowed.
        v8::Local<v8::ObjectTemplate> instanceTemplate = descriptor->InstanceTemplate();
        ASSERT(instanceTemplate->InternalFieldCount() == V8Custom::kDefaultWrapperInternalFieldCount);
        instanceTemplate->SetInternalFieldCount(V8Custom::kHTMLDocumentInternalFieldCount);
        break;
    }
#if ENABLE(SVG)
    case V8ClassIndex::SVGDOCUMENT:  // fall through
#endif
    case V8ClassIndex::DOCUMENT: {
        // We add an extra internal field to all Document wrappers for
        // storing a per document DOMImplementation wrapper.
        v8::Local<v8::ObjectTemplate> instanceTemplate = descriptor->InstanceTemplate();
        ASSERT(instanceTemplate->InternalFieldCount() == V8Custom::kDefaultWrapperInternalFieldCount);
        instanceTemplate->SetInternalFieldCount( V8Custom::kDocumentMinimumInternalFieldCount);
        break;
    }
    case V8ClassIndex::HTMLAPPLETELEMENT:  // fall through
    case V8ClassIndex::HTMLEMBEDELEMENT:  // fall through
    case V8ClassIndex::HTMLOBJECTELEMENT:
        // HTMLAppletElement, HTMLEmbedElement and HTMLObjectElement are
        // inherited from HTMLPlugInElement, and they share the same property
        // handling code.
        descriptor->InstanceTemplate()->SetNamedPropertyHandler(USE_NAMED_PROPERTY_GETTER(HTMLPlugInElement), USE_NAMED_PROPERTY_SETTER(HTMLPlugInElement));
        descriptor->InstanceTemplate()->SetIndexedPropertyHandler(USE_INDEXED_PROPERTY_GETTER(HTMLPlugInElement), USE_INDEXED_PROPERTY_SETTER(HTMLPlugInElement));
        descriptor->InstanceTemplate()->SetCallAsFunctionHandler(USE_CALLBACK(HTMLPlugInElement));
        break;
    case V8ClassIndex::HTMLFRAMESETELEMENT:
        descriptor->InstanceTemplate()->SetNamedPropertyHandler(USE_NAMED_PROPERTY_GETTER(HTMLFrameSetElement));
        break;
    case V8ClassIndex::HTMLFORMELEMENT:
        descriptor->InstanceTemplate()->SetNamedPropertyHandler(USE_NAMED_PROPERTY_GETTER(HTMLFormElement));
        descriptor->InstanceTemplate()->SetIndexedPropertyHandler(USE_INDEXED_PROPERTY_GETTER(HTMLFormElement), 0, 0, 0, nodeCollectionIndexedPropertyEnumerator<HTMLFormElement>, v8::Integer::New(V8ClassIndex::NODE));
        break;
    case V8ClassIndex::CANVASPIXELARRAY:
        descriptor->InstanceTemplate()->SetIndexedPropertyHandler(USE_INDEXED_PROPERTY_GETTER(CanvasPixelArray), USE_INDEXED_PROPERTY_SETTER(CanvasPixelArray));
        break;
    case V8ClassIndex::STYLESHEET:  // fall through
    case V8ClassIndex::CSSSTYLESHEET: {
        // We add an extra internal field to hold a reference to
        // the owner node.
        v8::Local<v8::ObjectTemplate> instanceTemplate = descriptor->InstanceTemplate();
        ASSERT(instanceTemplate->InternalFieldCount() == V8Custom::kDefaultWrapperInternalFieldCount);
        instanceTemplate->SetInternalFieldCount(V8Custom::kStyleSheetInternalFieldCount);
        break;
    }
    case V8ClassIndex::MEDIALIST:
        setCollectionStringOrNullIndexedGetter<MediaList>(descriptor);
        break;
    case V8ClassIndex::MIMETYPEARRAY:
        setCollectionIndexedAndNamedGetters<MimeTypeArray, MimeType>(descriptor, V8ClassIndex::MIMETYPE);
        break;
    case V8ClassIndex::NAMEDNODEMAP:
        descriptor->InstanceTemplate()->SetNamedPropertyHandler(USE_NAMED_PROPERTY_GETTER(NamedNodeMap));
        descriptor->InstanceTemplate()->SetIndexedPropertyHandler(USE_INDEXED_PROPERTY_GETTER(NamedNodeMap), 0, 0, 0, collectionIndexedPropertyEnumerator<NamedNodeMap>, v8::Integer::New(V8ClassIndex::NODE));
        break;
#if ENABLE(DOM_STORAGE)
    case V8ClassIndex::STORAGE:
        descriptor->InstanceTemplate()->SetNamedPropertyHandler(USE_NAMED_PROPERTY_GETTER(Storage), USE_NAMED_PROPERTY_SETTER(Storage), 0, USE_NAMED_PROPERTY_DELETER(Storage), V8Custom::v8StorageNamedPropertyEnumerator);
        descriptor->InstanceTemplate()->SetIndexedPropertyHandler(USE_INDEXED_PROPERTY_GETTER(Storage), USE_INDEXED_PROPERTY_SETTER(Storage), 0, USE_INDEXED_PROPERTY_DELETER(Storage));
        break;
#endif
    case V8ClassIndex::NODELIST:
        setCollectionIndexedGetter<NodeList, Node>(descriptor, V8ClassIndex::NODE);
        descriptor->InstanceTemplate()->SetNamedPropertyHandler(USE_NAMED_PROPERTY_GETTER(NodeList));
        break;
    case V8ClassIndex::PLUGIN:
        setCollectionIndexedAndNamedGetters<Plugin, MimeType>(descriptor, V8ClassIndex::MIMETYPE);
        break;
    case V8ClassIndex::PLUGINARRAY:
        setCollectionIndexedAndNamedGetters<PluginArray, Plugin>(descriptor, V8ClassIndex::PLUGIN);
        break;
    case V8ClassIndex::STYLESHEETLIST:
        descriptor->InstanceTemplate()->SetNamedPropertyHandler(USE_NAMED_PROPERTY_GETTER(StyleSheetList));
        setCollectionIndexedGetter<StyleSheetList, StyleSheet>(descriptor, V8ClassIndex::STYLESHEET);
        break;
    case V8ClassIndex::DOMWINDOW: {
        v8::Local<v8::Signature> defaultSignature = v8::Signature::New(descriptor);

        descriptor->PrototypeTemplate()->SetNamedPropertyHandler(USE_NAMED_PROPERTY_GETTER(DOMWindow));
        descriptor->PrototypeTemplate()->SetIndexedPropertyHandler(USE_INDEXED_PROPERTY_GETTER(DOMWindow));

        descriptor->SetHiddenPrototype(true);

        // Reserve spaces for references to location, history and
        // navigator objects.
        v8::Local<v8::ObjectTemplate> instanceTemplate = descriptor->InstanceTemplate();
        instanceTemplate->SetInternalFieldCount(V8Custom::kDOMWindowInternalFieldCount);

        // Set access check callbacks, but turned off initially.
        // When a context is detached from a frame, turn on the access check.
        // Turning on checks also invalidates inline caches of the object.
        instanceTemplate->SetAccessCheckCallbacks(V8Custom::v8DOMWindowNamedSecurityCheck, V8Custom::v8DOMWindowIndexedSecurityCheck, v8::Integer::New(V8ClassIndex::DOMWINDOW), false);
        break;
    }
    case V8ClassIndex::LOCATION: {
        // For security reasons, these functions are on the instance
        // instead of on the prototype object to insure that they cannot
        // be overwritten.
        v8::Local<v8::ObjectTemplate> instance = descriptor->InstanceTemplate();
        instance->SetAccessor(v8::String::New("reload"), V8Custom::v8LocationReloadAccessorGetter, 0, v8::Handle<v8::Value>(), v8::ALL_CAN_READ, static_cast<v8::PropertyAttribute>(v8::DontDelete | v8::ReadOnly));
        instance->SetAccessor(v8::String::New("replace"), V8Custom::v8LocationReplaceAccessorGetter, 0, v8::Handle<v8::Value>(), v8::ALL_CAN_READ, static_cast<v8::PropertyAttribute>(v8::DontDelete | v8::ReadOnly));
        instance->SetAccessor(v8::String::New("assign"), V8Custom::v8LocationAssignAccessorGetter, 0, v8::Handle<v8::Value>(), v8::ALL_CAN_READ, static_cast<v8::PropertyAttribute>(v8::DontDelete | v8::ReadOnly));
        break;
    }
    case V8ClassIndex::HISTORY:
        break;

    case V8ClassIndex::MESSAGECHANNEL: {
        // Reserve two more internal fields for referencing the port1
        // and port2 wrappers. This ensures that the port wrappers are
        // kept alive when the channel wrapper is.
        descriptor->SetCallHandler(USE_CALLBACK(MessageChannelConstructor));
        v8::Local<v8::ObjectTemplate> instanceTemplate = descriptor->InstanceTemplate();
        instanceTemplate->SetInternalFieldCount(V8Custom::kMessageChannelInternalFieldCount);
        break;
    }

    case V8ClassIndex::MESSAGEPORT: {
        // Reserve one more internal field for keeping event listeners.
        v8::Local<v8::ObjectTemplate> instanceTemplate = descriptor->InstanceTemplate();
        instanceTemplate->SetInternalFieldCount(V8Custom::kMessagePortInternalFieldCount);
        break;
    }

#if ENABLE(WORKERS)
    case V8ClassIndex::WORKER: {
        // Reserve one more internal field for keeping event listeners.
        v8::Local<v8::ObjectTemplate> instanceTemplate = descriptor->InstanceTemplate();
        instanceTemplate->SetInternalFieldCount(V8Custom::kWorkerInternalFieldCount);
        descriptor->SetCallHandler(USE_CALLBACK(WorkerConstructor));
        break;
    }

    case V8ClassIndex::WORKERCONTEXT: {
        // Reserve one more internal field for keeping event listeners.
        v8::Local<v8::ObjectTemplate> instanceTemplate = descriptor->InstanceTemplate();
        instanceTemplate->SetInternalFieldCount(V8Custom::kWorkerContextInternalFieldCount);
        break;
    }
#endif // WORKERS

    // The following objects are created from JavaScript.
    case V8ClassIndex::DOMPARSER:
        descriptor->SetCallHandler(USE_CALLBACK(DOMParserConstructor));
        break;
#if ENABLE(VIDEO)
    case V8ClassIndex::HTMLAUDIOELEMENT:
        descriptor->SetCallHandler(USE_CALLBACK(HTMLAudioElementConstructor));
        break;
#endif
    case V8ClassIndex::HTMLIMAGEELEMENT:
        descriptor->SetCallHandler(USE_CALLBACK(HTMLImageElementConstructor));
        break;
    case V8ClassIndex::HTMLOPTIONELEMENT:
        descriptor->SetCallHandler(USE_CALLBACK(HTMLOptionElementConstructor));
        break;
    case V8ClassIndex::WEBKITCSSMATRIX:
        descriptor->SetCallHandler(USE_CALLBACK(WebKitCSSMatrixConstructor));
        break;
    case V8ClassIndex::WEBKITPOINT:
        descriptor->SetCallHandler(USE_CALLBACK(WebKitPointConstructor));
        break;
    case V8ClassIndex::XMLSERIALIZER:
        descriptor->SetCallHandler(USE_CALLBACK(XMLSerializerConstructor));
        break;
    case V8ClassIndex::XMLHTTPREQUEST: {
        // Reserve one more internal field for keeping event listeners.
        v8::Local<v8::ObjectTemplate> instanceTemplate = descriptor->InstanceTemplate();
        instanceTemplate->SetInternalFieldCount(V8Custom::kXMLHttpRequestInternalFieldCount);
        descriptor->SetCallHandler(USE_CALLBACK(XMLHttpRequestConstructor));
        break;
    }
    case V8ClassIndex::XMLHTTPREQUESTUPLOAD: {
        // Reserve one more internal field for keeping event listeners.
        v8::Local<v8::ObjectTemplate> instanceTemplate = descriptor->InstanceTemplate();
        instanceTemplate->SetInternalFieldCount(V8Custom::kXMLHttpRequestInternalFieldCount);
        break;
    }
    case V8ClassIndex::XPATHEVALUATOR:
        descriptor->SetCallHandler(USE_CALLBACK(XPathEvaluatorConstructor));
        break;
    case V8ClassIndex::XSLTPROCESSOR:
        descriptor->SetCallHandler(USE_CALLBACK(XSLTProcessorConstructor));
        break;
    case V8ClassIndex::CLIENTRECTLIST:
        descriptor->InstanceTemplate()->SetIndexedPropertyHandler(USE_INDEXED_PROPERTY_GETTER(ClientRectList));
        break;
    default:
        break;
    }

    *cacheCell = descriptor;
    return descriptor;
}

bool V8Proxy::isContextInitialized()
{
    // m_context, m_global, m_objectPrototype and m_wrapperBoilerplates should
    // all be non-empty if if m_context is non-empty.
    ASSERT(m_context.IsEmpty() || !m_global.IsEmpty());
    ASSERT(m_context.IsEmpty() || !m_objectPrototype.IsEmpty());
    ASSERT(m_context.IsEmpty() || !m_wrapperBoilerplates.IsEmpty());
    return !m_context.IsEmpty();
}

DOMWindow* V8Proxy::retrieveWindow()
{
    // FIXME: This seems very fragile. How do we know that the global object
    // from the current context is something sensible? Do we need to use the
    // last entered here? Who calls this?
    return retrieveWindow(v8::Context::GetCurrent());
}

DOMWindow* V8Proxy::retrieveWindow(v8::Handle<v8::Context> context)
{
    v8::Handle<v8::Object> global = context->Global();
    ASSERT(!global.IsEmpty());
    global = V8DOMWrapper::lookupDOMWrapper(V8ClassIndex::DOMWINDOW, global);
    ASSERT(!global.IsEmpty());
    return V8DOMWrapper::convertToNativeObject<DOMWindow>(V8ClassIndex::DOMWINDOW, global);
}

Frame* V8Proxy::retrieveFrame(v8::Handle<v8::Context> context)
{
    return retrieveWindow(context)->frame();
}

Frame* V8Proxy::retrieveFrameForEnteredContext()
{
    v8::Handle<v8::Context> context = v8::Context::GetEntered();
    if (context.IsEmpty())
        return 0;
    return retrieveFrame(context);
}

Frame* V8Proxy::retrieveFrameForCurrentContext()
{
    v8::Handle<v8::Context> context = v8::Context::GetCurrent();
    if (context.IsEmpty())
        return 0;
    return retrieveFrame(context);
}

Frame* V8Proxy::retrieveFrameForCallingContext()
{
    v8::Handle<v8::Context> context = v8::Context::GetCalling();
    if (context.IsEmpty())
        return 0;
    return retrieveFrame(context);
}

Frame* V8Proxy::retrieveFrame()
{
    DOMWindow* window = retrieveWindow();
    return window ? window->frame() : 0;
}

V8Proxy* V8Proxy::retrieve()
{
    DOMWindow* window = retrieveWindow();
    ASSERT(window);
    return retrieve(window->frame());
}

V8Proxy* V8Proxy::retrieve(Frame* frame)
{
    if (!frame)
        return 0;
    return frame->script()->isEnabled() ? frame->script()->proxy() : 0;
}

V8Proxy* V8Proxy::retrieve(ScriptExecutionContext* context)
{
    if (!context->isDocument())
        return 0;
    return retrieve(static_cast<Document*>(context)->frame());
}

void V8Proxy::disconnectFrame()
{
    disconnectEventListeners();
}

bool V8Proxy::isEnabled()
{
    Settings* settings = m_frame->settings();
    if (!settings)
        return false;

    // In the common case, JavaScript is enabled and we're done.
    if (settings->isJavaScriptEnabled())
        return true;

    // If JavaScript has been disabled, we need to look at the frame to tell
    // whether this script came from the web or the embedder. Scripts from the
    // embedder are safe to run, but scripts from the other sources are
    // disallowed.
    Document* document = m_frame->document();
    if (!document)
        return false;

    SecurityOrigin* origin = document->securityOrigin();
    if (origin->protocol().isEmpty())
        return false; // Uninitialized document

    if (origin->protocol() == "http" || origin->protocol() == "https")
        return false; // Web site

    // FIXME: the following are application decisions, and they should
    // not be made at this layer. instead, we should bridge out to the
    // embedder to allow them to override policy here.

    if (origin->protocol() == ChromiumBridge::uiResourceProtocol())
        return true;   // Embedder's scripts are ok to run

    // If the scheme is ftp: or file:, an empty file name indicates a directory
    // listing, which requires JavaScript to function properly.
    const char* kDirProtocols[] = { "ftp", "file" };
    for (size_t i = 0; i < arraysize(kDirProtocols); ++i) {
        if (origin->protocol() == kDirProtocols[i]) {
            const KURL& url = document->url();
            return url.pathAfterLastSlash() == url.pathEnd();
        }
    }

    return false; // Other protocols fall through to here
}

void V8Proxy::updateDocumentWrapper(v8::Handle<v8::Value> wrapper)
{
    clearDocumentWrapper();

    ASSERT(m_document.IsEmpty());
    m_document = v8::Persistent<v8::Value>::New(wrapper);
#ifndef NDEBUG
    V8GCController::registerGlobalHandle(PROXY, this, m_document);
#endif
}

void V8Proxy::clearDocumentWrapper()
{
    if (!m_document.IsEmpty()) {
#ifndef NDEBUG
        V8GCController::unregisterGlobalHandle(this, m_document);
#endif
        m_document.Dispose();
        m_document.Clear();
    }
}

void V8Proxy::updateDocumentWrapperCache()
{
    v8::HandleScope handleScope;
    v8::Context::Scope contextScope(context());

    // If the document has no frame, NodeToV8Object might get the
    // document wrapper for a document that is about to be deleted.
    // If the ForceSet below causes a garbage collection, the document
    // might get deleted and the global handle for the document
    // wrapper cleared. Using the cleared global handle will lead to
    // crashes. In this case we clear the cache and let the DOMWindow
    // accessor handle access to the document.
    if (!m_frame->document()->frame()) {
        clearDocumentWrapperCache();
        return;
    }

    v8::Handle<v8::Value> documentWrapper = V8DOMWrapper::convertNodeToV8Object(m_frame->document());

    // If instantiation of the document wrapper fails, clear the cache
    // and let the DOMWindow accessor handle access to the document.
    if (documentWrapper.IsEmpty()) {
        clearDocumentWrapperCache();
        return;
    }
    m_context->Global()->ForceSet(v8::String::New("document"), documentWrapper, static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontDelete));
}

void V8Proxy::clearDocumentWrapperCache()
{
    ASSERT(!m_context.IsEmpty());
    m_context->Global()->ForceDelete(v8::String::New("document"));
}

void V8Proxy::disposeContextHandles()
{
    if (!m_context.IsEmpty()) {
        m_frame->loader()->client()->didDestroyScriptContext();
        m_context.Dispose();
        m_context.Clear();
    }

    if (!m_wrapperBoilerplates.IsEmpty()) {
#ifndef NDEBUG
        V8GCController::unregisterGlobalHandle(this, m_wrapperBoilerplates);
#endif
        m_wrapperBoilerplates.Dispose();
        m_wrapperBoilerplates.Clear();
    }

    if (!m_objectPrototype.IsEmpty()) {
#ifndef NDEBUG
        V8GCController::unregisterGlobalHandle(this, m_objectPrototype);
#endif
        m_objectPrototype.Dispose();
        m_objectPrototype.Clear();
    }
}

void V8Proxy::clearForClose()
{
    if (!m_context.IsEmpty()) {
        v8::HandleScope handleScope;

        clearDocumentWrapper();
        disposeContextHandles();
    }
}

void V8Proxy::clearForNavigation()
{
    disconnectEventListeners();

    if (!m_context.IsEmpty()) {
        v8::HandleScope handle;
        clearDocumentWrapper();

        v8::Context::Scope contextScope(m_context);

        // Clear the document wrapper cache before turning on access checks on
        // the old DOMWindow wrapper. This way, access to the document wrapper
        // will be protected by the security checks on the DOMWindow wrapper.
        clearDocumentWrapperCache();

        // Turn on access check on the old DOMWindow wrapper.
        v8::Handle<v8::Object> wrapper = V8DOMWrapper::lookupDOMWrapper(V8ClassIndex::DOMWINDOW, m_global);
        ASSERT(!wrapper.IsEmpty());
        wrapper->TurnOnAccessCheck();

        // Separate the context from its global object.
        m_context->DetachGlobal();

        disposeContextHandles();

        // Reinitialize the context so the global object points to
        // the new DOM window.
        initContextIfNeeded();
    }
}

void V8Proxy::setSecurityToken()
{
    Document* document = m_frame->document();
    // Setup security origin and security token.
    if (!document) {
        m_context->UseDefaultSecurityToken();
        return;
    }

    // Ask the document's SecurityOrigin to generate a security token.
    // If two tokens are equal, then the SecurityOrigins canAccess each other.
    // If two tokens are not equal, then we have to call canAccess.
    // Note: we can't use the HTTPOrigin if it was set from the DOM.
    SecurityOrigin* origin = document->securityOrigin();
    String token;
    if (!origin->domainWasSetInDOM())
        token = document->securityOrigin()->toString();

    // An empty or "null" token means we always have to call
    // canAccess. The toString method on securityOrigins returns the
    // string "null" for empty security origins and for security
    // origins that should only allow access to themselves. In this
    // case, we use the global object as the security token to avoid
    // calling canAccess when a script accesses its own objects.
    if (token.isEmpty() || token == "null") {
        m_context->UseDefaultSecurityToken();
        return;
    }

    CString utf8Token = token.utf8();
    // NOTE: V8 does identity comparison in fast path, must use a symbol
    // as the security token.
    m_context->SetSecurityToken(v8::String::NewSymbol(utf8Token.data(), utf8Token.length()));
}

void V8Proxy::updateDocument()
{
    if (!m_frame->document())
        return;

    if (m_global.IsEmpty()) {
        ASSERT(m_context.IsEmpty());
        return;
    }

    // We have a new document and we need to update the cache.
    updateDocumentWrapperCache();

    updateSecurityOrigin();
}

void V8Proxy::updateSecurityOrigin()
{
    v8::HandleScope scope;
    setSecurityToken();
}

// Same origin policy implementation:
//
// Same origin policy prevents JS code from domain A access JS & DOM objects
// in a different domain B. There are exceptions and several objects are
// accessible by cross-domain code. For example, the window.frames object is
// accessible by code from a different domain, but window.document is not.
//
// The binding code sets security check callbacks on a function template,
// and accessing instances of the template calls the callback function.
// The callback function checks same origin policy.
//
// Callback functions are expensive. V8 uses a security token string to do
// fast access checks for the common case where source and target are in the
// same domain. A security token is a string object that represents
// the protocol/url/port of a domain.
//
// There are special cases where a security token matching is not enough.
// For example, JavaScript can set its domain to a super domain by calling
// document.setDomain(...). In these cases, the binding code can reset
// a context's security token to its global object so that the fast access
// check will always fail.

// Check if the current execution context can access a target frame.
// First it checks same domain policy using the lexical context
//
// This is equivalent to KJS::Window::allowsAccessFrom(ExecState*, String&).
bool V8Proxy::canAccessPrivate(DOMWindow* targetWindow)
{
    ASSERT(targetWindow);

    String message;

    DOMWindow* originWindow = retrieveWindow();
    if (originWindow == targetWindow)
        return true;

    if (!originWindow)
        return false;

    const SecurityOrigin* activeSecurityOrigin = originWindow->securityOrigin();
    const SecurityOrigin* targetSecurityOrigin = targetWindow->securityOrigin();

    // We have seen crashes were the security origin of the target has not been
    // initialized. Defend against that.
    if (!targetSecurityOrigin)
        return false;

    if (activeSecurityOrigin->canAccess(targetSecurityOrigin))
        return true;

    // Allow access to a "about:blank" page if the dynamic context is a
    // detached context of the same frame as the blank page.
    if (targetSecurityOrigin->isEmpty() && originWindow->frame() == targetWindow->frame())
        return true;

    return false;
}

bool V8Proxy::canAccessFrame(Frame* target, bool reportError)
{
    // The subject is detached from a frame, deny accesses.
    if (!target)
        return false;

    if (!canAccessPrivate(target->domWindow())) {
        if (reportError)
            reportUnsafeAccessTo(target, ReportNow);
        return false;
    }
    return true;
}

bool V8Proxy::checkNodeSecurity(Node* node)
{
    if (!node)
        return false;

    Frame* target = node->document()->frame();

    if (!target)
        return false;

    return canAccessFrame(target, true);
}

v8::Persistent<v8::Context> V8Proxy::createNewContext(v8::Handle<v8::Object> global)
{
    v8::Persistent<v8::Context> result;

    // Create a new environment using an empty template for the shadow
    // object. Reuse the global object if one has been created earlier.
    v8::Persistent<v8::ObjectTemplate> globalTemplate = V8DOMWindow::GetShadowObjectTemplate();
    if (globalTemplate.IsEmpty())
        return result;

    // Install a security handler with V8.
    globalTemplate->SetAccessCheckCallbacks(V8Custom::v8DOMWindowNamedSecurityCheck, V8Custom::v8DOMWindowIndexedSecurityCheck, v8::Integer::New(V8ClassIndex::DOMWINDOW));

    // Dynamically tell v8 about our extensions now.
    OwnArrayPtr<const char*> extensionNames(new const char*[m_extensions.size()]);
    int index = 0;
    for (V8ExtensionList::iterator it = m_extensions.begin(); it != m_extensions.end(); ++it) {
        // Note: we check the loader URL here instead of the document URL
        // because we might be currently loading an URL into a blank page.
        // See http://code.google.com/p/chromium/issues/detail?id=10924
        if (it->scheme.length() > 0 && (it->scheme != m_frame->loader()->activeDocumentLoader()->url().protocol() || it->scheme != m_frame->page()->mainFrame()->loader()->activeDocumentLoader()->url().protocol()))
            continue;

        extensionNames[index++] = it->extension->name();
    }
    v8::ExtensionConfiguration extensions(index, extensionNames.get());
    result = v8::Context::New(&extensions, globalTemplate, global);

    return result;
}

bool V8Proxy::installDOMWindow(v8::Handle<v8::Context> context, DOMWindow* window)
{
    v8::Handle<v8::String> implicitProtoString = v8::String::New("__proto__");
    if (implicitProtoString.IsEmpty())
        return false;

    // Create a new JS window object and use it as the prototype for the  shadow global object.
    v8::Handle<v8::Function> windowConstructor = getConstructor(V8ClassIndex::DOMWINDOW);
    v8::Local<v8::Object> jsWindow = SafeAllocation::newInstance(windowConstructor);
    // Bail out if allocation failed.
    if (jsWindow.IsEmpty())
        return false;

    // Wrap the window.
    V8DOMWrapper::setDOMWrapper(jsWindow, V8ClassIndex::ToInt(V8ClassIndex::DOMWINDOW), window);

    window->ref();
    V8DOMWrapper::setJSWrapperForDOMObject(window, v8::Persistent<v8::Object>::New(jsWindow));

    // Insert the window instance as the prototype of the shadow object.
    v8::Handle<v8::Object> v8Global = context->Global();
    v8Global->Set(implicitProtoString, jsWindow);
    return true;
}

// Create a new environment and setup the global object.
//
// The global object corresponds to a DOMWindow instance. However, to
// allow properties of the JS DOMWindow instance to be shadowed, we
// use a shadow object as the global object and use the JS DOMWindow
// instance as the prototype for that shadow object. The JS DOMWindow
// instance is undetectable from javascript code because the __proto__
// accessors skip that object.
//
// The shadow object and the DOMWindow instance are seen as one object
// from javascript. The javascript object that corresponds to a
// DOMWindow instance is the shadow object. When mapping a DOMWindow
// instance to a V8 object, we return the shadow object.
//
// To implement split-window, see
//   1) https://bugs.webkit.org/show_bug.cgi?id=17249
//   2) https://wiki.mozilla.org/Gecko:SplitWindow
//   3) https://bugzilla.mozilla.org/show_bug.cgi?id=296639
// we need to split the shadow object further into two objects:
// an outer window and an inner window. The inner window is the hidden
// prototype of the outer window. The inner window is the default
// global object of the context. A variable declared in the global
// scope is a property of the inner window.
//
// The outer window sticks to a Frame, it is exposed to JavaScript
// via window.window, window.self, window.parent, etc. The outer window
// has a security token which is the domain. The outer window cannot
// have its own properties. window.foo = 'x' is delegated to the
// inner window.
//
// When a frame navigates to a new page, the inner window is cut off
// the outer window, and the outer window identify is preserved for
// the frame. However, a new inner window is created for the new page.
// If there are JS code holds a closure to the old inner window,
// it won't be able to reach the outer window via its global object.
void V8Proxy::initContextIfNeeded()
{
    // Bail out if the context has already been initialized.
    if (!m_context.IsEmpty())
        return;

    // Create a handle scope for all local handles.
    v8::HandleScope handleScope;

    // Setup the security handlers and message listener. This only has
    // to be done once.
    static bool isV8Initialized = false;
    if (!isV8Initialized) {
        // Tells V8 not to call the default OOM handler, binding code
        // will handle it.
        v8::V8::IgnoreOutOfMemoryException();
        v8::V8::SetFatalErrorHandler(reportFatalErrorInV8);

        v8::V8::SetGlobalGCPrologueCallback(&V8GCController::gcPrologue);
        v8::V8::SetGlobalGCEpilogueCallback(&V8GCController::gcEpilogue);

        v8::V8::AddMessageListener(handleConsoleMessage);

        v8::V8::SetFailedAccessCheckCallbackFunction(reportUnsafeJavaScriptAccess);

        isV8Initialized = true;
    }

    m_context = createNewContext(m_global);
    if (m_context.IsEmpty())
        return;

    // Starting from now, use local context only.
    v8::Local<v8::Context> v8Context = context();
    v8::Context::Scope contextScope(v8Context);

    // Store the first global object created so we can reuse it.
    if (m_global.IsEmpty()) {
        m_global = v8::Persistent<v8::Object>::New(v8Context->Global());
        // Bail out if allocation of the first global objects fails.
        if (m_global.IsEmpty()) {
            disposeContextHandles();
            return;
        }
#ifndef NDEBUG
        V8GCController::registerGlobalHandle(PROXY, this, m_global);
#endif
    }

    // Allocate strings used during initialization.
    v8::Handle<v8::String> objectString = v8::String::New("Object");
    v8::Handle<v8::String> prototypeString = v8::String::New("prototype");
    // Bail out if allocation failed.
    if (objectString.IsEmpty() || prototypeString.IsEmpty()) {
        disposeContextHandles();
        return;
    }

    // Allocate clone cache and pre-allocated objects
    v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(m_global->Get(objectString));
    m_objectPrototype = v8::Persistent<v8::Value>::New(object->Get(prototypeString));
    m_wrapperBoilerplates = v8::Persistent<v8::Array>::New(v8::Array::New(V8ClassIndex::WRAPPER_TYPE_COUNT));
    // Bail out if allocation failed.
    if (m_objectPrototype.IsEmpty()) {
        disposeContextHandles();
        return;
    }
#ifndef NDEBUG
    V8GCController::registerGlobalHandle(PROXY, this, m_objectPrototype);
    V8GCController::registerGlobalHandle(PROXY, this, m_wrapperBoilerplates);
#endif

    if (!installDOMWindow(v8Context, m_frame->domWindow()))
        disposeContextHandles();

    updateDocument();

    setSecurityToken();

    m_frame->loader()->client()->didCreateScriptContext();
    m_frame->loader()->dispatchWindowObjectAvailable();
}

template <class T>
void setDOMExceptionHelper(V8ClassIndex::V8WrapperType type, PassRefPtr<T> exception)
{
    v8::Handle<v8::Value> v8Exception;
    if (WorkerContextExecutionProxy::retrieve())
        v8Exception = WorkerContextExecutionProxy::ToV8Object(type, exception.get());
    else
        v8Exception = V8DOMWrapper::convertToV8Object(type, exception.get());

    v8::ThrowException(v8Exception);
}

void V8Proxy::setDOMException(int exceptionCode)
{
    if (exceptionCode <= 0)
        return;

    ExceptionCodeDescription description;
    getExceptionCodeDescription(exceptionCode, description);

    v8::Handle<v8::Value> exception;
    switch (description.type) {
    case DOMExceptionType:
        setDOMExceptionHelper(V8ClassIndex::DOMCOREEXCEPTION, DOMCoreException::create(description));
        break;
    case RangeExceptionType:
        setDOMExceptionHelper(V8ClassIndex::RANGEEXCEPTION, RangeException::create(description));
        break;
    case EventExceptionType:
        setDOMExceptionHelper(V8ClassIndex::EVENTEXCEPTION, EventException::create(description));
        break;
    case XMLHttpRequestExceptionType:
        setDOMExceptionHelper(V8ClassIndex::XMLHTTPREQUESTEXCEPTION, XMLHttpRequestException::create(description));
        break;
#if ENABLE(SVG)
    case SVGExceptionType:
        setDOMExceptionHelper(V8ClassIndex::SVGEXCEPTION, SVGException::create(description));
        break;
#endif
#if ENABLE(XPATH)
    case XPathExceptionType:
        setDOMExceptionHelper(V8ClassIndex::XPATHEXCEPTION, XPathException::create(description));
        break;
#endif
    default:
        ASSERT_NOT_REACHED();
        break;
    }
}

v8::Handle<v8::Value> V8Proxy::throwError(ErrorType type, const char* message)
{
    switch (type) {
    case RangeError:
        return v8::ThrowException(v8::Exception::RangeError(v8String(message)));
    case ReferenceError:
        return v8::ThrowException(v8::Exception::ReferenceError(v8String(message)));
    case SyntaxError:
        return v8::ThrowException(v8::Exception::SyntaxError(v8String(message)));
    case TypeError:
        return v8::ThrowException(v8::Exception::TypeError(v8String(message)));
    case GeneralError:
        return v8::ThrowException(v8::Exception::Error(v8String(message)));
    default:
        ASSERT_NOT_REACHED();
        return notHandledByInterceptor();
    }
}

v8::Local<v8::Context> V8Proxy::context(Frame* frame)
{
    V8Proxy* proxy = retrieve(frame);
    if (!proxy)
        return v8::Local<v8::Context>();

    proxy->initContextIfNeeded();
    return proxy->context();
}

v8::Local<v8::Context> V8Proxy::currentContext()
{
    return v8::Context::GetCurrent();
}

v8::Handle<v8::Value> V8DOMWrapper::convertToV8Object(V8ClassIndex::V8WrapperType type, void* impl)
{
    ASSERT(type != V8ClassIndex::EVENTLISTENER);
    ASSERT(type != V8ClassIndex::EVENTTARGET);
    ASSERT(type != V8ClassIndex::EVENT);

    bool isActiveDomObject = false;
    switch (type) {
#define MAKE_CASE(TYPE, NAME) case V8ClassIndex::TYPE:
        DOM_NODE_TYPES(MAKE_CASE)
#if ENABLE(SVG)
        SVG_NODE_TYPES(MAKE_CASE)
#endif
        return convertNodeToV8Object(static_cast<Node*>(impl));
    case V8ClassIndex::CSSVALUE:
        return convertCSSValueToV8Object(static_cast<CSSValue*>(impl));
    case V8ClassIndex::CSSRULE:
        return convertCSSRuleToV8Object(static_cast<CSSRule*>(impl));
    case V8ClassIndex::STYLESHEET:
        return convertStyleSheetToV8Object(static_cast<StyleSheet*>(impl));
    case V8ClassIndex::DOMWINDOW:
        return convertWindowToV8Object(static_cast<DOMWindow*>(impl));
#if ENABLE(SVG)
        SVG_NONNODE_TYPES(MAKE_CASE)
        if (type == V8ClassIndex::SVGELEMENTINSTANCE)
            return convertSVGElementInstanceToV8Object(static_cast<SVGElementInstance*>(impl));
        return convertSVGObjectWithContextToV8Object(type, impl);
#endif

        ACTIVE_DOM_OBJECT_TYPES(MAKE_CASE)
        isActiveDomObject = true;
        break;
    default:
        break;
  }

#undef MAKE_CASE

    if (!impl)
        return v8::Null();

    // Non DOM node
    v8::Persistent<v8::Object> result = isActiveDomObject ? getActiveDOMObjectMap().get(impl) : getDOMObjectMap().get(impl);
    if (result.IsEmpty()) {
        v8::Local<v8::Object> v8Object = instantiateV8Object(type, type, impl);
        if (!v8Object.IsEmpty()) {
            // Go through big switch statement, it has some duplications
            // that were handled by code above (such as CSSVALUE, CSSRULE, etc).
            switch (type) {
#define MAKE_CASE(TYPE, NAME) \
            case V8ClassIndex::TYPE: static_cast<NAME*>(impl)->ref(); break;
                DOM_OBJECT_TYPES(MAKE_CASE)
#undef MAKE_CASE
            default:
                ASSERT_NOT_REACHED();
            }
            result = v8::Persistent<v8::Object>::New(v8Object);
            if (isActiveDomObject)
                setJSWrapperForActiveDOMObject(impl, result);
            else
                setJSWrapperForDOMObject(impl, result);

            // Special case for non-node objects associated with a
            // DOMWindow. Both Safari and FF let the JS wrappers for these
            // objects survive GC. To mimic their behavior, V8 creates
            // hidden references from the DOMWindow to these wrapper
            // objects. These references get cleared when the DOMWindow is
            // reused by a new page.
            switch (type) {
            case V8ClassIndex::CONSOLE:
                setHiddenWindowReference(static_cast<Console*>(impl)->frame(), V8Custom::kDOMWindowConsoleIndex, result);
                break;
            case V8ClassIndex::HISTORY:
                setHiddenWindowReference(static_cast<History*>(impl)->frame(), V8Custom::kDOMWindowHistoryIndex, result);
                break;
            case V8ClassIndex::NAVIGATOR:
                setHiddenWindowReference(static_cast<Navigator*>(impl)->frame(), V8Custom::kDOMWindowNavigatorIndex, result);
                break;
            case V8ClassIndex::SCREEN:
                setHiddenWindowReference(static_cast<Screen*>(impl)->frame(), V8Custom::kDOMWindowScreenIndex, result);
                break;
            case V8ClassIndex::LOCATION:
                setHiddenWindowReference(static_cast<Location*>(impl)->frame(), V8Custom::kDOMWindowLocationIndex, result);
                break;
            case V8ClassIndex::DOMSELECTION:
                setHiddenWindowReference(static_cast<DOMSelection*>(impl)->frame(), V8Custom::kDOMWindowDOMSelectionIndex, result);
                break;
            case V8ClassIndex::BARINFO: {
                BarInfo* barInfo = static_cast<BarInfo*>(impl);
                Frame* frame = barInfo->frame();
                switch (barInfo->type()) {
                case BarInfo::Locationbar:
                    setHiddenWindowReference(frame, V8Custom::kDOMWindowLocationbarIndex, result);
                    break;
                case BarInfo::Menubar:
                    setHiddenWindowReference(frame, V8Custom::kDOMWindowMenubarIndex, result);
                    break;
                case BarInfo::Personalbar:
                    setHiddenWindowReference(frame, V8Custom::kDOMWindowPersonalbarIndex, result);
                    break;
                case BarInfo::Scrollbars:
                    setHiddenWindowReference(frame, V8Custom::kDOMWindowScrollbarsIndex, result);
                    break;
                case BarInfo::Statusbar:
                    setHiddenWindowReference(frame, V8Custom::kDOMWindowStatusbarIndex, result);
                    break;
                case BarInfo::Toolbar:
                    setHiddenWindowReference(frame, V8Custom::kDOMWindowToolbarIndex, result);
                    break;
                }
                break;
            }
            default:
                break;
            }
        }
    }
    return result;
}

void V8DOMWrapper::setHiddenWindowReference(Frame* frame, const int internalIndex, v8::Handle<v8::Object> jsObject)
{
    // Get DOMWindow
    if (!frame)
        return; // Object might be detached from window
    v8::Handle<v8::Context> v8Context = V8Proxy::context(frame);
    if (v8Context.IsEmpty())
        return;

    ASSERT(internalIndex < V8Custom::kDOMWindowInternalFieldCount);

    v8::Handle<v8::Object> global = v8Context->Global();
    // Look for real DOM wrapper.
    global = V8DOMWrapper::lookupDOMWrapper(V8ClassIndex::DOMWINDOW, global);
    ASSERT(!global.IsEmpty());
    ASSERT(global->GetInternalField(internalIndex)->IsUndefined());
    global->SetInternalField(internalIndex, jsObject);
}

V8ClassIndex::V8WrapperType V8DOMWrapper::domWrapperType(v8::Handle<v8::Object> object)
{
    ASSERT(V8DOMWrapper::maybeDOMWrapper(object));
    v8::Handle<v8::Value> type = object->GetInternalField(V8Custom::kDOMWrapperTypeIndex);
    return V8ClassIndex::FromInt(type->Int32Value());
}

void* V8DOMWrapper::convertToNativeObjectImpl(V8ClassIndex::V8WrapperType type, v8::Handle<v8::Value> object)
{
    // Native event listener is per frame, it cannot be handled by this generic function.
    ASSERT(type != V8ClassIndex::EVENTLISTENER);
    ASSERT(type != V8ClassIndex::EVENTTARGET);

    ASSERT(V8DOMWrapper::maybeDOMWrapper(object));

    switch (type) {
#define MAKE_CASE(TYPE, NAME) case V8ClassIndex::TYPE:
        DOM_NODE_TYPES(MAKE_CASE)
#if ENABLE(SVG)
        SVG_NODE_TYPES(MAKE_CASE)
#endif
        ASSERT_NOT_REACHED();
        return 0;
    case V8ClassIndex::XMLHTTPREQUEST:
        return convertDOMWrapperToNative<XMLHttpRequest>(object);
    case V8ClassIndex::EVENT:
        return convertDOMWrapperToNative<Event>(object);
    case V8ClassIndex::CSSRULE:
        return convertDOMWrapperToNative<CSSRule>(object);
    default:
        break;
    }
#undef MAKE_CASE

    return convertDOMWrapperToNative<void>(object);
}


void* V8DOMWrapper::convertToSVGPODTypeImpl(V8ClassIndex::V8WrapperType type, v8::Handle<v8::Value> object)
{
    return isWrapperOfType(object, type) ? convertDOMWrapperToNative<void>(object) : 0;
}

v8::Handle<v8::Object> V8DOMWrapper::lookupDOMWrapper(V8ClassIndex::V8WrapperType type, v8::Handle<v8::Value> value)
{
    if (value.IsEmpty())
        return notHandledByInterceptor();

    v8::Handle<v8::FunctionTemplate> descriptor = V8DOMWrapper::getTemplate(type);
    while (value->IsObject()) {
        v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(value);
        if (descriptor->HasInstance(object))
            return object;

        value = object->GetPrototype();
    }
    return notHandledByInterceptor();
} 

void* V8DOMWrapper::convertDOMWrapperToNodeHelper(v8::Handle<v8::Value> value)
{
    ASSERT(maybeDOMWrapper(value));

    v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(value);

    ASSERT(domWrapperType(object) == V8ClassIndex::NODE);

    v8::Handle<v8::Value> wrapper = object->GetInternalField(V8Custom::kDOMWrapperObjectIndex);
    return extractCPointer<Node>(wrapper);
}

PassRefPtr<NodeFilter> V8DOMWrapper::wrapNativeNodeFilter(v8::Handle<v8::Value> filter)
{
    // A NodeFilter is used when walking through a DOM tree or iterating tree
    // nodes.
    // FIXME: we may want to cache NodeFilterCondition and NodeFilter
    // object, but it is minor.
    // NodeFilter is passed to NodeIterator that has a ref counted pointer
    // to NodeFilter. NodeFilter has a ref counted pointer to NodeFilterCondition.
    // In NodeFilterCondition, filter object is persisted in its constructor,
    // and disposed in its destructor.
    if (!filter->IsFunction())
        return 0;

    NodeFilterCondition* condition = new V8NodeFilterCondition(filter);
    return NodeFilter::create(condition);
}

v8::Local<v8::Object> V8DOMWrapper::instantiateV8Object(V8Proxy* proxy, V8ClassIndex::V8WrapperType descriptorType, V8ClassIndex::V8WrapperType cptrType, void* impl)
{
    // Make a special case for document.all
    if (descriptorType == V8ClassIndex::HTMLCOLLECTION && static_cast<HTMLCollection*>(impl)->type() == DocAll)
        descriptorType = V8ClassIndex::UNDETECTABLEHTMLCOLLECTION;

    if (!proxy)
        V8Proxy* proxy = V8Proxy::retrieve();
    v8::Local<v8::Object> instance;
    if (proxy)
        instance = proxy->createWrapperFromCache(descriptorType);
    else {
        v8::Local<v8::Function> function = V8DOMWrapper::getTemplate(descriptorType)->GetFunction();
        instance = SafeAllocation::newInstance(function);
    }
    if (!instance.IsEmpty()) {
        // Avoid setting the DOM wrapper for failed allocations.
        V8DOMWrapper::setDOMWrapper(instance, V8ClassIndex::ToInt(cptrType), impl);
    }
    return instance;
}

v8::Handle<v8::Value> V8Proxy::checkNewLegal(const v8::Arguments& args)
{
    if (!AllowAllocation::m_current)
        return throwError(TypeError, "Illegal constructor");

    return args.This();
}

void V8DOMWrapper::setDOMWrapper(v8::Handle<v8::Object> object, int type, void* cptr)
{
    ASSERT(object->InternalFieldCount() >= 2);
    object->SetInternalField(V8Custom::kDOMWrapperObjectIndex, wrapCPointer(cptr));
    object->SetInternalField(V8Custom::kDOMWrapperTypeIndex, v8::Integer::New(type));
}

#ifndef NDEBUG
bool V8DOMWrapper::maybeDOMWrapper(v8::Handle<v8::Value> value)
{
    if (value.IsEmpty() || !value->IsObject())
        return false;

    v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(value);
    if (!object->InternalFieldCount())
        return false;

    ASSERT(object->InternalFieldCount() >= V8Custom::kDefaultWrapperInternalFieldCount);

    v8::Handle<v8::Value> type = object->GetInternalField(V8Custom::kDOMWrapperTypeIndex);
    ASSERT(type->IsInt32());
    ASSERT(V8ClassIndex::INVALID_CLASS_INDEX < type->Int32Value() && type->Int32Value() < V8ClassIndex::CLASSINDEX_END);

    v8::Handle<v8::Value> wrapper = object->GetInternalField(V8Custom::kDOMWrapperObjectIndex);
    ASSERT(wrapper->IsNumber() || wrapper->IsExternal());

    return true;
}
#endif

bool V8DOMWrapper::isDOMEventWrapper(v8::Handle<v8::Value> value)
{
    // All kinds of events use EVENT as dom type in JS wrappers.
    // See EventToV8Object
    return isWrapperOfType(value, V8ClassIndex::EVENT);
}

bool V8DOMWrapper::isWrapperOfType(v8::Handle<v8::Value> value, V8ClassIndex::V8WrapperType classType)
{
    if (value.IsEmpty() || !value->IsObject())
        return false;

    v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(value);
    if (!object->InternalFieldCount())
        return false;

    ASSERT(object->InternalFieldCount() >= V8Custom::kDefaultWrapperInternalFieldCount);

    v8::Handle<v8::Value> wrapper = object->GetInternalField(V8Custom::kDOMWrapperObjectIndex);
    ASSERT(wrapper->IsNumber() || wrapper->IsExternal());

    v8::Handle<v8::Value> type = object->GetInternalField(V8Custom::kDOMWrapperTypeIndex);
    ASSERT(type->IsInt32());
    ASSERT(V8ClassIndex::INVALID_CLASS_INDEX < type->Int32Value() && type->Int32Value() < V8ClassIndex::CLASSINDEX_END);

    return V8ClassIndex::FromInt(type->Int32Value()) == classType;
}

#if ENABLE(VIDEO)
#define FOR_EACH_VIDEO_TAG(macro)                  \
    macro(audio, AUDIO)                            \
    macro(source, SOURCE)                          \
    macro(video, VIDEO)
#else
#define FOR_EACH_VIDEO_TAG(macro)
#endif

#define FOR_EACH_TAG(macro)                        \
    macro(a, ANCHOR)                               \
    macro(applet, APPLET)                          \
    macro(area, AREA)                              \
    macro(base, BASE)                              \
    macro(basefont, BASEFONT)                      \
    macro(blockquote, BLOCKQUOTE)                  \
    macro(body, BODY)                              \
    macro(br, BR)                                  \
    macro(button, BUTTON)                          \
    macro(caption, TABLECAPTION)                   \
    macro(col, TABLECOL)                           \
    macro(colgroup, TABLECOL)                      \
    macro(del, MOD)                                \
    macro(canvas, CANVAS)                          \
    macro(dir, DIRECTORY)                          \
    macro(div, DIV)                                \
    macro(dl, DLIST)                               \
    macro(embed, EMBED)                            \
    macro(fieldset, FIELDSET)                      \
    macro(font, FONT)                              \
    macro(form, FORM)                              \
    macro(frame, FRAME)                            \
    macro(frameset, FRAMESET)                      \
    macro(h1, HEADING)                             \
    macro(h2, HEADING)                             \
    macro(h3, HEADING)                             \
    macro(h4, HEADING)                             \
    macro(h5, HEADING)                             \
    macro(h6, HEADING)                             \
    macro(head, HEAD)                              \
    macro(hr, HR)                                  \
    macro(html, HTML)                              \
    macro(img, IMAGE)                              \
    macro(iframe, IFRAME)                          \
    macro(image, IMAGE)                            \
    macro(input, INPUT)                            \
    macro(ins, MOD)                                \
    macro(isindex, ISINDEX)                        \
    macro(keygen, SELECT)                          \
    macro(label, LABEL)                            \
    macro(legend, LEGEND)                          \
    macro(li, LI)                                  \
    macro(link, LINK)                              \
    macro(listing, PRE)                            \
    macro(map, MAP)                                \
    macro(marquee, MARQUEE)                        \
    macro(menu, MENU)                              \
    macro(meta, META)                              \
    macro(object, OBJECT)                          \
    macro(ol, OLIST)                               \
    macro(optgroup, OPTGROUP)                      \
    macro(option, OPTION)                          \
    macro(p, PARAGRAPH)                            \
    macro(param, PARAM)                            \
    macro(pre, PRE)                                \
    macro(q, QUOTE)                                \
    macro(script, SCRIPT)                          \
    macro(select, SELECT)                          \
    macro(style, STYLE)                            \
    macro(table, TABLE)                            \
    macro(thead, TABLESECTION)                     \
    macro(tbody, TABLESECTION)                     \
    macro(tfoot, TABLESECTION)                     \
    macro(td, TABLECELL)                           \
    macro(th, TABLECELL)                           \
    macro(tr, TABLEROW)                            \
    macro(textarea, TEXTAREA)                      \
    macro(title, TITLE)                            \
    macro(ul, ULIST)                               \
    macro(xmp, PRE)

V8ClassIndex::V8WrapperType V8DOMWrapper::htmlElementType(HTMLElement* element)
{
    typedef HashMap<String, V8ClassIndex::V8WrapperType> WrapperTypeMap;
    DEFINE_STATIC_LOCAL(WrapperTypeMap, wrapperTypeMap, ());
    if (wrapperTypeMap.isEmpty()) {
#define ADD_TO_HASH_MAP(tag, name) \
        wrapperTypeMap.set(#tag, V8ClassIndex::HTML##name##ELEMENT);
        FOR_EACH_TAG(ADD_TO_HASH_MAP)
#if ENABLE(VIDEO)
        if (MediaPlayer::isAvailable()) {
            FOR_EACH_VIDEO_TAG(ADD_TO_HASH_MAP)
        }
#endif
#undef ADD_TO_HASH_MAP
    }

    V8ClassIndex::V8WrapperType type = wrapperTypeMap.get(element->localName().impl());
    if (!type)
        return V8ClassIndex::HTMLELEMENT;
    return type;
}
#undef FOR_EACH_TAG

#if ENABLE(SVG)

#if ENABLE(SVG_ANIMATION)
#define FOR_EACH_ANIMATION_TAG(macro) \
    macro(animateColor, ANIMATECOLOR) \
    macro(animate, ANIMATE) \
    macro(animateTransform, ANIMATETRANSFORM) \
    macro(set, SET)
#else
#define FOR_EACH_ANIMATION_TAG(macro)
#endif

#if ENABLE(SVG_FILTERS)
#define FOR_EACH_FILTERS_TAG(macro) \
    macro(feBlend, FEBLEND) \
    macro(feColorMatrix, FECOLORMATRIX) \
    macro(feComponentTransfer, FECOMPONENTTRANSFER) \
    macro(feComposite, FECOMPOSITE) \
    macro(feDiffuseLighting, FEDIFFUSELIGHTING) \
    macro(feDisplacementMap, FEDISPLACEMENTMAP) \
    macro(feDistantLight, FEDISTANTLIGHT) \
    macro(feFlood, FEFLOOD) \
    macro(feFuncA, FEFUNCA) \
    macro(feFuncB, FEFUNCB) \
    macro(feFuncG, FEFUNCG) \
    macro(feFuncR, FEFUNCR) \
    macro(feGaussianBlur, FEGAUSSIANBLUR) \
    macro(feImage, FEIMAGE) \
    macro(feMerge, FEMERGE) \
    macro(feMergeNode, FEMERGENODE) \
    macro(feOffset, FEOFFSET) \
    macro(fePointLight, FEPOINTLIGHT) \
    macro(feSpecularLighting, FESPECULARLIGHTING) \
    macro(feSpotLight, FESPOTLIGHT) \
    macro(feTile, FETILE) \
    macro(feTurbulence, FETURBULENCE) \
    macro(filter, FILTER)
#else
#define FOR_EACH_FILTERS_TAG(macro)
#endif

#if ENABLE(SVG_FONTS)
#define FOR_EACH_FONTS_TAG(macro) \
    macro(definition-src, DEFINITIONSRC) \
    macro(font-face, FONTFACE) \
    macro(font-face-format, FONTFACEFORMAT) \
    macro(font-face-name, FONTFACENAME) \
    macro(font-face-src, FONTFACESRC) \
    macro(font-face-uri, FONTFACEURI)
#else
#define FOR_EACH_FONTS_TAG(marco)
#endif

#if ENABLE(SVG_FOREIGN_OBJECT)
#define FOR_EACH_FOREIGN_OBJECT_TAG(macro) \
    macro(foreignObject, FOREIGNOBJECT)
#else
#define FOR_EACH_FOREIGN_OBJECT_TAG(macro)
#endif

#if ENABLE(SVG_USE)
#define FOR_EACH_USE_TAG(macro) \
    macro(use, USE)
#else
#define FOR_EACH_USE_TAG(macro)
#endif

#define FOR_EACH_TAG(macro) \
    FOR_EACH_ANIMATION_TAG(macro) \
    FOR_EACH_FILTERS_TAG(macro) \
    FOR_EACH_FONTS_TAG(macro) \
    FOR_EACH_FOREIGN_OBJECT_TAG(macro) \
    FOR_EACH_USE_TAG(macro) \
    macro(a, A) \
    macro(altGlyph, ALTGLYPH) \
    macro(circle, CIRCLE) \
    macro(clipPath, CLIPPATH) \
    macro(cursor, CURSOR) \
    macro(defs, DEFS) \
    macro(desc, DESC) \
    macro(ellipse, ELLIPSE) \
    macro(g, G) \
    macro(glyph, GLYPH) \
    macro(image, IMAGE) \
    macro(linearGradient, LINEARGRADIENT) \
    macro(line, LINE) \
    macro(marker, MARKER) \
    macro(mask, MASK) \
    macro(metadata, METADATA) \
    macro(path, PATH) \
    macro(pattern, PATTERN) \
    macro(polyline, POLYLINE) \
    macro(polygon, POLYGON) \
    macro(radialGradient, RADIALGRADIENT) \
    macro(rect, RECT) \
    macro(script, SCRIPT) \
    macro(stop, STOP) \
    macro(style, STYLE) \
    macro(svg, SVG) \
    macro(switch, SWITCH) \
    macro(symbol, SYMBOL) \
    macro(text, TEXT) \
    macro(textPath, TEXTPATH) \
    macro(title, TITLE) \
    macro(tref, TREF) \
    macro(tspan, TSPAN) \
    macro(view, VIEW) \
    // end of macro

V8ClassIndex::V8WrapperType V8DOMWrapper::svgElementType(SVGElement* element)
{
    typedef HashMap<String, V8ClassIndex::V8WrapperType> WrapperTypeMap;
    DEFINE_STATIC_LOCAL(WrapperTypeMap, wrapperTypeMap, ());
    if (wrapperTypeMap.isEmpty()) {
#define ADD_TO_HASH_MAP(tag, name) \
        wrapperTypeMap.set(#tag, V8ClassIndex::SVG##name##ELEMENT);
        FOR_EACH_TAG(ADD_TO_HASH_MAP)
#undef ADD_TO_HASH_MAP
    }

    V8ClassIndex::V8WrapperType type = wrapperTypeMap.get(element->localName().impl());
    if (!type)
        return V8ClassIndex::SVGELEMENT;
    return type;
}
#undef FOR_EACH_TAG

#endif // ENABLE(SVG)

v8::Handle<v8::Value> V8DOMWrapper::convertEventToV8Object(Event* event)
{
    if (!event)
        return v8::Null();

    v8::Handle<v8::Object> wrapper = getDOMObjectMap().get(event);
    if (!wrapper.IsEmpty())
        return wrapper;

    V8ClassIndex::V8WrapperType type = V8ClassIndex::EVENT;

    if (event->isUIEvent()) {
        if (event->isKeyboardEvent())
            type = V8ClassIndex::KEYBOARDEVENT;
        else if (event->isTextEvent())
            type = V8ClassIndex::TEXTEVENT;
        else if (event->isMouseEvent())
            type = V8ClassIndex::MOUSEEVENT;
        else if (event->isWheelEvent())
            type = V8ClassIndex::WHEELEVENT;
#if ENABLE(SVG)
        else if (event->isSVGZoomEvent())
            type = V8ClassIndex::SVGZOOMEVENT;
#endif
        else
            type = V8ClassIndex::UIEVENT;
    } else if (event->isMutationEvent())
        type = V8ClassIndex::MUTATIONEVENT;
    else if (event->isOverflowEvent())
        type = V8ClassIndex::OVERFLOWEVENT;
    else if (event->isMessageEvent())
        type = V8ClassIndex::MESSAGEEVENT;
    else if (event->isProgressEvent()) {
        if (event->isXMLHttpRequestProgressEvent())
            type = V8ClassIndex::XMLHTTPREQUESTPROGRESSEVENT;
        else
            type = V8ClassIndex::PROGRESSEVENT;
    } else if (event->isWebKitAnimationEvent())
        type = V8ClassIndex::WEBKITANIMATIONEVENT;
    else if (event->isWebKitTransitionEvent())
        type = V8ClassIndex::WEBKITTRANSITIONEVENT;


    v8::Handle<v8::Object> result = instantiateV8Object(type, V8ClassIndex::EVENT, event);
    if (result.IsEmpty()) {
        // Instantiation failed. Avoid updating the DOM object map and
        // return null which is already handled by callers of this function
        // in case the event is NULL.
        return v8::Null();
    }

    event->ref(); // fast ref
    setJSWrapperForDOMObject(event, v8::Persistent<v8::Object>::New(result));

    return result;
}


static const V8ClassIndex::V8WrapperType mapping[] = {
    V8ClassIndex::INVALID_CLASS_INDEX,    // NONE
    V8ClassIndex::INVALID_CLASS_INDEX,    // ELEMENT_NODE needs special treatment
    V8ClassIndex::ATTR,                   // ATTRIBUTE_NODE
    V8ClassIndex::TEXT,                   // TEXT_NODE
    V8ClassIndex::CDATASECTION,           // CDATA_SECTION_NODE
    V8ClassIndex::ENTITYREFERENCE,        // ENTITY_REFERENCE_NODE
    V8ClassIndex::ENTITY,                 // ENTITY_NODE
    V8ClassIndex::PROCESSINGINSTRUCTION,  // PROCESSING_INSTRUCTION_NODE
    V8ClassIndex::COMMENT,                // COMMENT_NODE
    V8ClassIndex::INVALID_CLASS_INDEX,    // DOCUMENT_NODE needs special treatment
    V8ClassIndex::DOCUMENTTYPE,           // DOCUMENT_TYPE_NODE
    V8ClassIndex::DOCUMENTFRAGMENT,       // DOCUMENT_FRAGMENT_NODE
    V8ClassIndex::NOTATION,               // NOTATION_NODE
    V8ClassIndex::NODE,                   // XPATH_NAMESPACE_NODE
};

// Caller checks node is not null.
v8::Handle<v8::Value> V8DOMWrapper::convertNodeToV8Object(Node* node)
{
    if (!node)
        return v8::Null();

    // Find a proxy for this node.
    //
    // Note that if proxy is found, we might initialize the context which can
    // instantiate a document wrapper.  Therefore, we get the proxy before
    // checking if the node already has a wrapper.
    V8Proxy* proxy = 0;
    Document* document = node->document();
    if (document) {
        Frame* frame = document->frame();
        proxy = V8Proxy::retrieve(frame);
        if (proxy)
            proxy->initContextIfNeeded();
    }

    DOMWrapperMap<Node>& domNodeMap = proxy ? proxy->domNodeMap() : getDOMNodeMap();
    v8::Handle<v8::Object> wrapper = domNodeMap.get(node);
    if (!wrapper.IsEmpty())
        return wrapper;

    bool isDocument = false; // document type node has special handling
    V8ClassIndex::V8WrapperType type;

    Node::NodeType nodeType = node->nodeType();
    if (nodeType == Node::ELEMENT_NODE) {
        if (node->isHTMLElement())
            type = htmlElementType(static_cast<HTMLElement*>(node));
#if ENABLE(SVG)
        else if (node->isSVGElement())
            type = svgElementType(static_cast<SVGElement*>(node));
#endif
        else
            type = V8ClassIndex::ELEMENT;
    } else if (nodeType == Node::DOCUMENT_NODE) {
        isDocument = true;
        Document* document = static_cast<Document*>(node);
        if (document->isHTMLDocument())
            type = V8ClassIndex::HTMLDOCUMENT;
#if ENABLE(SVG)
        else if (document->isSVGDocument())
            type = V8ClassIndex::SVGDOCUMENT;
#endif
        else
            type = V8ClassIndex::DOCUMENT;
    } else {
        ASSERT(nodeType < sizeof(mapping)/sizeof(mapping[0]));
        type = mapping[nodeType];
        ASSERT(type != V8ClassIndex::INVALID_CLASS_INDEX);
    }

    v8::Local<v8::Context> v8Context;
    if (proxy)
        v8Context = proxy->context();

    // Enter the node's context and create the wrapper in that context.
    if (!v8Context.IsEmpty())
        v8Context->Enter();

    v8::Local<v8::Object> result = instantiateV8Object(proxy, type, V8ClassIndex::NODE, node);

    // Exit the node's context if it was entered.
    if (!v8Context.IsEmpty())
        v8Context->Exit();

    if (result.IsEmpty()) {
        // If instantiation failed it's important not to add the result
        // to the DOM node map. Instead we return an empty handle, which
        // should already be handled by callers of this function in case
        // the node is NULL.
        return result;
    }

    node->ref();
    domNodeMap.set(node, v8::Persistent<v8::Object>::New(result));

    if (isDocument) {
        if (proxy)
            proxy->updateDocumentWrapper(result);

        if (type == V8ClassIndex::HTMLDOCUMENT) {
            // Create marker object and insert it in two internal fields.
            // This is used to implement temporary shadowing of
            // document.all.
            ASSERT(result->InternalFieldCount() == V8Custom::kHTMLDocumentInternalFieldCount);
            v8::Local<v8::Object> marker = v8::Object::New();
            result->SetInternalField(V8Custom::kHTMLDocumentMarkerIndex, marker);
            result->SetInternalField(V8Custom::kHTMLDocumentShadowIndex, marker);
        }
    }

    return result;
}

// A JS object of type EventTarget can only be the following possible types:
// 1) EventTargetNode; 2) DOMWindow 3) XMLHttpRequest; 4) MessagePort;
// 5) XMLHttpRequestUpload
// check EventTarget.h for new type conversion methods
v8::Handle<v8::Value> V8DOMWrapper::convertEventTargetToV8Object(EventTarget* target)
{
    if (!target)
        return v8::Null();

#if ENABLE(SVG)
    SVGElementInstance* instance = target->toSVGElementInstance();
    if (instance)
        return convertToV8Object(V8ClassIndex::SVGELEMENTINSTANCE, instance);
#endif

#if ENABLE(WORKERS)
    Worker* worker = target->toWorker();
    if (worker)
        return convertToV8Object(V8ClassIndex::WORKER, worker);
#endif // WORKERS

    Node* node = target->toNode();
    if (node)
        return convertNodeToV8Object(node);

    if (DOMWindow* domWindow = target->toDOMWindow())
        return convertToV8Object(V8ClassIndex::DOMWINDOW, domWindow);

    // XMLHttpRequest is created within its JS counterpart.
    XMLHttpRequest* xmlHttpRequest = target->toXMLHttpRequest();
    if (xmlHttpRequest) {
        v8::Handle<v8::Object> wrapper = getActiveDOMObjectMap().get(xmlHttpRequest);
        ASSERT(!wrapper.IsEmpty());
        return wrapper;
    }

    // MessagePort is created within its JS counterpart
    MessagePort* port = target->toMessagePort();
    if (port) {
        v8::Handle<v8::Object> wrapper = getActiveDOMObjectMap().get(port);
        ASSERT(!wrapper.IsEmpty());
        return wrapper;
    }

    XMLHttpRequestUpload* upload = target->toXMLHttpRequestUpload();
    if (upload) {
        v8::Handle<v8::Object> wrapper = getDOMObjectMap().get(upload);
        ASSERT(!wrapper.IsEmpty());
        return wrapper;
    }

    ASSERT(0);
    return notHandledByInterceptor();
}

v8::Handle<v8::Value> V8DOMWrapper::convertEventListenerToV8Object(EventListener* listener)
{
    if (!listener)
        return v8::Null();

    // FIXME: can a user take a lazy event listener and set to other places?
    V8AbstractEventListener* v8listener = static_cast<V8AbstractEventListener*>(listener);
    return v8listener->getListenerObject();
}

v8::Handle<v8::Value> V8DOMWrapper::convertDOMImplementationToV8Object(DOMImplementation* impl)
{
    v8::Handle<v8::Object> result = instantiateV8Object(V8ClassIndex::DOMIMPLEMENTATION, V8ClassIndex::DOMIMPLEMENTATION, impl);
    if (result.IsEmpty()) {
        // If the instantiation failed, we ignore it and return null instead
        // of returning an empty handle.
        return v8::Null();
    }
    return result;
}

v8::Handle<v8::Value> V8DOMWrapper::convertStyleSheetToV8Object(StyleSheet* sheet)
{
    if (!sheet)
        return v8::Null();

    v8::Handle<v8::Object> wrapper = getDOMObjectMap().get(sheet);
    if (!wrapper.IsEmpty())
        return wrapper;

    V8ClassIndex::V8WrapperType type = V8ClassIndex::STYLESHEET;
    if (sheet->isCSSStyleSheet())
        type = V8ClassIndex::CSSSTYLESHEET;

    v8::Handle<v8::Object> result = instantiateV8Object(type, V8ClassIndex::STYLESHEET, sheet);
    if (!result.IsEmpty()) {
        // Only update the DOM object map if the result is non-empty.
        sheet->ref();
        setJSWrapperForDOMObject(sheet, v8::Persistent<v8::Object>::New(result));
    }

    // Add a hidden reference from stylesheet object to its owner node.
    Node* ownerNode = sheet->ownerNode();
    if (ownerNode) {
        v8::Handle<v8::Object> owner = v8::Handle<v8::Object>::Cast(convertNodeToV8Object(ownerNode));
        result->SetInternalField(V8Custom::kStyleSheetOwnerNodeIndex, owner);
    }

    return result;
}

v8::Handle<v8::Value> V8DOMWrapper::convertCSSValueToV8Object(CSSValue* value)
{
    if (!value)
        return v8::Null();

    v8::Handle<v8::Object> wrapper = getDOMObjectMap().get(value);
    if (!wrapper.IsEmpty())
        return wrapper;

    V8ClassIndex::V8WrapperType type;

    if (value->isWebKitCSSTransformValue())
        type = V8ClassIndex::WEBKITCSSTRANSFORMVALUE;
    else if (value->isValueList())
        type = V8ClassIndex::CSSVALUELIST;
    else if (value->isPrimitiveValue())
        type = V8ClassIndex::CSSPRIMITIVEVALUE;
#if ENABLE(SVG)
    else if (value->isSVGPaint())
        type = V8ClassIndex::SVGPAINT;
    else if (value->isSVGColor())
        type = V8ClassIndex::SVGCOLOR;
#endif
    else
        type = V8ClassIndex::CSSVALUE;

    v8::Handle<v8::Object> result = instantiateV8Object(type, V8ClassIndex::CSSVALUE, value);
    if (!result.IsEmpty()) {
        // Only update the DOM object map if the result is non-empty.
        value->ref();
        setJSWrapperForDOMObject(value, v8::Persistent<v8::Object>::New(result));
    }

    return result;
}

v8::Handle<v8::Value> V8DOMWrapper::convertCSSRuleToV8Object(CSSRule* rule)
{
    if (!rule) 
        return v8::Null();

    v8::Handle<v8::Object> wrapper = getDOMObjectMap().get(rule);
    if (!wrapper.IsEmpty())
        return wrapper;

    V8ClassIndex::V8WrapperType type;

    switch (rule->type()) {
    case CSSRule::STYLE_RULE:
        type = V8ClassIndex::CSSSTYLERULE;
        break;
    case CSSRule::CHARSET_RULE:
        type = V8ClassIndex::CSSCHARSETRULE;
        break;
    case CSSRule::IMPORT_RULE:
        type = V8ClassIndex::CSSIMPORTRULE;
        break;
    case CSSRule::MEDIA_RULE:
        type = V8ClassIndex::CSSMEDIARULE;
        break;
    case CSSRule::FONT_FACE_RULE:
        type = V8ClassIndex::CSSFONTFACERULE;
        break;
    case CSSRule::PAGE_RULE:
        type = V8ClassIndex::CSSPAGERULE;
        break;
    case CSSRule::VARIABLES_RULE:
        type = V8ClassIndex::CSSVARIABLESRULE;
        break;
    case CSSRule::WEBKIT_KEYFRAME_RULE:
        type = V8ClassIndex::WEBKITCSSKEYFRAMERULE;
        break;
    case CSSRule::WEBKIT_KEYFRAMES_RULE:
        type = V8ClassIndex::WEBKITCSSKEYFRAMESRULE;
        break;
    default:  // CSSRule::UNKNOWN_RULE
        type = V8ClassIndex::CSSRULE;
        break;
    }

    v8::Handle<v8::Object> result = instantiateV8Object(type, V8ClassIndex::CSSRULE, rule);
    if (!result.IsEmpty()) {
        // Only update the DOM object map if the result is non-empty.
        rule->ref();
        setJSWrapperForDOMObject(rule, v8::Persistent<v8::Object>::New(result));
    }
    return result;
}

v8::Handle<v8::Value> V8DOMWrapper::convertWindowToV8Object(DOMWindow* window)
{
    if (!window)
        return v8::Null();
    // Initializes environment of a frame, and return the global object
    // of the frame.
    Frame* frame = window->frame();
    if (!frame)
        return v8::Handle<v8::Object>();

    // Special case: Because of evaluateInNewContext() one DOMWindow can have
    // multiple contexts and multiple global objects associated with it. When
    // code running in one of those contexts accesses the window object, we
    // want to return the global object associated with that context, not
    // necessarily the first global object associated with that DOMWindow.
    v8::Handle<v8::Context> currentContext = v8::Context::GetCurrent();
    v8::Handle<v8::Object> currentGlobal = currentContext->Global();
    v8::Handle<v8::Object> windowWrapper = V8DOMWrapper::lookupDOMWrapper(V8ClassIndex::DOMWINDOW, currentGlobal);
    if (!windowWrapper.IsEmpty()) {
        if (convertDOMWrapperToNative<DOMWindow>(windowWrapper) == window)
            return currentGlobal;
    }

    // Otherwise, return the global object associated with this frame.
    v8::Handle<v8::Context> v8Context = V8Proxy::context(frame);
    if (v8Context.IsEmpty())
        return v8::Handle<v8::Object>();

    v8::Handle<v8::Object> global = v8Context->Global();
    ASSERT(!global.IsEmpty());
    return global;
}

void V8Proxy::bindJsObjectToWindow(Frame* frame, const char* name, int type, v8::Handle<v8::FunctionTemplate> descriptor, void* impl)
{
    // Get environment.
    v8::Handle<v8::Context> v8Context = V8Proxy::context(frame);
    if (v8Context.IsEmpty())
        return; // JS not enabled.

    v8::Context::Scope scope(v8Context);
    v8::Handle<v8::Object> instance = descriptor->GetFunction();
    V8DOMWrapper::setDOMWrapper(instance, type, impl);

    v8::Handle<v8::Object> global = v8Context->Global();
    global->Set(v8::String::New(name), instance);
}

void V8Proxy::processConsoleMessages()
{
    ConsoleMessageManager::processDelayedMessages();
}

// Create the utility context for holding JavaScript functions used internally
// which are not visible to JavaScript executing on the page.
void V8Proxy::createUtilityContext()
{
    ASSERT(m_utilityContext.IsEmpty());

    v8::HandleScope scope;
    v8::Handle<v8::ObjectTemplate> globalTemplate = v8::ObjectTemplate::New();
    m_utilityContext = v8::Context::New(0, globalTemplate);
    v8::Context::Scope contextScope(m_utilityContext);

    // Compile JavaScript function for retrieving the source line of the top
    // JavaScript stack frame.
    DEFINE_STATIC_LOCAL(const char*, frameSourceLineSource,
        ("function frameSourceLine(exec_state) {"
        "  return exec_state.frame(0).sourceLine();"
        "}"));
    v8::Script::Compile(v8::String::New(frameSourceLineSource))->Run();

    // Compile JavaScript function for retrieving the source name of the top
    // JavaScript stack frame.
    DEFINE_STATIC_LOCAL(const char*, frameSourceNameSource,
        ("function frameSourceName(exec_state) {"
        "  var frame = exec_state.frame(0);"
        "  if (frame.func().resolved() && "
        "      frame.func().script() && "
        "      frame.func().script().name()) {"
        "    return frame.func().script().name();"
        "  }"
        "}"));
    v8::Script::Compile(v8::String::New(frameSourceNameSource))->Run();
}

int V8Proxy::sourceLineNumber()
{
    v8::HandleScope scope;
    v8::Handle<v8::Context> v8UtilityContext = V8Proxy::utilityContext();
    if (v8UtilityContext.IsEmpty())
        return 0;
    v8::Context::Scope contextScope(v8UtilityContext);
    v8::Handle<v8::Function> frameSourceLine;
    frameSourceLine = v8::Local<v8::Function>::Cast(v8UtilityContext->Global()->Get(v8::String::New("frameSourceLine")));
    if (frameSourceLine.IsEmpty())
        return 0;
    v8::Handle<v8::Value> result = v8::Debug::Call(frameSourceLine);
    if (result.IsEmpty())
        return 0;
    return result->Int32Value();
}

String V8Proxy::sourceName()
{
    v8::HandleScope scope;
    v8::Handle<v8::Context> v8UtilityContext = utilityContext();
    if (v8UtilityContext.IsEmpty())
        return String();
    v8::Context::Scope contextScope(v8UtilityContext);
    v8::Handle<v8::Function> frameSourceName;
    frameSourceName = v8::Local<v8::Function>::Cast(v8UtilityContext->Global()->Get(v8::String::New("frameSourceName")));
    if (frameSourceName.IsEmpty())
        return String();
    return toWebCoreString(v8::Debug::Call(frameSourceName));
}

void V8Proxy::registerExtension(v8::Extension* extension, const String& schemeRestriction)
{
    v8::RegisterExtension(extension);
    V8ExtensionInfo info = {schemeRestriction, extension};
    m_extensions.push_back(info);
}

bool V8Proxy::setContextDebugId(int debugId)
{
    ASSERT(debugId > 0);
    if (m_context.IsEmpty())
        return false;
    v8::HandleScope scope;
    if (!m_context->GetData()->IsUndefined())
        return false;

    v8::Handle<v8::Object> contextData = v8::Object::New();
    contextData->Set(v8::String::New(kContextDebugDataType), v8::String::New("page"));
    contextData->Set(v8::String::New(kContextDebugDataValue), v8::Integer::New(debugId));
    m_context->SetData(contextData);
    return true;
}

int V8Proxy::contextDebugId(v8::Handle<v8::Context> context)
{
    v8::HandleScope scope;
    if (!context->GetData()->IsObject())
        return -1;
    v8::Handle<v8::Value> data = context->GetData()->ToObject()->Get( v8::String::New(kContextDebugDataValue));
    return data->IsInt32() ? data->Int32Value() : -1;
}

}  // namespace WebCore
