/*
 * Copyright (C) 2006-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Collabora Ltd. All rights reserved.
 * Copyright (C) 2010 Girish Ramakrishnan <girish@forwardbias.in>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "PluginView.h"

#include "PluginDatabase.h"
#include "PluginDebug.h"
#include "PluginPackage.h"
#include "WebFrameLoaderClient.h"
#include <JavaScriptCore/JSCJSValue.h>
#include <JavaScriptCore/JSLock.h>
#include <WebCore/BridgeJSC.h>
#include <WebCore/Chrome.h>
#include <WebCore/CommonVM.h>
#include <WebCore/CookieJar.h>
#include <WebCore/Document.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/Element.h>
#include <WebCore/EventNames.h>
#include <WebCore/FocusController.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameLoadRequest.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/FrameLoaderClient.h>
#include <WebCore/FrameTree.h>
#include <WebCore/FrameView.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/HTMLNames.h>
#include <WebCore/HTMLPlugInElement.h>
#include <WebCore/HTTPHeaderNames.h>
#include <WebCore/Image.h>
#include <WebCore/JSDOMBinding.h>
#include <WebCore/JSDOMWindow.h>
#include <WebCore/KeyboardEvent.h>
#include <WebCore/MIMETypeRegistry.h>
#include <WebCore/MouseEvent.h>
#include <WebCore/NP_jsobject.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/Page.h>
#include <WebCore/PlatformMouseEvent.h>
#include <WebCore/ProxyServer.h>
#include <WebCore/RenderBox.h>
#include <WebCore/RenderObject.h>
#include <WebCore/ScriptController.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/Settings.h>
#include <WebCore/UserGestureIndicator.h>
#include <WebCore/WheelEvent.h>
#include <WebCore/c_instance.h>
#include <WebCore/npruntime_impl.h>
#include <WebCore/runtime_root.h>
#include <wtf/ASCIICType.h>
#include <wtf/text/WTFString.h>

#if ENABLE(NETSCAPE_PLUGIN_API)
#include "PluginMainThreadScheduler.h"
#include "PluginMessageThrottlerWin.h"
#endif

using JSC::JSLock;
using JSC::JSObject;
using JSC::JSValue;

#if ENABLE(NETSCAPE_PLUGIN_API)

namespace WebCore {

using namespace HTMLNames;

static int s_callingPlugin;

typedef HashMap<NPP, PluginView*> InstanceMap;

static InstanceMap& instanceMap()
{
    static InstanceMap& map = *new InstanceMap;
    return map;
}

static String scriptStringIfJavaScriptURL(const URL& url)
{
    if (!url.protocolIsJavaScript())
        return String();

    // This returns an unescaped string
    return decodeURLEscapeSequences(url.string().substring(11));
}

PluginView* PluginView::s_currentPluginView = 0;

void PluginView::popPopupsStateTimerFired()
{
    popPopupsEnabledState();
}

IntRect PluginView::windowClipRect() const
{
    // Start by clipping to our bounds.
    IntRect clipRect(m_windowRect);
    
    // Take our element and get the clip rect from the enclosing layer and frame view.
    FrameView* parentView = m_element->document().view();
    IntRect windowClipRect = parentView->windowClipRectForFrameOwner(m_element, true);
    windowClipRect.scale(deviceScaleFactor());
    clipRect.intersect(windowClipRect);

    return clipRect;
}

void PluginView::setFrameRect(const IntRect& rect)
{
    if (m_element->document().printing())
        return;

    if (rect != frameRect())
        Widget::setFrameRect(rect);

    updatePluginWidget();

    // On Windows always call plugin to change geometry.
    setNPWindowRect(rect);
}

void PluginView::frameRectsChanged()
{
    updatePluginWidget();
}

void PluginView::clipRectChanged()
{
    updatePluginWidget();
}

void PluginView::handleEvent(Event& event)
{
    if (!m_plugin || m_isWindowed)
        return;

    // Protect the plug-in from deletion while dispatching the event.
    RefPtr<PluginView> protect(this);

    if (event.isMouseEvent())
        handleMouseEvent(downcast<MouseEvent>(event));
    else if (event.isKeyboardEvent())
        handleKeyboardEvent(downcast<KeyboardEvent>(event));
    else if (event.type() == eventNames().contextmenuEvent)
        event.setDefaultHandled(); // We don't know if the plug-in has handled mousedown event by displaying a context menu, so we never want WebKit to show a default one.
}

void PluginView::init()
{
    if (m_haveInitialized)
        return;

    m_haveInitialized = true;

    if (!m_plugin) {
        ASSERT(m_status == PluginStatusCanNotFindPlugin);
        return;
    }

    LOG(Plugins, "PluginView::init(): Initializing plug-in '%s'", m_plugin->name().utf8().data());

    if (!m_plugin->load()) {
        m_plugin = nullptr;
        m_status = PluginStatusCanNotLoadPlugin;
        return;
    }

    if (!startOrAddToUnstartedList()) {
        m_status = PluginStatusCanNotLoadPlugin;
        return;
    }

    m_status = PluginStatusLoadedSuccessfully;
}

bool PluginView::startOrAddToUnstartedList()
{
    if (!m_parentFrame->page())
        return false;

    // We only delay starting the plug-in if we're going to kick off the load
    // ourselves. Otherwise, the loader will try to deliver data before we've
    // started the plug-in.
    if (!m_loadManually && !m_parentFrame->page()->canStartMedia()) {
        m_parentFrame->document()->addMediaCanStartListener(*this);
        m_isWaitingToStart = true;
        return true;
    }

    return start();
}

bool PluginView::start()
{
    if (m_isStarted)
        return false;

    m_isWaitingToStart = false;

    PluginMainThreadScheduler::scheduler().registerPlugin(m_instance);

    ASSERT(m_plugin);
    ASSERT(m_plugin->pluginFuncs()->newp);

    NPError npErr;
    {
        PluginView::setCurrentPluginView(this);
        JSC::JSLock::DropAllLocks dropAllLocks(commonVM());
        setCallingPlugin(true);
        npErr = m_plugin->pluginFuncs()->newp((NPMIMEType)m_mimeType.utf8().data(), m_instance, m_mode, m_paramCount, m_paramNames, m_paramValues, NULL);
        setCallingPlugin(false);
        LOG_NPERROR(npErr);
        PluginView::setCurrentPluginView(0);
    }

    if (npErr != NPERR_NO_ERROR) {
        m_status = PluginStatusCanNotLoadPlugin;
        PluginMainThreadScheduler::scheduler().unregisterPlugin(m_instance);
        return false;
    }

    m_isStarted = true;

    if (!m_url.isEmpty() && !m_loadManually) {
        FrameLoadRequest frameLoadRequest { *m_parentFrame->document(), m_parentFrame->document()->securityOrigin(), { }, { }, InitiatedByMainFrame::Unknown };
        frameLoadRequest.resourceRequest().setHTTPMethod("GET");
        frameLoadRequest.resourceRequest().setURL(m_url);
        load(WTFMove(frameLoadRequest), false, nullptr);
    }

    m_status = PluginStatusLoadedSuccessfully;

    if (!platformStart())
        m_status = PluginStatusCanNotLoadPlugin;

    if (m_status != PluginStatusLoadedSuccessfully)
        return false;

    return true;
}

void PluginView::mediaCanStart(Document&)
{
    ASSERT(!m_isStarted);
    if (!start())
        static_cast<WebFrameLoaderClient&>(parentFrame()->loader().client()).dispatchDidFailToStartPlugin(*this);
}

PluginView::~PluginView()
{
    LOG(Plugins, "PluginView::~PluginView()");

    ASSERT(!m_lifeSupportTimer.isActive());

    // If we failed to find the plug-in, we'll return early in our constructor, and
    // m_instance will be 0.
    if (m_instance)
        instanceMap().remove(m_instance);

    if (m_isWaitingToStart)
        m_parentFrame->document()->removeMediaCanStartListener(*this);

    stop();

    if (m_elementNPObject)
        _NPN_ReleaseObject(m_elementNPObject);
    
    freeStringArray(m_paramNames, m_paramCount);
    freeStringArray(m_paramValues, m_paramCount);

    platformDestroy();

    m_parentFrame->script().cleanupScriptObjectsForPlugin(this);

    if (m_plugin && !(m_plugin->quirks().contains(PluginQuirkDontUnloadPlugin)))
        m_plugin->unload();
}

void PluginView::stop()
{
    if (!m_isStarted)
        return;

    LOG(Plugins, "PluginView::stop(): Stopping plug-in '%s'", m_plugin->name().utf8().data());

    HashSet<RefPtr<PluginStream> > streams = m_streams;
    HashSet<RefPtr<PluginStream> >::iterator end = streams.end();
    for (HashSet<RefPtr<PluginStream> >::iterator it = streams.begin(); it != end; ++it) {
        (*it)->stop();
        disconnectStream((*it).get());
    }

    ASSERT(m_streams.isEmpty());

    m_isStarted = false;

    JSC::JSLock::DropAllLocks dropAllLocks(commonVM());

#if ENABLE(NETSCAPE_PLUGIN_API)
    // Unsubclass the window
    if (m_isWindowed) {
        WNDPROC currentWndProc = (WNDPROC)GetWindowLongPtr(platformPluginWidget(), GWLP_WNDPROC);

        if (currentWndProc == PluginViewWndProc)
            SetWindowLongPtr(platformPluginWidget(), GWLP_WNDPROC, (LONG_PTR)m_pluginWndProc);
    }
#endif // ENABLE(NETSCAPE_PLUGIN_API)

    // Clear the window
    m_npWindow.window = 0;

    if (m_plugin->pluginFuncs()->setwindow && !m_plugin->quirks().contains(PluginQuirkDontSetNullWindowHandleOnDestroy)) {
        PluginView::setCurrentPluginView(this);
        setCallingPlugin(true);
        m_plugin->pluginFuncs()->setwindow(m_instance, &m_npWindow);
        setCallingPlugin(false);
        PluginView::setCurrentPluginView(0);
    }

    PluginMainThreadScheduler::scheduler().unregisterPlugin(m_instance);

    NPSavedData* savedData = 0;
    PluginView::setCurrentPluginView(this);
    setCallingPlugin(true);
    NPError npErr = m_plugin->pluginFuncs()->destroy(m_instance, &savedData);
    setCallingPlugin(false);
    LOG_NPERROR(npErr);
    PluginView::setCurrentPluginView(0);

#if ENABLE(NETSCAPE_PLUGIN_API)
    if (savedData) {
        // TODO: Actually save this data instead of just discarding it
        if (savedData->buf)
            NPN_MemFree(savedData->buf);
        NPN_MemFree(savedData);
    }
#endif

    m_instance->pdata = 0;
}

void PluginView::setCurrentPluginView(PluginView* pluginView)
{
    s_currentPluginView = pluginView;
}

PluginView* PluginView::currentPluginView()
{
    return s_currentPluginView;
}

static char* createUTF8String(const String& str)
{
    CString cstr = str.utf8();
    const size_t cstrLength = cstr.length();
    char* result = reinterpret_cast<char*>(fastMalloc(cstrLength + 1));

    memcpy(result, cstr.data(), cstrLength);
    result[cstrLength] = '\0';

    return result;
}

void PluginView::performRequest(PluginRequest* request)
{
    if (!m_isStarted)
        return;

    // don't let a plugin start any loads if it is no longer part of a document that is being 
    // displayed unless the loads are in the same frame as the plugin.
    const String& targetFrameName = request->frameLoadRequest().frameName();
    if (m_parentFrame->loader().documentLoader() != m_parentFrame->loader().activeDocumentLoader() && (targetFrameName.isNull() || m_parentFrame->tree().find(targetFrameName, *m_parentFrame) != m_parentFrame))
        return;

    URL requestURL = request->frameLoadRequest().resourceRequest().url();
    String jsString = scriptStringIfJavaScriptURL(requestURL);

    UserGestureIndicator gestureIndicator(request->shouldAllowPopups() ? Optional<ProcessingUserGestureState>(ProcessingUserGesture) : WTF::nullopt);

    if (jsString.isNull()) {
        // if this is not a targeted request, create a stream for it. otherwise,
        // just pass it off to the loader
        if (targetFrameName.isEmpty()) {
            auto stream = PluginStream::create(this, m_parentFrame.get(), request->frameLoadRequest().resourceRequest(), request->sendNotification(), request->notifyData(), plugin()->pluginFuncs(), instance(), m_plugin->quirks());
            m_streams.add(stream.copyRef());
            stream->start();
        } else {
            // If the target frame is our frame, we could destroy the
            // PluginView, so we protect it. <rdar://problem/6991251>
            RefPtr<PluginView> protect(this);

            FrameLoadRequest frameLoadRequest { *m_parentFrame.get(), request->frameLoadRequest().resourceRequest() };
            frameLoadRequest.setFrameName(targetFrameName);
            frameLoadRequest.setShouldCheckNewWindowPolicy(true);
            m_parentFrame->loader().load(WTFMove(frameLoadRequest));

            // FIXME: <rdar://problem/4807469> This should be sent when the document has finished loading
            if (request->sendNotification()) {
                PluginView::setCurrentPluginView(this);
                JSC::JSLock::DropAllLocks dropAllLocks(commonVM());
                setCallingPlugin(true);
                m_plugin->pluginFuncs()->urlnotify(m_instance, requestURL.string().utf8().data(), NPRES_DONE, request->notifyData());
                setCallingPlugin(false);
                PluginView::setCurrentPluginView(0);
            }
        }
        return;
    }

    // Targeted JavaScript requests are only allowed on the frame that contains the JavaScript plugin
    // and this has been made sure in ::load.
    ASSERT(targetFrameName.isEmpty() || m_parentFrame->tree().find(targetFrameName, *m_parentFrame) == m_parentFrame);
    
    // Executing a script can cause the plugin view to be destroyed, so we keep a reference to it.
    RefPtr<PluginView> protector(this);
    auto result = m_parentFrame->script().executeScriptIgnoringException(jsString, request->shouldAllowPopups());

    if (targetFrameName.isNull()) {
        CString cstr;
        {
            JSC::JSGlobalObject& globalObject = *m_parentFrame->script().globalObject(pluginWorld());
            JSC::JSLockHolder lock(&globalObject);
            String resultString;
            if (result && result.getString(&globalObject, resultString))
                cstr = resultString.utf8();
        }

        auto stream = PluginStream::create(this, m_parentFrame.get(), request->frameLoadRequest().resourceRequest(), request->sendNotification(), request->notifyData(), plugin()->pluginFuncs(), instance(), m_plugin->quirks());
        m_streams.add(stream.copyRef());
        stream->sendJavaScriptStream(requestURL, cstr);
    }
}

void PluginView::requestTimerFired()
{
    ASSERT(!m_requests.isEmpty());
    ASSERT(!m_isJavaScriptPaused);

    std::unique_ptr<PluginRequest> request = WTFMove(m_requests[0]);
    m_requests.remove(0);
    
    // Schedule a new request before calling performRequest since the call to
    // performRequest can cause the plugin view to be deleted.
    if (!m_requests.isEmpty())
        m_requestTimer.startOneShot(0_s);

    performRequest(request.get());
}

void PluginView::scheduleRequest(std::unique_ptr<PluginRequest> request)
{
    m_requests.append(WTFMove(request));

    if (!m_isJavaScriptPaused)
        m_requestTimer.startOneShot(0_s);
}

NPError PluginView::load(FrameLoadRequest&& frameLoadRequest, bool sendNotification, void* notifyData)
{
    ASSERT(frameLoadRequest.resourceRequest().httpMethod() == "GET" || frameLoadRequest.resourceRequest().httpMethod() == "POST");

    URL url = frameLoadRequest.resourceRequest().url();
    
    if (url.isEmpty())
        return NPERR_INVALID_URL;

    // Don't allow requests to be made when the document loader is stopping all loaders.
    DocumentLoader* loader = m_parentFrame->loader().documentLoader();
    if (!loader || loader->isStopping())
        return NPERR_GENERIC_ERROR;

    const String& targetFrameName = frameLoadRequest.frameName();
    String jsString = scriptStringIfJavaScriptURL(url);

    if (!jsString.isNull()) {
        // Return NPERR_GENERIC_ERROR if JS is disabled. This is what Mozilla does.
        if (!m_parentFrame->script().canExecuteScripts(NotAboutToExecuteScript))
            return NPERR_GENERIC_ERROR;

        // For security reasons, only allow JS requests to be made on the frame that contains the plug-in.
        if (!targetFrameName.isNull() && m_parentFrame->tree().find(targetFrameName, *m_parentFrame) != m_parentFrame)
            return NPERR_INVALID_PARAM;
    } else if (!m_parentFrame->document()->securityOrigin().canDisplay(url))
        return NPERR_GENERIC_ERROR;

    scheduleRequest(makeUnique<PluginRequest>(WTFMove(frameLoadRequest), sendNotification, notifyData, arePopupsAllowed()));

    return NPERR_NO_ERROR;
}

static URL makeURL(const URL& baseURL, const char* relativeURLString)
{
    String urlString = relativeURLString;

    // Strip return characters.
    urlString.replaceWithLiteral('\n', "");
    urlString.replaceWithLiteral('\r', "");

    return URL(baseURL, urlString);
}

NPError PluginView::getURLNotify(const char* url, const char* target, void* notifyData)
{
    FrameLoadRequest frameLoadRequest { *m_parentFrame->document(), m_parentFrame->document()->securityOrigin(), { }, target, InitiatedByMainFrame::Unknown };

    frameLoadRequest.resourceRequest().setHTTPMethod("GET");
    frameLoadRequest.resourceRequest().setURL(makeURL(m_parentFrame->document()->baseURL(), url));

    return load(WTFMove(frameLoadRequest), true, notifyData);
}

NPError PluginView::getURL(const char* url, const char* target)
{
    FrameLoadRequest frameLoadRequest { *m_parentFrame->document(), m_parentFrame->document()->securityOrigin(), { }, target, InitiatedByMainFrame::Unknown };

    frameLoadRequest.resourceRequest().setHTTPMethod("GET");
    frameLoadRequest.resourceRequest().setURL(makeURL(m_parentFrame->document()->baseURL(), url));

    return load(WTFMove(frameLoadRequest), false, nullptr);
}

NPError PluginView::postURLNotify(const char* url, const char* target, uint32_t len, const char* buf, NPBool file, void* notifyData)
{
    return handlePost(url, target, len, buf, file, notifyData, true, true);
}

NPError PluginView::postURL(const char* url, const char* target, uint32_t len, const char* buf, NPBool file)
{
    // As documented, only allow headers to be specified via NPP_PostURL when using a file.
    return handlePost(url, target, len, buf, file, 0, false, file);
}

NPError PluginView::newStream(NPMIMEType, const char* /* target */, NPStream**)
{
    notImplemented();
    // Unsupported
    return NPERR_GENERIC_ERROR;
}

