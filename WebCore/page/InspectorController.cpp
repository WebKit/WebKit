/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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
#include "DocLoader.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "Element.h"
#include "FloatRect.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "HTMLFrameOwnerElement.h"
#include "InspectorClient.h"
#include "JSRange.h"
#include "Page.h"
#include "Range.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "Settings.h"
#include "Shared.h"
#include "SharedBuffer.h"
#include "SystemTime.h"
#include "TextEncoding.h"
#include "TextIterator.h"
#include "kjs_dom.h"
#include "kjs_proxy.h"
#include "kjs_window.h"
#include <JavaScriptCore/APICast.h>
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/JSStringRef.h>

namespace WebCore {

struct ConsoleMessage {
    ConsoleMessage(MessageSource s, MessageLevel l, const String& m, unsigned li, const String& u)
        : source(s)
        , level(l)
        , message(m)
        , line(li)
        , url(u)
    {
    }

    MessageSource source;
    MessageLevel level;
    String message;
    unsigned line;
    String url;
};

struct InspectorResource : public Shared<InspectorResource> {
    // Keep these in sync with WebInspector.Resource.Type
    enum Type {
        Doc,
        Stylesheet,
        Image,
        Script,
        Other
    };

    InspectorResource(long long identifier, DocumentLoader* documentLoader, Frame* frame)
        : identifier(identifier)
        , loader(documentLoader)
        , frame(frame)
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

    ~InspectorResource()
    {
        setScriptObject(0, 0);
    }

    Type type() const
    {
        if (requestURL == loader->requestURL())
            return Doc;

        FrameLoader* frameLoader = loader->frameLoader();
        if (!frameLoader)
            return Other;

        if (requestURL == frameLoader->iconURL())
            return Image;

        Document* doc = frameLoader->frame()->document();
        if (!doc)
            return Other;

        CachedResource* cachedResource = doc->docLoader()->cachedResource(requestURL.url());
        if (!cachedResource)
            return Other;

        switch (cachedResource->type()) {
            case CachedResource::ImageResource:
                return Image;
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

    long long identifier;
    RefPtr<DocumentLoader> loader;
    RefPtr<Frame> frame;
    KURL requestURL;
    HTTPHeaderMap requestHeaderFields;
    HTTPHeaderMap responseHeaderFields;
    String mimeType;
    String suggestedFilename;
    String textEncodingName;
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
};

static JSValueRef addSourceToFrame(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* /*exception*/)
{
    JSValueRef undefined = JSValueMakeUndefined(ctx);

    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (argumentCount < 2 || !controller)
        return undefined;

    JSValueRef identifierValue = arguments[0];
    if (!JSValueIsNumber(ctx, identifierValue))
        return undefined;

    unsigned long identifier = static_cast<unsigned long>(JSValueToNumber(ctx, identifierValue, 0));
    RefPtr<InspectorResource> resource = controller->resources().get(identifier);
    ASSERT(resource);
    if (!resource)
        return undefined;

    RefPtr<SharedBuffer> buffer;
    if (resource->requestURL == resource->loader->requestURL())
        buffer = resource->loader->mainResourceData();
    else {
        FrameLoader* frameLoader = resource->loader->frameLoader();
        if (!frameLoader)
            return undefined;

        Document* doc = frameLoader->frame()->document();
        if (!doc)
            return undefined;

        CachedResource* cachedResource = doc->docLoader()->cachedResource(resource->requestURL.url());
        if (!cachedResource)
            return undefined;

        buffer = cachedResource->data();
    }

    if (!buffer)
        return undefined;

    String textEncodingName = resource->loader->overrideEncoding();
    if (!textEncodingName)
        textEncodingName = resource->textEncodingName;

    TextEncoding encoding(textEncodingName);
    if (!encoding.isValid())
        encoding = WindowsLatin1Encoding();
    String sourceString = encoding.decode(buffer->data(), buffer->size());

    Node* node = toNode(toJS(arguments[1]));
    ASSERT(node);
    if (!node)
        return undefined;

    if (!node->attached()) {
        ASSERT_NOT_REACHED();
        return undefined;
    }

    ASSERT(node->isElementNode());
    if (!node->isElementNode())
        return undefined;

    Element* element = static_cast<Element*>(node);
    ASSERT(element->isFrameOwnerElement());
    if (!element->isFrameOwnerElement())
        return undefined;

    HTMLFrameOwnerElement* frameOwner = static_cast<HTMLFrameOwnerElement*>(element);
    ASSERT(frameOwner->contentFrame());
    if (!frameOwner->contentFrame())
        return undefined;

    FrameLoader* loader = frameOwner->contentFrame()->loader();

    loader->setResponseMIMEType(resource->mimeType);
    loader->begin();
    loader->write(sourceString);
    loader->end();

    return undefined;
}

static JSValueRef getResourceDocumentNode(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* /*exception*/)
{
    JSValueRef undefined = JSValueMakeUndefined(ctx);

    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!argumentCount || argumentCount > 1 || !controller)
        return undefined;

    JSValueRef identifierValue = arguments[0];
    if (!JSValueIsNumber(ctx, identifierValue))
        return undefined;

    unsigned long identifier = static_cast<unsigned long>(JSValueToNumber(ctx, identifierValue, 0));
    RefPtr<InspectorResource> resource = controller->resources().get(identifier);
    ASSERT(resource);
    if (!resource)
        return undefined;

    FrameLoader* frameLoader = resource->loader->frameLoader();
    if (!frameLoader)
        return undefined;

    Document* document = frameLoader->frame()->document();
    if (!document)
        return undefined;

    if (document->isPluginDocument() || document->isImageDocument())
        return undefined;

    KJS::JSLock lock;
    JSValueRef documentValue = toRef(toJS(toJS(controller->scriptContext()), document));
    return documentValue;
}

static JSValueRef highlightDOMNode(JSContextRef context, JSObjectRef /*function*/, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* /*exception*/)
{
    JSValueRef undefined = JSValueMakeUndefined(context);

    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (argumentCount < 1 || !controller)
        return undefined;

    Node* node = toNode(toJS(arguments[0]));
    if (!node)
        return undefined;

    controller->highlight(node);

    return undefined;
}

static JSValueRef hideDOMNodeHighlight(JSContextRef context, JSObjectRef /*function*/, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* /*exception*/)
{
    JSValueRef undefined = JSValueMakeUndefined(context);

    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (argumentCount || !controller)
        return undefined;

    controller->hideHighlight();

    return undefined;
}

static JSValueRef loaded(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t /*argumentCount*/, const JSValueRef[] /*arguments[]*/, JSValueRef* /*exception*/)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    controller->scriptObjectReady();
    return JSValueMakeUndefined(ctx);
}

static JSValueRef unloading(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t /*argumentCount*/, const JSValueRef[] /*arguments[]*/, JSValueRef* /*exception*/)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    controller->windowUnloading();
    return JSValueMakeUndefined(ctx);
}

