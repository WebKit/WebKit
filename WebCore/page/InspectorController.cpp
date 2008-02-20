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
#include "FloatConversion.h"
#include "FloatRect.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLFrameOwnerElement.h"
#include "InspectorClient.h"
#include "JSRange.h"
#include "Page.h"
#include "Range.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "Settings.h"
#include "SharedBuffer.h"
#include "SystemTime.h"
#include "TextEncoding.h"
#include "TextIterator.h"
#include "kjs_dom.h"
#include "kjs_proxy.h"
#include "kjs_window.h"
#include <JavaScriptCore/APICast.h>
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/JSRetainPtr.h>
#include <JavaScriptCore/JSStringRef.h>
#include <wtf/RefCounted.h>

#if ENABLE(DATABASE)
#include "Database.h"
#include "JSDatabase.h"
#endif

namespace WebCore {

static JSValueRef callSimpleFunction(JSContextRef context, JSObjectRef thisObject, const char* functionName)
{
    ASSERT_ARG(context, context);
    ASSERT_ARG(thisObject, thisObject);

    JSRetainPtr<JSStringRef> functionNameString(Adopt, JSStringCreateWithUTF8CString(functionName));
    JSObjectRef function = JSValueToObject(context, JSObjectGetProperty(context, thisObject, functionNameString.get(), 0), 0);

    return JSObjectCallAsFunction(context, function, thisObject, 0, 0, 0);
}

#pragma mark -
#pragma mark ConsoleMessage Struct

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

#pragma mark -
#pragma mark InspectorResource Struct

struct InspectorResource : public RefCounted<InspectorResource> {
    // Keep these in sync with WebInspector.Resource.Type
    enum Type {
        Doc,
        Stylesheet,
        Image,
        Font,
        Script,
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

    long long identifier;
    RefPtr<DocumentLoader> loader;
    RefPtr<Frame> frame;
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
    
private:
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
};

#pragma mark -
#pragma mark InspectorDatabaseResource Struct

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

#pragma mark -
#pragma mark JavaScript Callbacks

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
    String textEncodingName;
    if (resource->requestURL == resource->loader->requestURL()) {
        buffer = resource->loader->mainResourceData();
        textEncodingName = resource->frame->document()->inputEncoding();
    } else {
        CachedResource* cachedResource = resource->frame->document()->docLoader()->cachedResource(resource->requestURL.string());
        if (!cachedResource)
            return undefined;

        buffer = cachedResource->data();
        textEncodingName = cachedResource->encoding();
    }

    if (!buffer)
        return undefined;

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

    Document* document = resource->frame->document();
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

    controller->close();
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

    JSRetainPtr<JSStringRef> searchString(Adopt, JSValueToStringCopy(ctx, arguments[1], 0));
    String target(JSStringGetCharactersPtr(searchString.get()), JSStringGetLength(searchString.get()));

    JSObjectRef global = JSContextGetGlobalObject(ctx);
    JSRetainPtr<JSStringRef> arrayString(Adopt, JSStringCreateWithUTF8CString("Array"));
    JSObjectRef arrayConstructor = JSValueToObject(ctx, JSObjectGetProperty(ctx, global, arrayString.get(), 0), 0);

    JSObjectRef result = JSObjectCallAsConstructor(ctx, arrayConstructor, 0, 0, 0);

    JSRetainPtr<JSStringRef> pushString(Adopt, JSStringCreateWithUTF8CString("push"));
    JSObjectRef pushFunction = JSValueToObject(ctx, JSObjectGetProperty(ctx, result, pushString.get(), 0), 0);

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
        JSObjectCallAsFunction(ctx, pushFunction, result, 1, &arg0, 0);

        setStart(searchRange.get(), newStart);
    } while (true);

    return result;
}

