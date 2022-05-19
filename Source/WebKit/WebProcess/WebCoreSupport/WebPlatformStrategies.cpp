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
#include "SharedBufferCopy.h"
#include "WebCoreArgumentCoders.h"
#include "WebErrors.h"
#include "WebFrame.h"
#include "WebFrameLoaderClient.h"
#include "WebLoaderStrategy.h"
#include "WebMediaStrategy.h"
#include "WebPage.h"
#include "WebPasteboardOverrides.h"
#include "WebPasteboardProxyMessages.h"
#include "WebProcess.h"
#include "WebProcessProxyMessages.h"
#include <WebCore/AudioDestination.h>
#include <WebCore/Color.h>
#include <WebCore/Document.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/Frame.h>
#include <WebCore/LoaderStrategy.h>
#include <WebCore/MediaStrategy.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/Page.h>
#include <WebCore/PageGroup.h>
#include <WebCore/PagePasteboardContext.h>
#include <WebCore/PasteboardItemInfo.h>
#include <WebCore/PlatformPasteboard.h>
#include <WebCore/ProgressTracker.h>
#include <WebCore/ResourceError.h>
#include <WebCore/SameSiteInfo.h>
#include <WebCore/StorageNamespace.h>
#include <WebCore/SubframeLoader.h>
#include <wtf/Atomics.h>
#include <wtf/URL.h>

#if PLATFORM(MAC)
#include "StringUtilities.h"
#endif

#if PLATFORM(GTK)
#include <WebCore/SelectionData.h>
#endif

namespace WebKit {
using namespace WebCore;

class RemoteAudioDestination;

void WebPlatformStrategies::initialize()
{
    static NeverDestroyed<WebPlatformStrategies> platformStrategies;
    setPlatformStrategies(&platformStrategies.get());
}

WebPlatformStrategies::WebPlatformStrategies()
{
}

LoaderStrategy* WebPlatformStrategies::createLoaderStrategy()
{
    return &WebProcess::singleton().webLoaderStrategy();
}

PasteboardStrategy* WebPlatformStrategies::createPasteboardStrategy()
{
    return this;
}

MediaStrategy* WebPlatformStrategies::createMediaStrategy()
{
    return new WebMediaStrategy;
}

BlobRegistry* WebPlatformStrategies::createBlobRegistry()
{
    return new BlobRegistryProxy;
}

static std::optional<PageIdentifier> pageIdentifier(const PasteboardContext* context)
{
    if (!is<PagePasteboardContext>(context))
        return std::nullopt;

    return downcast<PagePasteboardContext>(*context).pageID();
}

#if PLATFORM(COCOA)
// PasteboardStrategy

void WebPlatformStrategies::getTypes(Vector<String>& types, const String& pasteboardName, const PasteboardContext* context)
{
    // First check the overrides.
    // The purpose of the overrides is to avoid messaging back to the UI process.
    // Therefore, if there are any overridden types, we return just those.
    types = WebPasteboardOverrides::sharedPasteboardOverrides().overriddenTypes(pasteboardName);
    if (!types.isEmpty())
        return;

    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::GetPasteboardTypes(pasteboardName, pageIdentifier(context)), Messages::WebPasteboardProxy::GetPasteboardTypes::Reply(types), 0);
}

RefPtr<WebCore::SharedBuffer> WebPlatformStrategies::bufferForType(const String& pasteboardType, const String& pasteboardName, const PasteboardContext* context)
{
    // First check the overrides.
    Vector<uint8_t> overrideBuffer;
    if (WebPasteboardOverrides::sharedPasteboardOverrides().getDataForOverride(pasteboardName, pasteboardType, overrideBuffer))
        return SharedBuffer::create(WTFMove(overrideBuffer));

    // Fallback to messaging the UI process for native pasteboard content.
    SharedMemory::IPCHandle ipcHandle;
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::GetPasteboardBufferForType(pasteboardName, pasteboardType, pageIdentifier(context)), Messages::WebPasteboardProxy::GetPasteboardBufferForType::Reply(ipcHandle), 0);
    if (ipcHandle.handle.isNull())
        return nullptr;
    RefPtr<SharedMemory> sharedMemoryBuffer = SharedMemory::map(ipcHandle.handle, SharedMemory::Protection::ReadOnly);
    if (!sharedMemoryBuffer)
        return nullptr;
    return sharedMemoryBuffer->createSharedBuffer(ipcHandle.dataSize);
}