static JSValueRef attach(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t /*argumentCount*/, const JSValueRef[] /*arguments[]*/, JSValueRef* /*exception*/)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    controller->attachWindow();
    return JSValueMakeUndefined(ctx);
}

static JSValueRef detach(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t /*argumentCount*/, const JSValueRef[] /*arguments[]*/, JSValueRef* /*exception*/)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    controller->detachWindow();
    return JSValueMakeUndefined(ctx);
}

static JSValueRef log(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef /*thisObject*/, size_t argumentCount, const JSValueRef arguments[], JSValueRef* /*exception*/)
{
    if (argumentCount < 1 || !JSValueIsString(ctx, arguments[0]))
        return JSValueMakeUndefined(ctx);

#ifndef NDEBUG
    JSStringRef string = JSValueToStringCopy(ctx, arguments[0], 0);
    String message(JSStringGetCharactersPtr(string), JSStringGetLength(string));
    JSStringRelease(string);

    fprintf(stderr, "%s\n", message.latin1().data());
#endif

    return JSValueMakeUndefined(ctx);
}

static JSValueRef search(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* /*exception*/)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    if (argumentCount < 2 || !JSValueIsString(ctx, arguments[1]))
        return JSValueMakeUndefined(ctx);

    Node* node = toNode(toJS(arguments[0]));
    if (!node)
        return JSValueMakeUndefined(ctx);

    JSStringRef string = JSValueToStringCopy(ctx, arguments[1], 0);
    String target(JSStringGetCharactersPtr(string), JSStringGetLength(string));
    JSStringRelease(string);

    JSObjectRef globalObject = JSContextGetGlobalObject(ctx);
    JSStringRef constructorString = JSStringCreateWithUTF8CString("Array");
    JSObjectRef arrayConstructor = JSValueToObject(ctx, JSObjectGetProperty(ctx, globalObject, constructorString, 0), 0);
    JSStringRelease(constructorString);
    JSObjectRef array = JSObjectCallAsConstructor(ctx, arrayConstructor, 0, 0, 0);

    JSStringRef pushString = JSStringCreateWithUTF8CString("push");
    JSValueRef pushValue = JSObjectGetProperty(ctx, array, pushString, 0);
    JSStringRelease(pushString);
    JSObjectRef push = JSValueToObject(ctx, pushValue, 0);

    RefPtr<Range> searchRange(rangeOfContents(node));

    int exception = 0;
    do {
        RefPtr<Range> resultRange(findPlainText(searchRange.get(), target, true, false));
        if (resultRange->collapsed(exception))
            break;

        // A non-collapsed result range can in some funky whitespace cases still not
        // advance the range's start position (4509328). Break to avoid infinite loop.
        VisiblePosition newStart = endVisiblePosition(resultRange.get(), DOWNSTREAM);
        if (newStart == startVisiblePosition(searchRange.get(), DOWNSTREAM))
            break;

        KJS::JSLock lock;
        JSValueRef arg0 = toRef(toJS(toJS(ctx), resultRange.get()));
        JSObjectCallAsFunction(ctx, push, array, 1, &arg0, 0);

        setStart(searchRange.get(), newStart);
    } while (true);

    return array;
}

