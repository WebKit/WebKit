/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WebPlatformStrategies.h"

#if USE(PLATFORM_STRATEGIES)

#include "BlockingResponseMap.h"
#include "PluginInfoStore.h"
#include "WebContextMessages.h"
#include "WebCookieManager.h"
#include "WebCoreArgumentCoders.h"
#include "WebProcess.h"
#include "WebProcessProxyMessages.h"
#include <WebCore/Color.h>
#include <WebCore/KURL.h>
#include <WebCore/Page.h>
#include <WebCore/PlatformPasteboard.h>
#include <wtf/Atomics.h>

#if USE(CF)
#include <wtf/RetainPtr.h>
#endif

using namespace WebCore;

namespace WebKit {

void WebPlatformStrategies::initialize()
{
    DEFINE_STATIC_LOCAL(WebPlatformStrategies, platformStrategies, ());
    setPlatformStrategies(&platformStrategies);
}

WebPlatformStrategies::WebPlatformStrategies()
    : m_pluginCacheIsPopulated(false)
    , m_shouldRefreshPlugins(false)
{
}

CookiesStrategy* WebPlatformStrategies::createCookiesStrategy()
{
    return this;
}

PluginStrategy* WebPlatformStrategies::createPluginStrategy()
{
    return this;
}

VisitedLinkStrategy* WebPlatformStrategies::createVisitedLinkStrategy()
{
    return this;
}

PasteboardStrategy* WebPlatformStrategies::createPasteboardStrategy()
{
    return this;
}

// CookiesStrategy

void WebPlatformStrategies::notifyCookiesChanged()
{
    WebCookieManager::shared().dispatchCookiesDidChange();
}

// PluginStrategy

void WebPlatformStrategies::refreshPlugins()
{
    m_cachedPlugins.clear();
    m_pluginCacheIsPopulated = false;
    m_shouldRefreshPlugins = true;

    populatePluginCache();
}

void WebPlatformStrategies::getPluginInfo(const WebCore::Page*, Vector<WebCore::PluginInfo>& plugins)
{
    populatePluginCache();
    plugins = m_cachedPlugins;
}

static BlockingResponseMap<Vector<WebCore::PluginInfo> >& responseMap()
{
    AtomicallyInitializedStatic(BlockingResponseMap<Vector<WebCore::PluginInfo> >&, responseMap = *new BlockingResponseMap<Vector<WebCore::PluginInfo> >);
    return responseMap;
}

void handleDidGetPlugins(uint64_t requestID, const Vector<WebCore::PluginInfo>& plugins)
{
    responseMap().didReceiveResponse(requestID, adoptPtr(new Vector<WebCore::PluginInfo>(plugins)));
}

static uint64_t generateRequestID()
{
    static int uniqueID;
    return atomicIncrement(&uniqueID);
}

void WebPlatformStrategies::populatePluginCache()
{
    if (m_pluginCacheIsPopulated)
        return;

    ASSERT(m_cachedPlugins.isEmpty());
    
    // FIXME: Should we do something in case of error here?
    uint64_t requestID = generateRequestID();
    WebProcess::shared().connection()->send(Messages::WebProcessProxy::GetPlugins(requestID, m_shouldRefreshPlugins), 0);

    m_cachedPlugins = *responseMap().waitForResponse(requestID);
    
    m_shouldRefreshPlugins = false;
    m_pluginCacheIsPopulated = true;
}

// VisitedLinkStrategy

bool WebPlatformStrategies::isLinkVisited(Page*, LinkHash linkHash, const KURL&, const AtomicString&)
{
    return WebProcess::shared().isLinkVisited(linkHash);
}

void WebPlatformStrategies::addVisitedLink(Page*, LinkHash linkHash)
{
    WebProcess::shared().addVisitedLink(linkHash);
}

#if PLATFORM(MAC)
// PasteboardStrategy

void WebPlatformStrategies::getTypes(Vector<String>& types, const String& pasteboardName)
{
    WebProcess::shared().connection()->sendSync(Messages::WebContext::GetPasteboardTypes(pasteboardName),
                                                Messages::WebContext::GetPasteboardTypes::Reply(types), 0);
}

PassRefPtr<WebCore::SharedBuffer> WebPlatformStrategies::bufferForType(const String& pasteboardType, const String& pasteboardName)
{
    SharedMemory::Handle handle;
    uint64_t size = 0;
    WebProcess::shared().connection()->sendSync(Messages::WebContext::GetPasteboardBufferForType(pasteboardName, pasteboardType),
                                                Messages::WebContext::GetPasteboardBufferForType::Reply(handle, size), 0);
    if (handle.isNull())
        return 0;
    RefPtr<SharedMemory> sharedMemoryBuffer = SharedMemory::create(handle, SharedMemory::ReadOnly);
    return SharedBuffer::create(static_cast<unsigned char *>(sharedMemoryBuffer->data()), size);
}

void WebPlatformStrategies::getPathnamesForType(Vector<String>& pathnames, const String& pasteboardType, const String& pasteboardName)
{
    WebProcess::shared().connection()->sendSync(Messages::WebContext::GetPasteboardPathnamesForType(pasteboardName, pasteboardType),
                                                Messages::WebContext::GetPasteboardPathnamesForType::Reply(pathnames), 0);
}

String WebPlatformStrategies::stringForType(const String& pasteboardType, const String& pasteboardName)
{
    String value;
    WebProcess::shared().connection()->sendSync(Messages::WebContext::GetPasteboardStringForType(pasteboardName, pasteboardType),
                                                Messages::WebContext::GetPasteboardStringForType::Reply(value), 0);
    return value;
}

void WebPlatformStrategies::copy(const String& fromPasteboard, const String& toPasteboard)
{
    WebProcess::shared().connection()->send(Messages::WebContext::PasteboardCopy(fromPasteboard, toPasteboard), 0);
}

int WebPlatformStrategies::changeCount(const WTF::String &pasteboardName)
{
    uint64_t changeCount;
    WebProcess::shared().connection()->sendSync(Messages::WebContext::GetPasteboardChangeCount(pasteboardName),
                                                Messages::WebContext::GetPasteboardChangeCount::Reply(changeCount), 0);
    return changeCount;
}

String WebPlatformStrategies::uniqueName()
{
    String pasteboardName;
    WebProcess::shared().connection()->sendSync(Messages::WebContext::GetPasteboardUniqueName(),
                                                Messages::WebContext::GetPasteboardUniqueName::Reply(pasteboardName), 0);
    return pasteboardName;
}

Color WebPlatformStrategies::color(const String& pasteboardName)
{
    Color color;
    WebProcess::shared().connection()->sendSync(Messages::WebContext::GetPasteboardColor(pasteboardName),
                                                Messages::WebContext::GetPasteboardColor::Reply(color), 0);
    return color;
}

KURL WebPlatformStrategies::url(const String& pasteboardName)
{
    String urlString;
    WebProcess::shared().connection()->sendSync(Messages::WebContext::GetPasteboardURL(pasteboardName),
                                                Messages::WebContext::GetPasteboardURL::Reply(urlString), 0);
    return KURL(ParsedURLString, urlString);
}

void WebPlatformStrategies::addTypes(const Vector<String>& pasteboardTypes, const String& pasteboardName)
{
    WebProcess::shared().connection()->send(Messages::WebContext::AddPasteboardTypes(pasteboardName, pasteboardTypes), 0);
}

void WebPlatformStrategies::setTypes(const Vector<String>& pasteboardTypes, const String& pasteboardName)
{
    WebProcess::shared().connection()->send(Messages::WebContext::SetPasteboardTypes(pasteboardName, pasteboardTypes), 0);
}

void WebPlatformStrategies::setBufferForType(PassRefPtr<SharedBuffer> buffer, const String& pasteboardType, const String& pasteboardName)
{
    SharedMemory::Handle handle;
    if (buffer) {
        RefPtr<SharedMemory> sharedMemoryBuffer = SharedMemory::create(buffer->size());
        memcpy(sharedMemoryBuffer->data(), buffer->data(), buffer->size());
        sharedMemoryBuffer->createHandle(handle, SharedMemory::ReadOnly);
    }
    WebProcess::shared().connection()->send(Messages::WebContext::SetPasteboardBufferForType(pasteboardName, pasteboardType, handle, buffer ? buffer->size() : 0), 0);
}

void WebPlatformStrategies::setPathnamesForType(const Vector<String>& pathnames, const String& pasteboardType, const String& pasteboardName)
{
    WebProcess::shared().connection()->send(Messages::WebContext::SetPasteboardPathnamesForType(pasteboardName, pasteboardType, pathnames), 0);
}

void WebPlatformStrategies::setStringForType(const String& string, const String& pasteboardType, const String& pasteboardName)
{
    WebProcess::shared().connection()->send(Messages::WebContext::SetPasteboardStringForType(pasteboardName, pasteboardType, string), 0);
}
#endif

} // namespace WebKit

#endif // USE(PLATFORM_STRATEGIES)