#if ENABLE(DATABASE)
static JSValueRef databaseTableNames(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* /*exception*/)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    if (argumentCount < 1)
        return JSValueMakeUndefined(ctx);

    Database* database = toDatabase(toJS(arguments[0]));
    if (!database)
        return JSValueMakeUndefined(ctx);

    JSObjectRef global = JSContextGetGlobalObject(ctx);
    JSRetainPtr<JSStringRef> arrayString(Adopt, JSStringCreateWithUTF8CString("Array"));
    JSObjectRef arrayConstructor = JSValueToObject(ctx, JSObjectGetProperty(ctx, global, arrayString.get(), 0), 0);

    JSObjectRef result = JSObjectCallAsConstructor(ctx, arrayConstructor, 0, 0, 0);

    JSRetainPtr<JSStringRef> pushString(Adopt, JSStringCreateWithUTF8CString("push"));
    JSObjectRef pushFunction = JSValueToObject(ctx, JSObjectGetProperty(ctx, result, pushString.get(), 0), 0);

    Vector<String> tableNames = database->tableNames();
    unsigned length = tableNames.size();
    for (unsigned i = 0; i < length; ++i) {
        String tableName = tableNames[i];
        JSRetainPtr<JSStringRef> tableNameString(Adopt, JSStringCreateWithCharacters(tableName.characters(), tableName.length()));
        JSValueRef tableNameValue = JSValueMakeString(ctx, tableNameString.get());

        JSValueRef pushArguments[] = { tableNameValue };
        JSObjectCallAsFunction(ctx, pushFunction, result, 1, pushArguments, 0);
    }

    return result;
}
#endif

static JSValueRef inspectedWindow(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t /*argumentCount*/, const JSValueRef[] /*arguments[]*/, JSValueRef* /*exception*/)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    return toRef(KJS::Window::retrieve(controller->inspectedPage()->mainFrame()));
}

static JSValueRef localizedStrings(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t /*argumentCount*/, const JSValueRef[] /*arguments[]*/, JSValueRef* /*exception*/)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    String url = controller->localizedStringsURL();
    if (url.isNull())
        return JSValueMakeNull(ctx);

    JSRetainPtr<JSStringRef> urlString(Adopt, JSStringCreateWithCharacters(url.characters(), url.length()));
    return JSValueMakeString(ctx, urlString.get());
}

static JSValueRef platform(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t /*argumentCount*/, const JSValueRef[] /*arguments[]*/, JSValueRef* /*exception*/)
{
#if PLATFORM(MAC)
#ifdef BUILDING_ON_TIGER
    static const String platform = "mac-tiger";
#else
    static const String platform = "mac-leopard";
#endif
#elif PLATFORM(WIN_OS)
    static const String platform = "windows";
#elif PLATFORM(QT)
    static const String platform = "qt";
#elif PLATFORM(GTK)
    static const String platform = "gtk";
#elif PLATFORM(WX)
    static const String platform = "wx";
#else
    static const String platform = "unknown";
#endif

    JSRetainPtr<JSStringRef> platformString(Adopt, JSStringCreateWithCharacters(platform.characters(), platform.length()));
    JSValueRef platformValue = JSValueMakeString(ctx, platformString.get());

    return platformValue;
}

static JSValueRef moveByUnrestricted(JSContextRef ctx, JSObjectRef /*function*/, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* /*exception*/)
{
    InspectorController* controller = reinterpret_cast<InspectorController*>(JSObjectGetPrivate(thisObject));
    if (!controller)
        return JSValueMakeUndefined(ctx);

    if (argumentCount < 2)
        return JSValueMakeUndefined(ctx);

    controller->moveWindowBy(narrowPrecisionToFloat(JSValueToNumber(ctx, arguments[0], 0)), narrowPrecisionToFloat(JSValueToNumber(ctx, arguments[1], 0)));

    return JSValueMakeUndefined(ctx);
}

#pragma mark -
#pragma mark InspectorController Class

InspectorController::InspectorController(Page* page, InspectorClient* client)
    : m_inspectedPage(page)
    , m_client(client)
    , m_page(0)
    , m_scriptObject(0)
    , m_controllerScriptObject(0)
    , m_scriptContext(0)
    , m_windowVisible(false)
    , m_showAfterVisible(FocusedNodeDocumentPanel)
    , m_nextIdentifier(-2)
{
    ASSERT_ARG(page, page);
    ASSERT_ARG(client, client);
}

