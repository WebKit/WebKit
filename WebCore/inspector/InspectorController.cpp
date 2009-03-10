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
#include "ConsoleMessage.h"
#include "DOMWindow.h"
#include "DocLoader.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "Element.h"
#include "FloatConversion.h"
#include "FloatQuad.h"
#include "FloatRect.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLFrameOwnerElement.h"
#include "HitTestResult.h"
#include "InspectorClient.h"
#include "InspectorDatabaseResource.h"
#include "InspectorDOMStorageResource.h"
#include "InspectorResource.h"
#include "JSDOMWindow.h"
#include "JSInspectedObjectWrapper.h"
#include "JSInspectorCallbackWrapper.h"
#include "JSInspectorController.h"
#include "JSNode.h"
#include "JSRange.h"
#include "JavaScriptProfile.h"
#include "Page.h"
#include "Range.h"
#include "RenderInline.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "ScriptCallStack.h"
#include "ScriptController.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "SharedBuffer.h"
#include "TextEncoding.h"
#include "TextIterator.h"
#include <JavaScriptCore/APICast.h>
#include <JavaScriptCore/JSRetainPtr.h>
#include <JavaScriptCore/JSStringRef.h>
#include <JavaScriptCore/OpaqueJSString.h>
#include <profiler/Profile.h>
#include <profiler/Profiler.h>
#include <runtime/CollectorHeapIterator.h>
#include <runtime/JSLock.h>
#include <runtime/UString.h>
#include <wtf/CurrentTime.h>
#include <wtf/RefCounted.h>
#include <wtf/StdLibExtras.h>

#if ENABLE(DATABASE)
#include "Database.h"
#include "JSDatabase.h"
#endif

#if ENABLE(DOM_STORAGE)
#include "Storage.h"
#include "StorageArea.h"
#include "JSStorage.h"
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

bool InspectorController::addSourceToFrame(const String& mimeType, const String& source, Node* frameNode)
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

const String& InspectorController::platform() const
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

    return platform;
}

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
    ASSERT(m_inspectedPage);
    m_inspectedPage = 0;

    close();
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