int32_t PluginView::write(NPStream*, int32_t /* len */, void* /* buffer */)
{
    notImplemented();
    // Unsupported
    return -1;
}

NPError PluginView::destroyStream(NPStream* stream, NPReason reason)
{
    if (!stream || PluginStream::ownerForStream(stream) != m_instance)
        return NPERR_INVALID_INSTANCE_ERROR;

    PluginStream* browserStream = static_cast<PluginStream*>(stream->ndata);
    browserStream->cancelAndDestroyStream(reason);

    return NPERR_NO_ERROR;
}

void PluginView::status(const char* message)
{
    if (Page* page = m_parentFrame->page())
        page->chrome().setStatusbarText(*m_parentFrame, String::fromUTF8(message));
}

NPError PluginView::setValue(NPPVariable variable, void* value)
{
    LOG(Plugins, "PluginView::setValue(%s): ", prettyNameForNPPVariable(variable, value).data());

    switch (variable) {
    case NPPVpluginWindowBool:
        m_isWindowed = value;
        return NPERR_NO_ERROR;
    case NPPVpluginTransparentBool:
        m_isTransparent = value;
        return NPERR_NO_ERROR;
    default:
        notImplemented();
        return NPERR_GENERIC_ERROR;
    }
}

void PluginView::invalidateTimerFired()
{
    for (unsigned i = 0; i < m_invalidRects.size(); i++)
        invalidateRect(m_invalidRects[i]);
    m_invalidRects.clear();
}