InspectorController::~InspectorController()
{
    m_client->inspectorDestroyed();

    if (m_scriptContext) {
        JSObjectRef global = JSContextGetGlobalObject(m_scriptContext);
        JSRetainPtr<JSStringRef> controllerProperty(Adopt, JSStringCreateWithUTF8CString("InspectorController"));
        JSObjectRef controller = JSValueToObject(m_scriptContext, JSObjectGetProperty(m_scriptContext, global, controllerProperty.get(), 0), 0);
        if (controller)
            JSObjectSetPrivate(controller, 0);
    }

    if (m_page)
        m_page->setParentInspectorController(0);

    deleteAllValues(m_frameResources);
    deleteAllValues(m_consoleMessages);
}

bool InspectorController::enabled() const
{
    return m_inspectedPage->settings()->developerExtrasEnabled();
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
    return frame && frame->scriptProxy()->isEnabled();
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
        m_showAfterVisible = FocusedNodeDocumentPanel;
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

    JSValueRef arg0;

    {
        KJS::JSLock lock;
        arg0 = toRef(toJS(toJS(m_scriptContext), m_nodeToFocus.get()));
    }

    m_nodeToFocus = 0;

    JSRetainPtr<JSStringRef> functionProperty(Adopt, JSStringCreateWithUTF8CString("updateFocusedNode"));
    JSObjectRef function = JSValueToObject(m_scriptContext, JSObjectGetProperty(m_scriptContext, m_scriptObject, functionProperty.get(), 0), 0);
    ASSERT(function);

    JSObjectCallAsFunction(m_scriptContext, function, m_scriptObject, 1, &arg0, 0);
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
        if (m_showAfterVisible == ConsolePanel)
            showConsole();
        else if (m_showAfterVisible == TimelinePanel)
            showTimeline();
    } else {
        clearScriptResources();
        clearScriptConsoleMessages();
        clearDatabaseScriptResources();
        clearNetworkTimeline();
    }

    m_showAfterVisible = FocusedNodeDocumentPanel;
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

    m_scriptContext = toRef(m_page->mainFrame()->scriptProxy()->globalObject()->globalExec());

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
        { "search", search, kJSPropertyAttributeNone },
#if ENABLE(DATABASE)
        { "databaseTableNames", databaseTableNames, kJSPropertyAttributeNone },
#endif
        { "inspectedWindow", inspectedWindow, kJSPropertyAttributeNone },
        { "localizedStringsURL", localizedStrings, kJSPropertyAttributeNone },
        { "platform", platform, kJSPropertyAttributeNone },
        { "moveByUnrestricted", moveByUnrestricted, kJSPropertyAttributeNone },
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

    JSRetainPtr<JSStringRef> controllerObjectString(Adopt, JSStringCreateWithUTF8CString("InspectorController"));
    JSObjectSetProperty(m_scriptContext, global, controllerObjectString.get(), m_controllerScriptObject, kJSPropertyAttributeNone, 0);
}

void InspectorController::scriptObjectReady()
{
    ASSERT(m_scriptContext);
    if (!m_scriptContext)
        return;

    JSObjectRef global = JSContextGetGlobalObject(m_scriptContext);
    ASSERT(global);

    JSRetainPtr<JSStringRef> inspectorString(Adopt, JSStringCreateWithUTF8CString("WebInspector"));
    JSValueRef inspectorValue = JSObjectGetProperty(m_scriptContext, global, inspectorString.get(), 0);

    ASSERT(inspectorValue);
    if (!inspectorValue)
        return;

    m_scriptObject = JSValueToObject(m_scriptContext, inspectorValue, 0);
    ASSERT(m_scriptObject);

    JSValueProtect(m_scriptContext, m_scriptObject);

    // Make sure our window is visible now that the page loaded
    m_client->showWindow();
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

        // m_client->showWindow() will be called after the page loads in scriptObjectReady()
        return;
    }

    m_client->showWindow();
}

void InspectorController::showConsole()
{
    if (!enabled())
        return;

    show();

    if (!m_scriptObject) {
        m_showAfterVisible = ConsolePanel;
        return;
    }

    callSimpleFunction(m_scriptContext, m_scriptObject, "showConsole");
}