void WebPlatformStrategies::getPathnamesForType(Vector<String>& pathnames, const String& pasteboardType, const String& pasteboardName, const PasteboardContext* context)
{
    Vector<SandboxExtension::Handle> sandboxExtensionsHandleArray;
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::GetPasteboardPathnamesForType(pasteboardName, pasteboardType, pageIdentifier(context)),
        Messages::WebPasteboardProxy::GetPasteboardPathnamesForType::Reply(pathnames, sandboxExtensionsHandleArray), 0);
    ASSERT(pathnames.size() == sandboxExtensionsHandleArray.size());
    SandboxExtension::consumePermanently(sandboxExtensionsHandleArray);
}

String WebPlatformStrategies::stringForType(const String& pasteboardType, const String& pasteboardName, const PasteboardContext* context)
{
    String value;
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::GetPasteboardStringForType(pasteboardName, pasteboardType, pageIdentifier(context)), Messages::WebPasteboardProxy::GetPasteboardStringForType::Reply(value), 0);
    return value;
}

Vector<String> WebPlatformStrategies::allStringsForType(const String& pasteboardType, const String& pasteboardName, const PasteboardContext* context)
{
    Vector<String> values;
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::GetPasteboardStringsForType(pasteboardName, pasteboardType, pageIdentifier(context)), Messages::WebPasteboardProxy::GetPasteboardStringsForType::Reply(values), 0);
    return values;
}

int64_t WebPlatformStrategies::changeCount(const String& pasteboardName, const PasteboardContext* context)
{
    int64_t changeCount { 0 };
    WebProcess::singleton().waitForPendingPasteboardWritesToFinish(pasteboardName);
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::GetPasteboardChangeCount(pasteboardName, pageIdentifier(context)), Messages::WebPasteboardProxy::GetPasteboardChangeCount::Reply(changeCount), 0);
    return changeCount;
}

Color WebPlatformStrategies::color(const String& pasteboardName, const PasteboardContext* context)
{
    Color color;
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::GetPasteboardColor(pasteboardName, pageIdentifier(context)), Messages::WebPasteboardProxy::GetPasteboardColor::Reply(color), 0);
    return color;
}

URL WebPlatformStrategies::url(const String& pasteboardName, const PasteboardContext* context)
{
    String urlString;
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::GetPasteboardURL(pasteboardName, pageIdentifier(context)), Messages::WebPasteboardProxy::GetPasteboardURL::Reply(urlString), 0);
    return URL({ }, urlString);
}

int64_t WebPlatformStrategies::addTypes(const Vector<String>& pasteboardTypes, const String& pasteboardName, const PasteboardContext* context)
{
    int64_t newChangeCount { 0 };
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::AddPasteboardTypes(pasteboardName, pasteboardTypes, pageIdentifier(context)), Messages::WebPasteboardProxy::AddPasteboardTypes::Reply(newChangeCount), 0);
    return newChangeCount;
}

int64_t WebPlatformStrategies::setTypes(const Vector<String>& pasteboardTypes, const String& pasteboardName, const PasteboardContext* context)
{
    int64_t newChangeCount { 0 };
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::SetPasteboardTypes(pasteboardName, pasteboardTypes, pageIdentifier(context)), Messages::WebPasteboardProxy::SetPasteboardTypes::Reply(newChangeCount), 0);
    return newChangeCount;
}

