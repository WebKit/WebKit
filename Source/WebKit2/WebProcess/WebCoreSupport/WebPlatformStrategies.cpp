/*
 * Copyright (C) 2010, 2011, 2012, 2015 Apple Inc. All rights reserved.
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

#include "BlobRegistryProxy.h"
#include "BlockingResponseMap.h"
#include "DataReference.h"
#include "HangDetectionDisabler.h"
#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkProcessConnection.h"
#include "NetworkResourceLoadParameters.h"
#include "PluginInfoStore.h"
#include "SessionTracker.h"
#include "WebCookieManager.h"
#include "WebCoreArgumentCoders.h"
#include "WebErrors.h"
#include "WebFrame.h"
#include "WebFrameLoaderClient.h"
#include "WebIDBFactoryBackend.h"
#include "WebLoaderStrategy.h"
#include "WebPage.h"
#include "WebPasteboardOverrides.h"
#include "WebPasteboardProxyMessages.h"
#include "WebProcess.h"
#include "WebProcessProxyMessages.h"
#include <WebCore/Color.h>
#include <WebCore/Document.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/IDBFactoryBackendInterface.h>
#include <WebCore/LoaderStrategy.h>
#include <WebCore/MainFrame.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/NetworkingContext.h>
#include <WebCore/Page.h>
#include <WebCore/PageGroup.h>
#include <WebCore/PlatformCookieJar.h>
#include <WebCore/PlatformPasteboard.h>
#include <WebCore/ProgressTracker.h>
#include <WebCore/ResourceError.h>
#include <WebCore/SessionID.h>
#include <WebCore/StorageNamespace.h>
#include <WebCore/SubframeLoader.h>
#include <WebCore/URL.h>
#include <wtf/Atomics.h>

#if PLATFORM(MAC)
#include "StringUtilities.h"
#endif

using namespace WebCore;

namespace WebKit {

void WebPlatformStrategies::initialize()
{
    static NeverDestroyed<WebPlatformStrategies> platformStrategies;
    setPlatformStrategies(&platformStrategies.get());
}

WebPlatformStrategies::WebPlatformStrategies()
#if ENABLE(NETSCAPE_PLUGIN_API)
    : m_pluginCacheIsPopulated(false)
    , m_shouldRefreshPlugins(false)
#endif // ENABLE(NETSCAPE_PLUGIN_API)
{
}

CookiesStrategy* WebPlatformStrategies::createCookiesStrategy()
{
    return this;
}

LoaderStrategy* WebPlatformStrategies::createLoaderStrategy()
{
    return &WebProcess::singleton().webLoaderStrategy();
}

PasteboardStrategy* WebPlatformStrategies::createPasteboardStrategy()
{
    return this;
}

PluginStrategy* WebPlatformStrategies::createPluginStrategy()
{
    return this;
}

BlobRegistry* WebPlatformStrategies::createBlobRegistry()
{
    return new BlobRegistryProxy;
}

// CookiesStrategy

String WebPlatformStrategies::cookiesForDOM(const NetworkStorageSession& session, const URL& firstParty, const URL& url)
{
    String result;
    if (!WebProcess::singleton().networkConnection()->connection()->sendSync(Messages::NetworkConnectionToWebProcess::CookiesForDOM(SessionTracker::sessionID(session), firstParty, url), Messages::NetworkConnectionToWebProcess::CookiesForDOM::Reply(result), 0))
        return String();
    return result;
}

void WebPlatformStrategies::setCookiesFromDOM(const NetworkStorageSession& session, const URL& firstParty, const URL& url, const String& cookieString)
{
    WebProcess::singleton().networkConnection()->connection()->send(Messages::NetworkConnectionToWebProcess::SetCookiesFromDOM(SessionTracker::sessionID(session), firstParty, url, cookieString), 0);
}

bool WebPlatformStrategies::cookiesEnabled(const NetworkStorageSession& session, const URL& firstParty, const URL& url)
{
    bool result;
    if (!WebProcess::singleton().networkConnection()->connection()->sendSync(Messages::NetworkConnectionToWebProcess::CookiesEnabled(SessionTracker::sessionID(session), firstParty, url), Messages::NetworkConnectionToWebProcess::CookiesEnabled::Reply(result), 0))
        return false;
    return result;
}

String WebPlatformStrategies::cookieRequestHeaderFieldValue(const NetworkStorageSession& session, const URL& firstParty, const URL& url)
{
    String result;
    if (!WebProcess::singleton().networkConnection()->connection()->sendSync(Messages::NetworkConnectionToWebProcess::CookieRequestHeaderFieldValue(SessionTracker::sessionID(session), firstParty, url), Messages::NetworkConnectionToWebProcess::CookieRequestHeaderFieldValue::Reply(result), 0))
        return String();
    return result;
}

bool WebPlatformStrategies::getRawCookies(const NetworkStorageSession& session, const URL& firstParty, const URL& url, Vector<Cookie>& rawCookies)
{
    if (!WebProcess::singleton().networkConnection()->connection()->sendSync(Messages::NetworkConnectionToWebProcess::GetRawCookies(SessionTracker::sessionID(session), firstParty, url), Messages::NetworkConnectionToWebProcess::GetRawCookies::Reply(rawCookies), 0))
        return false;
    return true;
}

void WebPlatformStrategies::deleteCookie(const NetworkStorageSession& session, const URL& url, const String& cookieName)
{
    WebProcess::singleton().networkConnection()->connection()->send(Messages::NetworkConnectionToWebProcess::DeleteCookie(SessionTracker::sessionID(session), url, cookieName), 0);
}

// PluginStrategy

void WebPlatformStrategies::refreshPlugins()
{
#if ENABLE(NETSCAPE_PLUGIN_API)
    m_cachedPlugins.clear();
    m_pluginCacheIsPopulated = false;
    m_shouldRefreshPlugins = true;
#endif // ENABLE(NETSCAPE_PLUGIN_API)
}

void WebPlatformStrategies::getPluginInfo(const WebCore::Page* page, Vector<WebCore::PluginInfo>& plugins)
{
#if ENABLE(NETSCAPE_PLUGIN_API)
    ASSERT_ARG(page, page);
    populatePluginCache(*page);

    if (page->mainFrame().loader().subframeLoader().allowPlugins()) {
        plugins = m_cachedPlugins;
        return;
    }

    plugins = m_cachedApplicationPlugins;
#else
    UNUSED_PARAM(page);
    UNUSED_PARAM(plugins);
#endif // ENABLE(NETSCAPE_PLUGIN_API)
}

void WebPlatformStrategies::getWebVisiblePluginInfo(const Page* page, Vector<PluginInfo>& plugins)
{
    ASSERT_ARG(page, page);
    ASSERT_ARG(plugins, plugins.isEmpty());

    getPluginInfo(page, plugins);

#if PLATFORM(MAC)
    if (Document* document = page->mainFrame().document()) {
        if (SecurityOrigin* securityOrigin = document->securityOrigin()) {
            if (securityOrigin->isLocal())
                return;
        }
    }
    
    for (int32_t i = plugins.size() - 1; i >= 0; --i) {
        PluginInfo& info = plugins.at(i);
        PluginLoadClientPolicy clientPolicy = info.clientLoadPolicy;
        // Allow built-in plugins. Also tentatively allow plugins that the client might later selectively permit.
        if (info.isApplicationPlugin || clientPolicy == PluginLoadClientPolicyAsk)
            continue;

        if (clientPolicy == PluginLoadClientPolicyBlock)
            plugins.remove(i);
    }
#endif
}

#if PLATFORM(MAC)
void WebPlatformStrategies::setPluginLoadClientPolicy(PluginLoadClientPolicy clientPolicy, const String& host, const String& bundleIdentifier, const String& versionString)
{
    String hostToSet = host.isNull() || !host.length() ? "*" : host;
    String bundleIdentifierToSet = bundleIdentifier.isNull() || !bundleIdentifier.length() ? "*" : bundleIdentifier;
    String versionStringToSet = versionString.isNull() || !versionString.length() ? "*" : versionString;

    PluginPolicyMapsByIdentifier policiesByIdentifier;
    if (m_hostsToPluginIdentifierData.contains(hostToSet))
        policiesByIdentifier = m_hostsToPluginIdentifierData.get(hostToSet);

    PluginLoadClientPoliciesByBundleVersion versionsToPolicies;
    if (policiesByIdentifier.contains(bundleIdentifierToSet))
        versionsToPolicies = policiesByIdentifier.get(bundleIdentifierToSet);

    versionsToPolicies.set(versionStringToSet, clientPolicy);
    policiesByIdentifier.set(bundleIdentifierToSet, versionsToPolicies);
    m_hostsToPluginIdentifierData.set(hostToSet, policiesByIdentifier);
}

void WebPlatformStrategies::clearPluginClientPolicies()
{
    m_hostsToPluginIdentifierData.clear();
}

#endif

#if ENABLE(NETSCAPE_PLUGIN_API)
#if PLATFORM(MAC)

String WebPlatformStrategies::longestMatchedWildcardHostForHost(const String& host) const
{
    String longestMatchedHost;

    for (auto& key : m_hostsToPluginIdentifierData.keys()) {
        if (key.contains('*') && key != "*" && stringMatchesWildcardString(host, key)) {
            if (key.length() > longestMatchedHost.length())
                longestMatchedHost = key;
            else if (key.length() == longestMatchedHost.length() && codePointCompareLessThan(key, longestMatchedHost))
                longestMatchedHost = key;
        }
    }

    return longestMatchedHost;
}

bool WebPlatformStrategies::replaceHostWithMatchedWildcardHost(String& host, const String& identifier) const
{
    String matchedWildcardHost = longestMatchedWildcardHostForHost(host);

    if (matchedWildcardHost.isNull())
        return false;

    auto plugInIdentifierData = m_hostsToPluginIdentifierData.find(matchedWildcardHost);
    if (plugInIdentifierData == m_hostsToPluginIdentifierData.end() || !plugInIdentifierData->value.contains(identifier))
        return false;

    host = matchedWildcardHost;
    return true;
}

bool WebPlatformStrategies::pluginLoadClientPolicyForHost(const String& host, const PluginInfo& info, PluginLoadClientPolicy& policy) const
{
    String hostToLookUp = host;
    String identifier = info.bundleIdentifier;

    auto policiesByIdentifierIterator = m_hostsToPluginIdentifierData.find(hostToLookUp);

    if (!identifier.isNull() && policiesByIdentifierIterator == m_hostsToPluginIdentifierData.end()) {
        if (!replaceHostWithMatchedWildcardHost(hostToLookUp, identifier))
            hostToLookUp = "*";
        policiesByIdentifierIterator = m_hostsToPluginIdentifierData.find(hostToLookUp);
        if (hostToLookUp != "*" && policiesByIdentifierIterator == m_hostsToPluginIdentifierData.end()) {
            hostToLookUp = "*";
            policiesByIdentifierIterator = m_hostsToPluginIdentifierData.find(hostToLookUp);
        }
    }
    if (policiesByIdentifierIterator == m_hostsToPluginIdentifierData.end())
        return false;

    auto& policiesByIdentifier = policiesByIdentifierIterator->value;

    if (!identifier)
        identifier = "*";

    auto identifierPolicyIterator = policiesByIdentifier.find(identifier);
    if (identifier != "*" && identifierPolicyIterator == policiesByIdentifier.end()) {
        identifier = "*";
        identifierPolicyIterator = policiesByIdentifier.find(identifier);
    }
    if (identifierPolicyIterator == policiesByIdentifier.end())
        return false;

    auto& versionsToPolicies = identifierPolicyIterator->value;

    String version = info.versionString;
    if (!version)
        version = "*";
    auto policyIterator = versionsToPolicies.find(version);
    if (version != "*" && policyIterator == versionsToPolicies.end()) {
        version = "*";
        policyIterator = versionsToPolicies.find(version);
    }

    if (policyIterator == versionsToPolicies.end())
        return false;

    policy = policyIterator->value;
    return true;
}
#endif // PLATFORM(MAC)

void WebPlatformStrategies::populatePluginCache(const WebCore::Page& page)
{
    if (!m_pluginCacheIsPopulated) {
        HangDetectionDisabler hangDetectionDisabler;

        if (!WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebProcessProxy::GetPlugins(m_shouldRefreshPlugins), Messages::WebProcessProxy::GetPlugins::Reply(m_cachedPlugins, m_cachedApplicationPlugins), 0))
            return;

        m_shouldRefreshPlugins = false;
        m_pluginCacheIsPopulated = true;
    }

#if PLATFORM(MAC)
    String pageHost = page.mainFrame().loader().documentLoader()->responseURL().host();
    for (PluginInfo& info : m_cachedPlugins) {
        PluginLoadClientPolicy clientPolicy;
        if (pluginLoadClientPolicyForHost(pageHost, info, clientPolicy))
            info.clientLoadPolicy = clientPolicy;
    }
#else
    UNUSED_PARAM(page);
#endif // not PLATFORM(MAC)
}
#endif // ENABLE(NETSCAPE_PLUGIN_API)

#if PLATFORM(COCOA)
// PasteboardStrategy

void WebPlatformStrategies::getTypes(Vector<String>& types, const String& pasteboardName)
{
    // First check the overrides.
    // The purpose of the overrides is to avoid messaging back to the UI process.
    // Therefore, if there are any overridden types, we return just those.
    types = WebPasteboardOverrides::sharedPasteboardOverrides().overriddenTypes(pasteboardName);
    if (!types.isEmpty())
        return;

    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::GetPasteboardTypes(pasteboardName), Messages::WebPasteboardProxy::GetPasteboardTypes::Reply(types), 0);
}

PassRefPtr<WebCore::SharedBuffer> WebPlatformStrategies::bufferForType(const String& pasteboardType, const String& pasteboardName)
{
    // First check the overrides.
    Vector<char> overrideBuffer;
    if (WebPasteboardOverrides::sharedPasteboardOverrides().getDataForOverride(pasteboardName, pasteboardType, overrideBuffer))
        return SharedBuffer::adoptVector(overrideBuffer);

    // Fallback to messaging the UI process for native pasteboard content.
    SharedMemory::Handle handle;
    uint64_t size = 0;
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::GetPasteboardBufferForType(pasteboardName, pasteboardType), Messages::WebPasteboardProxy::GetPasteboardBufferForType::Reply(handle, size), 0);
    if (handle.isNull())
        return 0;
    RefPtr<SharedMemory> sharedMemoryBuffer = SharedMemory::map(handle, SharedMemory::Protection::ReadOnly);
    return SharedBuffer::create(static_cast<unsigned char *>(sharedMemoryBuffer->data()), size);
}

void WebPlatformStrategies::getPathnamesForType(Vector<String>& pathnames, const String& pasteboardType, const String& pasteboardName)
{
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::GetPasteboardPathnamesForType(pasteboardName, pasteboardType), Messages::WebPasteboardProxy::GetPasteboardPathnamesForType::Reply(pathnames), 0);
}

String WebPlatformStrategies::stringForType(const String& pasteboardType, const String& pasteboardName)
{
    String value;
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::GetPasteboardStringForType(pasteboardName, pasteboardType), Messages::WebPasteboardProxy::GetPasteboardStringForType::Reply(value), 0);
    return value;
}

long WebPlatformStrategies::copy(const String& fromPasteboard, const String& toPasteboard)
{
    uint64_t newChangeCount;
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::PasteboardCopy(fromPasteboard, toPasteboard), Messages::WebPasteboardProxy::PasteboardCopy::Reply(newChangeCount), 0);
    return newChangeCount;
}

long WebPlatformStrategies::changeCount(const WTF::String &pasteboardName)
{
    uint64_t changeCount;
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::GetPasteboardChangeCount(pasteboardName), Messages::WebPasteboardProxy::GetPasteboardChangeCount::Reply(changeCount), 0);
    return changeCount;
}

String WebPlatformStrategies::uniqueName()
{
    String pasteboardName;
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::GetPasteboardUniqueName(), Messages::WebPasteboardProxy::GetPasteboardUniqueName::Reply(pasteboardName), 0);
    return pasteboardName;
}

Color WebPlatformStrategies::color(const String& pasteboardName)
{
    Color color;
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::GetPasteboardColor(pasteboardName), Messages::WebPasteboardProxy::GetPasteboardColor::Reply(color), 0);
    return color;
}

URL WebPlatformStrategies::url(const String& pasteboardName)
{
    String urlString;
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::GetPasteboardURL(pasteboardName), Messages::WebPasteboardProxy::GetPasteboardURL::Reply(urlString), 0);
    return URL(ParsedURLString, urlString);
}

long WebPlatformStrategies::addTypes(const Vector<String>& pasteboardTypes, const String& pasteboardName)
{
    uint64_t newChangeCount;
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::AddPasteboardTypes(pasteboardName, pasteboardTypes), Messages::WebPasteboardProxy::AddPasteboardTypes::Reply(newChangeCount), 0);
    return newChangeCount;
}

long WebPlatformStrategies::setTypes(const Vector<String>& pasteboardTypes, const String& pasteboardName)
{
    uint64_t newChangeCount;
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::SetPasteboardTypes(pasteboardName, pasteboardTypes), Messages::WebPasteboardProxy::SetPasteboardTypes::Reply(newChangeCount), 0);
    return newChangeCount;
}

long WebPlatformStrategies::setBufferForType(PassRefPtr<SharedBuffer> buffer, const String& pasteboardType, const String& pasteboardName)
{
    SharedMemory::Handle handle;
    if (buffer) {
        RefPtr<SharedMemory> sharedMemoryBuffer = SharedMemory::allocate(buffer->size());
        // FIXME: Null check prevents crashing, but it is not great that we will have empty pasteboard content for this type,
        // because we've already set the types.
        if (sharedMemoryBuffer) {
            memcpy(sharedMemoryBuffer->data(), buffer->data(), buffer->size());
            sharedMemoryBuffer->createHandle(handle, SharedMemory::Protection::ReadOnly);
        }
    }
    uint64_t newChangeCount;
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::SetPasteboardBufferForType(pasteboardName, pasteboardType, handle, buffer ? buffer->size() : 0), Messages::WebPasteboardProxy::SetPasteboardBufferForType::Reply(newChangeCount), 0);
    return newChangeCount;
}

long WebPlatformStrategies::setPathnamesForType(const Vector<String>& pathnames, const String& pasteboardType, const String& pasteboardName)
{
    uint64_t newChangeCount;
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::SetPasteboardPathnamesForType(pasteboardName, pasteboardType, pathnames), Messages::WebPasteboardProxy::SetPasteboardPathnamesForType::Reply(newChangeCount), 0);
    return newChangeCount;
}

long WebPlatformStrategies::setStringForType(const String& string, const String& pasteboardType, const String& pasteboardName)
{
    uint64_t newChangeCount;
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::SetPasteboardStringForType(pasteboardName, pasteboardType, string), Messages::WebPasteboardProxy::SetPasteboardStringForType::Reply(newChangeCount), 0);
    return newChangeCount;
}

#if PLATFORM(IOS)
void WebPlatformStrategies::writeToPasteboard(const WebCore::PasteboardWebContent& content)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebPasteboardProxy::WriteWebContentToPasteboard(content), 0);
}

void WebPlatformStrategies::writeToPasteboard(const WebCore::PasteboardImage& image)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebPasteboardProxy::WriteImageToPasteboard(image), 0);
}

void WebPlatformStrategies::writeToPasteboard(const String& pasteboardType, const String& text)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebPasteboardProxy::WriteStringToPasteboard(pasteboardType, text), 0);
}

int WebPlatformStrategies::getPasteboardItemsCount()
{
    uint64_t itemsCount;
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::GetPasteboardItemsCount(), Messages::WebPasteboardProxy::GetPasteboardItemsCount::Reply(itemsCount), 0);
    return itemsCount;
}

PassRefPtr<WebCore::SharedBuffer> WebPlatformStrategies::readBufferFromPasteboard(int index, const String& pasteboardType)
{
    SharedMemory::Handle handle;
    uint64_t size = 0;
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::ReadBufferFromPasteboard(index, pasteboardType), Messages::WebPasteboardProxy::ReadBufferFromPasteboard::Reply(handle, size), 0);
    if (handle.isNull())
        return 0;
    RefPtr<SharedMemory> sharedMemoryBuffer = SharedMemory::map(handle, SharedMemory::Protection::ReadOnly);
    return SharedBuffer::create(static_cast<unsigned char *>(sharedMemoryBuffer->data()), size);
}

WebCore::URL WebPlatformStrategies::readURLFromPasteboard(int index, const String& pasteboardType)
{
    String urlString;
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::ReadURLFromPasteboard(index, pasteboardType), Messages::WebPasteboardProxy::ReadURLFromPasteboard::Reply(urlString), 0);
    return URL(ParsedURLString, urlString);
}

String WebPlatformStrategies::readStringFromPasteboard(int index, const String& pasteboardType)
{
    String value;
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::ReadStringFromPasteboard(index, pasteboardType), Messages::WebPasteboardProxy::ReadStringFromPasteboard::Reply(value), 0);
    return value;
}

long WebPlatformStrategies::changeCount()
{
    uint64_t changeCount;
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::GetPasteboardChangeCount(String()), Messages::WebPasteboardProxy::GetPasteboardChangeCount::Reply(changeCount), 0);
    return changeCount;
}

#endif // PLATFORM(IOS)

#endif // PLATFORM(COCOA)

} // namespace WebKit