void PluginView::pushPopupsEnabledState(bool state)
{
    m_popupStateStack.append(state);
}
 
void PluginView::popPopupsEnabledState()
{
    m_popupStateStack.removeLast();
}

bool PluginView::arePopupsAllowed() const
{
    if (!m_popupStateStack.isEmpty())
        return m_popupStateStack.last();

    return false;
}

void PluginView::setJavaScriptPaused(bool paused)
{
    if (m_isJavaScriptPaused == paused)
        return;
    m_isJavaScriptPaused = paused;

    if (m_isJavaScriptPaused)
        m_requestTimer.stop();
    else if (!m_requests.isEmpty())
        m_requestTimer.startOneShot(0_s);
}

#if ENABLE(NETSCAPE_PLUGIN_API)
NPObject* PluginView::npObject()
{
    NPObject* object = 0;

    if (!m_isStarted || !m_plugin || !m_plugin->pluginFuncs()->getvalue)
        return 0;

    // On Windows, calling Java's NPN_GetValue can allow the message loop to
    // run, allowing loading to take place or JavaScript to run. Protect the
    // PluginView from destruction. <rdar://problem/6978804>
    RefPtr<PluginView> protect(this);

    NPError npErr;
    {
        PluginView::setCurrentPluginView(this);
        JSC::JSLock::DropAllLocks dropAllLocks(commonVM());
        setCallingPlugin(true);
        npErr = m_plugin->pluginFuncs()->getvalue(m_instance, NPPVpluginScriptableNPObject, &object);
        setCallingPlugin(false);
        PluginView::setCurrentPluginView(0);
    }

    if (npErr != NPERR_NO_ERROR)
        return 0;

    return object;
}
#endif

