/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Matt Lilek <webkit@mattlilek.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "InspectorController.h"

#include "CString.h"
#include "CachedResource.h"
#include "Console.h"
#include "DOMWindow.h"
#include "DocLoader.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "Element.h"
#include "FloatConversion.h"
#include "FloatRect.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HitTestResult.h"
#include "HTMLFrameOwnerElement.h"
#include "InspectorClient.h"
#include "JSDOMWindow.h"
#include "JSInspectedObjectWrapper.h"
#include "JSInspectorCallbackWrapper.h"
#include "JSNode.h"
#include "JSRange.h"
#include "JavaScriptProfile.h"
#include "Page.h"
#include "Range.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "Settings.h"
#include "ScriptCallStack.h"
#include "SharedBuffer.h"
#include "TextEncoding.h"
#include "TextIterator.h"
#include "ScriptController.h"
#include <JavaScriptCore/APICast.h>
#include <JavaScriptCore/JSRetainPtr.h>
#include <JavaScriptCore/JSStringRef.h>
#include <JavaScriptCore/OpaqueJSString.h>
#include <runtime/JSLock.h>
#include <runtime/UString.h>
#include <runtime/CollectorHeapIterator.h>
#include <profiler/Profile.h>
#include <profiler/Profiler.h>
#include <wtf/CurrentTime.h>
#include <wtf/RefCounted.h>
#include <wtf/StdLibExtras.h>

#if ENABLE(DATABASE)
#include "Database.h"
#include "JSDatabase.h"
#endif

#if ENABLE(JAVASCRIPT_DEBUGGER)
#include "JavaScriptCallFrame.h"
#include "JavaScriptDebugServer.h"
#include "JSJavaScriptCallFrame.h"
#endif

using namespace JSC;
using namespace std;

namespace WebCore {

static const char* const UserInitiatedProfileName = "org.webkit.profiles.user-initiated";

static JSRetainPtr<JSStringRef> jsStringRef(const char* str)
{
    return JSRetainPtr<JSStringRef>(Adopt, JSStringCreateWithUTF8CString(str));
}

static JSRetainPtr<JSStringRef> jsStringRef(const SourceCode& str)
{
    return JSRetainPtr<JSStringRef>(Adopt, JSStringCreateWithCharacters(str.data(), str.length()));
}

static JSRetainPtr<JSStringRef> jsStringRef(const String& str)
{
    return JSRetainPtr<JSStringRef>(Adopt, JSStringCreateWithCharacters(str.characters(), str.length()));
}

static JSRetainPtr<JSStringRef> jsStringRef(const UString& str)
{
    return JSRetainPtr<JSStringRef>(Adopt, OpaqueJSString::create(str).releaseRef());
}

static String toString(JSContextRef context, JSValueRef value, JSValueRef* exception)
{
    ASSERT_ARG(value, value);
    if (!value)
        return String();
    JSRetainPtr<JSStringRef> scriptString(Adopt, JSValueToStringCopy(context, value, exception));
    if (exception && *exception)
        return String();
    return String(JSStringGetCharactersPtr(scriptString.get()), JSStringGetLength(scriptString.get()));
}

#define HANDLE_EXCEPTION(context, exception) handleException((context), (exception), __LINE__)

JSValueRef InspectorController::callSimpleFunction(JSContextRef context, JSObjectRef thisObject, const char* functionName) const
{
    JSValueRef exception = 0;
    return callFunction(context, thisObject, functionName, 0, 0, exception);
}

JSValueRef InspectorController::callFunction(JSContextRef context, JSObjectRef thisObject, const char* functionName, size_t argumentCount, const JSValueRef arguments[], JSValueRef& exception) const
{
    ASSERT_ARG(context, context);
    ASSERT_ARG(thisObject, thisObject);

    if (exception)
        return JSValueMakeUndefined(context);

    JSValueRef functionProperty = JSObjectGetProperty(context, thisObject, jsStringRef(functionName).get(), &exception);
    if (HANDLE_EXCEPTION(context, exception))
        return JSValueMakeUndefined(context);

    JSObjectRef function = JSValueToObject(context, functionProperty, &exception);
    if (HANDLE_EXCEPTION(context, exception))
        return JSValueMakeUndefined(context);

    JSValueRef result = JSObjectCallAsFunction(context, function, thisObject, argumentCount, arguments, &exception);
    if (HANDLE_EXCEPTION(context, exception))
        return JSValueMakeUndefined(context);

    return result;
}

// ConsoleMessage Struct

struct ConsoleMessage {
    ConsoleMessage(MessageSource s, MessageLevel l, const String& m, unsigned li, const String& u, unsigned g)
        : source(s)
        , level(l)
        , message(m)
        , line(li)
        , url(u)
        , groupLevel(g)
        , repeatCount(1)
    {
    }

    ConsoleMessage(MessageSource s, MessageLevel l, ScriptCallStack* callStack, unsigned g, bool storeTrace = false)
        : source(s)
        , level(l)
        , wrappedArguments(callStack->at(0).argumentCount())
        , frames(storeTrace ? callStack->size() : 0)
        , groupLevel(g)
        , repeatCount(1)
    {
        const ScriptCallFrame& lastCaller = callStack->at(0);
        line = lastCaller.lineNumber();
        url = lastCaller.sourceURL().string();

        // FIXME: For now, just store function names as strings.
        // As ScriptCallStack start storing line number and source URL for all
        // frames, refactor to use that, as well.
        if (storeTrace) {
            unsigned stackSize = callStack->size();
            for (unsigned i = 0; i < stackSize; ++i)
                frames[i] = callStack->at(i).functionName();
        }

        JSLock lock(false);

        for (unsigned i = 0; i < lastCaller.argumentCount(); ++i)
            wrappedArguments[i] = JSInspectedObjectWrapper::wrap(callStack->state(), lastCaller.argumentAt(i).jsValue());
    }
    
    bool isEqual(ExecState* exec, ConsoleMessage* msg) const
    {
        if (msg->wrappedArguments.size() != this->wrappedArguments.size() ||
           (!exec && msg->wrappedArguments.size()))
            return false;

        for (size_t i = 0; i < msg->wrappedArguments.size(); ++i) {
            ASSERT_ARG(exec, exec);
            if (!JSValueIsEqual(toRef(exec), toRef(msg->wrappedArguments[i].get()), toRef(this->wrappedArguments[i].get()), 0))
                return false;
        }

        size_t frameCount = msg->frames.size();
        if (frameCount != this->frames.size())
            return false;
        
        for (size_t i = 0; i < frameCount; ++i) {
            const ScriptString& myFrameFunctionName = this->frames[i];
            if (myFrameFunctionName != msg->frames[i])
                return false;
        }
    
        return msg->source == this->source
            && msg->level == this->level
            && msg->message == this->message
            && msg->line == this->line
            && msg->url == this->url
            && msg->groupLevel == this->groupLevel;
    }

    MessageSource source;
    MessageLevel level;
    String message;
    Vector<ProtectedJSValuePtr> wrappedArguments;
    Vector<ScriptString> frames;
    unsigned line;
    String url;
    unsigned groupLevel;
    unsigned repeatCount;
};

// XMLHttpRequestResource Class

struct XMLHttpRequestResource {
    XMLHttpRequestResource(const JSC::UString& sourceString)
    {
        JSC::JSLock lock(false);
        this->sourceString = sourceString.rep();
    }

    ~XMLHttpRequestResource()
    {
        JSC::JSLock lock(false);
        sourceString.clear();
    }

    RefPtr<JSC::UString::Rep> sourceString;
};

// InspectorResource Struct

struct InspectorResource : public RefCounted<InspectorResource> {
    // Keep these in sync with WebInspector.Resource.Type
    enum Type {
        Doc,
        Stylesheet,
        Image,
        Font,
        Script,
        XHR,
        Media,
        Other
    };

    static PassRefPtr<InspectorResource> create(long long identifier, DocumentLoader* documentLoader, Frame* frame)
    {
        return adoptRef(new InspectorResource(identifier, documentLoader, frame));
    }
    
    ~InspectorResource()
    {
        setScriptObject(0, 0);
    }

    Type type() const
    {
        if (xmlHttpRequestResource)
            return XHR;

        if (requestURL == loader->requestURL())
            return Doc;

        if (loader->frameLoader() && requestURL == loader->frameLoader()->iconURL())
            return Image;

        CachedResource* cachedResource = frame->document()->docLoader()->cachedResource(requestURL.string());
        if (!cachedResource)
            return Other;

        switch (cachedResource->type()) {
            case CachedResource::ImageResource:
                return Image;
            case CachedResource::FontResource:
                return Font;
            case CachedResource::CSSStyleSheet:
#if ENABLE(XSLT)
            case CachedResource::XSLStyleSheet:
#endif
                return Stylesheet;
            case CachedResource::Script:
                return Script;
            default:
                return Other;
        }
    }

    void setScriptObject(JSContextRef context, JSObjectRef newScriptObject)
    {
        if (scriptContext && scriptObject)
            JSValueUnprotect(scriptContext, scriptObject);

        scriptObject = newScriptObject;
        scriptContext = context;

        ASSERT((context && newScriptObject) || (!context && !newScriptObject));
        if (context && newScriptObject)
            JSValueProtect(context, newScriptObject);
    }

    void setXMLHttpRequestProperties(const JSC::UString& data)
    {
        xmlHttpRequestResource.set(new XMLHttpRequestResource(data));
    }
    
    String sourceString() const
     {
         if (xmlHttpRequestResource)
            return JSC::UString(xmlHttpRequestResource->sourceString);

        RefPtr<SharedBuffer> buffer;
        String textEncodingName;

        if (requestURL == loader->requestURL()) {
            buffer = loader->mainResourceData();
            textEncodingName = frame->document()->inputEncoding();
        } else {
            CachedResource* cachedResource = frame->document()->docLoader()->cachedResource(requestURL.string());
            if (!cachedResource)
                return String();

            if (cachedResource->isPurgeable()) {
                // If the resource is purgeable then make it unpurgeable to get
                // get its data. This might fail, in which case we return an
                // empty String.
                // FIXME: should we do something else in the case of a purged
                // resource that informs the user why there is no data in the
                // inspector?
                if (!cachedResource->makePurgeable(false))
                    return String();
            }

            buffer = cachedResource->data();
            textEncodingName = cachedResource->encoding();
        }

        if (!buffer)
            return String();

        TextEncoding encoding(textEncodingName);
        if (!encoding.isValid())
            encoding = WindowsLatin1Encoding();
        return encoding.decode(buffer->data(), buffer->size());
     }

    long long identifier;
    RefPtr<DocumentLoader> loader;
    RefPtr<Frame> frame;
    OwnPtr<XMLHttpRequestResource> xmlHttpRequestResource;
    KURL requestURL;
    HTTPHeaderMap requestHeaderFields;
    HTTPHeaderMap responseHeaderFields;
    String mimeType;
    String suggestedFilename;
    JSContextRef scriptContext;
    JSObjectRef scriptObject;
    long long expectedContentLength;
    bool cached;
    bool finished;
    bool failed;
    int length;
    int responseStatusCode;
    double startTime;
    double responseReceivedTime;
    double endTime;

protected:
    InspectorResource(long long identifier, DocumentLoader* documentLoader, Frame* frame)
        : identifier(identifier)
        , loader(documentLoader)
        , frame(frame)
        , xmlHttpRequestResource(0)
        , scriptContext(0)
        , scriptObject(0)
        , expectedContentLength(0)
        , cached(false)
        , finished(false)
        , failed(false)
        , length(0)
        , responseStatusCode(0)
        , startTime(-1.0)
        , responseReceivedTime(-1.0)
        , endTime(-1.0)
    {
    }
};

// InspectorDatabaseResource Struct

#if ENABLE(DATABASE)
struct InspectorDatabaseResource : public RefCounted<InspectorDatabaseResource> {
    static PassRefPtr<InspectorDatabaseResource> create(Database* database, const String& domain, const String& name, const String& version)
    {
        return adoptRef(new InspectorDatabaseResource(database, domain, name, version));
    }

    void setScriptObject(JSContextRef context, JSObjectRef newScriptObject)
    {
        if (scriptContext && scriptObject)
            JSValueUnprotect(scriptContext, scriptObject);

        scriptObject = newScriptObject;
        scriptContext = context;

        ASSERT((context && newScriptObject) || (!context && !newScriptObject));
        if (context && newScriptObject)
            JSValueProtect(context, newScriptObject);
    }