int64_t WebPlatformStrategies::setBufferForType(SharedBuffer* buffer, const String& pasteboardType, const String& pasteboardName, const PasteboardContext* context)
{
    SharedMemory::Handle handle;
    if (buffer && buffer->size()) {
        auto sharedMemoryBuffer = SharedMemory::copyBuffer(*buffer);
        // FIXME: Null check prevents crashing, but it is not great that we will have empty pasteboard content for this type,
        // because we've already set the types.
        if (sharedMemoryBuffer)
            sharedMemoryBuffer->createHandle(handle, SharedMemory::Protection::ReadOnly);
    }
    int64_t newChangeCount { 0 };
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::SetPasteboardBufferForType(pasteboardName, pasteboardType, SharedMemory::IPCHandle { WTFMove(handle), buffer ? buffer->size() : 0 }, pageIdentifier(context)), Messages::WebPasteboardProxy::SetPasteboardBufferForType::Reply(newChangeCount), 0);
    return newChangeCount;
}

int64_t WebPlatformStrategies::setURL(const PasteboardURL& pasteboardURL, const String& pasteboardName, const PasteboardContext* context)
{
    int64_t newChangeCount;
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::SetPasteboardURL(pasteboardURL, pasteboardName, pageIdentifier(context)), Messages::WebPasteboardProxy::SetPasteboardURL::Reply(newChangeCount), 0);
    return newChangeCount;
}

int64_t WebPlatformStrategies::setColor(const Color& color, const String& pasteboardName, const PasteboardContext* context)
{
    int64_t newChangeCount { 0 };
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::SetPasteboardColor(pasteboardName, color, pageIdentifier(context)), Messages::WebPasteboardProxy::SetPasteboardColor::Reply(newChangeCount), 0);
    return newChangeCount;
}

int64_t WebPlatformStrategies::setStringForType(const String& string, const String& pasteboardType, const String& pasteboardName, const PasteboardContext* context)
{
    int64_t newChangeCount { 0 };
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::SetPasteboardStringForType(pasteboardName, pasteboardType, string, pageIdentifier(context)), Messages::WebPasteboardProxy::SetPasteboardStringForType::Reply(newChangeCount), 0);
    return newChangeCount;
}

int WebPlatformStrategies::getNumberOfFiles(const String& pasteboardName, const PasteboardContext* context)
{
    uint64_t numberOfFiles { 0 };
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::GetNumberOfFiles(pasteboardName, pageIdentifier(context)), Messages::WebPasteboardProxy::GetNumberOfFiles::Reply(numberOfFiles), 0);
    return numberOfFiles;
}

bool WebPlatformStrategies::containsURLStringSuitableForLoading(const String& pasteboardName, const PasteboardContext* context)
{
    bool result;
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::ContainsURLStringSuitableForLoading(pasteboardName, pageIdentifier(context)), Messages::WebPasteboardProxy::ContainsURLStringSuitableForLoading::Reply(result), 0);
    return result;
}

String WebPlatformStrategies::urlStringSuitableForLoading(const String& pasteboardName, String& title, const PasteboardContext* context)
{
    String url;
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::URLStringSuitableForLoading(pasteboardName, pageIdentifier(context)), Messages::WebPasteboardProxy::URLStringSuitableForLoading::Reply(url, title), 0);
    return url;
}

#if PLATFORM(IOS_FAMILY)

void WebPlatformStrategies::writeToPasteboard(const PasteboardURL& url, const String& pasteboardName, const PasteboardContext* context)
{
    WebProcess::singleton().willWriteToPasteboardAsynchronously(pasteboardName);
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebPasteboardProxy::WriteURLToPasteboard(url, pasteboardName, pageIdentifier(context)), 0);
}

void WebPlatformStrategies::writeToPasteboard(const WebCore::PasteboardWebContent& content, const String& pasteboardName, const PasteboardContext* context)
{
    WebProcess::singleton().willWriteToPasteboardAsynchronously(pasteboardName);
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebPasteboardProxy::WriteWebContentToPasteboard(content, pasteboardName, pageIdentifier(context)), 0);
}