RefPtr<JSC::Bindings::Instance> PluginView::bindingInstance()
{
#if ENABLE(NETSCAPE_PLUGIN_API)
    NPObject* object = npObject();
    if (!object)
        return nullptr;

    if (hasOneRef()) {
        // The renderer for the PluginView was destroyed during the above call, and
        // the PluginView will be destroyed when this function returns, so we
        // return null.
        return nullptr;
    }

    auto root = m_parentFrame->script().createRootObject(this);
    auto instance = JSC::Bindings::CInstance::create(object, WTFMove(root));

    _NPN_ReleaseObject(object);

    return WTFMove(instance);
#else
    return nullptr;
#endif
}

void PluginView::disconnectStream(PluginStream* stream)
{
    ASSERT(m_streams.contains(stream));

    m_streams.remove(stream);
}

void PluginView::setParameters(const Vector<String>& paramNames, const Vector<String>& paramValues)
{
    ASSERT(paramNames.size() == paramValues.size());

    unsigned size = paramNames.size();
    unsigned paramCount = 0;

    m_paramNames = reinterpret_cast<char**>(fastMalloc(sizeof(char*) * size));
    m_paramValues = reinterpret_cast<char**>(fastMalloc(sizeof(char*) * size));

    for (unsigned i = 0; i < size; i++) {
        if (m_plugin->quirks().contains(PluginQuirkRemoveWindowlessVideoParam) && equalLettersIgnoringASCIICase(paramNames[i], "windowlessvideo"))
            continue;

        if (paramNames[i] == "pluginspage")
            m_pluginsPage = paramValues[i];

        m_paramNames[paramCount] = createUTF8String(paramNames[i]);
        m_paramValues[paramCount] = createUTF8String(paramValues[i]);

        paramCount++;
    }

    m_paramCount = paramCount;
}

