/*
 * Copyright (C) 2010-2018 Apple Inc. All rights reserved.
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
#include "WebCoreArgumentCoders.h"
#include "WebErrors.h"
#include "WebFrame.h"
#include "WebFrameLoaderClient.h"
#include "WebLoaderStrategy.h"
#include "WebPage.h"
#include "WebPasteboardOverrides.h"
#include "WebPasteboardProxyMessages.h"
#include "WebProcess.h"
#include "WebProcessProxyMessages.h"
#include <WebCore/Color.h>
#include <WebCore/Document.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/Frame.h>
#include <WebCore/LoaderStrategy.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/Page.h>
#include <WebCore/PageGroup.h>
#include <WebCore/PasteboardItemInfo.h>
#include <WebCore/PlatformPasteboard.h>
#include <WebCore/ProgressTracker.h>
#include <WebCore/ResourceError.h>
#include <WebCore/SameSiteInfo.h>
#include <WebCore/StorageNamespace.h>
#include <WebCore/SubframeLoader.h>
#include <pal/SessionID.h>
#include <wtf/Atomics.h>
#include <wtf/URL.h>

#if PLATFORM(MAC)
#include "StringUtilities.h"
#endif

#if PLATFORM(GTK)
#include "WebSelectionData.h"
#endif

namespace WebKit {
using namespace WebCore;

void WebPlatformStrategies::initialize()
{
    static NeverDestroyed<WebPlatformStrategies> platformStrategies;
    setPlatformStrategies(&platformStrategies.get());
}

WebPlatformStrategies::WebPlatformStrategies()
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

BlobRegistry* WebPlatformStrategies::createBlobRegistry()
{
    return new BlobRegistryProxy;
}

// CookiesStrategy

std::pair<String, bool> WebPlatformStrategies::cookiesForDOM(const PAL::SessionID& sessionID, const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, Optional<uint64_t> frameID, Optional<uint64_t> pageID, IncludeSecureCookies includeSecureCookies)
{
    String cookieString;
    bool secureCookiesAccessed = false;
    if (!WebProcess::singleton().ensureNetworkProcessConnection().connection().sendSync(Messages::NetworkConnectionToWebProcess::CookiesForDOM(sessionID, firstParty, sameSiteInfo, url, frameID, pageID, includeSecureCookies), Messages::NetworkConnectionToWebProcess::CookiesForDOM::Reply(cookieString, secureCookiesAccessed), 0))
        return { String(), false };

    return { cookieString, secureCookiesAccessed };
}

void WebPlatformStrategies::setCookiesFromDOM(const PAL::SessionID& sessionID, const URL& firstParty, const WebCore::SameSiteInfo& sameSiteInfo, const URL& url, Optional<uint64_t> frameID, Optional<uint64_t> pageID, const String& cookieString)
{
    WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::SetCookiesFromDOM(sessionID, firstParty, sameSiteInfo, url, frameID, pageID, cookieString), 0);
}

bool WebPlatformStrategies::cookiesEnabled(const PAL::SessionID& sessionID)
{
    bool result;
    if (!WebProcess::singleton().ensureNetworkProcessConnection().connection().sendSync(Messages::NetworkConnectionToWebProcess::CookiesEnabled(sessionID), Messages::NetworkConnectionToWebProcess::CookiesEnabled::Reply(result), 0))
        return false;
    return result;
}

std::pair<String, bool> WebPlatformStrategies::cookieRequestHeaderFieldValue(const PAL::SessionID& sessionID, const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, Optional<uint64_t> frameID, Optional<uint64_t> pageID, IncludeSecureCookies includeSecureCookies)
{
    String cookieString;
    bool secureCookiesAccessed = false;
    if (!WebProcess::singleton().ensureNetworkProcessConnection().connection().sendSync(Messages::NetworkConnectionToWebProcess::CookieRequestHeaderFieldValue(sessionID, firstParty, sameSiteInfo, url, frameID, pageID, includeSecureCookies), Messages::NetworkConnectionToWebProcess::CookieRequestHeaderFieldValue::Reply(cookieString, secureCookiesAccessed), 0))
        return { String(), false };
    return { cookieString, secureCookiesAccessed };
}

bool WebPlatformStrategies::getRawCookies(const PAL::SessionID& sessionID, const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, Optional<uint64_t> frameID, Optional<uint64_t> pageID, Vector<Cookie>& rawCookies)
{
    if (!WebProcess::singleton().ensureNetworkProcessConnection().connection().sendSync(Messages::NetworkConnectionToWebProcess::GetRawCookies(sessionID, firstParty, sameSiteInfo, url, frameID, pageID), Messages::NetworkConnectionToWebProcess::GetRawCookies::Reply(rawCookies), 0))
        return false;
    return true;
}

void WebPlatformStrategies::deleteCookie(const PAL::SessionID& sessionID, const URL& url, const String& cookieName)
{
    WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::DeleteCookie(sessionID, url, cookieName), 0);
}

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

RefPtr<WebCore::SharedBuffer> WebPlatformStrategies::bufferForType(const String& pasteboardType, const String& pasteboardName)
{
    // First check the overrides.
    Vector<char> overrideBuffer;
    if (WebPasteboardOverrides::sharedPasteboardOverrides().getDataForOverride(pasteboardName, pasteboardType, overrideBuffer))
        return SharedBuffer::create(WTFMove(overrideBuffer));

    // Fallback to messaging the UI process for native pasteboard content.
    SharedMemory::Handle handle;
    uint64_t size { 0 };
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::GetPasteboardBufferForType(pasteboardName, pasteboardType), Messages::WebPasteboardProxy::GetPasteboardBufferForType::Reply(handle, size), 0);
    if (handle.isNull())
        return nullptr;
    RefPtr<SharedMemory> sharedMemoryBuffer = SharedMemory::map(handle, SharedMemory::Protection::ReadOnly);
    return SharedBuffer::create(static_cast<unsigned char *>(sharedMemoryBuffer->data()), size);
}

void WebPlatformStrategies::getPathnamesForType(Vector<String>& pathnames, const String& pasteboardType, const String& pasteboardName)
{
    SandboxExtension::HandleArray sandboxExtensionsHandleArray;
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::GetPasteboardPathnamesForType(pasteboardName, pasteboardType),
        Messages::WebPasteboardProxy::GetPasteboardPathnamesForType::Reply(pathnames, sandboxExtensionsHandleArray), 0);
    ASSERT(pathnames.size() == sandboxExtensionsHandleArray.size());
    for (size_t i = 0; i < sandboxExtensionsHandleArray.size(); i++) {
        if (auto extension = SandboxExtension::create(WTFMove(sandboxExtensionsHandleArray[i])))
            extension->consumePermanently();
    }
}

String WebPlatformStrategies::stringForType(const String& pasteboardType, const String& pasteboardName)
{
    String value;
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::GetPasteboardStringForType(pasteboardName, pasteboardType), Messages::WebPasteboardProxy::GetPasteboardStringForType::Reply(value), 0);
    return value;
}

Vector<String> WebPlatformStrategies::allStringsForType(const String& pasteboardType, const String& pasteboardName)
{
    Vector<String> values;
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::GetPasteboardStringsForType(pasteboardName, pasteboardType), Messages::WebPasteboardProxy::GetPasteboardStringsForType::Reply(values), 0);
    return values;
}

long WebPlatformStrategies::changeCount(const WTF::String &pasteboardName)
{
    uint64_t changeCount { 0 };
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
    return URL({ }, urlString);
}

long WebPlatformStrategies::addTypes(const Vector<String>& pasteboardTypes, const String& pasteboardName)
{
    uint64_t newChangeCount { 0 };
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::AddPasteboardTypes(pasteboardName, pasteboardTypes), Messages::WebPasteboardProxy::AddPasteboardTypes::Reply(newChangeCount), 0);
    return newChangeCount;
}

long WebPlatformStrategies::setTypes(const Vector<String>& pasteboardTypes, const String& pasteboardName)
{
    uint64_t newChangeCount { 0 };
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::SetPasteboardTypes(pasteboardName, pasteboardTypes), Messages::WebPasteboardProxy::SetPasteboardTypes::Reply(newChangeCount), 0);
    return newChangeCount;
}

long WebPlatformStrategies::setBufferForType(SharedBuffer* buffer, const String& pasteboardType, const String& pasteboardName)
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
    uint64_t newChangeCount { 0 };
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::SetPasteboardBufferForType(pasteboardName, pasteboardType, handle, buffer ? buffer->size() : 0), Messages::WebPasteboardProxy::SetPasteboardBufferForType::Reply(newChangeCount), 0);
    return newChangeCount;
}

long WebPlatformStrategies::setURL(const PasteboardURL& pasteboardURL, const String& pasteboardName)
{
    uint64_t newChangeCount;
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::SetPasteboardURL(pasteboardURL, pasteboardName), Messages::WebPasteboardProxy::SetPasteboardURL::Reply(newChangeCount), 0);
    return newChangeCount;
}

long WebPlatformStrategies::setColor(const Color& color, const String& pasteboardName)
{
    uint64_t newChangeCount { 0 };
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::SetPasteboardColor(pasteboardName, color), Messages::WebPasteboardProxy::SetPasteboardColor::Reply(newChangeCount), 0);
    return newChangeCount;
}

long WebPlatformStrategies::setStringForType(const String& string, const String& pasteboardType, const String& pasteboardName)
{
    uint64_t newChangeCount { 0 };
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::SetPasteboardStringForType(pasteboardName, pasteboardType, string), Messages::WebPasteboardProxy::SetPasteboardStringForType::Reply(newChangeCount), 0);
    return newChangeCount;
}

int WebPlatformStrategies::getNumberOfFiles(const String& pasteboardName)
{
    uint64_t numberOfFiles { 0 };
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::GetNumberOfFiles(pasteboardName), Messages::WebPasteboardProxy::GetNumberOfFiles::Reply(numberOfFiles), 0);
    return numberOfFiles;
}

#if PLATFORM(IOS_FAMILY)
void WebPlatformStrategies::getTypesByFidelityForItemAtIndex(Vector<String>& types, uint64_t index, const String& pasteboardName)
{
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::GetPasteboardTypesByFidelityForItemAtIndex(index, pasteboardName), Messages::WebPasteboardProxy::GetPasteboardTypesByFidelityForItemAtIndex::Reply(types), 0);
}

void WebPlatformStrategies::writeToPasteboard(const PasteboardURL& url, const String& pasteboardName)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebPasteboardProxy::WriteURLToPasteboard(url, pasteboardName), 0);
}

void WebPlatformStrategies::writeToPasteboard(const WebCore::PasteboardWebContent& content, const String& pasteboardName)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebPasteboardProxy::WriteWebContentToPasteboard(content, pasteboardName), 0);
}

void WebPlatformStrategies::writeToPasteboard(const WebCore::PasteboardImage& image, const String& pasteboardName)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebPasteboardProxy::WriteImageToPasteboard(image, pasteboardName), 0);
}

void WebPlatformStrategies::writeToPasteboard(const String& pasteboardType, const String& text, const String& pasteboardName)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebPasteboardProxy::WriteStringToPasteboard(pasteboardType, text, pasteboardName), 0);
}

int WebPlatformStrategies::getPasteboardItemsCount(const String& pasteboardName)
{
    uint64_t itemsCount { 0 };
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::GetPasteboardItemsCount(pasteboardName), Messages::WebPasteboardProxy::GetPasteboardItemsCount::Reply(itemsCount), 0);
    return itemsCount;
}

Vector<PasteboardItemInfo> WebPlatformStrategies::allPasteboardItemInfo(const String& pasteboardName)
{
    Vector<PasteboardItemInfo> allInfo;
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::AllPasteboardItemInfo(pasteboardName), Messages::WebPasteboardProxy::AllPasteboardItemInfo::Reply(allInfo), 0);
    return allInfo;
}

PasteboardItemInfo WebPlatformStrategies::informationForItemAtIndex(int index, const String& pasteboardName)
{
    PasteboardItemInfo info;
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::InformationForItemAtIndex(index, pasteboardName), Messages::WebPasteboardProxy::InformationForItemAtIndex::Reply(info), 0);
    return info;
}

void WebPlatformStrategies::updateSupportedTypeIdentifiers(const Vector<String>& identifiers, const String& pasteboardName)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebPasteboardProxy::UpdateSupportedTypeIdentifiers(identifiers, pasteboardName), 0);
}

RefPtr<WebCore::SharedBuffer> WebPlatformStrategies::readBufferFromPasteboard(int index, const String& pasteboardType, const String& pasteboardName)
{
    SharedMemory::Handle handle;
    uint64_t size { 0 };
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::ReadBufferFromPasteboard(index, pasteboardType, pasteboardName), Messages::WebPasteboardProxy::ReadBufferFromPasteboard::Reply(handle, size), 0);
    if (handle.isNull())
        return nullptr;
    RefPtr<SharedMemory> sharedMemoryBuffer = SharedMemory::map(handle, SharedMemory::Protection::ReadOnly);
    return SharedBuffer::create(static_cast<unsigned char *>(sharedMemoryBuffer->data()), size);
}

URL WebPlatformStrategies::readURLFromPasteboard(int index, const String& pasteboardName, String& title)
{
    String urlString;
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::ReadURLFromPasteboard(index, pasteboardName), Messages::WebPasteboardProxy::ReadURLFromPasteboard::Reply(urlString, title), 0);
    return URL({ }, urlString);
}

String WebPlatformStrategies::readStringFromPasteboard(int index, const String& pasteboardType, const String& pasteboardName)
{
    String value;
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::ReadStringFromPasteboard(index, pasteboardType, pasteboardName), Messages::WebPasteboardProxy::ReadStringFromPasteboard::Reply(value), 0);
    return value;
}
#endif // PLATFORM(IOS_FAMILY)

#endif // PLATFORM(COCOA)

#if PLATFORM(GTK)
// PasteboardStrategy

void WebPlatformStrategies::writeToClipboard(const String& pasteboardName, const SelectionData& selection)
{
    WebSelectionData selectionData(selection);
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebPasteboardProxy::WriteToClipboard(pasteboardName, selectionData), 0);
}

Ref<SelectionData> WebPlatformStrategies::readFromClipboard(const String& pasteboardName)
{
    WebSelectionData selection;
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::ReadFromClipboard(pasteboardName), Messages::WebPasteboardProxy::ReadFromClipboard::Reply(selection), 0);
    return WTFMove(selection.selectionData);
}

#endif // PLATFORM(GTK)

#if USE(LIBWPE)
// PasteboardStrategy

void WebPlatformStrategies::getTypes(Vector<String>& types)
{
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::GetPasteboardTypes(), Messages::WebPasteboardProxy::GetPasteboardTypes::Reply(types), 0);
}

String WebPlatformStrategies::readStringFromPasteboard(int index, const String& pasteboardType)
{
    String value;
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::ReadStringFromPasteboard(index, pasteboardType), Messages::WebPasteboardProxy::ReadStringFromPasteboard::Reply(value), 0);
    return value;
}

void WebPlatformStrategies::writeToPasteboard(const WebCore::PasteboardWebContent& content)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebPasteboardProxy::WriteWebContentToPasteboard(content), 0);
}

void WebPlatformStrategies::writeToPasteboard(const String& pasteboardType, const String& text)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebPasteboardProxy::WriteStringToPasteboard(pasteboardType, text), 0);
}

#endif // USE(LIBWPE)

Vector<String> WebPlatformStrategies::typesSafeForDOMToReadAndWrite(const String& pasteboardName, const String& origin)
{
    Vector<String> types;
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::TypesSafeForDOMToReadAndWrite(pasteboardName, origin), Messages::WebPasteboardProxy::TypesSafeForDOMToReadAndWrite::Reply(types), 0);
    return types;
}

long WebPlatformStrategies::writeCustomData(const WebCore::PasteboardCustomData& data, const String& pasteboardName)
{
    uint64_t newChangeCount { 0 };
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::WriteCustomData(data, pasteboardName), Messages::WebPasteboardProxy::WriteCustomData::Reply(newChangeCount), 0);
    return newChangeCount;
}

} // namespace WebKit