    RefPtr<Database> database;
    String domain;
    String name;
    String version;
    JSContextRef scriptContext;
    JSObjectRef scriptObject;
    
private:
    InspectorDatabaseResource(Database* database, const String& domain, const String& name, const String& version)
        : database(database)
        , domain(domain)
        , name(name)
        , version(version)
        , scriptContext(0)
        , scriptObject(0)
    {
    }
};
#endif

// JavaScript Callbacks

#define SIMPLE_INSPECTOR_CALLBACK(jsFunction, inspectorControllerMethod) \
static JSValueRef jsFunction(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t, const JSValueRef[], JSValueRef*) \
{ \
    if (InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject))) \
        controller->inspectorControllerMethod(); \
    return JSValueMakeUndefined(ctx); \
}

SIMPLE_INSPECTOR_CALLBACK(hideDOMNodeHighlight, hideHighlight);
SIMPLE_INSPECTOR_CALLBACK(loaded, scriptObjectReady);
SIMPLE_INSPECTOR_CALLBACK(unloading, close);
SIMPLE_INSPECTOR_CALLBACK(attach, attachWindow);
SIMPLE_INSPECTOR_CALLBACK(detach, detachWindow);
#if ENABLE(JAVASCRIPT_DEBUGGER)
SIMPLE_INSPECTOR_CALLBACK(enableDebugger, enableDebugger);
SIMPLE_INSPECTOR_CALLBACK(disableDebugger, disableDebugger);
SIMPLE_INSPECTOR_CALLBACK(pauseInDebugger, pauseInDebugger);
SIMPLE_INSPECTOR_CALLBACK(resumeDebugger, resumeDebugger);
SIMPLE_INSPECTOR_CALLBACK(stepOverStatementInDebugger, stepOverStatementInDebugger);
SIMPLE_INSPECTOR_CALLBACK(stepIntoStatementInDebugger, stepIntoStatementInDebugger);
SIMPLE_INSPECTOR_CALLBACK(stepOutOfFunctionInDebugger, stepOutOfFunctionInDebugger);
#endif
SIMPLE_INSPECTOR_CALLBACK(closeWindow, closeWindow);
SIMPLE_INSPECTOR_CALLBACK(clearMessages, clearConsoleMessages);
SIMPLE_INSPECTOR_CALLBACK(startProfiling, startUserInitiatedProfilingSoon);
SIMPLE_INSPECTOR_CALLBACK(stopProfiling, stopUserInitiatedProfiling);
SIMPLE_INSPECTOR_CALLBACK(enableProfiler, enableProfiler);
SIMPLE_INSPECTOR_CALLBACK(disableProfiler, disableProfiler);
SIMPLE_INSPECTOR_CALLBACK(toggleNodeSearch, toggleSearchForNodeInPage);

#define BOOL_INSPECTOR_CALLBACK(jsFunction, inspectorControllerMethod) \
static JSValueRef jsFunction(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t, const JSValueRef[], JSValueRef*) \
{ \
    if (InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject))) \
        return JSValueMakeBoolean(ctx, controller->inspectorControllerMethod()); \
    return JSValueMakeUndefined(ctx); \
}

#if ENABLE(JAVASCRIPT_DEBUGGER)
BOOL_INSPECTOR_CALLBACK(debuggerEnabled, debuggerEnabled);
BOOL_INSPECTOR_CALLBACK(pauseOnExceptions, pauseOnExceptions);
#endif
BOOL_INSPECTOR_CALLBACK(profilerEnabled, profilerEnabled);
BOOL_INSPECTOR_CALLBACK(isWindowVisible, windowVisible);
BOOL_INSPECTOR_CALLBACK(searchingForNode, searchingForNodeInPage);

static bool addSourceToFrame(const String& mimeType, const String& source, Node* frameNode)
{
    ASSERT_ARG(frameNode, frameNode);

    if (!frameNode)
        return false;

    if (!frameNode->attached()) {
        ASSERT_NOT_REACHED();
        return false;
    }

    ASSERT(frameNode->isElementNode());
    if (!frameNode->isElementNode())
        return false;

    Element* element = static_cast<Element*>(frameNode);
    ASSERT(element->isFrameOwnerElement());
    if (!element->isFrameOwnerElement())
        return false;

    HTMLFrameOwnerElement* frameOwner = static_cast<HTMLFrameOwnerElement*>(element);
    ASSERT(frameOwner->contentFrame());
    if (!frameOwner->contentFrame())
        return false;

    FrameLoader* loader = frameOwner->contentFrame()->loader();

    loader->setResponseMIMEType(mimeType);
    loader->begin();
    loader->write(source);
    loader->end();

    return true;
}

static JSValueRef addResourceSourceToFrame(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    JSValueRef undefined = JSValueMakeUndefined(ctx);

    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (argumentCount < 2 || !controller)
        return undefined;

    JSValueRef identifierValue = arguments[0];
    if (!JSValueIsNumber(ctx, identifierValue))
        return undefined;

    long long identifier = static_cast<long long>(JSValueToNumber(ctx, identifierValue, exception));
    if (exception && *exception)
        return undefined;

    RefPtr<InspectorResource> resource = controller->resources().get(identifier);
    ASSERT(resource);
    if (!resource)
        return undefined;

    String sourceString = resource->sourceString();
    if (sourceString.isEmpty())
        return undefined;

    bool successfullyAddedSource = addSourceToFrame(resource->mimeType, sourceString, toNode(toJS(arguments[1])));
    return JSValueMakeBoolean(ctx, successfullyAddedSource);
}

static JSValueRef addSourceToFrame(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    JSValueRef undefined = JSValueMakeUndefined(ctx);

    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (argumentCount < 3 || !controller)
        return undefined;

    JSValueRef mimeTypeValue = arguments[0];
    if (!JSValueIsString(ctx, mimeTypeValue))
        return undefined;

    JSValueRef sourceValue = arguments[1];
    if (!JSValueIsString(ctx, sourceValue))
        return undefined;

    String mimeType = toString(ctx, mimeTypeValue, exception);
    if (mimeType.isEmpty())
        return undefined;

    String source = toString(ctx, sourceValue, exception);
    if (source.isEmpty())
        return undefined;

    bool successfullyAddedSource = addSourceToFrame(mimeType, source, toNode(toJS(arguments[2])));
    return JSValueMakeBoolean(ctx, successfullyAddedSource);
}

static JSValueRef getResourceDocumentNode(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    JSValueRef undefined = JSValueMakeUndefined(ctx);

    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!argumentCount || argumentCount > 1 || !controller)
        return undefined;

    JSValueRef identifierValue = arguments[0];
    if (!JSValueIsNumber(ctx, identifierValue))
        return undefined;

    long long identifier = static_cast<long long>(JSValueToNumber(ctx, identifierValue, exception));
    if (exception && *exception)
        return undefined;

    RefPtr<InspectorResource> resource = controller->resources().get(identifier);
    ASSERT(resource);
    if (!resource)
        return undefined;

    Frame* frame = resource->frame.get();

    Document* document = frame->document();
    if (!document)
        return undefined;

    if (document->isPluginDocument() || document->isImageDocument() || document->isMediaDocument())
        return undefined;

    ExecState* exec = toJSDOMWindowShell(resource->frame.get())->window()->globalExec();

    JSC::JSLock lock(false);
    JSValueRef documentValue = toRef(JSInspectedObjectWrapper::wrap(exec, toJS(exec, document)));
    return documentValue;
}

static JSValueRef highlightDOMNode(JSContextRef context, JSObjectRef /*function*/, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* /*exception*/)
{
    JSValueRef undefined = JSValueMakeUndefined(context);

    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (argumentCount < 1 || !controller)
        return undefined;

    JSQuarantinedObjectWrapper* wrapper = JSQuarantinedObjectWrapper::asWrapper(toJS(arguments[0]));
    if (!wrapper)
        return undefined;
    Node* node = toNode(wrapper->unwrappedObject());
    if (!node)
        return undefined;

    controller->highlight(node);

    return undefined;
}