PluginView::PluginView(Frame* parentFrame, const IntSize& size, PluginPackage* plugin, HTMLPlugInElement* element, const URL& url, const Vector<String>& paramNames, const Vector<String>& paramValues, const String& mimeType, bool loadManually)
    : m_parentFrame(parentFrame)
    , m_plugin(plugin)
    , m_element(element)
    , m_isStarted(false)
    , m_url(url)
    , m_status(PluginStatusLoadedSuccessfully)
    , m_requestTimer(*this, &PluginView::requestTimerFired)
    , m_invalidateTimer(*this, &PluginView::invalidateTimerFired)
    , m_popPopupsStateTimer(*this, &PluginView::popPopupsStateTimerFired)
    , m_lifeSupportTimer(*this, &PluginView::lifeSupportTimerFired)
    , m_mode(loadManually ? NP_FULL : NP_EMBED)
    , m_paramNames(0)
    , m_paramValues(0)
    , m_mimeType(mimeType)
    , m_instance(0)
    , m_elementNPObject(nullptr)
    , m_isWindowed(true)
    , m_isTransparent(false)
    , m_haveInitialized(false)
    , m_isWaitingToStart(false)
#if ENABLE(NETSCAPE_PLUGIN_API)
    , m_pluginWndProc(0)
    , m_lastMessage(0)
    , m_isCallingPluginWndProc(false)
    , m_wmPrintHDC(0)
    , m_haveUpdatedPluginWidget(false)
#endif
    , m_loadManually(loadManually)
    , m_manualStream(0)
    , m_isJavaScriptPaused(false)
    , m_haveCalledSetWindow(false)
{
    if (!m_plugin) {
        m_status = PluginStatusCanNotFindPlugin;
        return;
    }

    m_instance = &m_instanceStruct;
    m_instance->ndata = this;
    m_instance->pdata = 0;

    instanceMap().add(m_instance, this);

    setParameters(paramNames, paramValues);

    memset(&m_npWindow, 0, sizeof(m_npWindow));

    resize(size);
}

void PluginView::focusPluginElement()
{
    if (Page* page = m_parentFrame->page())
        page->focusController().setFocusedElement(m_element, *m_parentFrame);
    else
        m_parentFrame->document()->setFocusedElement(m_element);
}

void PluginView::didReceiveResponse(const ResourceResponse& response)
{
    if (m_status != PluginStatusLoadedSuccessfully)
        return;

    ASSERT(m_loadManually);
    ASSERT(!m_manualStream);

    m_manualStream = PluginStream::create(this, m_parentFrame.get(), m_parentFrame->loader().activeDocumentLoader()->request(), false, 0, plugin()->pluginFuncs(), instance(), m_plugin->quirks());
    m_manualStream->setLoadManually(true);

    m_manualStream->didReceiveResponse(0, response);
}

void PluginView::didReceiveData(const char* data, int length)
{
    if (m_status != PluginStatusLoadedSuccessfully)
        return;

    ASSERT(m_loadManually);
    ASSERT(m_manualStream);
    
    m_manualStream->didReceiveData(0, data, length);
}

void PluginView::didFinishLoading()
{
    if (m_status != PluginStatusLoadedSuccessfully)
        return;

    ASSERT(m_loadManually);
    ASSERT(m_manualStream);

    m_manualStream->didFinishLoading(0);
}

void PluginView::didFail(const ResourceError& error)
{
    if (m_status != PluginStatusLoadedSuccessfully)
        return;

    ASSERT(m_loadManually);
    
    if (m_manualStream)
        m_manualStream->didFail(0, error);
}

void PluginView::setCallingPlugin(bool b) const
{
    if (!m_plugin->quirks().contains(PluginQuirkHasModalMessageLoop))
        return;

    if (b)
        ++s_callingPlugin;
    else
        --s_callingPlugin;

    ASSERT(s_callingPlugin >= 0);
}

bool PluginView::isCallingPlugin()
{
    return s_callingPlugin > 0;
}

Ref<PluginView> PluginView::create(Frame* parentFrame, const IntSize& size, HTMLPlugInElement* element, const URL& url, const Vector<String>& paramNames, const Vector<String>& paramValues, const String& mimeType, bool loadManually)
{
    // if we fail to find a plugin for this MIME type, findPlugin will search for
    // a plugin by the file extension and update the MIME type, so pass a mutable String
    String mimeTypeCopy = mimeType;
    PluginPackage* plugin = PluginDatabase::installedPlugins()->findPlugin(url, mimeTypeCopy);

    // No plugin was found, try refreshing the database and searching again
    if (!plugin && PluginDatabase::installedPlugins()->refresh()) {
        mimeTypeCopy = mimeType;
        plugin = PluginDatabase::installedPlugins()->findPlugin(url, mimeTypeCopy);
    }

    return adoptRef(*new PluginView(parentFrame, size, plugin, element, url, paramNames, paramValues, mimeTypeCopy, loadManually));
}