void InspectorController::showTimeline()
{
    if (!enabled())
        return;

    show();

    if (!m_scriptObject) {
        m_showAfterVisible = TimelinePanel;
        return;
    }

    callSimpleFunction(m_scriptContext, m_scriptObject, "showTimeline");
}

void InspectorController::close()
{
    if (!enabled())
        return;

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
        JSRetainPtr<JSStringRef> field(Adopt, JSStringCreateWithCharacters(it->first.characters(), it->first.length()));
        JSRetainPtr<JSStringRef> valueString(Adopt, JSStringCreateWithCharacters(it->second.characters(), it->second.length()));
        JSValueRef value = JSValueMakeString(context, valueString.get());
        JSObjectSetProperty(context, object, field.get(), value, kJSPropertyAttributeNone, 0);
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

    ASSERT(m_scriptContext);
    ASSERT(m_scriptObject);
    if (!m_scriptContext || !m_scriptObject)
        return 0;

    if (!resource->scriptObject) {
        JSRetainPtr<JSStringRef> resourceString(Adopt, JSStringCreateWithUTF8CString("Resource"));
        JSObjectRef resourceConstructor = JSValueToObject(m_scriptContext, JSObjectGetProperty(m_scriptContext, m_scriptObject, resourceString.get(), 0), 0);

        String urlString = resource->requestURL.string();
        JSRetainPtr<JSStringRef> url(Adopt, JSStringCreateWithCharacters(urlString.characters(), urlString.length()));
        JSValueRef urlValue = JSValueMakeString(m_scriptContext, url.get());

        urlString = resource->requestURL.host();
        JSRetainPtr<JSStringRef> domain(Adopt, JSStringCreateWithCharacters(urlString.characters(), urlString.length()));
        JSValueRef domainValue = JSValueMakeString(m_scriptContext, domain.get());

        urlString = resource->requestURL.path();
        JSRetainPtr<JSStringRef> path(Adopt, JSStringCreateWithCharacters(urlString.characters(), urlString.length()));
        JSValueRef pathValue = JSValueMakeString(m_scriptContext, path.get());

        urlString = resource->requestURL.lastPathComponent();
        JSRetainPtr<JSStringRef> lastPathComponent(Adopt, JSStringCreateWithCharacters(urlString.characters(), urlString.length()));
        JSValueRef lastPathComponentValue = JSValueMakeString(m_scriptContext, lastPathComponent.get());

        JSValueRef identifier = JSValueMakeNumber(m_scriptContext, resource->identifier);
        JSValueRef mainResource = JSValueMakeBoolean(m_scriptContext, m_mainResource == resource);
        JSValueRef cached = JSValueMakeBoolean(m_scriptContext, resource->cached);

        JSValueRef arguments[] = { scriptObjectForRequest(m_scriptContext, resource), urlValue, domainValue, pathValue, lastPathComponentValue, identifier, mainResource, cached };
        JSObjectRef result = JSObjectCallAsConstructor(m_scriptContext, resourceConstructor, 8, arguments, 0);
        ASSERT(result);

        resource->setScriptObject(m_scriptContext, result);
    }

    JSRetainPtr<JSStringRef> addResourceString(Adopt, JSStringCreateWithUTF8CString("addResource"));
    JSObjectRef addResourceFunction = JSValueToObject(m_scriptContext, JSObjectGetProperty(m_scriptContext, m_scriptObject, addResourceString.get(), 0), 0);

    JSValueRef addArguments[] = { resource->scriptObject };
    JSObjectCallAsFunction(m_scriptContext, addResourceFunction, m_scriptObject, 1, addArguments, 0);

    return resource->scriptObject;
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

    JSRetainPtr<JSStringRef> removeResourceString(Adopt, JSStringCreateWithUTF8CString("removeResource"));
    JSObjectRef removeResourceFunction = JSValueToObject(m_scriptContext, JSObjectGetProperty(m_scriptContext, m_scriptObject, removeResourceString.get(), 0), 0);

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
}

