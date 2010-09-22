/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "NetscapePlugin.h"

#include "NPRuntimeObjectMap.h"
#include "NetscapePluginStream.h"
#include "PluginController.h"
#include <WebCore/GraphicsContext.h>
#include <WebCore/HTTPHeaderMap.h>
#include <WebCore/IntRect.h>
#include <WebCore/KURL.h>
#include <utility>
#include <wtf/text/CString.h>

using namespace WebCore;
using namespace std;

namespace WebKit {

NetscapePlugin::NetscapePlugin(PassRefPtr<NetscapePluginModule> pluginModule)
    : m_pluginController(0)
    , m_nextRequestID(0)
    , m_pluginModule(pluginModule)
    , m_npWindow()
    , m_isStarted(false)
#if PLATFORM(MAC)
    , m_isWindowed(false)
#else
    , m_isWindowed(true)
#endif
    , m_inNPPNew(false)
    , m_loadManually(false)
#if PLATFORM(MAC)
    , m_drawingModel(static_cast<NPDrawingModel>(-1))
    , m_eventModel(static_cast<NPEventModel>(-1))
#endif
{
    m_npp.ndata = this;
    m_npp.pdata = 0;
    
    m_pluginModule->pluginCreated();
}

NetscapePlugin::~NetscapePlugin()
{
    ASSERT(!m_isStarted);

    m_pluginModule->pluginDestroyed();
}

PassRefPtr<NetscapePlugin> NetscapePlugin::fromNPP(NPP npp)
{
    if (npp) {
        NetscapePlugin* plugin = static_cast<NetscapePlugin*>(npp->ndata);
        ASSERT(npp == &plugin->m_npp);
        
        return plugin;
    }

    // FIXME: Return the current NetscapePlugin here.
    ASSERT_NOT_REACHED();
    return 0;
}

void NetscapePlugin::invalidate(const NPRect* invalidRect)
{
    IntRect rect;
    
    if (!invalidRect)
        rect = IntRect(0, 0, m_frameRect.width(), m_frameRect.height());
    else
        rect = IntRect(invalidRect->left, invalidRect->top,
                       invalidRect->right - invalidRect->left, invalidRect->bottom - invalidRect->top);
    
    m_pluginController->invalidate(rect);
}

const char* NetscapePlugin::userAgent()
{
    if (m_userAgent.isNull()) {
        // FIXME: Pass the src URL.
        m_userAgent = m_pluginController->userAgent(KURL()).utf8();
        ASSERT(!m_userAgent.isNull());
    }
    
    return m_userAgent.data();
}

void NetscapePlugin::loadURL(const String& method, const String& urlString, const String& target, const HTTPHeaderMap& headerFields, const Vector<uint8_t>& httpBody,
                             bool sendNotification, void* notificationData)
{
    uint64_t requestID = ++m_nextRequestID;
    
    m_pluginController->loadURL(requestID, method, urlString, target, headerFields, httpBody, allowPopups());

    if (target.isNull()) {
        // The browser is going to send the data in a stream, create a plug-in stream.
        RefPtr<NetscapePluginStream> pluginStream = NetscapePluginStream::create(this, requestID, sendNotification, notificationData);
        ASSERT(!m_streams.contains(requestID));

        m_streams.set(requestID, pluginStream.release());
        return;
    }

    if (sendNotification) {
        // Eventually we are going to get a frameDidFinishLoading or frameDidFail call for this request.
        // Keep track of the notification data so we can call NPP_URLNotify.
        ASSERT(!m_pendingURLNotifications.contains(requestID));
        m_pendingURLNotifications.set(requestID, make_pair(urlString, notificationData));
    }
}

NPError NetscapePlugin::destroyStream(NPStream* stream, NPReason reason)
{
    NetscapePluginStream* pluginStream = 0;

    for (StreamsMap::const_iterator it = m_streams.begin(), end = m_streams.end(); it != end; ++it) {
        if (it->second->npStream() == stream) {
            pluginStream = it->second.get();
            break;
        }
    }

    if (!pluginStream)
        return NPERR_INVALID_INSTANCE_ERROR;

    return pluginStream->destroy(reason);
}

void NetscapePlugin::setStatusbarText(const String& statusbarText)
{
    m_pluginController->setStatusbarText(statusbarText);
}

void NetscapePlugin::setException(const String& exceptionString)
{
    // FIXME: If the plug-in is running in its own process, this needs to send a CoreIPC message instead of
    // calling the runtime object map directly.
    NPRuntimeObjectMap::setGlobalException(exceptionString);
}

bool NetscapePlugin::evaluate(NPObject* npObject, const String& scriptString, NPVariant* result)
{
    return m_pluginController->evaluate(npObject, scriptString, result, allowPopups());
}

NPObject* NetscapePlugin::windowScriptNPObject()
{
    return m_pluginController->windowScriptNPObject();
}

NPObject* NetscapePlugin::pluginElementNPObject()
{
    return m_pluginController->pluginElementNPObject();
}

void NetscapePlugin::cancelStreamLoad(NetscapePluginStream* pluginStream)
{
    if (pluginStream == m_manualStream) {
        m_pluginController->cancelManualStreamLoad();
        return;
    }

    // Ask the plug-in controller to cancel this stream load.
    m_pluginController->cancelStreamLoad(pluginStream->streamID());
}

void NetscapePlugin::removePluginStream(NetscapePluginStream* pluginStream)
{
    if (pluginStream == m_manualStream) {
        m_manualStream = 0;
        return;
    }

    ASSERT(m_streams.get(pluginStream->streamID()) == pluginStream);
    m_streams.remove(pluginStream->streamID());
}

bool NetscapePlugin::isAcceleratedCompositingEnabled()
{
#if USE(ACCELERATED_COMPOSITING)
    return m_pluginController->isAcceleratedCompositingEnabled();
#else
    return false;
#endif
}

NPError NetscapePlugin::NPP_New(NPMIMEType pluginType, uint16_t mode, int16_t argc, char* argn[], char* argv[], NPSavedData* savedData)
{
    return m_pluginModule->pluginFuncs().newp(pluginType, &m_npp, mode, argc, argn, argv, savedData);
}
    
NPError NetscapePlugin::NPP_Destroy(NPSavedData** savedData)
{
    return m_pluginModule->pluginFuncs().destroy(&m_npp, savedData);
}

NPError NetscapePlugin::NPP_SetWindow(NPWindow* npWindow)
{
    return m_pluginModule->pluginFuncs().setwindow(&m_npp, npWindow);
}

NPError NetscapePlugin::NPP_NewStream(NPMIMEType mimeType, NPStream* stream, NPBool seekable, uint16_t* streamType)
{
    return m_pluginModule->pluginFuncs().newstream(&m_npp, mimeType, stream, seekable, streamType);
}

NPError NetscapePlugin::NPP_DestroyStream(NPStream* stream, NPReason reason)
{
    return m_pluginModule->pluginFuncs().destroystream(&m_npp, stream, reason);
}

void NetscapePlugin::NPP_StreamAsFile(NPStream* stream, const char* filename)
{
    return m_pluginModule->pluginFuncs().asfile(&m_npp, stream, filename);
}

int32_t NetscapePlugin::NPP_WriteReady(NPStream* stream)
{
    return m_pluginModule->pluginFuncs().writeready(&m_npp, stream);
}

int32_t NetscapePlugin::NPP_Write(NPStream* stream, int32_t offset, int32_t len, void* buffer)
{
    return m_pluginModule->pluginFuncs().write(&m_npp, stream, offset, len, buffer);
}

int16_t NetscapePlugin::NPP_HandleEvent(void* event)
{
    return m_pluginModule->pluginFuncs().event(&m_npp, event);
}

void NetscapePlugin::NPP_URLNotify(const char* url, NPReason reason, void* notifyData)
{
    m_pluginModule->pluginFuncs().urlnotify(&m_npp, url, reason, notifyData);
}

NPError NetscapePlugin::NPP_GetValue(NPPVariable variable, void *value)
{
    return m_pluginModule->pluginFuncs().getvalue(&m_npp, variable, value);
}

void NetscapePlugin::callSetWindow()
{
    m_npWindow.x = m_frameRect.x();
    m_npWindow.y = m_frameRect.y();
    m_npWindow.width = m_frameRect.width();
    m_npWindow.height = m_frameRect.height();
    m_npWindow.clipRect.top = m_clipRect.y();
    m_npWindow.clipRect.left = m_clipRect.x();
    m_npWindow.clipRect.bottom = m_clipRect.bottom();
    m_npWindow.clipRect.right = m_clipRect.right();

    NPP_SetWindow(&m_npWindow);
}

bool NetscapePlugin::shouldLoadSrcURL()
{
    // Check if we should cancel the load
    NPBool cancelSrcStream = false;

    if (NPP_GetValue(NPPVpluginCancelSrcStream, &cancelSrcStream) != NPERR_NO_ERROR)
        return true;

    return !cancelSrcStream;
}

NetscapePluginStream* NetscapePlugin::streamFromID(uint64_t streamID)
{
    return m_streams.get(streamID).get();
}

void NetscapePlugin::stopAllStreams()
{
    Vector<RefPtr<NetscapePluginStream> > streams;
    copyValuesToVector(m_streams, streams);

    for (size_t i = 0; i < streams.size(); ++i)
        streams[i]->stop(NPRES_USER_BREAK);
}

bool NetscapePlugin::allowPopups() const
{
    // FIXME: Handle popups.
    return false;
}

bool NetscapePlugin::initialize(PluginController* pluginController, const Parameters& parameters)
{
    ASSERT(!m_pluginController);
    ASSERT(pluginController);

    m_pluginController = pluginController;
    
    uint16_t mode = parameters.loadManually ? NP_FULL : NP_EMBED;
    
    m_loadManually = parameters.loadManually;
    m_inNPPNew = true;

    CString mimeTypeCString = parameters.mimeType.utf8();

    ASSERT(parameters.names.size() == parameters.values.size());

    Vector<CString> paramNames;
    Vector<CString> paramValues;
    for (size_t i = 0; i < parameters.names.size(); ++i) {
        paramNames.append(parameters.names[i].utf8());
        paramValues.append(parameters.values[i].utf8());
    }

    // The strings that these pointers point to are kept alive by paramNames and paramValues.
    Vector<const char*> names;
    Vector<const char*> values;
    for (size_t i = 0; i < paramNames.size(); ++i) {
        names.append(paramNames[i].data());
        values.append(paramValues[i].data());
    }
    
    NPError error = NPP_New(const_cast<char*>(mimeTypeCString.data()), mode, names.size(),
                            const_cast<char**>(names.data()), const_cast<char**>(values.data()), 0);
    m_inNPPNew = false;

    if (error != NPERR_NO_ERROR)
        return false;

    m_isStarted = true;

    // FIXME: This is not correct in all cases.
    m_npWindow.type = NPWindowTypeDrawable;

    if (!platformPostInitialize()) {
        destroy();
        return false;
    }

    // Load the src URL if needed.
    if (!parameters.loadManually && !parameters.url.isEmpty() && shouldLoadSrcURL())
        loadURL("GET", parameters.url.string(), String(), HTTPHeaderMap(), Vector<uint8_t>(), false, 0);
    
    return true;
}
    
void NetscapePlugin::destroy()
{
    ASSERT(m_isStarted);

    // Stop all streams.
    stopAllStreams();

    NPP_Destroy(0);

    m_isStarted = false;
    m_pluginController = 0;
}
    
void NetscapePlugin::paint(GraphicsContext* context, const IntRect& dirtyRect)
{
    ASSERT(m_isStarted);
    
    platformPaint(context, dirtyRect);
}

void NetscapePlugin::geometryDidChange(const IntRect& frameRect, const IntRect& clipRect)
{
    ASSERT(m_isStarted);

    if (m_frameRect == frameRect && m_clipRect == clipRect) {
        // Nothing to do.
        return;
    }

    m_frameRect = frameRect;
    m_clipRect = clipRect;

    callSetWindow();
}

void NetscapePlugin::frameDidFinishLoading(uint64_t requestID)
{
    PendingURLNotifyMap::iterator it = m_pendingURLNotifications.find(requestID);
    if (it == m_pendingURLNotifications.end())
        return;

    String url = it->second.first;
    void* notificationData = it->second.second;

    m_pendingURLNotifications.remove(it);
    
    NPP_URLNotify(url.utf8().data(), NPRES_DONE, notificationData);
}

void NetscapePlugin::frameDidFail(uint64_t requestID, bool wasCancelled)
{
    PendingURLNotifyMap::iterator it = m_pendingURLNotifications.find(requestID);
    if (it == m_pendingURLNotifications.end())
        return;

    String url = it->second.first;
    void* notificationData = it->second.second;

    m_pendingURLNotifications.remove(it);
    
    NPP_URLNotify(url.utf8().data(), wasCancelled ? NPRES_USER_BREAK : NPRES_NETWORK_ERR, notificationData);
}

void NetscapePlugin::didEvaluateJavaScript(uint64_t requestID, const String& requestURLString, const String& result)
{
    if (NetscapePluginStream* pluginStream = streamFromID(requestID))
        pluginStream->sendJavaScriptStream(requestURLString, result);
}

void NetscapePlugin::streamDidReceiveResponse(uint64_t streamID, const KURL& responseURL, uint32_t streamLength, 
                                              uint32_t lastModifiedTime, const String& mimeType, const String& headers)
{
    if (NetscapePluginStream* pluginStream = streamFromID(streamID))
        pluginStream->didReceiveResponse(responseURL, streamLength, lastModifiedTime, mimeType, headers);
}

void NetscapePlugin::streamDidReceiveData(uint64_t streamID, const char* bytes, int length)
{
    if (NetscapePluginStream* pluginStream = streamFromID(streamID))
        pluginStream->didReceiveData(bytes, length);
}

void NetscapePlugin::streamDidFinishLoading(uint64_t streamID)
{
    if (NetscapePluginStream* pluginStream = streamFromID(streamID))
        pluginStream->didFinishLoading();
}

void NetscapePlugin::streamDidFail(uint64_t streamID, bool wasCancelled)
{
    if (NetscapePluginStream* pluginStream = streamFromID(streamID))
        pluginStream->didFail(wasCancelled);
}

void NetscapePlugin::manualStreamDidReceiveResponse(const KURL& responseURL, uint32_t streamLength, uint32_t lastModifiedTime, 
                                                    const String& mimeType, const String& headers)
{
    ASSERT(m_loadManually);
    ASSERT(!m_manualStream);
    
    m_manualStream = NetscapePluginStream::create(this, 0, false, 0);
    m_manualStream->didReceiveResponse(responseURL, streamLength, lastModifiedTime, mimeType, headers);
}

void NetscapePlugin::manualStreamDidReceiveData(const char* bytes, int length)
{
    ASSERT(m_loadManually);
    ASSERT(m_manualStream);

    m_manualStream->didReceiveData(bytes, length);
}

void NetscapePlugin::manualStreamDidFinishLoading()
{
    ASSERT(m_loadManually);
    ASSERT(m_manualStream);

    m_manualStream->didFinishLoading();
}

void NetscapePlugin::manualStreamDidFail(bool wasCancelled)
{
    ASSERT(m_loadManually);
    ASSERT(m_manualStream);

    m_manualStream->didFail(wasCancelled);
}

bool NetscapePlugin::handleMouseEvent(const WebMouseEvent& mouseEvent)
{
    return platformHandleMouseEvent(mouseEvent);
}
    
bool NetscapePlugin::handleWheelEvent(const WebWheelEvent& wheelEvent)
{
    return platformHandleWheelEvent(wheelEvent);
}

bool NetscapePlugin::handleMouseEnterEvent(const WebMouseEvent& mouseEvent)
{
    return platformHandleMouseEnterEvent(mouseEvent);
}

bool NetscapePlugin::handleMouseLeaveEvent(const WebMouseEvent& mouseEvent)
{
    return platformHandleMouseLeaveEvent(mouseEvent);
}

void NetscapePlugin::setFocus(bool hasFocus)
{
    platformSetFocus(hasFocus);
}

NPObject* NetscapePlugin::pluginScriptableNPObject()
{
    NPObject* scriptableNPObject = 0;
    
    if (NPP_GetValue(NPPVpluginScriptableNPObject, &scriptableNPObject) != NPERR_NO_ERROR)
        return 0;
    
    return scriptableNPObject;
}

PluginController* NetscapePlugin::controller()
{
    return m_pluginController;
}

} // namespace WebKit