void PluginView::freeStringArray(char** stringArray, int length)
{
    if (!stringArray)
        return;

    for (int i = 0; i < length; i++)
        fastFree(stringArray[i]);

    fastFree(stringArray);
}

static inline bool startsWithBlankLine(const Vector<char>& buffer)
{
    return buffer.size() > 0 && buffer[0] == '\n';
}

static inline int locationAfterFirstBlankLine(const Vector<char>& buffer)
{
    const char* bytes = buffer.data();
    unsigned length = buffer.size();

    for (unsigned i = 0; i < length - 4; i++) {
        // Support for Acrobat. It sends "\n\n".
        if (bytes[i] == '\n' && bytes[i + 1] == '\n')
            return i + 2;
        
        // Returns the position after 2 CRLF's or 1 CRLF if it is the first line.
        if (bytes[i] == '\r' && bytes[i + 1] == '\n') {
            i += 2;
            if (i == 2)
                return i;
            else if (bytes[i] == '\n')
                // Support for Director. It sends "\r\n\n" (3880387).
                return i + 1;
            else if (bytes[i] == '\r' && bytes[i + 1] == '\n')
                // Support for Flash. It sends "\r\n\r\n" (3758113).
                return i + 2;
        }
    }

    return -1;
}

static inline const char* findEOL(const char* bytes, unsigned length)
{
    // According to the HTTP specification EOL is defined as
    // a CRLF pair. Unfortunately, some servers will use LF
    // instead. Worse yet, some servers will use a combination
    // of both (e.g. <header>CRLFLF<body>), so findEOL needs
    // to be more forgiving. It will now accept CRLF, LF or
    // CR.
    //
    // It returns NULL if EOLF is not found or it will return
    // a pointer to the first terminating character.
    for (unsigned i = 0; i < length; i++) {
        if (bytes[i] == '\n')
            return bytes + i;
        if (bytes[i] == '\r') {
            // Check to see if spanning buffer bounds
            // (CRLF is across reads). If so, wait for
            // next read.
            if (i + 1 == length)
                break;

            return bytes + i;
        }
    }

    return 0;
}

static inline String capitalizeRFC822HeaderFieldName(const String& name)
{
    bool capitalizeCharacter = true;
    String result;

    for (unsigned i = 0; i < name.length(); i++) {
        UChar c;

        if (capitalizeCharacter && name[i] >= 'a' && name[i] <= 'z')
            c = toASCIIUpper(name[i]);
        else if (!capitalizeCharacter && name[i] >= 'A' && name[i] <= 'Z')
            c = toASCIILower(name[i]);
        else
            c = name[i];

        if (name[i] == '-')
            capitalizeCharacter = true;
        else
            capitalizeCharacter = false;

        result.append(c);
    }

    return result;
}

static inline HTTPHeaderMap parseRFC822HeaderFields(const Vector<char>& buffer, unsigned length)
{
    const char* bytes = buffer.data();
    const char* eol;
    String lastKey;
    HTTPHeaderMap headerFields;

    // Loop ove rlines until we're past the header, or we can't find any more end-of-lines
    while ((eol = findEOL(bytes, length))) {
        const char* line = bytes;
        int lineLength = eol - bytes;
        
        // Move bytes to the character after the terminator as returned by findEOL.
        bytes = eol + 1;
        if ((*eol == '\r') && (*bytes == '\n'))
            bytes++; // Safe since findEOL won't return a spanning CRLF.

        length -= (bytes - line);
        if (lineLength == 0)
            // Blank line; we're at the end of the header
            break;
        else if (*line == ' ' || *line == '\t') {
            // Continuation of the previous header
            if (lastKey.isNull()) {
                // malformed header; ignore it and continue
                continue;
            } else {
                // Merge the continuation of the previous header
                String currentValue = headerFields.get(lastKey);
                String newValue(line, lineLength);

                headerFields.set(lastKey, currentValue + newValue);
            }
        } else {
            // Brand new header
            const char* colon;
            for (colon = line; *colon != ':' && colon != eol; colon++) {
                // empty loop
            }
            if (colon == eol) 
                // malformed header; ignore it and continue
                continue;
            else {
                lastKey = capitalizeRFC822HeaderFieldName(String(line, colon - line));
                String value;

                for (colon++; colon != eol; colon++) {
                    if (*colon != ' ' && *colon != '\t')
                        break;
                }
                if (colon == eol)
                    value = emptyString();
                else
                    value = String(colon, eol - colon);

                String oldValue = headerFields.get(lastKey);
                if (!oldValue.isNull())
                    value = oldValue + ", " + value;

                headerFields.set(lastKey, value);
            }
        }
    }

    return headerFields;
}