void InspectorController::updateScriptResourceRequest(InspectorResource* resource)
{
    ASSERT(resource->scriptObject);
    ASSERT(m_scriptContext);
    if (!resource->scriptObject || !m_scriptContext)
        return;

    String urlString = resource->requestURL.string();
    JSRetainPtr<JSStringRef> url(Adopt, JSStringCreateWithCharacters(urlString.characters(), urlString.length()));
    JSValueRef urlValue = JSValueMakeString(m_scriptContext, url.get());

    urlString = resource->requestURL.host();
    JSRetainPtr<JSStringRef> domain(Adopt, JSStringCreateWithCharacters(urlString.characters(), urlString.length()));
    JSValueRef domainValue = JSValueMakeString(m_scriptContext, domain.get());

    urlString = resource->requestURL.path();
    JSRetainPtr<JSStringRef> path(Adopt, JSStringCreateWithCharacters(urlString.characters(), urlString.length()));
    JSValueRef pathValue = JSValueMakeString(m_scriptContext, path.get());

    urlString = resource->requestURL.lastPathComponent();
    JSRetainPtr<JSStringRef> lastPathComponent(Adopt, JSStringCreateWithCharacters(urlString.characters(), urlString.length()));
    JSValueRef lastPathComponentValue = JSValueMakeString(m_scriptContext, lastPathComponent.get());

    JSValueRef mainResourceValue = JSValueMakeBoolean(m_scriptContext, m_mainResource == resource);

    JSRetainPtr<JSStringRef> propertyName(Adopt, JSStringCreateWithUTF8CString("url"));
    JSObjectSetProperty(m_scriptContext, resource->scriptObject, propertyName.get(), urlValue, kJSPropertyAttributeNone, 0);

    propertyName.adopt(JSStringCreateWithUTF8CString("domain"));
    JSObjectSetProperty(m_scriptContext, resource->scriptObject, propertyName.get(), domainValue, kJSPropertyAttributeNone, 0);

    propertyName.adopt(JSStringCreateWithUTF8CString("path"));
    JSObjectSetProperty(m_scriptContext, resource->scriptObject, propertyName.get(), pathValue, kJSPropertyAttributeNone, 0);

    propertyName.adopt(JSStringCreateWithUTF8CString("lastPathComponent"));
    JSObjectSetProperty(m_scriptContext, resource->scriptObject, propertyName.get(), lastPathComponentValue, kJSPropertyAttributeNone, 0);

    propertyName.adopt(JSStringCreateWithUTF8CString("requestHeaders"));
    JSObjectSetProperty(m_scriptContext, resource->scriptObject, propertyName.get(), scriptObjectForRequest(m_scriptContext, resource), kJSPropertyAttributeNone, 0);

    propertyName.adopt(JSStringCreateWithUTF8CString("mainResource"));
    JSObjectSetProperty(m_scriptContext, resource->scriptObject, propertyName.get(), mainResourceValue, kJSPropertyAttributeNone, 0);
}

void InspectorController::updateScriptResourceResponse(InspectorResource* resource)
{
    ASSERT(resource->scriptObject);
    ASSERT(m_scriptContext);
    if (!resource->scriptObject || !m_scriptContext)
        return;

    JSRetainPtr<JSStringRef> mimeType(Adopt, JSStringCreateWithCharacters(resource->mimeType.characters(), resource->mimeType.length()));
    JSValueRef mimeTypeValue = JSValueMakeString(m_scriptContext, mimeType.get());

    JSRetainPtr<JSStringRef> suggestedFilename(Adopt, JSStringCreateWithCharacters(resource->suggestedFilename.characters(), resource->suggestedFilename.length()));
    JSValueRef suggestedFilenameValue = JSValueMakeString(m_scriptContext, suggestedFilename.get());

    JSValueRef expectedContentLengthValue = JSValueMakeNumber(m_scriptContext, static_cast<double>(resource->expectedContentLength));
    JSValueRef statusCodeValue = JSValueMakeNumber(m_scriptContext, resource->responseStatusCode);

    JSRetainPtr<JSStringRef> propertyName(Adopt, JSStringCreateWithUTF8CString("mimeType"));
    JSObjectSetProperty(m_scriptContext, resource->scriptObject, propertyName.get(), mimeTypeValue, kJSPropertyAttributeNone, 0);

    propertyName.adopt(JSStringCreateWithUTF8CString("suggestedFilename"));
    JSObjectSetProperty(m_scriptContext, resource->scriptObject, propertyName.get(), suggestedFilenameValue, kJSPropertyAttributeNone, 0);

    propertyName.adopt(JSStringCreateWithUTF8CString("expectedContentLength"));
    JSObjectSetProperty(m_scriptContext, resource->scriptObject, propertyName.get(), expectedContentLengthValue, kJSPropertyAttributeNone, 0);

    propertyName.adopt(JSStringCreateWithUTF8CString("statusCode"));
    JSObjectSetProperty(m_scriptContext, resource->scriptObject, propertyName.get(), statusCodeValue, kJSPropertyAttributeNone, 0);

    propertyName.adopt(JSStringCreateWithUTF8CString("responseHeaders"));
    JSObjectSetProperty(m_scriptContext, resource->scriptObject, propertyName.get(), scriptObjectForResponse(m_scriptContext, resource), kJSPropertyAttributeNone, 0);

    JSValueRef typeValue = JSValueMakeNumber(m_scriptContext, resource->type());
    propertyName.adopt(JSStringCreateWithUTF8CString("type"));
    JSObjectSetProperty(m_scriptContext, resource->scriptObject, propertyName.get(), typeValue, kJSPropertyAttributeNone, 0);
}