static JSValueRef search(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    if (argumentCount < 2 || !JSValueIsString(ctx, arguments[1]))
        return JSValueMakeUndefined(ctx);

    Node* node = toNode(toJS(arguments[0]));
    if (!node)
        return JSValueMakeUndefined(ctx);

    String target = toString(ctx, arguments[1], exception);

    JSObjectRef global = JSContextGetGlobalObject(ctx);

    JSValueRef arrayProperty = JSObjectGetProperty(ctx, global, jsStringRef("Array").get(), exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    JSObjectRef arrayConstructor = JSValueToObject(ctx, arrayProperty, exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    JSObjectRef result = JSObjectCallAsConstructor(ctx, arrayConstructor, 0, 0, exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    JSValueRef pushProperty = JSObjectGetProperty(ctx, result, jsStringRef("push").get(), exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    JSObjectRef pushFunction = JSValueToObject(ctx, pushProperty, exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    RefPtr<Range> searchRange(rangeOfContents(node));

    ExceptionCode ec = 0;
    do {
        RefPtr<Range> resultRange(findPlainText(searchRange.get(), target, true, false));
        if (resultRange->collapsed(ec))
            break;

        // A non-collapsed result range can in some funky whitespace cases still not
        // advance the range's start position (4509328). Break to avoid infinite loop.
        VisiblePosition newStart = endVisiblePosition(resultRange.get(), DOWNSTREAM);
        if (newStart == startVisiblePosition(searchRange.get(), DOWNSTREAM))
            break;

        JSC::JSLock lock(false);
        JSValueRef arg0 = toRef(toJS(toJS(ctx), resultRange.get()));
        JSObjectCallAsFunction(ctx, pushFunction, result, 1, &arg0, exception);
        if (exception && *exception)
            return JSValueMakeUndefined(ctx);

        setStart(searchRange.get(), newStart);
    } while (true);

    return result;
}

#if ENABLE(DATABASE)
static JSValueRef databaseTableNames(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    if (argumentCount < 1)
        return JSValueMakeUndefined(ctx);

    JSQuarantinedObjectWrapper* wrapper = JSQuarantinedObjectWrapper::asWrapper(toJS(arguments[0]));
    if (!wrapper)
        return JSValueMakeUndefined(ctx);

    Database* database = toDatabase(wrapper->unwrappedObject());
    if (!database)
        return JSValueMakeUndefined(ctx);

    JSObjectRef global = JSContextGetGlobalObject(ctx);

    JSValueRef arrayProperty = JSObjectGetProperty(ctx, global, jsStringRef("Array").get(), exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    JSObjectRef arrayConstructor = JSValueToObject(ctx, arrayProperty, exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    JSObjectRef result = JSObjectCallAsConstructor(ctx, arrayConstructor, 0, 0, exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    JSValueRef pushProperty = JSObjectGetProperty(ctx, result, jsStringRef("push").get(), exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    JSObjectRef pushFunction = JSValueToObject(ctx, pushProperty, exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    Vector<String> tableNames = database->tableNames();
    unsigned length = tableNames.size();
    for (unsigned i = 0; i < length; ++i) {
        String tableName = tableNames[i];
        JSValueRef tableNameValue = JSValueMakeString(ctx, jsStringRef(tableName).get());

        JSValueRef pushArguments[] = { tableNameValue };
        JSObjectCallAsFunction(ctx, pushFunction, result, 1, pushArguments, exception);
        if (exception && *exception)
            return JSValueMakeUndefined(ctx);
    }

    return result;
}
#endif

static JSValueRef inspectedWindow(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t /*argumentCount*/, const JSValueRef[] /*arguments[]*/, JSValueRef* /*exception*/)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    JSDOMWindow* inspectedWindow = toJSDOMWindow(controller->inspectedPage()->mainFrame());
    JSLock lock(false);
    return toRef(JSInspectedObjectWrapper::wrap(inspectedWindow->globalExec(), inspectedWindow));
}

static JSValueRef setting(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t /*argumentCount*/, const JSValueRef arguments[], JSValueRef* exception)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    JSValueRef keyValue = arguments[0];
    if (!JSValueIsString(ctx, keyValue))
        return JSValueMakeUndefined(ctx);

    const InspectorController::Setting& setting = controller->setting(toString(ctx, keyValue, exception));

    switch (setting.type()) {
        default:
        case InspectorController::Setting::NoType:
            return JSValueMakeUndefined(ctx);
        case InspectorController::Setting::StringType:
            return JSValueMakeString(ctx, jsStringRef(setting.string()).get());
        case InspectorController::Setting::DoubleType:
            return JSValueMakeNumber(ctx, setting.doubleValue());
        case InspectorController::Setting::IntegerType:
            return JSValueMakeNumber(ctx, setting.integerValue());
        case InspectorController::Setting::BooleanType:
            return JSValueMakeBoolean(ctx, setting.booleanValue());
        case InspectorController::Setting::StringVectorType: {
            Vector<JSValueRef> stringValues;
            const Vector<String>& strings = setting.stringVector();
            const unsigned length = strings.size();
            for (unsigned i = 0; i < length; ++i)
                stringValues.append(JSValueMakeString(ctx, jsStringRef(strings[i]).get()));

            JSObjectRef stringsArray = JSObjectMakeArray(ctx, stringValues.size(), stringValues.data(), exception);
            if (exception && *exception)
                return JSValueMakeUndefined(ctx);
            return stringsArray;
        }
    }
}

static JSValueRef setSetting(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t /*argumentCount*/, const JSValueRef arguments[], JSValueRef* exception)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    JSValueRef keyValue = arguments[0];
    if (!JSValueIsString(ctx, keyValue))
        return JSValueMakeUndefined(ctx);

    InspectorController::Setting setting;

    JSValueRef value = arguments[1];
    switch (JSValueGetType(ctx, value)) {
        default:
        case kJSTypeUndefined:
        case kJSTypeNull:
            // Do nothing. The setting is already NoType.
            ASSERT(setting.type() == InspectorController::Setting::NoType);
            break;
        case kJSTypeString:
            setting.set(toString(ctx, value, exception));
            break;
        case kJSTypeNumber:
            setting.set(JSValueToNumber(ctx, value, exception));
            break;
        case kJSTypeBoolean:
            setting.set(JSValueToBoolean(ctx, value));
            break;
        case kJSTypeObject: {
            JSObjectRef object = JSValueToObject(ctx, value, 0);
            JSValueRef lengthValue = JSObjectGetProperty(ctx, object, jsStringRef("length").get(), exception);
            if (exception && *exception)
                return JSValueMakeUndefined(ctx);

            Vector<String> strings;
            const unsigned length = static_cast<unsigned>(JSValueToNumber(ctx, lengthValue, 0));
            for (unsigned i = 0; i < length; ++i) {
                JSValueRef itemValue = JSObjectGetPropertyAtIndex(ctx, object, i, exception);
                if (exception && *exception)
                    return JSValueMakeUndefined(ctx);
                strings.append(toString(ctx, itemValue, exception));
                if (exception && *exception)
                    return JSValueMakeUndefined(ctx);
            }

            setting.set(strings);
            break;
        }
    }

    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    controller->setSetting(toString(ctx, keyValue, exception), setting);

    return JSValueMakeUndefined(ctx);
}

static JSValueRef localizedStrings(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t /*argumentCount*/, const JSValueRef[] /*arguments[]*/, JSValueRef* /*exception*/)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    String url = controller->localizedStringsURL();
    if (url.isNull())
        return JSValueMakeNull(ctx);

    return JSValueMakeString(ctx, jsStringRef(url).get());
}

static JSValueRef platform(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef /*thisObject*/, size_t /*argumentCount*/, const JSValueRef[] /*arguments[]*/, JSValueRef* /*exception*/)
{
#if PLATFORM(MAC)
#ifdef BUILDING_ON_TIGER
    DEFINE_STATIC_LOCAL(const String, platform, ("mac-tiger"));
#else
    DEFINE_STATIC_LOCAL(const String, platform, ("mac-leopard"));
#endif
#elif PLATFORM(WIN_OS)
    DEFINE_STATIC_LOCAL(const String, platform, ("windows"));
#elif PLATFORM(QT)
    DEFINE_STATIC_LOCAL(const String, platform, ("qt"));
#elif PLATFORM(GTK)
    DEFINE_STATIC_LOCAL(const String, platform, ("gtk"));
#elif PLATFORM(WX)
    DEFINE_STATIC_LOCAL(const String, platform, ("wx"));
#else
    DEFINE_STATIC_LOCAL(const String, platform, ("unknown"));
#endif

    JSValueRef platformValue = JSValueMakeString(ctx, jsStringRef(platform).get());

    return platformValue;
}

static JSValueRef moveByUnrestricted(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    if (argumentCount < 2)
        return JSValueMakeUndefined(ctx);

    double x = JSValueToNumber(ctx, arguments[0], exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    double y = JSValueToNumber(ctx, arguments[1], exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    controller->moveWindowBy(narrowPrecisionToFloat(x), narrowPrecisionToFloat(y));

    return JSValueMakeUndefined(ctx);
}

static JSValueRef setAttachedWindowHeight(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    if (argumentCount < 1)
        return JSValueMakeUndefined(ctx);

    unsigned height = static_cast<unsigned>(JSValueToNumber(ctx, arguments[0], exception));
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    controller->setAttachedWindowHeight(height);

    return JSValueMakeUndefined(ctx);
}

static JSValueRef wrapCallback(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* /*exception*/)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    if (argumentCount < 1)
        return JSValueMakeUndefined(ctx);

    JSLock lock(false);
    return toRef(JSInspectorCallbackWrapper::wrap(toJS(ctx), toJS(arguments[0])));
}

#if ENABLE(JAVASCRIPT_DEBUGGER)
static JSValueRef currentCallFrame(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t /*argumentCount*/, const JSValueRef[] /*arguments*/, JSValueRef* /*exception*/)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    JavaScriptCallFrame* callFrame = controller->currentCallFrame();
    if (!callFrame || !callFrame->isValid())
        return JSValueMakeNull(ctx);

    ExecState* globalExec = callFrame->scopeChain()->globalObject()->globalExec();

    JSLock lock(false);
    return toRef(JSInspectedObjectWrapper::wrap(globalExec, toJS(toJS(ctx), callFrame)));
}

static JSValueRef setPauseOnExceptions(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* /*exception*/)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    if (argumentCount < 1)
        return JSValueMakeUndefined(ctx);

    controller->setPauseOnExceptions(JSValueToBoolean(ctx, arguments[0]));

    return JSValueMakeUndefined(ctx);
}

static JSValueRef addBreakpoint(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    if (argumentCount < 2)
        return JSValueMakeUndefined(ctx);

    double sourceID = JSValueToNumber(ctx, arguments[0], exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    double lineNumber = JSValueToNumber(ctx, arguments[1], exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    controller->addBreakpoint(static_cast<int>(sourceID), static_cast<unsigned>(lineNumber));

    return JSValueMakeUndefined(ctx);
}

static JSValueRef removeBreakpoint(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    if (argumentCount < 2)
        return JSValueMakeUndefined(ctx);

    double sourceID = JSValueToNumber(ctx, arguments[0], exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    double lineNumber = JSValueToNumber(ctx, arguments[1], exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    controller->removeBreakpoint(static_cast<int>(sourceID), static_cast<unsigned>(lineNumber));

    return JSValueMakeUndefined(ctx);
}
#endif

static JSValueRef profiles(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t /*argumentCount*/, const JSValueRef[] /*arguments*/, JSValueRef* exception)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    JSLock lock(false);

    const Vector<RefPtr<Profile> >& profiles = controller->profiles();

    JSObjectRef global = JSContextGetGlobalObject(ctx);

    JSValueRef arrayProperty = JSObjectGetProperty(ctx, global, jsStringRef("Array").get(), exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    JSObjectRef arrayConstructor = JSValueToObject(ctx, arrayProperty, exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    JSObjectRef result = JSObjectCallAsConstructor(ctx, arrayConstructor, 0, 0, exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    JSValueRef pushProperty = JSObjectGetProperty(ctx, result, jsStringRef("push").get(), exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    JSObjectRef pushFunction = JSValueToObject(ctx, pushProperty, exception);
    if (exception && *exception)
        return JSValueMakeUndefined(ctx);

    for (size_t i = 0; i < profiles.size(); ++i) {
        JSValueRef arg0 = toRef(toJS(toJS(ctx), profiles[i].get()));
        JSObjectCallAsFunction(ctx, pushFunction, result, 1, &arg0, exception);
        if (exception && *exception)
            return JSValueMakeUndefined(ctx);
    }

    return result;
}

// InspectorController Class

static unsigned s_inspectorControllerCount;
static HashMap<String, InspectorController::Setting*>* s_settingCache;

InspectorController::InspectorController(Page* page, InspectorClient* client)
    : m_inspectedPage(page)
    , m_client(client)
    , m_page(0)
    , m_scriptObject(0)
    , m_controllerScriptObject(0)
    , m_scriptContext(0)
    , m_windowVisible(false)
#if ENABLE(JAVASCRIPT_DEBUGGER)
    , m_debuggerEnabled(false)
    , m_attachDebuggerWhenShown(false)
#endif
    , m_profilerEnabled(false)
    , m_recordingUserInitiatedProfile(false)
    , m_showAfterVisible(ElementsPanel)
    , m_nextIdentifier(-2)
    , m_groupLevel(0)
    , m_searchingForNode(false)
    , m_currentUserInitiatedProfileNumber(-1)
    , m_nextUserInitiatedProfileNumber(1)
    , m_previousMessage(0)
    , m_startProfiling(this, &InspectorController::startUserInitiatedProfiling)
{
    ASSERT_ARG(page, page);
    ASSERT_ARG(client, client);
    ++s_inspectorControllerCount;
}

InspectorController::~InspectorController()
{
    m_client->inspectorDestroyed();

    if (m_scriptContext) {
        JSValueRef exception = 0;

        JSObjectRef global = JSContextGetGlobalObject(m_scriptContext);
        JSValueRef controllerProperty = JSObjectGetProperty(m_scriptContext, global, jsStringRef("InspectorController").get(), &exception);
        if (!HANDLE_EXCEPTION(m_scriptContext, exception)) {
            if (JSObjectRef controller = JSValueToObject(m_scriptContext, controllerProperty, &exception)) {
                if (!HANDLE_EXCEPTION(m_scriptContext, exception))
                    JSObjectSetPrivate(controller, 0);
            }
        }
    }

    if (m_page)
        m_page->setParentInspectorController(0);

    // m_inspectedPage should have been cleared in inspectedPageDestroyed().
    ASSERT(!m_inspectedPage);

    deleteAllValues(m_frameResources);
    deleteAllValues(m_consoleMessages);

    ASSERT(s_inspectorControllerCount);
    --s_inspectorControllerCount;

    if (!s_inspectorControllerCount && s_settingCache) {
        deleteAllValues(*s_settingCache);
        delete s_settingCache;
        s_settingCache = 0;
    }
}

void InspectorController::inspectedPageDestroyed()
{
    close();

    ASSERT(m_inspectedPage);
    m_inspectedPage = 0;
}

bool InspectorController::enabled() const
{
    if (!m_inspectedPage)
        return false;
    return m_inspectedPage->settings()->developerExtrasEnabled();
}

const InspectorController::Setting& InspectorController::setting(const String& key) const
{
    if (!s_settingCache)
        s_settingCache = new HashMap<String, Setting*>;

    if (Setting* cachedSetting = s_settingCache->get(key))
        return *cachedSetting;

    Setting* newSetting = new Setting;
    s_settingCache->set(key, newSetting);

    m_client->populateSetting(key, *newSetting);

    return *newSetting;
}

void InspectorController::setSetting(const String& key, const Setting& setting)
{
    if (setting.type() == Setting::NoType) {
        if (s_settingCache) {
            Setting* cachedSetting = s_settingCache->get(key);
            if (cachedSetting) {
                s_settingCache->remove(key);
                delete cachedSetting;
            }
        }

        m_client->removeSetting(key);
        return;
    }

    if (!s_settingCache)
        s_settingCache = new HashMap<String, Setting*>;

    if (Setting* cachedSetting = s_settingCache->get(key))
        *cachedSetting = setting;
    else
        s_settingCache->set(key, new Setting(setting));

    m_client->storeSetting(key, setting);
}

String InspectorController::localizedStringsURL()
{
    if (!enabled())
        return String();
    return m_client->localizedStringsURL();
}

// Trying to inspect something in a frame with JavaScript disabled would later lead to
// crashes trying to create JavaScript wrappers. Some day we could fix this issue, but
// for now prevent crashes here by never targeting a node in such a frame.
static bool canPassNodeToJavaScript(Node* node)
{
    if (!node)
        return false;
    Frame* frame = node->document()->frame();
    return frame && frame->script()->isEnabled();
}

void InspectorController::inspect(Node* node)
{
    if (!canPassNodeToJavaScript(node) || !enabled())
        return;

    show();

    if (node->nodeType() != Node::ELEMENT_NODE && node->nodeType() != Node::DOCUMENT_NODE)
        node = node->parentNode();
    m_nodeToFocus = node;

    if (!m_scriptObject) {
        m_showAfterVisible = ElementsPanel;
        return;
    }

    if (windowVisible())
        focusNode();
}

void InspectorController::focusNode()
{
    if (!enabled())
        return;

    ASSERT(m_scriptContext);
    ASSERT(m_scriptObject);
    ASSERT(m_nodeToFocus);

    Frame* frame = m_nodeToFocus->document()->frame();
    if (!frame)
        return;

    ExecState* exec = toJSDOMWindow(frame)->globalExec();

    JSValueRef arg0;

    {
        JSC::JSLock lock(false);
        arg0 = toRef(JSInspectedObjectWrapper::wrap(exec, toJS(exec, m_nodeToFocus.get())));
    }

    m_nodeToFocus = 0;

    JSValueRef exception = 0;
    callFunction(m_scriptContext, m_scriptObject, "updateFocusedNode", 1, &arg0, exception);
}

void InspectorController::highlight(Node* node)
{
    if (!enabled())
        return;
    ASSERT_ARG(node, node);
    m_highlightedNode = node;
    m_client->highlight(node);
}

void InspectorController::hideHighlight()
{
    if (!enabled())
        return;
    m_client->hideHighlight();
}

bool InspectorController::windowVisible()
{
    return m_windowVisible;
}

void InspectorController::setWindowVisible(bool visible, bool attached)
{
    if (visible == m_windowVisible)
        return;

    m_windowVisible = visible;

    if (!m_scriptContext || !m_scriptObject)
        return;

    if (m_windowVisible) {
        setAttachedWindow(attached);
        populateScriptObjects();
        if (m_nodeToFocus)
            focusNode();
#if ENABLE(JAVASCRIPT_DEBUGGER)
        if (m_attachDebuggerWhenShown)
            enableDebugger();
#endif
        if (m_showAfterVisible != CurrentPanel)
            showPanel(m_showAfterVisible);
    } else {
#if ENABLE(JAVASCRIPT_DEBUGGER)
        disableDebugger();
#endif
        resetScriptObjects();
    }

    m_showAfterVisible = CurrentPanel;
}

void InspectorController::addMessageToConsole(MessageSource source, MessageLevel level, ScriptCallStack* callStack)
{
    if (!enabled())
        return;

    addConsoleMessage(callStack->state(), new ConsoleMessage(source, level, callStack, m_groupLevel, level == TraceMessageLevel));
}

void InspectorController::addMessageToConsole(MessageSource source, MessageLevel level, const String& message, unsigned lineNumber, const String& sourceID)
{
    if (!enabled())
        return;

    addConsoleMessage(0, new ConsoleMessage(source, level, message, lineNumber, sourceID, m_groupLevel));
}

void InspectorController::addConsoleMessage(ExecState* exec, ConsoleMessage* consoleMessage)
{
    ASSERT(enabled());
    ASSERT_ARG(consoleMessage, consoleMessage);

    if (m_previousMessage && m_previousMessage->isEqual(exec, consoleMessage)) {
        ++m_previousMessage->repeatCount;
        delete consoleMessage;
    } else {
        m_previousMessage = consoleMessage;
        m_consoleMessages.append(consoleMessage);
    }

    if (windowVisible())
        addScriptConsoleMessage(m_previousMessage);
}

void InspectorController::clearConsoleMessages()
{
    deleteAllValues(m_consoleMessages);
    m_consoleMessages.clear();
    m_previousMessage = 0;
    m_groupLevel = 0;
}

void InspectorController::toggleRecordButton(bool isProfiling)
{
    if (!m_scriptContext)
        return;

    JSValueRef exception = 0;
    JSValueRef isProvingValue = JSValueMakeBoolean(m_scriptContext, isProfiling);
    callFunction(m_scriptContext, m_scriptObject, "setRecordingProfile", 1, &isProvingValue, exception);
}

void InspectorController::startGroup(MessageSource source, ScriptCallStack* callStack)
{    
    ++m_groupLevel;

    addConsoleMessage(callStack->state(), new ConsoleMessage(source, StartGroupMessageLevel, callStack, m_groupLevel));
}

void InspectorController::endGroup(MessageSource source, unsigned lineNumber, const String& sourceURL)
{
    if (m_groupLevel == 0)
        return;

    --m_groupLevel;

    addConsoleMessage(0, new ConsoleMessage(source, EndGroupMessageLevel, String(), lineNumber, sourceURL, m_groupLevel));
}

void InspectorController::addProfile(PassRefPtr<Profile> prpProfile, unsigned lineNumber, const UString& sourceURL)
{
    if (!enabled())
        return;

    RefPtr<Profile> profile = prpProfile;
    m_profiles.append(profile);

    if (windowVisible())
        addScriptProfile(profile.get());

    addProfileMessageToConsole(profile, lineNumber, sourceURL);
}

void InspectorController::addProfileMessageToConsole(PassRefPtr<Profile> prpProfile, unsigned lineNumber, const UString& sourceURL)
{
    RefPtr<Profile> profile = prpProfile;

    UString message = "Profile \"webkit-profile://";
    message += encodeWithURLEscapeSequences(profile->title());
    message += "/";
    message += UString::from(profile->uid());
    message += "\" finished.";
    addMessageToConsole(JSMessageSource, LogMessageLevel, message, lineNumber, sourceURL);
}

void InspectorController::attachWindow()
{
    if (!enabled())
        return;
    m_client->attachWindow();
}

void InspectorController::detachWindow()
{
    if (!enabled())
        return;
    m_client->detachWindow();
}

void InspectorController::setAttachedWindow(bool attached)
{
    if (!enabled() || !m_scriptContext || !m_scriptObject)
        return;

    JSValueRef attachedValue = JSValueMakeBoolean(m_scriptContext, attached);

    JSValueRef exception = 0;
    callFunction(m_scriptContext, m_scriptObject, "setAttachedWindow", 1, &attachedValue, exception);
}

void InspectorController::setAttachedWindowHeight(unsigned height)
{
    if (!enabled())
        return;
    m_client->setAttachedWindowHeight(height);
}

void InspectorController::toggleSearchForNodeInPage()
{
    if (!enabled())
        return;

    m_searchingForNode = !m_searchingForNode;
    if (!m_searchingForNode)
        hideHighlight();
}

void InspectorController::mouseDidMoveOverElement(const HitTestResult& result, unsigned)
{
    if (!enabled() || !m_searchingForNode)
        return;

    Node* node = result.innerNode();
    if (node)
        highlight(node);
}

void InspectorController::handleMousePressOnNode(Node* node)
{
    if (!enabled())
        return;

    ASSERT(m_searchingForNode);
    ASSERT(node);
    if (!node)
        return;

    // inspect() will implicitly call ElementsPanel's focusedNodeChanged() and the hover feedback will be stopped there.
    inspect(node);
}

void InspectorController::inspectedWindowScriptObjectCleared(Frame* frame)
{
    if (!enabled() || !m_scriptContext || !m_scriptObject)
        return;

    JSDOMWindow* win = toJSDOMWindow(frame);
    ExecState* exec = win->globalExec();

    JSValueRef arg0;

    {
        JSC::JSLock lock(false);
        arg0 = toRef(JSInspectedObjectWrapper::wrap(exec, win));
    }

    JSValueRef exception = 0;
    callFunction(m_scriptContext, m_scriptObject, "inspectedWindowCleared", 1, &arg0, exception);
}

void InspectorController::windowScriptObjectAvailable()
{
    if (!m_page || !enabled())
        return;

    m_scriptContext = toRef(m_page->mainFrame()->script()->globalObject()->globalExec());

    JSObjectRef global = JSContextGetGlobalObject(m_scriptContext);
    ASSERT(global);

    static JSStaticFunction staticFunctions[] = {
        // SIMPLE_INSPECTOR_CALLBACK
        { "hideDOMNodeHighlight", WebCore::hideDOMNodeHighlight, kJSPropertyAttributeNone },
        { "loaded", WebCore::loaded, kJSPropertyAttributeNone },
        { "windowUnloading", WebCore::unloading, kJSPropertyAttributeNone },
        { "attach", WebCore::attach, kJSPropertyAttributeNone },
        { "detach", WebCore::detach, kJSPropertyAttributeNone },
#if ENABLE(JAVASCRIPT_DEBUGGER)
        { "enableDebugger", WebCore::enableDebugger, kJSPropertyAttributeNone },
        { "disableDebugger", WebCore::disableDebugger, kJSPropertyAttributeNone },
        { "pauseInDebugger", WebCore::pauseInDebugger, kJSPropertyAttributeNone },
        { "resumeDebugger", WebCore::resumeDebugger, kJSPropertyAttributeNone },
        { "stepOverStatementInDebugger", WebCore::stepOverStatementInDebugger, kJSPropertyAttributeNone },
        { "stepIntoStatementInDebugger", WebCore::stepIntoStatementInDebugger, kJSPropertyAttributeNone },
        { "stepOutOfFunctionInDebugger", WebCore::stepOutOfFunctionInDebugger, kJSPropertyAttributeNone },
#endif
        { "closeWindow", WebCore::closeWindow, kJSPropertyAttributeNone },
        { "clearMessages", WebCore::clearMessages, kJSPropertyAttributeNone },
        { "startProfiling", WebCore::startProfiling, kJSPropertyAttributeNone },
        { "stopProfiling", WebCore::stopProfiling, kJSPropertyAttributeNone },
        { "enableProfiler", WebCore::enableProfiler, kJSPropertyAttributeNone },
        { "disableProfiler", WebCore::disableProfiler, kJSPropertyAttributeNone },
        { "toggleNodeSearch", WebCore::toggleNodeSearch, kJSPropertyAttributeNone },

        // BOOL_INSPECTOR_CALLBACK
#if ENABLE(JAVASCRIPT_DEBUGGER)
        { "debuggerEnabled", WebCore::debuggerEnabled, kJSPropertyAttributeNone },
        { "pauseOnExceptions", WebCore::pauseOnExceptions, kJSPropertyAttributeNone },
#endif
        { "profilerEnabled", WebCore::profilerEnabled, kJSPropertyAttributeNone },
        { "isWindowVisible", WebCore::isWindowVisible, kJSPropertyAttributeNone },
        { "searchingForNode", WebCore::searchingForNode, kJSPropertyAttributeNone },

        // Custom callbacks
        { "addResourceSourceToFrame", WebCore::addResourceSourceToFrame, kJSPropertyAttributeNone },
        { "addSourceToFrame", WebCore::addSourceToFrame, kJSPropertyAttributeNone },
        { "getResourceDocumentNode", WebCore::getResourceDocumentNode, kJSPropertyAttributeNone },
        { "highlightDOMNode", WebCore::highlightDOMNode, kJSPropertyAttributeNone },
        { "search", WebCore::search, kJSPropertyAttributeNone },
#if ENABLE(DATABASE)
        { "databaseTableNames", WebCore::databaseTableNames, kJSPropertyAttributeNone },
#endif
        { "setting", WebCore::setting, kJSPropertyAttributeNone },
        { "setSetting", WebCore::setSetting, kJSPropertyAttributeNone },
        { "inspectedWindow", WebCore::inspectedWindow, kJSPropertyAttributeNone },
        { "localizedStringsURL", WebCore::localizedStrings, kJSPropertyAttributeNone },
        { "platform", WebCore::platform, kJSPropertyAttributeNone },
        { "moveByUnrestricted", WebCore::moveByUnrestricted, kJSPropertyAttributeNone },
        { "setAttachedWindowHeight", WebCore::setAttachedWindowHeight, kJSPropertyAttributeNone },
        { "wrapCallback", WebCore::wrapCallback, kJSPropertyAttributeNone },
#if ENABLE(JAVASCRIPT_DEBUGGER)
        { "currentCallFrame", WebCore::currentCallFrame, kJSPropertyAttributeNone },
        { "setPauseOnExceptions", WebCore::setPauseOnExceptions, kJSPropertyAttributeNone },
        { "addBreakpoint", WebCore::addBreakpoint, kJSPropertyAttributeNone },
        { "removeBreakpoint", WebCore::removeBreakpoint, kJSPropertyAttributeNone },
#endif
        { "profiles", WebCore::profiles, kJSPropertyAttributeNone },
        { 0, 0, 0 }
    };

    JSClassDefinition inspectorControllerDefinition = {
        0, kJSClassAttributeNone, "InspectorController", 0, 0, staticFunctions,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

    JSClassRef controllerClass = JSClassCreate(&inspectorControllerDefinition);
    ASSERT(controllerClass);

    m_controllerScriptObject = JSObjectMake(m_scriptContext, controllerClass, reinterpret_cast<void*>(this));
    ASSERT(m_controllerScriptObject);

    JSObjectSetProperty(m_scriptContext, global, jsStringRef("InspectorController").get(), m_controllerScriptObject, kJSPropertyAttributeNone, 0);
}

void InspectorController::scriptObjectReady()
{
    ASSERT(m_scriptContext);
    if (!m_scriptContext)
        return;

    JSObjectRef global = JSContextGetGlobalObject(m_scriptContext);
    ASSERT(global);

    JSValueRef exception = 0;

    JSValueRef inspectorValue = JSObjectGetProperty(m_scriptContext, global, jsStringRef("WebInspector").get(), &exception);
    if (HANDLE_EXCEPTION(m_scriptContext, exception))
        return;

    ASSERT(inspectorValue);
    if (!inspectorValue)
        return;

    m_scriptObject = JSValueToObject(m_scriptContext, inspectorValue, &exception);
    if (HANDLE_EXCEPTION(m_scriptContext, exception))
        return;

    ASSERT(m_scriptObject);

    JSValueProtect(m_scriptContext, m_scriptObject);

    // Make sure our window is visible now that the page loaded
    showWindow();
}

void InspectorController::show()
{
    if (!enabled())
        return;

    if (!m_page) {
        m_page = m_client->createPage();
        if (!m_page)
            return;
        m_page->setParentInspectorController(this);

        // showWindow() will be called after the page loads in scriptObjectReady()
        return;
    }

    showWindow();
}

void InspectorController::showPanel(SpecialPanels panel)
{
    if (!enabled())
        return;

    show();

    if (!m_scriptObject) {
        m_showAfterVisible = panel;
        return;
    }

    if (panel == CurrentPanel)
        return;

    const char* showFunctionName;
    switch (panel) {
        case ConsolePanel:
            showFunctionName = "showConsole";
            break;
        case DatabasesPanel:
            showFunctionName = "showDatabasesPanel";
            break;
        case ElementsPanel:
            showFunctionName = "showElementsPanel";
            break;
        case ProfilesPanel:
            showFunctionName = "showProfilesPanel";
            break;
        case ResourcesPanel:
            showFunctionName = "showResourcesPanel";
            break;
        case ScriptsPanel:
            showFunctionName = "showScriptsPanel";
            break;
        default:
            ASSERT_NOT_REACHED();
            showFunctionName = 0;
    }

    if (showFunctionName)
        callSimpleFunction(m_scriptContext, m_scriptObject, showFunctionName);
}

void InspectorController::close()
{
    if (!enabled())
        return;

    stopUserInitiatedProfiling();
#if ENABLE(JAVASCRIPT_DEBUGGER)
    disableDebugger();
#endif
    closeWindow();

    if (m_scriptContext && m_scriptObject)
        JSValueUnprotect(m_scriptContext, m_scriptObject);

    m_scriptObject = 0;
    m_scriptContext = 0;
}

void InspectorController::showWindow()
{
    ASSERT(enabled());
    m_client->showWindow();
}

void InspectorController::closeWindow()
{
    m_client->closeWindow();
}

void InspectorController::startUserInitiatedProfilingSoon()
{
    m_startProfiling.startOneShot(0);
}

void InspectorController::startUserInitiatedProfiling(Timer<InspectorController>*)
{
    if (!enabled())
        return;

    if (!profilerEnabled()) {
        enableProfiler(true);
        JavaScriptDebugServer::shared().recompileAllJSFunctions();
    }

    m_recordingUserInitiatedProfile = true;
    m_currentUserInitiatedProfileNumber = m_nextUserInitiatedProfileNumber++;

    UString title = UserInitiatedProfileName;
    title += ".";
    title += UString::from(m_currentUserInitiatedProfileNumber);

    ExecState* exec = toJSDOMWindow(m_inspectedPage->mainFrame())->globalExec();
    Profiler::profiler()->startProfiling(exec, title);

    toggleRecordButton(true);
}

void InspectorController::stopUserInitiatedProfiling()
{
    if (!enabled())
        return;

    m_recordingUserInitiatedProfile = false;

    UString title =  UserInitiatedProfileName;
    title += ".";
    title += UString::from(m_currentUserInitiatedProfileNumber);

    ExecState* exec = toJSDOMWindow(m_inspectedPage->mainFrame())->globalExec();
    RefPtr<Profile> profile = Profiler::profiler()->stopProfiling(exec, title);
    if (profile)
        addProfile(profile, 0, UString());

    toggleRecordButton(false);
}

void InspectorController::enableProfiler(bool skipRecompile)
{
    if (m_profilerEnabled)
        return;

    m_profilerEnabled = true;

    if (!skipRecompile)
        JavaScriptDebugServer::shared().recompileAllJSFunctionsSoon();

    if (m_scriptContext && m_scriptObject)
        callSimpleFunction(m_scriptContext, m_scriptObject, "profilerWasEnabled");
}

void InspectorController::disableProfiler()
{
    if (!m_profilerEnabled)
        return;

    m_profilerEnabled = false;

    JavaScriptDebugServer::shared().recompileAllJSFunctionsSoon();

    if (m_scriptContext && m_scriptObject)
        callSimpleFunction(m_scriptContext, m_scriptObject, "profilerWasDisabled");
}

static void addHeaders(JSContextRef context, JSObjectRef object, const HTTPHeaderMap& headers, JSValueRef* exception)
{
    ASSERT_ARG(context, context);
    ASSERT_ARG(object, object);

    HTTPHeaderMap::const_iterator end = headers.end();
    for (HTTPHeaderMap::const_iterator it = headers.begin(); it != end; ++it) {
        JSValueRef value = JSValueMakeString(context, jsStringRef(it->second).get());
        JSObjectSetProperty(context, object, jsStringRef((it->first).string()).get(), value, kJSPropertyAttributeNone, exception);
        if (exception && *exception)
            return;
    }
}

static JSObjectRef scriptObjectForRequest(JSContextRef context, const InspectorResource* resource, JSValueRef* exception)
{
    ASSERT_ARG(context, context);

    JSObjectRef object = JSObjectMake(context, 0, 0);
    addHeaders(context, object, resource->requestHeaderFields, exception);

    return object;
}

static JSObjectRef scriptObjectForResponse(JSContextRef context, const InspectorResource* resource, JSValueRef* exception)
{
    ASSERT_ARG(context, context);

    JSObjectRef object = JSObjectMake(context, 0, 0);
    addHeaders(context, object, resource->responseHeaderFields, exception);

    return object;
}

JSObjectRef InspectorController::addScriptResource(InspectorResource* resource)
{
    ASSERT_ARG(resource, resource);

    ASSERT(m_scriptContext);
    ASSERT(m_scriptObject);
    if (!m_scriptContext || !m_scriptObject)
        return 0;

    if (!resource->scriptObject) {
        JSValueRef exception = 0;

        JSValueRef resourceProperty = JSObjectGetProperty(m_scriptContext, m_scriptObject, jsStringRef("Resource").get(), &exception);
        if (HANDLE_EXCEPTION(m_scriptContext, exception))
            return 0;

        JSObjectRef resourceConstructor = JSValueToObject(m_scriptContext, resourceProperty, &exception);
        if (HANDLE_EXCEPTION(m_scriptContext, exception))
            return 0;

        JSValueRef urlValue = JSValueMakeString(m_scriptContext, jsStringRef(resource->requestURL.string()).get());
        JSValueRef domainValue = JSValueMakeString(m_scriptContext, jsStringRef(resource->requestURL.host()).get());
        JSValueRef pathValue = JSValueMakeString(m_scriptContext, jsStringRef(resource->requestURL.path()).get());
        JSValueRef lastPathComponentValue = JSValueMakeString(m_scriptContext, jsStringRef(resource->requestURL.lastPathComponent()).get());

        JSValueRef identifier = JSValueMakeNumber(m_scriptContext, resource->identifier);
        JSValueRef mainResource = JSValueMakeBoolean(m_scriptContext, m_mainResource == resource);
        JSValueRef cached = JSValueMakeBoolean(m_scriptContext, resource->cached);

        JSObjectRef scriptObject = scriptObjectForRequest(m_scriptContext, resource, &exception);
        if (HANDLE_EXCEPTION(m_scriptContext, exception))
            return 0;

        JSValueRef arguments[] = { scriptObject, urlValue, domainValue, pathValue, lastPathComponentValue, identifier, mainResource, cached };
        JSObjectRef result = JSObjectCallAsConstructor(m_scriptContext, resourceConstructor, 8, arguments, &exception);
        if (HANDLE_EXCEPTION(m_scriptContext, exception))
            return 0;

        ASSERT(result);

        resource->setScriptObject(m_scriptContext, result);
    }

    JSValueRef exception = 0;
    callFunction(m_scriptContext, m_scriptObject, "addResource", 1, &resource->scriptObject, exception);

    if (exception)
        return 0;

    return resource->scriptObject;
}

JSObjectRef InspectorController::addAndUpdateScriptResource(InspectorResource* resource)
{
    ASSERT_ARG(resource, resource);

    JSObjectRef scriptResource = addScriptResource(resource);
    if (!scriptResource)
        return 0;

    updateScriptResourceResponse(resource);
    updateScriptResource(resource, resource->length);
    updateScriptResource(resource, resource->startTime, resource->responseReceivedTime, resource->endTime);
    updateScriptResource(resource, resource->finished, resource->failed);
    return scriptResource;
}

void InspectorController::removeScriptResource(InspectorResource* resource)
{
    ASSERT(m_scriptContext);
    ASSERT(m_scriptObject);
    if (!m_scriptContext || !m_scriptObject)
        return;

    ASSERT(resource);
    ASSERT(resource->scriptObject);
    if (!resource || !resource->scriptObject)
        return;

    JSObjectRef scriptObject = resource->scriptObject;
    resource->setScriptObject(0, 0);

    JSValueRef exception = 0;
    callFunction(m_scriptContext, m_scriptObject, "removeResource", 1, &scriptObject, exception);
}

static void updateResourceRequest(InspectorResource* resource, const ResourceRequest& request)
{
    resource->requestHeaderFields = request.httpHeaderFields();
    resource->requestURL = request.url();
}

static void updateResourceResponse(InspectorResource* resource, const ResourceResponse& response)
{
    resource->expectedContentLength = response.expectedContentLength();
    resource->mimeType = response.mimeType();
    resource->responseHeaderFields = response.httpHeaderFields();
    resource->responseStatusCode = response.httpStatusCode();
    resource->suggestedFilename = response.suggestedFilename();
}

void InspectorController::updateScriptResourceRequest(InspectorResource* resource)
{
    ASSERT(resource->scriptObject);
    ASSERT(m_scriptContext);
    if (!resource->scriptObject || !m_scriptContext)
        return;

    JSValueRef urlValue = JSValueMakeString(m_scriptContext, jsStringRef(resource->requestURL.string()).get());
    JSValueRef domainValue = JSValueMakeString(m_scriptContext, jsStringRef(resource->requestURL.host()).get());
    JSValueRef pathValue = JSValueMakeString(m_scriptContext, jsStringRef(resource->requestURL.path()).get());
    JSValueRef lastPathComponentValue = JSValueMakeString(m_scriptContext, jsStringRef(resource->requestURL.lastPathComponent()).get());

    JSValueRef mainResourceValue = JSValueMakeBoolean(m_scriptContext, m_mainResource == resource);

    JSValueRef exception = 0;

    JSObjectSetProperty(m_scriptContext, resource->scriptObject, jsStringRef("url").get(), urlValue, kJSPropertyAttributeNone, &exception);
    if (HANDLE_EXCEPTION(m_scriptContext, exception))
        return;

    JSObjectSetProperty(m_scriptContext, resource->scriptObject, jsStringRef("domain").get(), domainValue, kJSPropertyAttributeNone, &exception);
    if (HANDLE_EXCEPTION(m_scriptContext, exception))
        return;

    JSObjectSetProperty(m_scriptContext, resource->scriptObject, jsStringRef("path").get(), pathValue, kJSPropertyAttributeNone, &exception);
    if (HANDLE_EXCEPTION(m_scriptContext, exception))
        return;

    JSObjectSetProperty(m_scriptContext, resource->scriptObject, jsStringRef("lastPathComponent").get(), lastPathComponentValue, kJSPropertyAttributeNone, &exception);
    if (HANDLE_EXCEPTION(m_scriptContext, exception))
        return;

    JSObjectRef scriptObject = scriptObjectForRequest(m_scriptContext, resource, &exception);
    if (HANDLE_EXCEPTION(m_scriptContext, exception))
        return;

    JSObjectSetProperty(m_scriptContext, resource->scriptObject, jsStringRef("requestHeaders").get(), scriptObject, kJSPropertyAttributeNone, &exception);
    if (HANDLE_EXCEPTION(m_scriptContext, exception))
        return;

    JSObjectSetProperty(m_scriptContext, resource->scriptObject, jsStringRef("mainResource").get(), mainResourceValue, kJSPropertyAttributeNone, &exception);
    HANDLE_EXCEPTION(m_scriptContext, exception);
}

void InspectorController::updateScriptResourceResponse(InspectorResource* resource)
{
    ASSERT(resource->scriptObject);
    ASSERT(m_scriptContext);
    if (!resource->scriptObject || !m_scriptContext)
        return;

    JSValueRef mimeTypeValue = JSValueMakeString(m_scriptContext, jsStringRef(resource->mimeType).get());

    JSValueRef suggestedFilenameValue = JSValueMakeString(m_scriptContext, jsStringRef(resource->suggestedFilename).get());

    JSValueRef expectedContentLengthValue = JSValueMakeNumber(m_scriptContext, static_cast<double>(resource->expectedContentLength));
    JSValueRef statusCodeValue = JSValueMakeNumber(m_scriptContext, resource->responseStatusCode);

    JSValueRef exception = 0;

    JSObjectSetProperty(m_scriptContext, resource->scriptObject, jsStringRef("mimeType").get(), mimeTypeValue, kJSPropertyAttributeNone, &exception);
    if (HANDLE_EXCEPTION(m_scriptContext, exception))
        return;

    JSObjectSetProperty(m_scriptContext, resource->scriptObject, jsStringRef("suggestedFilename").get(), suggestedFilenameValue, kJSPropertyAttributeNone, &exception);
    if (HANDLE_EXCEPTION(m_scriptContext, exception))
        return;

    JSObjectSetProperty(m_scriptContext, resource->scriptObject, jsStringRef("expectedContentLength").get(), expectedContentLengthValue, kJSPropertyAttributeNone, &exception);
    if (HANDLE_EXCEPTION(m_scriptContext, exception))
        return;

    JSObjectSetProperty(m_scriptContext, resource->scriptObject, jsStringRef("statusCode").get(), statusCodeValue, kJSPropertyAttributeNone, &exception);
    if (HANDLE_EXCEPTION(m_scriptContext, exception))
        return;

    JSObjectRef scriptObject = scriptObjectForResponse(m_scriptContext, resource, &exception);
    if (HANDLE_EXCEPTION(m_scriptContext, exception))
        return;

    JSObjectSetProperty(m_scriptContext, resource->scriptObject, jsStringRef("responseHeaders").get(), scriptObject, kJSPropertyAttributeNone, &exception);
    if (HANDLE_EXCEPTION(m_scriptContext, exception))
        return;

    updateScriptResourceType(resource);
}

void InspectorController::updateScriptResourceType(InspectorResource* resource)
{
    ASSERT(resource->scriptObject);
    ASSERT(m_scriptContext);
    if (!resource->scriptObject || !m_scriptContext)
        return;

    JSValueRef exception = 0;

    JSValueRef typeValue = JSValueMakeNumber(m_scriptContext, resource->type());
    JSObjectSetProperty(m_scriptContext, resource->scriptObject, jsStringRef("type").get(), typeValue, kJSPropertyAttributeNone, &exception);
    HANDLE_EXCEPTION(m_scriptContext, exception);
}

void InspectorController::updateScriptResource(InspectorResource* resource, int length)
{
    ASSERT(resource->scriptObject);
    ASSERT(m_scriptContext);
    if (!resource->scriptObject || !m_scriptContext)
        return;

    JSValueRef lengthValue = JSValueMakeNumber(m_scriptContext, length);

    JSValueRef exception = 0;

    JSObjectSetProperty(m_scriptContext, resource->scriptObject, jsStringRef("contentLength").get(), lengthValue, kJSPropertyAttributeNone, &exception);
    HANDLE_EXCEPTION(m_scriptContext, exception);
}

void InspectorController::updateScriptResource(InspectorResource* resource, bool finished, bool failed)
{
    ASSERT(resource->scriptObject);
    ASSERT(m_scriptContext);
    if (!resource->scriptObject || !m_scriptContext)
        return;

    JSValueRef failedValue = JSValueMakeBoolean(m_scriptContext, failed);
    JSValueRef finishedValue = JSValueMakeBoolean(m_scriptContext, finished);

    JSValueRef exception = 0;

    JSObjectSetProperty(m_scriptContext, resource->scriptObject, jsStringRef("failed").get(), failedValue, kJSPropertyAttributeNone, &exception);
    if (HANDLE_EXCEPTION(m_scriptContext, exception))
        return;

    JSObjectSetProperty(m_scriptContext, resource->scriptObject, jsStringRef("finished").get(), finishedValue, kJSPropertyAttributeNone, &exception);
    HANDLE_EXCEPTION(m_scriptContext, exception);
}

void InspectorController::updateScriptResource(InspectorResource* resource, double startTime, double responseReceivedTime, double endTime)
{
    ASSERT(resource->scriptObject);
    ASSERT(m_scriptContext);
    if (!resource->scriptObject || !m_scriptContext)
        return;

    JSValueRef startTimeValue = JSValueMakeNumber(m_scriptContext, startTime);
    JSValueRef responseReceivedTimeValue = JSValueMakeNumber(m_scriptContext, responseReceivedTime);
    JSValueRef endTimeValue = JSValueMakeNumber(m_scriptContext, endTime);

    JSValueRef exception = 0;

    JSObjectSetProperty(m_scriptContext, resource->scriptObject, jsStringRef("startTime").get(), startTimeValue, kJSPropertyAttributeNone, &exception);
    if (HANDLE_EXCEPTION(m_scriptContext, exception))
        return;

    JSObjectSetProperty(m_scriptContext, resource->scriptObject, jsStringRef("responseReceivedTime").get(), responseReceivedTimeValue, kJSPropertyAttributeNone, &exception);
    if (HANDLE_EXCEPTION(m_scriptContext, exception))
        return;

    JSObjectSetProperty(m_scriptContext, resource->scriptObject, jsStringRef("endTime").get(), endTimeValue, kJSPropertyAttributeNone, &exception);
    HANDLE_EXCEPTION(m_scriptContext, exception);
}

void InspectorController::populateScriptObjects()
{
    ASSERT(m_scriptContext);
    if (!m_scriptContext)
        return;

    ResourcesMap::iterator resourcesEnd = m_resources.end();
    for (ResourcesMap::iterator it = m_resources.begin(); it != resourcesEnd; ++it)
        addAndUpdateScriptResource(it->second.get());

    unsigned messageCount = m_consoleMessages.size();
    for (unsigned i = 0; i < messageCount; ++i)
        addScriptConsoleMessage(m_consoleMessages[i]);

#if ENABLE(DATABASE)
    DatabaseResourcesSet::iterator databasesEnd = m_databaseResources.end();
    for (DatabaseResourcesSet::iterator it = m_databaseResources.begin(); it != databasesEnd; ++it)
        addDatabaseScriptResource((*it).get());
#endif

    callSimpleFunction(m_scriptContext, m_scriptObject, "populateInterface");
}

#if ENABLE(DATABASE)
JSObjectRef InspectorController::addDatabaseScriptResource(InspectorDatabaseResource* resource)
{
    ASSERT_ARG(resource, resource);

    if (resource->scriptObject)
        return resource->scriptObject;

    ASSERT(m_scriptContext);
    ASSERT(m_scriptObject);
    if (!m_scriptContext || !m_scriptObject)
        return 0;

    Frame* frame = resource->database->document()->frame();
    if (!frame)
        return 0;

    JSValueRef exception = 0;

    JSValueRef databaseProperty = JSObjectGetProperty(m_scriptContext, m_scriptObject, jsStringRef("Database").get(), &exception);
    if (HANDLE_EXCEPTION(m_scriptContext, exception))
        return 0;

    JSObjectRef databaseConstructor = JSValueToObject(m_scriptContext, databaseProperty, &exception);
    if (HANDLE_EXCEPTION(m_scriptContext, exception))
        return 0;

    ExecState* exec = toJSDOMWindow(frame)->globalExec();

    JSValueRef database;

    {
        JSC::JSLock lock(false);
        database = toRef(JSInspectedObjectWrapper::wrap(exec, toJS(exec, resource->database.get())));
    }

    JSValueRef domainValue = JSValueMakeString(m_scriptContext, jsStringRef(resource->domain).get());
    JSValueRef nameValue = JSValueMakeString(m_scriptContext, jsStringRef(resource->name).get());
    JSValueRef versionValue = JSValueMakeString(m_scriptContext, jsStringRef(resource->version).get());

    JSValueRef arguments[] = { database, domainValue, nameValue, versionValue };
    JSObjectRef result = JSObjectCallAsConstructor(m_scriptContext, databaseConstructor, 4, arguments, &exception);
    if (HANDLE_EXCEPTION(m_scriptContext, exception))
        return 0;

    ASSERT(result);

    callFunction(m_scriptContext, m_scriptObject, "addDatabase", 1, &result, exception);

    if (exception)
        return 0;

    resource->setScriptObject(m_scriptContext, result);

    return result;
}

void InspectorController::removeDatabaseScriptResource(InspectorDatabaseResource* resource)
{
    ASSERT(m_scriptContext);
    ASSERT(m_scriptObject);
    if (!m_scriptContext || !m_scriptObject)
        return;

    ASSERT(resource);
    ASSERT(resource->scriptObject);
    if (!resource || !resource->scriptObject)
        return;

    JSObjectRef scriptObject = resource->scriptObject;
    resource->setScriptObject(0, 0);

    JSValueRef exception = 0;
    callFunction(m_scriptContext, m_scriptObject, "removeDatabase", 1, &scriptObject, exception);
}
#endif

void InspectorController::addScriptConsoleMessage(const ConsoleMessage* message)
{
    ASSERT_ARG(message, message);

    JSValueRef exception = 0;

    JSValueRef messageConstructorProperty = JSObjectGetProperty(m_scriptContext, m_scriptObject, jsStringRef("ConsoleMessage").get(), &exception);
    if (HANDLE_EXCEPTION(m_scriptContext, exception))
        return;

    JSObjectRef messageConstructor = JSValueToObject(m_scriptContext, messageConstructorProperty, &exception);
    if (HANDLE_EXCEPTION(m_scriptContext, exception))
        return;

    JSValueRef sourceValue = JSValueMakeNumber(m_scriptContext, message->source);
    JSValueRef levelValue = JSValueMakeNumber(m_scriptContext, message->level);
    JSValueRef lineValue = JSValueMakeNumber(m_scriptContext, message->line);
    JSValueRef urlValue = JSValueMakeString(m_scriptContext, jsStringRef(message->url).get());
    JSValueRef groupLevelValue = JSValueMakeNumber(m_scriptContext, message->groupLevel);
    JSValueRef repeatCountValue = JSValueMakeNumber(m_scriptContext, message->repeatCount);

    static const unsigned maximumMessageArguments = 256;
    JSValueRef arguments[maximumMessageArguments];
    unsigned argumentCount = 0;
    arguments[argumentCount++] = sourceValue;
    arguments[argumentCount++] = levelValue;
    arguments[argumentCount++] = lineValue;
    arguments[argumentCount++] = urlValue;
    arguments[argumentCount++] = groupLevelValue;
    arguments[argumentCount++] = repeatCountValue;

    if (!message->frames.isEmpty()) {
        unsigned remainingSpaceInArguments = maximumMessageArguments - argumentCount;
        unsigned argumentsToAdd = min(remainingSpaceInArguments, static_cast<unsigned>(message->frames.size()));
        for (unsigned i = 0; i < argumentsToAdd; ++i)
            arguments[argumentCount++] = JSValueMakeString(m_scriptContext, jsStringRef(message->frames[i]).get());
    } else if (!message->wrappedArguments.isEmpty()) {
        unsigned remainingSpaceInArguments = maximumMessageArguments - argumentCount;
        unsigned argumentsToAdd = min(remainingSpaceInArguments, static_cast<unsigned>(message->wrappedArguments.size()));
        for (unsigned i = 0; i < argumentsToAdd; ++i)
            arguments[argumentCount++] = toRef(message->wrappedArguments[i]);
    } else {
        JSValueRef messageValue = JSValueMakeString(m_scriptContext, jsStringRef(message->message).get());
        arguments[argumentCount++] = messageValue;
    }

    JSObjectRef messageObject = JSObjectCallAsConstructor(m_scriptContext, messageConstructor, argumentCount, arguments, &exception);
    if (HANDLE_EXCEPTION(m_scriptContext, exception))
        return;

    callFunction(m_scriptContext, m_scriptObject, "addMessageToConsole", 1, &messageObject, exception);
}

void InspectorController::addScriptProfile(Profile* profile)
{
    JSLock lock(false);
    JSValueRef exception = 0;
    JSValueRef profileObject = toRef(toJS(toJS(m_scriptContext), profile));
    callFunction(m_scriptContext, m_scriptObject, "addProfile", 1, &profileObject, exception);
}

void InspectorController::resetScriptObjects()
{
    if (!m_scriptContext || !m_scriptObject)
        return;

    ResourcesMap::iterator resourcesEnd = m_resources.end();
    for (ResourcesMap::iterator it = m_resources.begin(); it != resourcesEnd; ++it) {
        InspectorResource* resource = it->second.get();
        resource->setScriptObject(0, 0);
    }

#if ENABLE(DATABASE)
    DatabaseResourcesSet::iterator databasesEnd = m_databaseResources.end();
    for (DatabaseResourcesSet::iterator it = m_databaseResources.begin(); it != databasesEnd; ++it) {
        InspectorDatabaseResource* resource = (*it).get();
        resource->setScriptObject(0, 0);
    }
#endif

    callSimpleFunction(m_scriptContext, m_scriptObject, "reset");
}

void InspectorController::pruneResources(ResourcesMap* resourceMap, DocumentLoader* loaderToKeep)
{
    ASSERT_ARG(resourceMap, resourceMap);

    ResourcesMap mapCopy(*resourceMap);
    ResourcesMap::iterator end = mapCopy.end();
    for (ResourcesMap::iterator it = mapCopy.begin(); it != end; ++it) {
        InspectorResource* resource = (*it).second.get();
        if (resource == m_mainResource)
            continue;

        if (!loaderToKeep || resource->loader != loaderToKeep) {
            removeResource(resource);
            if (windowVisible() && resource->scriptObject)
                removeScriptResource(resource);
        }
    }
}

void InspectorController::didCommitLoad(DocumentLoader* loader)
{
    if (!enabled())
        return;

    ASSERT(m_inspectedPage);

    if (loader->frame() == m_inspectedPage->mainFrame()) {
        m_client->inspectedURLChanged(loader->url().string());

        clearConsoleMessages();

        m_times.clear();
        m_counts.clear();
        m_profiles.clear();

#if ENABLE(DATABASE)
        m_databaseResources.clear();
#endif

        if (windowVisible()) {
            resetScriptObjects();

            if (!loader->isLoadingFromCachedPage()) {
                ASSERT(m_mainResource && m_mainResource->loader == loader);
                // We don't add the main resource until its load is committed. This is
                // needed to keep the load for a user-entered URL from showing up in the
                // list of resources for the page they are navigating away from.
                addAndUpdateScriptResource(m_mainResource.get());
            } else {
                // Pages loaded from the page cache are committed before
                // m_mainResource is the right resource for this load, so we
                // clear it here. It will be re-assigned in
                // identifierForInitialRequest.
                m_mainResource = 0;
            }
        }
    }

    for (Frame* frame = loader->frame(); frame; frame = frame->tree()->traverseNext(loader->frame()))
        if (ResourcesMap* resourceMap = m_frameResources.get(frame))
            pruneResources(resourceMap, loader);
}

void InspectorController::frameDetachedFromParent(Frame* frame)
{
    if (!enabled())
        return;
    if (ResourcesMap* resourceMap = m_frameResources.get(frame))
        removeAllResources(resourceMap);
}

void InspectorController::addResource(InspectorResource* resource)
{
    m_resources.set(resource->identifier, resource);
    m_knownResources.add(resource->requestURL.string());

    Frame* frame = resource->frame.get();
    ResourcesMap* resourceMap = m_frameResources.get(frame);
    if (resourceMap)
        resourceMap->set(resource->identifier, resource);
    else {
        resourceMap = new ResourcesMap;
        resourceMap->set(resource->identifier, resource);
        m_frameResources.set(frame, resourceMap);
    }
}

void InspectorController::removeResource(InspectorResource* resource)
{
    m_resources.remove(resource->identifier);
    m_knownResources.remove(resource->requestURL.string());

    Frame* frame = resource->frame.get();
    ResourcesMap* resourceMap = m_frameResources.get(frame);
    if (!resourceMap) {
        ASSERT_NOT_REACHED();
        return;
    }

    resourceMap->remove(resource->identifier);
    if (resourceMap->isEmpty()) {
        m_frameResources.remove(frame);
        delete resourceMap;
    }
}

void InspectorController::didLoadResourceFromMemoryCache(DocumentLoader* loader, const CachedResource* cachedResource)
{
    if (!enabled())
        return;

    // If the resource URL is already known, we don't need to add it again since this is just a cached load.
    if (m_knownResources.contains(cachedResource->url()))
        return;

    RefPtr<InspectorResource> resource = InspectorResource::create(m_nextIdentifier--, loader, loader->frame());
    resource->finished = true;

    resource->requestURL = KURL(cachedResource->url());
    updateResourceResponse(resource.get(), cachedResource->response());

    resource->length = cachedResource->encodedSize();
    resource->cached = true;
    resource->startTime = currentTime();
    resource->responseReceivedTime = resource->startTime;
    resource->endTime = resource->startTime;

    ASSERT(m_inspectedPage);

    if (loader->frame() == m_inspectedPage->mainFrame() && cachedResource->url() == loader->requestURL())
        m_mainResource = resource;

    addResource(resource.get());

    if (windowVisible())
        addAndUpdateScriptResource(resource.get());
}

void InspectorController::identifierForInitialRequest(unsigned long identifier, DocumentLoader* loader, const ResourceRequest& request)
{
    if (!enabled())
        return;

    RefPtr<InspectorResource> resource = InspectorResource::create(identifier, loader, loader->frame());

    updateResourceRequest(resource.get(), request);

    ASSERT(m_inspectedPage);

    if (loader->frame() == m_inspectedPage->mainFrame() && request.url() == loader->requestURL())
        m_mainResource = resource;

    addResource(resource.get());

    if (windowVisible() && loader->isLoadingFromCachedPage() && resource == m_mainResource)
        addAndUpdateScriptResource(resource.get());
}

void InspectorController::willSendRequest(DocumentLoader*, unsigned long identifier, ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    if (!enabled())
        return;

    InspectorResource* resource = m_resources.get(identifier).get();
    if (!resource)
        return;

    resource->startTime = currentTime();

    if (!redirectResponse.isNull()) {
        updateResourceRequest(resource, request);
        updateResourceResponse(resource, redirectResponse);
    }

    if (resource != m_mainResource && windowVisible()) {
        if (!resource->scriptObject)
            addScriptResource(resource);
        else
            updateScriptResourceRequest(resource);

        updateScriptResource(resource, resource->startTime, resource->responseReceivedTime, resource->endTime);

        if (!redirectResponse.isNull())
            updateScriptResourceResponse(resource);
    }
}

void InspectorController::didReceiveResponse(DocumentLoader*, unsigned long identifier, const ResourceResponse& response)
{
    if (!enabled())
        return;

    InspectorResource* resource = m_resources.get(identifier).get();
    if (!resource)
        return;

    updateResourceResponse(resource, response);

    resource->responseReceivedTime = currentTime();

    if (windowVisible() && resource->scriptObject) {
        updateScriptResourceResponse(resource);
        updateScriptResource(resource, resource->startTime, resource->responseReceivedTime, resource->endTime);
    }
}

void InspectorController::didReceiveContentLength(DocumentLoader*, unsigned long identifier, int lengthReceived)
{
    if (!enabled())
        return;

    InspectorResource* resource = m_resources.get(identifier).get();
    if (!resource)
        return;

    resource->length += lengthReceived;

    if (windowVisible() && resource->scriptObject)
        updateScriptResource(resource, resource->length);
}

void InspectorController::didFinishLoading(DocumentLoader*, unsigned long identifier)
{
    if (!enabled())
        return;

    RefPtr<InspectorResource> resource = m_resources.get(identifier);
    if (!resource)
        return;

    removeResource(resource.get());

    resource->finished = true;
    resource->endTime = currentTime();

    addResource(resource.get());

    if (windowVisible() && resource->scriptObject) {
        updateScriptResource(resource.get(), resource->startTime, resource->responseReceivedTime, resource->endTime);
        updateScriptResource(resource.get(), resource->finished);
    }
}

void InspectorController::didFailLoading(DocumentLoader*, unsigned long identifier, const ResourceError& /*error*/)
{
    if (!enabled())
        return;

    RefPtr<InspectorResource> resource = m_resources.get(identifier);
    if (!resource)
        return;

    removeResource(resource.get());

    resource->finished = true;
    resource->failed = true;
    resource->endTime = currentTime();

    addResource(resource.get());

    if (windowVisible() && resource->scriptObject) {
        updateScriptResource(resource.get(), resource->startTime, resource->responseReceivedTime, resource->endTime);
        updateScriptResource(resource.get(), resource->finished, resource->failed);
    }
}

void InspectorController::resourceRetrievedByXMLHttpRequest(unsigned long identifier, const JSC::UString& sourceString)
{
    if (!enabled())
        return;

    InspectorResource* resource = m_resources.get(identifier).get();
    if (!resource)
        return;

    resource->setXMLHttpRequestProperties(sourceString);

    if (windowVisible() && resource->scriptObject)
        updateScriptResourceType(resource);
}


#if ENABLE(DATABASE)
void InspectorController::didOpenDatabase(Database* database, const String& domain, const String& name, const String& version)
{
    if (!enabled())
        return;

    RefPtr<InspectorDatabaseResource> resource = InspectorDatabaseResource::create(database, domain, name, version);

    m_databaseResources.add(resource);

    if (windowVisible())
        addDatabaseScriptResource(resource.get());
}
#endif

void InspectorController::moveWindowBy(float x, float y) const
{
    if (!m_page || !enabled())
        return;

    FloatRect frameRect = m_page->chrome()->windowRect();
    frameRect.move(x, y);
    m_page->chrome()->setWindowRect(frameRect);
}

#if ENABLE(JAVASCRIPT_DEBUGGER)
void InspectorController::enableDebugger()
{
    if (!enabled())
        return;

    if (!m_scriptContext || !m_scriptObject) {
        m_attachDebuggerWhenShown = true;
        return;
    }

    ASSERT(m_inspectedPage);

    JavaScriptDebugServer::shared().addListener(this, m_inspectedPage);
    JavaScriptDebugServer::shared().clearBreakpoints();

    m_debuggerEnabled = true;
    m_attachDebuggerWhenShown = false;

    callSimpleFunction(m_scriptContext, m_scriptObject, "debuggerWasEnabled");
}

void InspectorController::disableDebugger()
{
    if (!enabled())
        return;

    ASSERT(m_inspectedPage);

    JavaScriptDebugServer::shared().removeListener(this, m_inspectedPage);

    m_debuggerEnabled = false;
    m_attachDebuggerWhenShown = false;

    if (m_scriptContext && m_scriptObject)
        callSimpleFunction(m_scriptContext, m_scriptObject, "debuggerWasDisabled");
}

JavaScriptCallFrame* InspectorController::currentCallFrame() const
{
    return JavaScriptDebugServer::shared().currentCallFrame();
}

bool InspectorController::pauseOnExceptions()
{
    return JavaScriptDebugServer::shared().pauseOnExceptions();
}

void InspectorController::setPauseOnExceptions(bool pause)
{
    JavaScriptDebugServer::shared().setPauseOnExceptions(pause);
}

void InspectorController::pauseInDebugger()
{
    if (!m_debuggerEnabled)
        return;
    JavaScriptDebugServer::shared().pauseProgram();
}

void InspectorController::resumeDebugger()
{
    if (!m_debuggerEnabled)
        return;
    JavaScriptDebugServer::shared().continueProgram();
}

void InspectorController::stepOverStatementInDebugger()
{
    if (!m_debuggerEnabled)
        return;
    JavaScriptDebugServer::shared().stepOverStatement();
}

void InspectorController::stepIntoStatementInDebugger()
{
    if (!m_debuggerEnabled)
        return;
    JavaScriptDebugServer::shared().stepIntoStatement();
}

void InspectorController::stepOutOfFunctionInDebugger()
{
    if (!m_debuggerEnabled)
        return;
    JavaScriptDebugServer::shared().stepOutOfFunction();
}

void InspectorController::addBreakpoint(intptr_t sourceID, unsigned lineNumber)
{
    JavaScriptDebugServer::shared().addBreakpoint(sourceID, lineNumber);
}

void InspectorController::removeBreakpoint(intptr_t sourceID, unsigned lineNumber)
{
    JavaScriptDebugServer::shared().removeBreakpoint(sourceID, lineNumber);
}
#endif

static void drawOutlinedQuad(GraphicsContext& context, const FloatQuad& quad, const Color& fillColor)
{
    static const int outlineThickness = 2;
    static const Color outlineColor(62, 86, 180, 228);

    Path quadPath;
    quadPath.moveTo(quad.p1());
    quadPath.addLineTo(quad.p2());
    quadPath.addLineTo(quad.p3());
    quadPath.addLineTo(quad.p4());
    quadPath.closeSubpath();
    
    // Clear the quad
    {
        context.save();
        context.setCompositeOperation(CompositeClear);
        context.addPath(quadPath);
        context.fillPath();
        context.restore();
    }

    // Clip out the quad, then draw with a 2px stroke to get a pixel
    // of outline (because inflating a quad is hard)
    {
        context.save();
        context.addPath(quadPath);
        context.clipOut(quadPath);

        context.addPath(quadPath);
        context.setStrokeThickness(outlineThickness);
        context.setStrokeColor(outlineColor);
        context.strokePath();

        context.restore();
    }
    
    // Now do the fill
    context.addPath(quadPath);
    context.setFillColor(fillColor);
    context.fillPath();
}

static void drawHighlightForBoxes(GraphicsContext& context, const Vector<FloatQuad>& lineBoxQuads, const FloatQuad& contentQuad, const FloatQuad& paddingQuad, const FloatQuad& borderQuad, const FloatQuad& marginQuad)
{
    static const Color contentBoxColor(125, 173, 217, 128);
    static const Color paddingBoxColor(125, 173, 217, 160);
    static const Color borderBoxColor(125, 173, 217, 192);
    static const Color marginBoxColor(125, 173, 217, 228);

    if (!lineBoxQuads.isEmpty()) {
        for (size_t i = 0; i < lineBoxQuads.size(); ++i)
            drawOutlinedQuad(context, lineBoxQuads[i], contentBoxColor);
        return;
    }

    if (marginQuad != borderQuad)
        drawOutlinedQuad(context, marginQuad, marginBoxColor);
    if (borderQuad != paddingQuad)
        drawOutlinedQuad(context, borderQuad, borderBoxColor);
    if (paddingQuad != contentQuad)
        drawOutlinedQuad(context, paddingQuad, paddingBoxColor);

    drawOutlinedQuad(context, contentQuad, contentBoxColor);
}

static inline void convertFromFrameToMainFrame(Frame* frame, IntRect& rect)
{
    rect = frame->page()->mainFrame()->view()->windowToContents(frame->view()->contentsToWindow(rect));
}

static inline IntSize frameToMainFrameOffset(Frame* frame)
{
    IntPoint mainFramePoint = frame->page()->mainFrame()->view()->windowToContents(frame->view()->contentsToWindow(IntPoint()));
    return mainFramePoint - IntPoint();
}

void InspectorController::drawNodeHighlight(GraphicsContext& context) const
{
    if (!m_highlightedNode)
        return;

    RenderBox* renderer = m_highlightedNode->renderBox();
    Frame* containingFrame = m_highlightedNode->document()->frame();
    if (!renderer || !containingFrame)
        return;

    IntRect contentBox = renderer->contentBoxRect();

    // FIXME: Should we add methods to RenderObject to obtain these rects?
    IntRect paddingBox(contentBox.x() - renderer->paddingLeft(), contentBox.y() - renderer->paddingTop(),
                       contentBox.width() + renderer->paddingLeft() + renderer->paddingRight(), contentBox.height() + renderer->paddingTop() + renderer->paddingBottom());
    IntRect borderBox(paddingBox.x() - renderer->borderLeft(), paddingBox.y() - renderer->borderTop(),
                      paddingBox.width() + renderer->borderLeft() + renderer->borderRight(), paddingBox.height() + renderer->borderTop() + renderer->borderBottom());
    IntRect marginBox(borderBox.x() - renderer->marginLeft(), borderBox.y() - renderer->marginTop(),
                      borderBox.width() + renderer->marginLeft() + renderer->marginRight(), borderBox.height() + renderer->marginTop() + renderer->marginBottom());


    IntSize mainFrameOffset = frameToMainFrameOffset(containingFrame);

    FloatQuad absContentQuad = renderer->localToAbsoluteQuad(FloatRect(contentBox));
    FloatQuad absPaddingQuad = renderer->localToAbsoluteQuad(FloatRect(paddingBox));
    FloatQuad absBorderQuad = renderer->localToAbsoluteQuad(FloatRect(borderBox));
    FloatQuad absMarginQuad = renderer->localToAbsoluteQuad(FloatRect(marginBox));

    absContentQuad.move(mainFrameOffset);
    absPaddingQuad.move(mainFrameOffset);
    absBorderQuad.move(mainFrameOffset);
    absMarginQuad.move(mainFrameOffset);

    IntRect boundingBox = renderer->absoluteBoundingBoxRect(true);
    boundingBox.move(mainFrameOffset);

    Vector<FloatQuad> lineBoxQuads;
    if (renderer->isInline() || (renderer->isText() && !m_highlightedNode->isSVGElement())) {
        // FIXME: We should show margins/padding/border for inlines.
        renderer->collectAbsoluteLineBoxQuads(lineBoxQuads);
    }

    for (unsigned i = 0; i < lineBoxQuads.size(); ++i)
        lineBoxQuads[i] += mainFrameOffset;

    if (lineBoxQuads.isEmpty() && contentBox.isEmpty()) {
        // If we have no line boxes and our content box is empty, we'll just draw our bounding box.
        // This can happen, e.g., with an <a> enclosing an <img style="float:right">.
        // FIXME: Can we make this better/more accurate? The <a> in the above case has no
        // width/height but the highlight makes it appear to be the size of the <img>.
        lineBoxQuads.append(FloatRect(boundingBox));
    }

    ASSERT(m_inspectedPage);

    FrameView* view = m_inspectedPage->mainFrame()->view();
    FloatRect overlayRect = view->visibleContentRect();

    if (!overlayRect.contains(boundingBox) && !boundingBox.contains(enclosingIntRect(overlayRect))) {
        Element* element;
        if (m_highlightedNode->isElementNode())
            element = static_cast<Element*>(m_highlightedNode.get());
        else
            element = static_cast<Element*>(m_highlightedNode->parent());
        overlayRect = view->visibleContentRect();
    }

    context.translate(-overlayRect.x(), -overlayRect.y());

    drawHighlightForBoxes(context, lineBoxQuads, absContentQuad, absPaddingQuad, absBorderQuad, absMarginQuad);
}

void InspectorController::count(const String& title, unsigned lineNumber, const String& sourceID)
{
    String identifier = title + String::format("@%s:%d", sourceID.utf8().data(), lineNumber);
    HashMap<String, unsigned>::iterator it = m_counts.find(identifier);
    int count;
    if (it == m_counts.end())
        count = 1;
    else {
        count = it->second + 1;
        m_counts.remove(it);
    }

    m_counts.add(identifier, count);

    String message = String::format("%s: %d", title.utf8().data(), count);
    addMessageToConsole(JSMessageSource, LogMessageLevel, message, lineNumber, sourceID);
}

void InspectorController::startTiming(const String& title)
{
    m_times.add(title, currentTime() * 1000);
}

bool InspectorController::stopTiming(const String& title, double& elapsed)
{
    HashMap<String, double>::iterator it = m_times.find(title);
    if (it == m_times.end())
        return false;

    double startTime = it->second;
    m_times.remove(it);
    
    elapsed = currentTime() * 1000 - startTime;
    return true;
}

bool InspectorController::handleException(JSContextRef context, JSValueRef exception, unsigned lineNumber) const
{
    if (!exception)
        return false;

    if (!m_page)
        return true;

    String message = toString(context, exception, 0);
    String file(__FILE__);

    if (JSObjectRef exceptionObject = JSValueToObject(context, exception, 0)) {
        JSValueRef lineValue = JSObjectGetProperty(context, exceptionObject, jsStringRef("line").get(), NULL);
        if (lineValue)
            lineNumber = static_cast<unsigned>(JSValueToNumber(context, lineValue, 0));

        JSValueRef fileValue = JSObjectGetProperty(context, exceptionObject, jsStringRef("sourceURL").get(), NULL);
        if (fileValue)
            file = toString(context, fileValue, 0);
    }

    m_page->mainFrame()->domWindow()->console()->addMessage(JSMessageSource, ErrorMessageLevel, message, lineNumber, file);
    return true;
}

#if ENABLE(JAVASCRIPT_DEBUGGER)

// JavaScriptDebugListener functions

void InspectorController::didParseSource(ExecState*, const SourceCode& source)
{
    JSValueRef sourceIDValue = JSValueMakeNumber(m_scriptContext, source.provider()->asID());
    JSValueRef sourceURLValue = JSValueMakeString(m_scriptContext, jsStringRef(source.provider()->url()).get());
    JSValueRef sourceValue = JSValueMakeString(m_scriptContext, jsStringRef(source).get());
    JSValueRef firstLineValue = JSValueMakeNumber(m_scriptContext, source.firstLine());

    JSValueRef exception = 0;
    JSValueRef arguments[] = { sourceIDValue, sourceURLValue, sourceValue, firstLineValue };
    callFunction(m_scriptContext, m_scriptObject, "parsedScriptSource", 4, arguments, exception);
}

void InspectorController::failedToParseSource(ExecState*, const SourceCode& source, int errorLine, const UString& errorMessage)
{
    JSValueRef sourceURLValue = JSValueMakeString(m_scriptContext, jsStringRef(source.provider()->url()).get());
    JSValueRef sourceValue = JSValueMakeString(m_scriptContext, jsStringRef(source.data()).get());
    JSValueRef firstLineValue = JSValueMakeNumber(m_scriptContext, source.firstLine());
    JSValueRef errorLineValue = JSValueMakeNumber(m_scriptContext, errorLine);
    JSValueRef errorMessageValue = JSValueMakeString(m_scriptContext, jsStringRef(errorMessage).get());

    JSValueRef exception = 0;
    JSValueRef arguments[] = { sourceURLValue, sourceValue, firstLineValue, errorLineValue, errorMessageValue };
    callFunction(m_scriptContext, m_scriptObject, "failedToParseScriptSource", 5, arguments, exception);
}

void InspectorController::didPause()
{
    JSValueRef exception = 0;
    callFunction(m_scriptContext, m_scriptObject, "pausedScript", 0, 0, exception);
}

#endif

} // namespace WebCore