NPError PluginView::handlePost(const char* url, const char* target, uint32_t len, const char* buf, bool file, void* notifyData, bool sendNotification, bool allowHeaders)
{
    if (!url || !len || !buf)
        return NPERR_INVALID_PARAM;

    HTTPHeaderMap headerFields;
    Vector<char> buffer;
    
    if (file) {
        NPError readResult = handlePostReadFile(buffer, len, buf);
        if(readResult != NPERR_NO_ERROR)
            return readResult;
    } else {
        buffer.resize(len);
        memcpy(buffer.data(), buf, len);
    }

    const char* postData = buffer.data();
    int postDataLength = buffer.size();

    if (allowHeaders) {
        if (startsWithBlankLine(buffer)) {
            postData++;
            postDataLength--;
        } else {
            int location = locationAfterFirstBlankLine(buffer);
            if (location != -1) {
                // If the blank line is somewhere in the middle of the buffer, everything before is the header
                headerFields = parseRFC822HeaderFields(buffer, location);
                unsigned dataLength = buffer.size() - location;

                // Sometimes plugins like to set Content-Length themselves when they post,
                // but WebFoundation does not like that. So we will remove the header
                // and instead truncate the data to the requested length.
                String contentLength = headerFields.get(HTTPHeaderName::ContentLength);

                if (!contentLength.isNull())
                    dataLength = std::min(contentLength.toInt(), (int)dataLength);
                headerFields.remove(HTTPHeaderName::ContentLength);

                postData += location;
                postDataLength = dataLength;
            }
        }
    }

    FrameLoadRequest frameLoadRequest { *m_parentFrame->document(), m_parentFrame->document()->securityOrigin(), { }, target, InitiatedByMainFrame::Unknown };
    frameLoadRequest.resourceRequest().setHTTPMethod("POST");
    frameLoadRequest.resourceRequest().setURL(makeURL(m_parentFrame->document()->baseURL(), url));
    frameLoadRequest.resourceRequest().setHTTPHeaderFields(WTFMove(headerFields));
    frameLoadRequest.resourceRequest().setHTTPBody(FormData::create(postData, postDataLength));

    return load(WTFMove(frameLoadRequest), sendNotification, notifyData);
}

void PluginView::invalidateWindowlessPluginRect(const IntRect& rect)
{
    if (!isVisible())
        return;
    
    if (!m_element->renderer())
        return;
    auto& renderer = downcast<RenderBox>(*m_element->renderer());
    
    IntRect dirtyRect = rect;
    dirtyRect.move(renderer.borderLeft() + renderer.paddingLeft(), renderer.borderTop() + renderer.paddingTop());
    renderer.repaintRectangle(dirtyRect);
}

void PluginView::paintMissingPluginIcon(GraphicsContext& context, const IntRect& rect)
{
    static RefPtr<Image> nullPluginImage;
    if (!nullPluginImage)
        nullPluginImage = Image::loadPlatformResource("nullPlugin");

    IntRect imageRect(frameRect().x(), frameRect().y(), nullPluginImage->width(), nullPluginImage->height());

    int xOffset = (frameRect().width() - imageRect.width()) / 2;
    int yOffset = (frameRect().height() - imageRect.height()) / 2;

    imageRect.move(xOffset, yOffset);

    if (!rect.intersects(imageRect))
        return;

    context.save();
    context.clip(windowClipRect());
    context.drawImage(*nullPluginImage, imageRect.location());
    context.restore();
}

static const char* MozillaUserAgent = "Mozilla/5.0 ("
        "Windows; U; Windows NT 5.1;"
        " en-US; rv:1.8.1) Gecko/20061010 Firefox/2.0";

static const char* const ChromeUserAgent = "Mozilla/5.0 ("
    "Windows; U; Windows NT 5.1;"
    " AppleWebKit/534.34 (KHTML, like Gecko) Chrome/19.0.1055.1 Safari/534.34";

const char* PluginView::userAgent()
{
    if (m_plugin->quirks().contains(PluginQuirkWantsMozillaUserAgent))
        return MozillaUserAgent;
    else if (m_plugin->quirks().contains(PluginQuirkWantsChromeUserAgent))
        return ChromeUserAgent;
    if (m_userAgent.isNull())
        m_userAgent = m_parentFrame->loader().userAgent(m_url).utf8();

    return m_userAgent.data();
}

#if ENABLE(NETSCAPE_PLUGIN_API)
const char* PluginView::userAgentStatic()
{
    return MozillaUserAgent;
}
#endif


void PluginView::lifeSupportTimerFired()
{
    deref();
}

void PluginView::keepAlive()
{
    if (m_lifeSupportTimer.isActive())
        return;

    ref();
    m_lifeSupportTimer.startOneShot(0_s);
}

#if ENABLE(NETSCAPE_PLUGIN_API)
void PluginView::keepAlive(NPP instance)
{
    PluginView* view = instanceMap().get(instance);
    if (!view)
        return;

    view->keepAlive();
}

NPError PluginView::getValueStatic(NPNVariable variable, void* value)
{
    LOG(Plugins, "PluginView::getValueStatic(%s)", prettyNameForNPNVariable(variable).data());

    NPError result;
    if (platformGetValueStatic(variable, value, &result))
        return result;

    return NPERR_GENERIC_ERROR;
}

static Frame* getFrame(Frame* parentFrame, Element* element)
{
    if (parentFrame)
        return parentFrame;
    
    return element->document().frame();
}