void InspectorController::updateScriptResource(InspectorResource* resource, int length)
{
    ASSERT(resource->scriptObject);
    ASSERT(m_scriptContext);
    if (!resource->scriptObject || !m_scriptContext)
        return;

    JSValueRef lengthValue = JSValueMakeNumber(m_scriptContext, length);

    JSRetainPtr<JSStringRef> propertyName(Adopt, JSStringCreateWithUTF8CString("contentLength"));
    JSObjectSetProperty(m_scriptContext, resource->scriptObject, propertyName.get(), lengthValue, kJSPropertyAttributeNone, 0);
}

void InspectorController::updateScriptResource(InspectorResource* resource, bool finished, bool failed)
{
    ASSERT(resource->scriptObject);
    ASSERT(m_scriptContext);
    if (!resource->scriptObject || !m_scriptContext)
        return;

    JSValueRef failedValue = JSValueMakeBoolean(m_scriptContext, failed);
    JSValueRef finishedValue = JSValueMakeBoolean(m_scriptContext, finished);

    JSRetainPtr<JSStringRef> propertyName(Adopt, JSStringCreateWithUTF8CString("failed"));
    JSObjectSetProperty(m_scriptContext, resource->scriptObject, propertyName.get(), failedValue, kJSPropertyAttributeNone, 0);

    propertyName.adopt(JSStringCreateWithUTF8CString("finished"));
    JSObjectSetProperty(m_scriptContext, resource->scriptObject, propertyName.get(), finishedValue, kJSPropertyAttributeNone, 0);
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

    JSRetainPtr<JSStringRef> propertyName(Adopt, JSStringCreateWithUTF8CString("startTime"));
    JSObjectSetProperty(m_scriptContext, resource->scriptObject, propertyName.get(), startTimeValue, kJSPropertyAttributeNone, 0);

    propertyName.adopt(JSStringCreateWithUTF8CString("responseReceivedTime"));
    JSObjectSetProperty(m_scriptContext, resource->scriptObject, propertyName.get(), responseReceivedTimeValue, kJSPropertyAttributeNone, 0);

    propertyName.adopt(JSStringCreateWithUTF8CString("endTime"));
    JSObjectSetProperty(m_scriptContext, resource->scriptObject, propertyName.get(), endTimeValue, kJSPropertyAttributeNone, 0);
}