String InspectorController::hiddenPanels()
{
    if (!enabled())
        return String();
    return m_client->hiddenPanels();
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
    m_highlightedNode = 0;
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

    // FIXME: This should be cleaned up. API Mix-up.
    JSGlobalObject* globalObject = m_page->mainFrame()->script()->globalObject();
    ExecState* exec = globalObject->globalExec();
    m_scriptContext = toRef(exec);
    JSValuePtr jsInspector = toJS(exec, this);
    m_controllerScriptObject = toRef(asObject(jsInspector));
    globalObject->putDirect(Identifier(exec, "InspectorController"), jsInspector);
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

    if (m_inspectedPage) {
        ExecState* exec = toJSDOMWindow(m_inspectedPage->mainFrame())->globalExec();
        RefPtr<Profile> profile = Profiler::profiler()->stopProfiling(exec, title);
        if (profile)
            addProfile(profile, 0, UString());
    }

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
#if ENABLE(DOM_STORAGE)
    DOMStorageResourcesSet::iterator domStorageEnd = m_domStorageResources.end();
    for (DOMStorageResourcesSet::iterator it = m_domStorageResources.begin(); it != domStorageEnd; ++it)
        addDOMStorageScriptResource(it->get());
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

#if ENABLE(DOM_STORAGE)
JSObjectRef InspectorController::addDOMStorageScriptResource(InspectorDOMStorageResource* resource)
{
    ASSERT_ARG(resource, resource);

    if (resource->scriptObject)
        return resource->scriptObject;

    ASSERT(m_scriptContext);
    ASSERT(m_scriptObject);
    if (!m_scriptContext || !m_scriptObject)
        return 0;

    JSValueRef exception = 0;

    JSValueRef domStorageProperty = JSObjectGetProperty(m_scriptContext, m_scriptObject, jsStringRef("DOMStorage").get(), &exception);
    if (HANDLE_EXCEPTION(m_scriptContext, exception))
        return 0;

    JSObjectRef domStorageConstructor = JSValueToObject(m_scriptContext, domStorageProperty, &exception);
    if (HANDLE_EXCEPTION(m_scriptContext, exception))
        return 0;

    ExecState* exec = toJSDOMWindow(resource->frame.get())->globalExec();

    JSValueRef domStorage;

    {
        JSC::JSLock lock(false);
        domStorage = toRef(JSInspectedObjectWrapper::wrap(exec, toJS(exec, resource->domStorage.get())));
    }

    JSValueRef domainValue = JSValueMakeString(m_scriptContext, jsStringRef(resource->frame->document()->securityOrigin()->host()).get());
    JSValueRef isLocalStorageValue = JSValueMakeBoolean(m_scriptContext, resource->isLocalStorage);

    JSValueRef arguments[] = { domStorage, domainValue, isLocalStorageValue };
    JSObjectRef result = JSObjectCallAsConstructor(m_scriptContext, domStorageConstructor, 3, arguments, &exception);
    if (HANDLE_EXCEPTION(m_scriptContext, exception))
        return 0;

    ASSERT(result);

    callFunction(m_scriptContext, m_scriptObject, "addDOMStorage", 1, &result, exception);

    if (exception)
        return 0;

    resource->setScriptObject(m_scriptContext, result);

    return result;
}

void InspectorController::removeDOMStorageScriptResource(InspectorDOMStorageResource* resource)
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
    callFunction(m_scriptContext, m_scriptObject, "removeDOMStorage", 1, &scriptObject, exception);
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
#if ENABLE(DOM_STORAGE)
    DOMStorageResourcesSet::iterator domStorageEnd = m_domStorageResources.end();
    for (DOMStorageResourcesSet::iterator it = m_domStorageResources.begin(); it != domStorageEnd; ++it) {
        InspectorDOMStorageResource* resource = it->get();
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
#if ENABLE(DOM_STORAGE)
        m_domStorageResources.clear();
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

void InspectorController::scriptImported(unsigned long identifier, const JSC::UString& sourceString)
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

#if ENABLE(DOM_STORAGE)
void InspectorController::didUseDOMStorage(StorageArea* storageArea, bool isLocalStorage, Frame* frame)
{
    if (!enabled())
        return;

    DOMStorageResourcesSet::iterator domStorageEnd = m_domStorageResources.end();
    for (DOMStorageResourcesSet::iterator it = m_domStorageResources.begin(); it != domStorageEnd; ++it) {
        InspectorDOMStorageResource* resource = it->get();
        if (equalIgnoringCase(resource->frame->document()->securityOrigin()->host(), frame->document()->securityOrigin()->host()) && resource->isLocalStorage == isLocalStorage)
            return;
    }
    RefPtr<Storage> domStorage = Storage::create(frame, storageArea);
    RefPtr<InspectorDOMStorageResource> resource = InspectorDOMStorageResource::create(domStorage.get(), isLocalStorage, frame);

    m_domStorageResources.add(resource);
    if (windowVisible())
        addDOMStorageScriptResource(resource.get());
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

static Path quadToPath(const FloatQuad& quad)
{
    Path quadPath;
    quadPath.moveTo(quad.p1());
    quadPath.addLineTo(quad.p2());
    quadPath.addLineTo(quad.p3());
    quadPath.addLineTo(quad.p4());
    quadPath.closeSubpath();
    return quadPath;
}

static void drawOutlinedQuad(GraphicsContext& context, const FloatQuad& quad, const Color& fillColor)
{
    static const int outlineThickness = 2;
    static const Color outlineColor(62, 86, 180, 228);

    Path quadPath = quadToPath(quad);

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

static void drawOutlinedQuadWithClip(GraphicsContext& context, const FloatQuad& quad, const FloatQuad& clipQuad, const Color& fillColor)
{
    context.save();
    Path clipQuadPath = quadToPath(clipQuad);
    context.clipOut(clipQuadPath);
    drawOutlinedQuad(context, quad, fillColor);
    context.restore();
}

static void drawHighlightForBox(GraphicsContext& context, const FloatQuad& contentQuad, const FloatQuad& paddingQuad, const FloatQuad& borderQuad, const FloatQuad& marginQuad)
{
    static const Color contentBoxColor(125, 173, 217, 128);
    static const Color paddingBoxColor(125, 173, 217, 160);
    static const Color borderBoxColor(125, 173, 217, 192);
    static const Color marginBoxColor(125, 173, 217, 228);

    if (marginQuad != borderQuad)
        drawOutlinedQuadWithClip(context, marginQuad, borderQuad, marginBoxColor);
    if (borderQuad != paddingQuad)
        drawOutlinedQuadWithClip(context, borderQuad, paddingQuad, borderBoxColor);
    if (paddingQuad != contentQuad)
        drawOutlinedQuadWithClip(context, paddingQuad, contentQuad, paddingBoxColor);

    drawOutlinedQuad(context, contentQuad, contentBoxColor);
}

static void drawHighlightForLineBoxes(GraphicsContext& context, const Vector<FloatQuad>& lineBoxQuads)
{
    static const Color lineBoxColor(125, 173, 217, 128);

    for (size_t i = 0; i < lineBoxQuads.size(); ++i)
        drawOutlinedQuad(context, lineBoxQuads[i], lineBoxColor);
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

    RenderObject* renderer = m_highlightedNode->renderer();
    Frame* containingFrame = m_highlightedNode->document()->frame();
    if (!renderer || !containingFrame)
        return;

    IntSize mainFrameOffset = frameToMainFrameOffset(containingFrame);
    IntRect boundingBox = renderer->absoluteBoundingBoxRect(true);
    boundingBox.move(mainFrameOffset);

    ASSERT(m_inspectedPage);

    FrameView* view = m_inspectedPage->mainFrame()->view();
    FloatRect overlayRect = view->visibleContentRect();
    if (!overlayRect.contains(boundingBox) && !boundingBox.contains(enclosingIntRect(overlayRect)))
        overlayRect = view->visibleContentRect();
    context.translate(-overlayRect.x(), -overlayRect.y());

    if (renderer->isBox()) {
        RenderBox* renderBox = toRenderBox(renderer);

        IntRect contentBox = renderBox->contentBoxRect();

        IntRect paddingBox(contentBox.x() - renderBox->paddingLeft(), contentBox.y() - renderBox->paddingTop(),
                           contentBox.width() + renderBox->paddingLeft() + renderBox->paddingRight(), contentBox.height() + renderBox->paddingTop() + renderBox->paddingBottom());
        IntRect borderBox(paddingBox.x() - renderBox->borderLeft(), paddingBox.y() - renderBox->borderTop(),
                          paddingBox.width() + renderBox->borderLeft() + renderBox->borderRight(), paddingBox.height() + renderBox->borderTop() + renderBox->borderBottom());
        IntRect marginBox(borderBox.x() - renderBox->marginLeft(), borderBox.y() - renderBox->marginTop(),
                          borderBox.width() + renderBox->marginLeft() + renderBox->marginRight(), borderBox.height() + renderBox->marginTop() + renderBox->marginBottom());

        FloatQuad absContentQuad = renderBox->localToAbsoluteQuad(FloatRect(contentBox));
        FloatQuad absPaddingQuad = renderBox->localToAbsoluteQuad(FloatRect(paddingBox));
        FloatQuad absBorderQuad = renderBox->localToAbsoluteQuad(FloatRect(borderBox));
        FloatQuad absMarginQuad = renderBox->localToAbsoluteQuad(FloatRect(marginBox));

        absContentQuad.move(mainFrameOffset);
        absPaddingQuad.move(mainFrameOffset);
        absBorderQuad.move(mainFrameOffset);
        absMarginQuad.move(mainFrameOffset);

        drawHighlightForBox(context, absContentQuad, absPaddingQuad, absBorderQuad, absMarginQuad);
    } else if (renderer->isRenderInline()) {
        RenderInline* renderInline = toRenderInline(renderer);

        // FIXME: We should show margins/padding/border for inlines.
        Vector<FloatQuad> lineBoxQuads;
        renderInline->absoluteQuadsForRange(lineBoxQuads);
        for (unsigned i = 0; i < lineBoxQuads.size(); ++i)
            lineBoxQuads[i] += mainFrameOffset;

        drawHighlightForLineBoxes(context, lineBoxQuads);
    }
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