static JSValueRef inspectedWindow(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t /*argumentCount*/, const JSValueRef[] /*arguments[]*/, JSValueRef* /*exception*/)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    return toRef(KJS::Window::retrieve(controller->inspectedPage()->mainFrame()));
}

InspectorController::InspectorController(Page* page, InspectorClient* client)
    : m_inspectedPage(page)
    , m_client(client)
    , m_page(0)
    , m_scriptObject(0)
    , m_controllerScriptObject(0)
    , m_scriptContext(0)
    , m_windowVisible(false)
    , m_nextIdentifier(-2)
{
    ASSERT_ARG(page, page);
    ASSERT_ARG(client, client);
}

InspectorController::~InspectorController()
{
    if (m_scriptContext) {
        JSObjectRef global = JSContextGetGlobalObject(m_scriptContext);
        JSStringRef controllerProperty = JSStringCreateWithUTF8CString("InspectorController");
        JSObjectRef controller = JSValueToObject(m_scriptContext, JSObjectGetProperty(m_scriptContext, global, controllerProperty, 0), 0);
        JSStringRelease(controllerProperty);
        JSObjectSetPrivate(controller, 0);
    }

    m_client->closeWindow();
    m_client->inspectorDestroyed();

    if (m_page)
        m_page->setParentInspectorController(0);

    deleteAllValues(m_frameResources);
    deleteAllValues(m_consoleMessages);
}

bool InspectorController::enabled() const
{
    return m_inspectedPage->settings()->developerExtrasEnabled();
}

void InspectorController::inspect(Node* node)
{
    if (!node || !enabled())
        return;

    if (!m_page) {
        m_page = m_client->createPage();
        if (!m_page)
            return;

        m_page->setParentInspectorController(this);
    }

    if (node->nodeType() != Node::ELEMENT_NODE && node->nodeType() != Node::DOCUMENT_NODE)
        node = node->parentNode();
    m_nodeToFocus = node;

    if (!m_scriptObject)
        return;

    if (windowVisible())
        focusNode();
    else
        m_client->showWindow();
}

void InspectorController::focusNode()
{
    if (!enabled())
        return;

    ASSERT(m_scriptContext);
    ASSERT(m_scriptObject);
    ASSERT(m_nodeToFocus);

    JSValueRef arg0;

    {
        KJS::JSLock lock;
        arg0 = toRef(toJS(toJS(m_scriptContext), m_nodeToFocus.get()));
    }

    m_nodeToFocus = 0;

    JSStringRef functionProperty = JSStringCreateWithUTF8CString("updateFocusedNode");
    JSObjectRef function = JSValueToObject(m_scriptContext, JSObjectGetProperty(m_scriptContext, m_scriptObject, functionProperty, 0), 0);
    JSStringRelease(functionProperty);
    ASSERT(function);

    JSObjectCallAsFunction(m_scriptContext, function, m_scriptObject, 1, &arg0, 0);
}