void InspectorController::populateScriptResources()
{
    ASSERT(m_scriptContext);
    if (!m_scriptContext)
        return;

    clearScriptResources();
    clearScriptConsoleMessages();
    clearDatabaseScriptResources();
    clearNetworkTimeline();

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

    JSRetainPtr<JSStringRef> databaseString(Adopt, JSStringCreateWithUTF8CString("Database"));
    JSObjectRef databaseConstructor = JSValueToObject(m_scriptContext, JSObjectGetProperty(m_scriptContext, m_scriptObject, databaseString.get(), 0), 0);

    JSValueRef database;

    {
        KJS::JSLock lock;
        database = toRef(toJS(toJS(m_scriptContext), resource->database.get()));
    }

    JSRetainPtr<JSStringRef> domain(Adopt, JSStringCreateWithCharacters(resource->domain.characters(), resource->domain.length()));
    JSValueRef domainValue = JSValueMakeString(m_scriptContext, domain.get());

    JSRetainPtr<JSStringRef> name(Adopt, JSStringCreateWithCharacters(resource->name.characters(), resource->name.length()));
    JSValueRef nameValue = JSValueMakeString(m_scriptContext, name.get());

    JSRetainPtr<JSStringRef> version(Adopt, JSStringCreateWithCharacters(resource->version.characters(), resource->version.length()));
    JSValueRef versionValue = JSValueMakeString(m_scriptContext, version.get());

    JSValueRef arguments[] = { database, domainValue, nameValue, versionValue };
    JSObjectRef result = JSObjectCallAsConstructor(m_scriptContext, databaseConstructor, 4, arguments, 0);

    resource->setScriptObject(m_scriptContext, result);

    ASSERT(result);

    JSRetainPtr<JSStringRef> addResourceString(Adopt, JSStringCreateWithUTF8CString("addResource"));
    JSObjectRef addResourceFunction = JSValueToObject(m_scriptContext, JSObjectGetProperty(m_scriptContext, m_scriptObject, addResourceString.get(), 0), 0);

    JSValueRef addArguments[] = { result };
    JSObjectCallAsFunction(m_scriptContext, addResourceFunction, m_scriptObject, 1, addArguments, 0);

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

    JSRetainPtr<JSStringRef> removeResourceString(Adopt, JSStringCreateWithUTF8CString("removeResource"));
    JSObjectRef removeResourceFunction = JSValueToObject(m_scriptContext, JSObjectGetProperty(m_scriptContext, m_scriptObject, removeResourceString.get(), 0), 0);

    JSValueRef arguments[] = { resource->scriptObject };
    JSObjectCallAsFunction(m_scriptContext, removeResourceFunction, m_scriptObject, 1, arguments, 0);

    resource->setScriptObject(0, 0);
}
#endif

void InspectorController::addScriptConsoleMessage(const ConsoleMessage* message)
{
    ASSERT_ARG(message, message);

    JSRetainPtr<JSStringRef> messageConstructorString(Adopt, JSStringCreateWithUTF8CString("ConsoleMessage"));
    JSObjectRef messageConstructor = JSValueToObject(m_scriptContext, JSObjectGetProperty(m_scriptContext, m_scriptObject, messageConstructorString.get(), 0), 0);

    JSRetainPtr<JSStringRef> addMessageString(Adopt, JSStringCreateWithUTF8CString("addMessageToConsole"));
    JSObjectRef addMessage = JSValueToObject(m_scriptContext, JSObjectGetProperty(m_scriptContext, m_scriptObject, addMessageString.get(), 0), 0);

    JSValueRef sourceValue = JSValueMakeNumber(m_scriptContext, message->source);
    JSValueRef levelValue = JSValueMakeNumber(m_scriptContext, message->level);
    JSRetainPtr<JSStringRef> messageString(Adopt, JSStringCreateWithCharacters(message->message.characters(), message->message.length()));
    JSValueRef messageValue = JSValueMakeString(m_scriptContext, messageString.get());
    JSValueRef lineValue = JSValueMakeNumber(m_scriptContext, message->line);
    JSRetainPtr<JSStringRef> urlString(Adopt, JSStringCreateWithCharacters(message->url.characters(), message->url.length()));
    JSValueRef urlValue = JSValueMakeString(m_scriptContext, urlString.get());

    JSValueRef args[] = { sourceValue, levelValue, messageValue, lineValue, urlValue };
    JSObjectRef messageObject = JSObjectCallAsConstructor(m_scriptContext, messageConstructor, 5, args, 0);

    JSObjectCallAsFunction(m_scriptContext, addMessage, m_scriptObject, 1, &messageObject, 0);
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

    callSimpleFunction(m_scriptContext, m_scriptObject, "clearResources");
}