NPError PluginView::getValue(NPNVariable variable, void* value)
{
    LOG(Plugins, "PluginView::getValue(%s)", prettyNameForNPNVariable(variable).data());

    NPError result;
    if (platformGetValue(variable, value, &result))
        return result;

    if (platformGetValueStatic(variable, value, &result))
        return result;

    switch (variable) {
    case NPNVWindowNPObject: {
        if (m_isJavaScriptPaused)
            return NPERR_GENERIC_ERROR;

        NPObject* windowScriptObject = m_parentFrame->script().windowScriptNPObject();

        // Return value is expected to be retained, as described here: <http://www.mozilla.org/projects/plugin/npruntime.html>
        if (windowScriptObject)
            _NPN_RetainObject(windowScriptObject);

        void** v = (void**)value;
        *v = windowScriptObject;

        return NPERR_NO_ERROR;
    }

    case NPNVPluginElementNPObject: {
        if (m_isJavaScriptPaused)
            return NPERR_GENERIC_ERROR;

        if (!m_elementNPObject) {
            Frame* frame = getFrame(parentFrame(), m_element);
            if (!frame)
                return NPERR_GENERIC_ERROR;

            JSC::JSObject* object = frame->script().jsObjectForPluginElement(m_element);
            if (!object)
                m_elementNPObject = _NPN_CreateNoScriptObject();
            else
                m_elementNPObject = _NPN_CreateScriptObject(0, object, frame->script().bindingRootObject());
        }

        // Return value is expected to be retained, as described here: <http://www.mozilla.org/projects/plugins/npruntime.html#browseraccess>
        if (m_elementNPObject)
            _NPN_RetainObject(m_elementNPObject);

        *(void **)value = m_elementNPObject;
        return NPERR_NO_ERROR;
    }

    case NPNVprivateModeBool: {
        Page* page = m_parentFrame->page();
        if (!page)
            return NPERR_GENERIC_ERROR;
        *((NPBool*)value) = page->usesEphemeralSession();
        return NPERR_NO_ERROR;
    }

    default:
        return NPERR_GENERIC_ERROR;
    }
}

NPError PluginView::getValueForURL(NPNURLVariable variable, const char* url, char** value, uint32_t* len)
{
    LOG(Plugins, "PluginView::getValueForURL(%s)", prettyNameForNPNURLVariable(variable).data());

    NPError result = NPERR_NO_ERROR;

    switch (variable) {
    case NPNURLVCookie: {
        URL u(m_parentFrame->document()->baseURL(), url);
        if (u.isValid()) {
            Frame* frame = getFrame(parentFrame(), m_element);
            if (frame && frame->document()) {
                if (auto* page = frame->document()->page()) {
                    const CString cookieStr = page->cookieJar().cookies(*frame->document(), u).utf8();
                    if (!cookieStr.isNull()) {
                        const int size = cookieStr.length();
                        *value = static_cast<char*>(NPN_MemAlloc(size+1));
                        if (*value) {
                            memset(*value, 0, size+1);
                            memcpy(*value, cookieStr.data(), size+1);
                            if (len)
                                *len = size;
                        } else
                            result = NPERR_OUT_OF_MEMORY_ERROR;
                    }
                }
            }
        } else
            result = NPERR_INVALID_URL;
        break;
    }
    case NPNURLVProxy: {
        URL u(m_parentFrame->document()->baseURL(), url);
        if (u.isValid()) {
            const CString proxyStr = toString(WebCore::proxyServersForURL(u)).utf8();
            if (!proxyStr.isNull()) {
                const int size = proxyStr.length();
                *value = static_cast<char*>(NPN_MemAlloc(size+1));
                if (*value) {
                    memset(*value, 0, size+1);
                    memcpy(*value, proxyStr.data(), size+1);
                    if (len)
                        *len = size;
                } else
                    result = NPERR_OUT_OF_MEMORY_ERROR;
            }
        } else
            result = NPERR_INVALID_URL;
        break;
    }
    default:
        result = NPERR_GENERIC_ERROR;
        LOG(Plugins, "PluginView::getValueForURL: %s", prettyNameForNPNURLVariable(variable).data());
        break;
    }

    return result;
}


NPError PluginView::setValueForURL(NPNURLVariable variable, const char* url, const char* value, uint32_t len)
{
    LOG(Plugins, "PluginView::setValueForURL(%s)", prettyNameForNPNURLVariable(variable).data());

    NPError result = NPERR_NO_ERROR;

    switch (variable) {
    case NPNURLVCookie: {
        URL u(m_parentFrame->document()->baseURL(), url);
        if (u.isValid()) {
            const String cookieStr = String::fromUTF8(value, len);
            Frame* frame = getFrame(parentFrame(), m_element);
            if (frame && frame->document() && !cookieStr.isEmpty()) {
                if (auto* page = frame->document()->page())
                    page->cookieJar().setCookies(*frame->document(), u, cookieStr);
            }
        } else
            result = NPERR_INVALID_URL;
        break;
    }
    case NPNURLVProxy:
        LOG(Plugins, "PluginView::setValueForURL(%s): Plugins are NOT allowed to set proxy information.", prettyNameForNPNURLVariable(variable).data());
        result = NPERR_GENERIC_ERROR;
        break;
    default:
        LOG(Plugins, "PluginView::setValueForURL: %s", prettyNameForNPNURLVariable(variable).data());
        result = NPERR_GENERIC_ERROR;
        break;
    }

    return result;
}

NPError PluginView::getAuthenticationInfo(const char* protocol, const char* host, int32_t port, const char* /* scheme */, const char* /* realm */, char**  /* username */, uint32_t* /* ulen */, char** /* password */, uint32_t* /* plen */)
{
#if LOG_DISABLED
    UNUSED_PARAM(protocol);
    UNUSED_PARAM(host);
    UNUSED_PARAM(port);
#endif
    LOG(Plugins, "PluginView::getAuthenticationInfo: protocol=%s, host=%s, port=%d", protocol, host, port);
    notImplemented();
    return NPERR_GENERIC_ERROR;
}
#endif

void PluginView::privateBrowsingStateChanged(bool privateBrowsingEnabled)
{
    NPP_SetValueProcPtr setValue = m_plugin->pluginFuncs()->setvalue;
    if (!setValue)
        return;

    PluginView::setCurrentPluginView(this);
    JSC::JSLock::DropAllLocks dropAllLocks(commonVM());
    setCallingPlugin(true);
    NPBool value = privateBrowsingEnabled;
    setValue(m_instance, NPNVprivateModeBool, &value);
    setCallingPlugin(false);
    PluginView::setCurrentPluginView(0);
}

} // namespace WebCore

#endif // ENABLE(NETSCAPE_PLUGIN_API)