void InspectorController::highlight(Node* node)
{
    if (!enabled())
        return;
    ASSERT_ARG(node, node);
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

void InspectorController::setWindowVisible(bool visible)
{
    if (visible == m_windowVisible)
        return;

    m_windowVisible = visible;

    if (!m_scriptContext || !m_scriptObject)
        return;

    if (m_windowVisible) {
        populateScriptResources();
        if (m_nodeToFocus)
            focusNode();
    } else {
        clearScriptResources();
        clearScriptConsoleMessages();
        clearNetworkTimeline();
    }
}

void InspectorController::addMessageToConsole(MessageSource source, MessageLevel level, const String& message, unsigned lineNumber, const String& sourceID)
{
    if (!enabled())
        return;

    ConsoleMessage* consoleMessage = new ConsoleMessage(source, level, message, lineNumber, sourceID);
    m_consoleMessages.append(consoleMessage);

    if (windowVisible())
        addScriptConsoleMessage(consoleMessage);
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

void InspectorController::windowScriptObjectAvailable()
{
    if (!m_page || !enabled())
        return;

    m_scriptContext = toRef(m_page->mainFrame()->scriptProxy()->interpreter()->globalExec());

    JSObjectRef global = JSContextGetGlobalObject(m_scriptContext);
    ASSERT(global);

    static JSStaticFunction staticFunctions[] = {
        { "addSourceToFrame", addSourceToFrame, kJSPropertyAttributeNone },
        { "getResourceDocumentNode", getResourceDocumentNode, kJSPropertyAttributeNone },
        { "highlightDOMNode", highlightDOMNode, kJSPropertyAttributeNone },
        { "hideDOMNodeHighlight", hideDOMNodeHighlight, kJSPropertyAttributeNone },
        { "loaded", loaded, kJSPropertyAttributeNone },
        { "windowUnloading", unloading, kJSPropertyAttributeNone },
        { "attach", attach, kJSPropertyAttributeNone },
        { "detach", detach, kJSPropertyAttributeNone },
        { "log", log, kJSPropertyAttributeNone },
        { "search", search, kJSPropertyAttributeNone },
        { "inspectedWindow", inspectedWindow, kJSPropertyAttributeNone },
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

    JSStringRef controllerObjectString = JSStringCreateWithUTF8CString("InspectorController");
    JSObjectSetProperty(m_scriptContext, global, controllerObjectString, m_controllerScriptObject, kJSPropertyAttributeNone, 0);
    JSStringRelease(controllerObjectString);
}

void InspectorController::scriptObjectReady()
{
    ASSERT(m_scriptContext);
    if (!m_scriptContext)
        return;

    JSObjectRef global = JSContextGetGlobalObject(m_scriptContext);
    ASSERT(global);

    JSStringRef inspectorString = JSStringCreateWithUTF8CString("WebInspector");
    JSValueRef inspectorValue = JSObjectGetProperty(m_scriptContext, global, inspectorString, 0);
    JSStringRelease(inspectorString);

    ASSERT(inspectorValue);
    if (!inspectorValue)
        return;

    m_scriptObject = JSValueToObject(m_scriptContext, inspectorValue, 0);
    ASSERT(m_scriptObject);

    JSValueProtect(m_scriptContext, m_scriptObject);

    // Make sure our window is visible now that the page loaded
    m_client->showWindow();
}

void InspectorController::windowUnloading()
{
    m_client->closeWindow();
    if (m_page)
        m_page->setParentInspectorController(0);

    ASSERT(m_scriptContext && m_scriptObject);
    JSValueUnprotect(m_scriptContext, m_scriptObject);

    m_page = 0;
    m_scriptObject = 0;
    m_scriptContext = 0;
}

static void addHeaders(JSContextRef context, JSObjectRef object, const HTTPHeaderMap& headers)
{
    ASSERT_ARG(context, context);
    ASSERT_ARG(object, object);
    
    HTTPHeaderMap::const_iterator end = headers.end();
    for (HTTPHeaderMap::const_iterator it = headers.begin(); it != end; ++it) {
        JSStringRef field = JSStringCreateWithCharacters(it->first.characters(), it->first.length());
        JSStringRef valueString = JSStringCreateWithCharacters(it->second.characters(), it->second.length());
        JSValueRef value = JSValueMakeString(context, valueString);
        JSObjectSetProperty(context, object, field, value, kJSPropertyAttributeNone, 0);
        JSStringRelease(field);
        JSStringRelease(valueString);
    }
}

static JSObjectRef scriptObjectForRequest(JSContextRef context, const InspectorResource* resource)
{
    ASSERT_ARG(context, context);

    JSObjectRef object = JSObjectMake(context, 0, 0);
    addHeaders(context, object, resource->requestHeaderFields);

    return object;
}

static JSObjectRef scriptObjectForResponse(JSContextRef context, const InspectorResource* resource)
{
    ASSERT_ARG(context, context);

    JSObjectRef object = JSObjectMake(context, 0, 0);
    addHeaders(context, object, resource->responseHeaderFields);

    return object;
}

JSObjectRef InspectorController::addScriptResource(InspectorResource* resource)
{
    ASSERT_ARG(resource, resource);

    // This happens for pages loaded from the back/forward cache.
    if (resource->scriptObject)
        return resource->scriptObject;

    ASSERT(m_scriptContext);
    ASSERT(m_scriptObject);
    if (!m_scriptContext || !m_scriptObject)
        return 0;

    JSStringRef resourceString = JSStringCreateWithUTF8CString("Resource");
    JSObjectRef resourceConstructor = JSValueToObject(m_scriptContext, JSObjectGetProperty(m_scriptContext, m_scriptObject, resourceString, 0), 0);
    JSStringRelease(resourceString);

    String urlString = resource->requestURL.url();
    JSStringRef url = JSStringCreateWithCharacters(urlString.characters(), urlString.length());
    JSValueRef urlValue = JSValueMakeString(m_scriptContext, url);
    JSStringRelease(url);

    urlString = resource->requestURL.host();
    JSStringRef domain = JSStringCreateWithCharacters(urlString.characters(), urlString.length());
    JSValueRef domainValue = JSValueMakeString(m_scriptContext, domain);
    JSStringRelease(domain);

    urlString = resource->requestURL.path();
    JSStringRef path = JSStringCreateWithCharacters(urlString.characters(), urlString.length());
    JSValueRef pathValue = JSValueMakeString(m_scriptContext, path);
    JSStringRelease(path);

    urlString = resource->requestURL.lastPathComponent();
    JSStringRef lastPathComponent = JSStringCreateWithCharacters(urlString.characters(), urlString.length());
    JSValueRef lastPathComponentValue = JSValueMakeString(m_scriptContext, lastPathComponent);
    JSStringRelease(lastPathComponent);

    JSValueRef identifier = JSValueMakeNumber(m_scriptContext, resource->identifier);
    JSValueRef mainResource = JSValueMakeBoolean(m_scriptContext, m_mainResource == resource);
    JSValueRef cached = JSValueMakeBoolean(m_scriptContext, resource->cached);

    JSValueRef arguments[] = { scriptObjectForRequest(m_scriptContext, resource), urlValue, domainValue, pathValue, lastPathComponentValue, identifier, mainResource, cached };
    JSObjectRef result = JSObjectCallAsConstructor(m_scriptContext, resourceConstructor, 8, arguments, 0);

    resource->setScriptObject(m_scriptContext, result);

    ASSERT(result);

    JSStringRef addResourceString = JSStringCreateWithUTF8CString("addResource");
    JSObjectRef addResourceFunction = JSValueToObject(m_scriptContext, JSObjectGetProperty(m_scriptContext, m_scriptObject, addResourceString, 0), 0);
    JSStringRelease(addResourceString);

    JSValueRef addArguments[] = { result };
    JSObjectCallAsFunction(m_scriptContext, addResourceFunction, m_scriptObject, 1, addArguments, 0);

    return result;
}

JSObjectRef InspectorController::addAndUpdateScriptResource(InspectorResource* resource)
{
    ASSERT_ARG(resource, resource);

    JSObjectRef scriptResource = addScriptResource(resource);
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

    JSStringRef removeResourceString = JSStringCreateWithUTF8CString("removeResource");
    JSObjectRef removeResourceFunction = JSValueToObject(m_scriptContext, JSObjectGetProperty(m_scriptContext, m_scriptObject, removeResourceString, 0), 0);
    JSStringRelease(removeResourceString);

    JSValueRef arguments[] = { resource->scriptObject };
    JSObjectCallAsFunction(m_scriptContext, removeResourceFunction, m_scriptObject, 1, arguments, 0);

    resource->setScriptObject(0, 0);
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
    resource->textEncodingName = response.textEncodingName();
}

void InspectorController::updateScriptResourceRequest(InspectorResource* resource)
{
    ASSERT(resource->scriptObject);
    ASSERT(m_scriptContext);
    if (!resource->scriptObject || !m_scriptContext)
        return;

    String urlString = resource->requestURL.url();
    JSStringRef url = JSStringCreateWithCharacters(urlString.characters(), urlString.length());
    JSValueRef urlValue = JSValueMakeString(m_scriptContext, url);
    JSStringRelease(url);

    urlString = resource->requestURL.host();
    JSStringRef domain = JSStringCreateWithCharacters(urlString.characters(), urlString.length());
    JSValueRef domainValue = JSValueMakeString(m_scriptContext, domain);
    JSStringRelease(domain);

    urlString = resource->requestURL.path();
    JSStringRef path = JSStringCreateWithCharacters(urlString.characters(), urlString.length());
    JSValueRef pathValue = JSValueMakeString(m_scriptContext, path);
    JSStringRelease(path);

    urlString = resource->requestURL.lastPathComponent();
    JSStringRef lastPathComponent = JSStringCreateWithCharacters(urlString.characters(), urlString.length());
    JSValueRef lastPathComponentValue = JSValueMakeString(m_scriptContext, lastPathComponent);
    JSStringRelease(lastPathComponent);

    JSValueRef mainResourceValue = JSValueMakeBoolean(m_scriptContext, m_mainResource == resource);

    JSStringRef propertyName = JSStringCreateWithUTF8CString("url");
    JSObjectSetProperty(m_scriptContext, resource->scriptObject, propertyName, urlValue, kJSPropertyAttributeNone, 0);
    JSStringRelease(propertyName);

    propertyName = JSStringCreateWithUTF8CString("domain");
    JSObjectSetProperty(m_scriptContext, resource->scriptObject, propertyName, domainValue, kJSPropertyAttributeNone, 0);
    JSStringRelease(propertyName);

    propertyName = JSStringCreateWithUTF8CString("path");
    JSObjectSetProperty(m_scriptContext, resource->scriptObject, propertyName, pathValue, kJSPropertyAttributeNone, 0);
    JSStringRelease(propertyName);

    propertyName = JSStringCreateWithUTF8CString("lastPathComponent");
    JSObjectSetProperty(m_scriptContext, resource->scriptObject, propertyName, lastPathComponentValue, kJSPropertyAttributeNone, 0);
    JSStringRelease(propertyName);

    propertyName = JSStringCreateWithUTF8CString("requestHeaders");
    JSObjectSetProperty(m_scriptContext, resource->scriptObject, propertyName, scriptObjectForRequest(m_scriptContext, resource), kJSPropertyAttributeNone, 0);
    JSStringRelease(propertyName);

    propertyName = JSStringCreateWithUTF8CString("mainResource");
    JSObjectSetProperty(m_scriptContext, resource->scriptObject, propertyName, mainResourceValue, kJSPropertyAttributeNone, 0);
    JSStringRelease(propertyName);
}

void InspectorController::updateScriptResourceResponse(InspectorResource* resource)
{
    ASSERT(resource->scriptObject);
    ASSERT(m_scriptContext);
    if (!resource->scriptObject || !m_scriptContext)
        return;

    JSStringRef mimeType = JSStringCreateWithCharacters(resource->mimeType.characters(), resource->mimeType.length());
    JSValueRef mimeTypeValue = JSValueMakeString(m_scriptContext, mimeType);
    JSStringRelease(mimeType);

    JSStringRef suggestedFilename = JSStringCreateWithCharacters(resource->suggestedFilename.characters(), resource->suggestedFilename.length());
    JSValueRef suggestedFilenameValue = JSValueMakeString(m_scriptContext, suggestedFilename);
    JSStringRelease(suggestedFilename);

    JSValueRef expectedContentLengthValue = JSValueMakeNumber(m_scriptContext, static_cast<double>(resource->expectedContentLength));
    JSValueRef statusCodeValue = JSValueMakeNumber(m_scriptContext, resource->responseStatusCode);

    JSStringRef propertyName = JSStringCreateWithUTF8CString("mimeType");
    JSObjectSetProperty(m_scriptContext, resource->scriptObject, propertyName, mimeTypeValue, kJSPropertyAttributeNone, 0);
    JSStringRelease(propertyName);

    propertyName = JSStringCreateWithUTF8CString("suggestedFilename");
    JSObjectSetProperty(m_scriptContext, resource->scriptObject, propertyName, suggestedFilenameValue, kJSPropertyAttributeNone, 0);
    JSStringRelease(propertyName);

    propertyName = JSStringCreateWithUTF8CString("expectedContentLength");
    JSObjectSetProperty(m_scriptContext, resource->scriptObject, propertyName, expectedContentLengthValue, kJSPropertyAttributeNone, 0);
    JSStringRelease(propertyName);

    propertyName = JSStringCreateWithUTF8CString("statusCode");
    JSObjectSetProperty(m_scriptContext, resource->scriptObject, propertyName, statusCodeValue, kJSPropertyAttributeNone, 0);
    JSStringRelease(propertyName);
    
    propertyName = JSStringCreateWithUTF8CString("responseHeaders");
    JSObjectSetProperty(m_scriptContext, resource->scriptObject, propertyName, scriptObjectForResponse(m_scriptContext, resource), kJSPropertyAttributeNone, 0);
    JSStringRelease(propertyName);

    JSValueRef typeValue = JSValueMakeNumber(m_scriptContext, resource->type());
    propertyName = JSStringCreateWithUTF8CString("type");
    JSObjectSetProperty(m_scriptContext, resource->scriptObject, propertyName, typeValue, kJSPropertyAttributeNone, 0);
    JSStringRelease(propertyName);
}

void InspectorController::updateScriptResource(InspectorResource* resource, int length)
{
    ASSERT(resource->scriptObject);
    ASSERT(m_scriptContext);
    if (!resource->scriptObject || !m_scriptContext)
        return;

    JSValueRef lengthValue = JSValueMakeNumber(m_scriptContext, length);

    JSStringRef propertyName = JSStringCreateWithUTF8CString("contentLength");
    JSObjectSetProperty(m_scriptContext, resource->scriptObject, propertyName, lengthValue, kJSPropertyAttributeNone, 0);
    JSStringRelease(propertyName);
}

void InspectorController::updateScriptResource(InspectorResource* resource, bool finished, bool failed)
{
    ASSERT(resource->scriptObject);
    ASSERT(m_scriptContext);
    if (!resource->scriptObject || !m_scriptContext)
        return;

    JSValueRef failedValue = JSValueMakeBoolean(m_scriptContext, failed);
    JSValueRef finishedValue = JSValueMakeBoolean(m_scriptContext, finished);

    JSStringRef propertyName = JSStringCreateWithUTF8CString("failed");
    JSObjectSetProperty(m_scriptContext, resource->scriptObject, propertyName, failedValue, kJSPropertyAttributeNone, 0);
    JSStringRelease(propertyName);

    propertyName = JSStringCreateWithUTF8CString("finished");
    JSObjectSetProperty(m_scriptContext, resource->scriptObject, propertyName, finishedValue, kJSPropertyAttributeNone, 0);
    JSStringRelease(propertyName);

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

    JSStringRef propertyName = JSStringCreateWithUTF8CString("startTime");
    JSObjectSetProperty(m_scriptContext, resource->scriptObject, propertyName, startTimeValue, kJSPropertyAttributeNone, 0);
    JSStringRelease(propertyName);

    propertyName = JSStringCreateWithUTF8CString("responseReceivedTime");
    JSObjectSetProperty(m_scriptContext, resource->scriptObject, propertyName, responseReceivedTimeValue, kJSPropertyAttributeNone, 0);
    JSStringRelease(propertyName);

    propertyName = JSStringCreateWithUTF8CString("endTime");
    JSObjectSetProperty(m_scriptContext, resource->scriptObject, propertyName, endTimeValue, kJSPropertyAttributeNone, 0);
    JSStringRelease(propertyName);
}

void InspectorController::populateScriptResources()
{
    ASSERT(m_scriptContext);
    if (!m_scriptContext)
        return;

    clearScriptResources();
    clearScriptConsoleMessages();
    clearNetworkTimeline();

    ResourcesMap::iterator resourcesEnd = m_resources.end();
    for (ResourcesMap::iterator it = m_resources.begin(); it != resourcesEnd; ++it)
        addAndUpdateScriptResource(it->second.get());

    unsigned messageCount = m_consoleMessages.size();
    for (unsigned i = 0; i < messageCount; ++i)
        addScriptConsoleMessage(m_consoleMessages[i]);
}

void InspectorController::addScriptConsoleMessage(const ConsoleMessage* message)
{
    ASSERT_ARG(message, message);

    JSStringRef messageConstructorString = JSStringCreateWithUTF8CString("ConsoleMessage");
    JSObjectRef messageConstructor = JSValueToObject(m_scriptContext, JSObjectGetProperty(m_scriptContext, m_scriptObject, messageConstructorString, 0), 0);
    JSStringRelease(messageConstructorString);

    JSStringRef addMessageString = JSStringCreateWithUTF8CString("addMessageToConsole");
    JSObjectRef addMessage = JSValueToObject(m_scriptContext, JSObjectGetProperty(m_scriptContext, m_scriptObject, addMessageString, 0), 0);
    JSStringRelease(addMessageString);

    JSValueRef sourceValue = JSValueMakeNumber(m_scriptContext, message->source);
    JSValueRef levelValue = JSValueMakeNumber(m_scriptContext, message->level);
    JSStringRef messageString = JSStringCreateWithCharacters(message->message.characters(), message->message.length());
    JSValueRef messageValue = JSValueMakeString(m_scriptContext, messageString);
    JSValueRef lineValue = JSValueMakeNumber(m_scriptContext, message->line);
    JSStringRef urlString = JSStringCreateWithCharacters(message->url.characters(), message->url.length());
    JSValueRef urlValue = JSValueMakeString(m_scriptContext, urlString);

    JSValueRef args[] = { sourceValue, levelValue, messageValue, lineValue, urlValue };
    JSObjectRef messageObject = JSObjectCallAsConstructor(m_scriptContext, messageConstructor, 5, args, 0);
    JSStringRelease(messageString);
    JSStringRelease(urlString);

    JSObjectCallAsFunction(m_scriptContext, addMessage, m_scriptObject, 1, &messageObject, 0);
}

static void callClearFunction(JSContextRef context, JSObjectRef thisObject, const char* functionName)
{
    ASSERT_ARG(context, context);
    ASSERT_ARG(thisObject, thisObject);

    JSStringRef string = JSStringCreateWithUTF8CString(functionName);
    JSObjectRef function = JSValueToObject(context, JSObjectGetProperty(context, thisObject, string, 0), 0);
    JSStringRelease(string);

    JSObjectCallAsFunction(context, function, thisObject, 0, 0, 0);
}

void InspectorController::clearScriptResources()
{
    if (!m_scriptContext || !m_scriptObject)
        return;

    ResourcesMap::iterator resourcesEnd = m_resources.end();
    for (ResourcesMap::iterator it = m_resources.begin(); it != resourcesEnd; ++it) {
        InspectorResource* resource = it->second.get();
        resource->setScriptObject(0, 0);
    }

    callClearFunction(m_scriptContext, m_scriptObject, "clearResources");
}

void InspectorController::clearScriptConsoleMessages()
{
    if (!m_scriptContext || !m_scriptObject)
        return;

    callClearFunction(m_scriptContext, m_scriptObject, "clearConsoleMessages");
}

void InspectorController::clearNetworkTimeline()
{
    if (!m_scriptContext || !m_scriptObject)
        return;

    callClearFunction(m_scriptContext, m_scriptObject, "clearNetworkTimeline");
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

    if (loader->frame() == m_inspectedPage->mainFrame()) {
        ASSERT(m_mainResource);
        // FIXME: Should look into asserting that m_mainResource->loader == loader here.

        m_client->inspectedURLChanged(loader->URL().url());
        deleteAllValues(m_consoleMessages);
        m_consoleMessages.clear();
        if (windowVisible()) {
            clearScriptConsoleMessages();
            clearNetworkTimeline();

            // We don't add the main resource until its load is committed. This
            // is needed to keep the load for a user-entered URL from showing
            // up in the list of resources for the page they are navigating
            // away from.
            addAndUpdateScriptResource(m_mainResource.get());
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

void InspectorController::didLoadResourceFromMemoryCache(DocumentLoader* loader, const ResourceRequest& request, const ResourceResponse& response, int length)
{
    if (!enabled())
        return;

    InspectorResource* resource = new InspectorResource(m_nextIdentifier--, loader, loader->frame());
    resource->finished = true;

    updateResourceRequest(resource, request);
    updateResourceResponse(resource, response);

    resource->length = length;
    resource->cached = true;
    resource->startTime = currentTime();
    resource->responseReceivedTime = resource->startTime;
    resource->endTime = resource->startTime;

    if (loader->frame() == m_inspectedPage->mainFrame() && request.url() == loader->requestURL())
        m_mainResource = resource;

    addResource(resource);

    if (windowVisible())
        addAndUpdateScriptResource(resource);
}

void InspectorController::identifierForInitialRequest(unsigned long identifier, DocumentLoader* loader, const ResourceRequest& request)
{
    if (!enabled())
        return;

    InspectorResource* resource = new InspectorResource(identifier, loader, loader->frame());

    updateResourceRequest(resource, request);

    if (loader->frame() == m_inspectedPage->mainFrame() && request.url() == loader->requestURL())
        m_mainResource = resource;

    addResource(resource);
}

void InspectorController::willSendRequest(DocumentLoader* loader, unsigned long identifier, ResourceRequest& request, const ResourceResponse& redirectResponse)
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

void InspectorController::didFinishLoading(DocumentLoader* loader, unsigned long identifier)
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

void InspectorController::didFailLoading(DocumentLoader* loader, unsigned long identifier, const ResourceError& /*error*/)
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

} // namespace WebCore