void WebPlatformStrategies::writeToPasteboard(const WebCore::PasteboardImage& image, const String& pasteboardName, const PasteboardContext* context)
{
    WebProcess::singleton().willWriteToPasteboardAsynchronously(pasteboardName);
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebPasteboardProxy::WriteImageToPasteboard(image, pasteboardName, pageIdentifier(context)), 0);
}

void WebPlatformStrategies::writeToPasteboard(const String& pasteboardType, const String& text, const String& pasteboardName, const PasteboardContext* context)
{
    WebProcess::singleton().willWriteToPasteboardAsynchronously(pasteboardName);
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebPasteboardProxy::WriteStringToPasteboard(pasteboardType, text, pasteboardName, pageIdentifier(context)), 0);
}

void WebPlatformStrategies::updateSupportedTypeIdentifiers(const Vector<String>& identifiers, const String& pasteboardName, const PasteboardContext* context)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebPasteboardProxy::UpdateSupportedTypeIdentifiers(identifiers, pasteboardName, pageIdentifier(context)), 0);
}
#endif // PLATFORM(IOS_FAMILY)

#endif // PLATFORM(COCOA)

#if PLATFORM(GTK)
// PasteboardStrategy

Vector<String> WebPlatformStrategies::types(const String& pasteboardName)
{
    Vector<String> result;
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::GetTypes(pasteboardName), Messages::WebPasteboardProxy::GetTypes::Reply(result), 0);
    return result;
}

String WebPlatformStrategies::readTextFromClipboard(const String& pasteboardName)
{
    String result;
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::ReadText(pasteboardName), Messages::WebPasteboardProxy::ReadText::Reply(result), 0);
    return result;
}

Vector<String> WebPlatformStrategies::readFilePathsFromClipboard(const String& pasteboardName)
{
    Vector<String> result;
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::ReadFilePaths(pasteboardName), Messages::WebPasteboardProxy::ReadFilePaths::Reply(result), 0);
    return result;
}

RefPtr<SharedBuffer> WebPlatformStrategies::readBufferFromClipboard(const String& pasteboardName, const String& pasteboardType)
{

    IPC::SharedBufferCopy data;
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::ReadBuffer(pasteboardName, pasteboardType), Messages::WebPasteboardProxy::ReadBuffer::Reply(data), 0);
    return data.buffer();
}

void WebPlatformStrategies::writeToClipboard(const String& pasteboardName, SelectionData&& selectionData)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebPasteboardProxy::WriteToClipboard(pasteboardName, WTFMove(selectionData)), 0);
}

void WebPlatformStrategies::clearClipboard(const String& pasteboardName)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebPasteboardProxy::ClearClipboard(pasteboardName), 0);
}

#endif // PLATFORM(GTK)

#if USE(LIBWPE)
// PasteboardStrategy

void WebPlatformStrategies::getTypes(Vector<String>& types)
{
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::GetPasteboardTypes(), Messages::WebPasteboardProxy::GetPasteboardTypes::Reply(types), 0);
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

Vector<String> WebPlatformStrategies::typesSafeForDOMToReadAndWrite(const String& pasteboardName, const String& origin, const PasteboardContext* context)
{
    Vector<String> types;
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::TypesSafeForDOMToReadAndWrite(pasteboardName, origin, pageIdentifier(context)), Messages::WebPasteboardProxy::TypesSafeForDOMToReadAndWrite::Reply(types), 0);
    return types;
}

int64_t WebPlatformStrategies::writeCustomData(const Vector<PasteboardCustomData>& data, const String& pasteboardName, const PasteboardContext* context)
{
    int64_t newChangeCount { 0 };
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::WriteCustomData(data, pasteboardName, pageIdentifier(context)), Messages::WebPasteboardProxy::WriteCustomData::Reply(newChangeCount), 0);
    return newChangeCount;
}

bool WebPlatformStrategies::containsStringSafeForDOMToReadForType(const String& type, const String& pasteboardName, const PasteboardContext* context)
{
    bool result = false;
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::ContainsStringSafeForDOMToReadForType(type, pasteboardName, pageIdentifier(context)), Messages::WebPasteboardProxy::ContainsStringSafeForDOMToReadForType::Reply(result), 0);
    return result;
}