void InspectorController::clearDatabaseScriptResources()
{
#if ENABLE(DATABASE)
    if (!m_scriptContext || !m_scriptObject)
        return;

    DatabaseResourcesSet::iterator databasesEnd = m_databaseResources.end();
    for (DatabaseResourcesSet::iterator it = m_databaseResources.begin(); it != databasesEnd; ++it) {
        InspectorDatabaseResource* resource = (*it).get();
        resource->setScriptObject(0, 0);
    }

    callSimpleFunction(m_scriptContext, m_scriptObject, "clearDatabaseResources");
#endif
}

void InspectorController::clearScriptConsoleMessages()
{
    if (!m_scriptContext || !m_scriptObject)
        return;

    callSimpleFunction(m_scriptContext, m_scriptObject, "clearConsoleMessages");
}

void InspectorController::clearNetworkTimeline()
{
    if (!m_scriptContext || !m_scriptObject)
        return;

    callSimpleFunction(m_scriptContext, m_scriptObject, "clearNetworkTimeline");
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
        m_client->inspectedURLChanged(loader->url().string());

        deleteAllValues(m_consoleMessages);
        m_consoleMessages.clear();

#if ENABLE(DATABASE)
        m_databaseResources.clear();
#endif

        if (windowVisible()) {
            clearScriptConsoleMessages();
#if ENABLE(DATABASE)
            clearDatabaseScriptResources();
#endif
            clearNetworkTimeline();

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

    RefPtr<InspectorResource> resource = InspectorResource::create(m_nextIdentifier--, loader, loader->frame());
    resource->finished = true;

    updateResourceRequest(resource.get(), request);
    updateResourceResponse(resource.get(), response);

    resource->length = length;
    resource->cached = true;
    resource->startTime = currentTime();
    resource->responseReceivedTime = resource->startTime;
    resource->endTime = resource->startTime;

    if (loader->frame() == m_inspectedPage->mainFrame() && request.url() == loader->requestURL())
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

    if (loader->frame() == m_inspectedPage->mainFrame() && request.url() == loader->requestURL())
        m_mainResource = resource;

    addResource(resource.get());

    if (windowVisible() && loader->isLoadingFromCachedPage() && resource == m_mainResource)
        addAndUpdateScriptResource(resource.get());
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

void InspectorController::drawNodeHighlight(GraphicsContext& context) const
{
    static const Color overlayFillColor(0, 0, 0, 128);
    static const int outlineThickness = 1;

    if (!m_highlightedNode)
        return;

    RenderObject* renderer = m_highlightedNode->renderer();
    if (!renderer)
        return;
    IntRect nodeRect(renderer->absoluteBoundingBoxRect());

    Vector<IntRect> rects;
    if (renderer->isInline() || (renderer->isText() && !m_highlightedNode->isSVGElement()))
        renderer->addLineBoxRects(rects);
    if (rects.isEmpty())
        rects.append(nodeRect);

    FrameView* view = m_inspectedPage->mainFrame()->view();
    FloatRect overlayRect = static_cast<ScrollView*>(view)->visibleContentRect();

    if (!overlayRect.contains(nodeRect) && !nodeRect.contains(enclosingIntRect(overlayRect))) {
        Element* element;
        if (m_highlightedNode->isElementNode())
            element = static_cast<Element*>(m_highlightedNode.get());
        else
            element = static_cast<Element*>(m_highlightedNode->parent());
        element->scrollIntoViewIfNeeded();
        overlayRect = static_cast<ScrollView*>(view)->visibleContentRect();
    }

    context.translate(-overlayRect.x(), -overlayRect.y());

    // Draw translucent gray fill, out of which we will cut holes.
    context.fillRect(overlayRect, overlayFillColor);

    // Draw white frames around holes in first pass, so they will be erased in
    // places where holes overlap or abut.
    for (size_t i = 0; i < rects.size(); ++i) {
        IntRect rect = rects[i];
        rect.inflate(outlineThickness);
        context.fillRect(rect, Color::white);
    }

    // Erase holes in second pass.
    for (size_t i = 0; i < rects.size(); ++i)
        context.clearRect(rects[i]);
}

} // namespace WebCore