int WebPlatformStrategies::getPasteboardItemsCount(const String& pasteboardName, const PasteboardContext* context)
{
    if (!WebPasteboardOverrides::sharedPasteboardOverrides().overriddenTypes(pasteboardName).isEmpty()) {
        // Override pasteboards currently only support single pasteboard items.
        return 1;
    }

    uint64_t itemsCount { 0 };
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::GetPasteboardItemsCount(pasteboardName, pageIdentifier(context)), Messages::WebPasteboardProxy::GetPasteboardItemsCount::Reply(itemsCount), 0);
    return itemsCount;
}

std::optional<Vector<PasteboardItemInfo>> WebPlatformStrategies::allPasteboardItemInfo(const String& pasteboardName, int64_t changeCount, const PasteboardContext* context)
{
    if (auto info = WebPasteboardOverrides::sharedPasteboardOverrides().overriddenInfo(pasteboardName))
        return { { WTFMove(*info) } };

    std::optional<Vector<PasteboardItemInfo>> allInfo;
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::AllPasteboardItemInfo(pasteboardName, changeCount, pageIdentifier(context)), Messages::WebPasteboardProxy::AllPasteboardItemInfo::Reply(allInfo), 0);
    return allInfo;
}

std::optional<PasteboardItemInfo> WebPlatformStrategies::informationForItemAtIndex(size_t index, const String& pasteboardName, int64_t changeCount, const PasteboardContext* context)
{
    if (auto info = WebPasteboardOverrides::sharedPasteboardOverrides().overriddenInfo(pasteboardName))
        return info;

    std::optional<PasteboardItemInfo> info;
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::InformationForItemAtIndex(index, pasteboardName, changeCount, pageIdentifier(context)), Messages::WebPasteboardProxy::InformationForItemAtIndex::Reply(info), 0);
    return info;
}

RefPtr<WebCore::SharedBuffer> WebPlatformStrategies::readBufferFromPasteboard(std::optional<size_t> index, const String& pasteboardType, const String& pasteboardName, const PasteboardContext* context)
{
    Vector<uint8_t> overrideBuffer;
    if (WebPasteboardOverrides::sharedPasteboardOverrides().getDataForOverride(pasteboardName, pasteboardType, overrideBuffer))
        return SharedBuffer::create(WTFMove(overrideBuffer));

    SharedMemory::IPCHandle ipcHandle;
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::ReadBufferFromPasteboard(index, pasteboardType, pasteboardName, pageIdentifier(context)), Messages::WebPasteboardProxy::ReadBufferFromPasteboard::Reply(ipcHandle), 0);
    if (ipcHandle.handle.isNull())
        return nullptr;

    RefPtr<SharedMemory> sharedMemoryBuffer = SharedMemory::map(ipcHandle.handle, SharedMemory::Protection::ReadOnly);
    if (!sharedMemoryBuffer)
        return nullptr;
    return sharedMemoryBuffer->createSharedBuffer(ipcHandle.dataSize);
}

URL WebPlatformStrategies::readURLFromPasteboard(size_t index, const String& pasteboardName, String& title, const PasteboardContext* context)
{
    String urlString;
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::ReadURLFromPasteboard(index, pasteboardName, pageIdentifier(context)), Messages::WebPasteboardProxy::ReadURLFromPasteboard::Reply(urlString, title), 0);
    return URL({ }, urlString);
}

String WebPlatformStrategies::readStringFromPasteboard(size_t index, const String& pasteboardType, const String& pasteboardName, const PasteboardContext* context)
{
    String value;
    WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::ReadStringFromPasteboard(index, pasteboardType, pasteboardName, pageIdentifier(context)), Messages::WebPasteboardProxy::ReadStringFromPasteboard::Reply(value), 0);
    return value;
}

} // namespace WebKit
