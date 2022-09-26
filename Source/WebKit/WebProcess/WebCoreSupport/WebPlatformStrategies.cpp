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
#include "SharedBufferReference.h"
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

    auto sendResult = WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::GetPasteboardTypes(pasteboardName, pageIdentifier(context)), 0);
    if (sendResult)
        std::tie(types) = sendResult.takeReply();
}

RefPtr<WebCore::SharedBuffer> WebPlatformStrategies::bufferForType(const String& pasteboardType, const String& pasteboardName, const PasteboardContext* context)
{
    // First check the overrides.
    Vector<uint8_t> overrideBuffer;
    if (WebPasteboardOverrides::sharedPasteboardOverrides().getDataForOverride(pasteboardName, pasteboardType, overrideBuffer))
        return SharedBuffer::create(WTFMove(overrideBuffer));

    // Fallback to messaging the UI process for native pasteboard content.
    auto sendResult = WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::GetPasteboardBufferForType(pasteboardName, pasteboardType, pageIdentifier(context)), 0);
    auto [buffer] = sendResult.takeReplyOr(nullptr);
    return buffer;
}

void WebPlatformStrategies::getPathnamesForType(Vector<String>& pathnames, const String& pasteboardType, const String& pasteboardName, const PasteboardContext* context)
{
    Vector<SandboxExtension::Handle> sandboxExtensionsHandleArray;
    auto sendResult = WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::GetPasteboardPathnamesForType(pasteboardName, pasteboardType, pageIdentifier(context)), 0);
    if (sendResult)
        std::tie(pathnames, sandboxExtensionsHandleArray) = sendResult.takeReply();
    ASSERT(pathnames.size() == sandboxExtensionsHandleArray.size());
    SandboxExtension::consumePermanently(sandboxExtensionsHandleArray);
}

String WebPlatformStrategies::stringForType(const String& pasteboardType, const String& pasteboardName, const PasteboardContext* context)
{
    auto sendResult = WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::GetPasteboardStringForType(pasteboardName, pasteboardType, pageIdentifier(context)), 0);
    auto [value] = sendResult.takeReplyOr(String { });
    return value;
}

Vector<String> WebPlatformStrategies::allStringsForType(const String& pasteboardType, const String& pasteboardName, const PasteboardContext* context)
{
    auto sendResult = WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::GetPasteboardStringsForType(pasteboardName, pasteboardType, pageIdentifier(context)), 0);
    auto [values] = sendResult.takeReplyOr(Vector<String> { });
    return values;
}

int64_t WebPlatformStrategies::changeCount(const String& pasteboardName, const PasteboardContext* context)
{
    WebProcess::singleton().waitForPendingPasteboardWritesToFinish(pasteboardName);
    auto sendResult = WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::GetPasteboardChangeCount(pasteboardName, pageIdentifier(context)), 0);
    auto [changeCount] = sendResult.takeReplyOr(0);
    return changeCount;
}

Color WebPlatformStrategies::color(const String& pasteboardName, const PasteboardContext* context)
{
    auto sendResult = WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::GetPasteboardColor(pasteboardName, pageIdentifier(context)), 0);
    auto [color] = sendResult.takeReplyOr(Color { });
    return color;
}

URL WebPlatformStrategies::url(const String& pasteboardName, const PasteboardContext* context)
{
    auto sendResult = WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::GetPasteboardURL(pasteboardName, pageIdentifier(context)), 0);
    auto [urlString] = sendResult.takeReplyOr(String { });
    return URL({ }, urlString);
}

int64_t WebPlatformStrategies::addTypes(const Vector<String>& pasteboardTypes, const String& pasteboardName, const PasteboardContext* context)
{
    auto sendResult = WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::AddPasteboardTypes(pasteboardName, pasteboardTypes, pageIdentifier(context)), 0);
    auto [newChangeCount] = sendResult.takeReplyOr(0);
    return newChangeCount;
}

int64_t WebPlatformStrategies::setTypes(const Vector<String>& pasteboardTypes, const String& pasteboardName, const PasteboardContext* context)
{
    auto sendResult = WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::SetPasteboardTypes(pasteboardName, pasteboardTypes, pageIdentifier(context)), 0);
    auto [newChangeCount] = sendResult.takeReplyOr(0);
    return newChangeCount;
}

int64_t WebPlatformStrategies::setBufferForType(SharedBuffer* buffer, const String& pasteboardType, const String& pasteboardName, const PasteboardContext* context)
{
    SharedMemory::Handle handle;
    // FIXME: Null check prevents crashing, but it is not great that we will have empty pasteboard content for this type,
    // because we've already set the types.
    auto sendResult = WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::SetPasteboardBufferForType(pasteboardName, pasteboardType, buffer ? RefPtr { buffer } : SharedBuffer::create(), pageIdentifier(context)), 0);
    auto [newChangeCount] = sendResult.takeReplyOr(0);
    return newChangeCount;
}

int64_t WebPlatformStrategies::setURL(const PasteboardURL& pasteboardURL, const String& pasteboardName, const PasteboardContext* context)
{
    auto sendResult = WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::SetPasteboardURL(pasteboardURL, pasteboardName, pageIdentifier(context)), 0);
    auto [newChangeCount] = sendResult.takeReplyOr(0);
    return newChangeCount;
}

int64_t WebPlatformStrategies::setColor(const Color& color, const String& pasteboardName, const PasteboardContext* context)
{
    auto sendResult = WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::SetPasteboardColor(pasteboardName, color, pageIdentifier(context)), 0);
    auto [newChangeCount] = sendResult.takeReplyOr(0);
    return newChangeCount;
}

int64_t WebPlatformStrategies::setStringForType(const String& string, const String& pasteboardType, const String& pasteboardName, const PasteboardContext* context)
{
    auto sendResult = WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::SetPasteboardStringForType(pasteboardName, pasteboardType, string, pageIdentifier(context)), 0);
    auto [newChangeCount] = sendResult.takeReplyOr(0);
    return newChangeCount;
}

int WebPlatformStrategies::getNumberOfFiles(const String& pasteboardName, const PasteboardContext* context)
{
    auto sendResult = WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::GetNumberOfFiles(pasteboardName, pageIdentifier(context)), 0);
    auto [numberOfFiles] = sendResult.takeReplyOr(0);
    return numberOfFiles;
}

bool WebPlatformStrategies::containsURLStringSuitableForLoading(const String& pasteboardName, const PasteboardContext* context)
{
    auto sendResult = WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::ContainsURLStringSuitableForLoading(pasteboardName, pageIdentifier(context)), 0);
    auto [result] = sendResult.takeReplyOr(false);
    return result;
}

String WebPlatformStrategies::urlStringSuitableForLoading(const String& pasteboardName, String& title, const PasteboardContext* context)
{
    String url;
    auto sendResult = WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::URLStringSuitableForLoading(pasteboardName, pageIdentifier(context)), 0);
    if (sendResult)
        std::tie(url, title) = sendResult.takeReply();
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
    auto sendResult = WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::GetTypes(pasteboardName), 0);
    auto [result] = sendResult.takeReplyOr(Vector<String> { });
    return result;
}

String WebPlatformStrategies::readTextFromClipboard(const String& pasteboardName)
{
    auto sendResult = WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::ReadText(pasteboardName), 0);
    auto [result] = sendResult.takeReplyOr(String { });
    return result;
}

Vector<String> WebPlatformStrategies::readFilePathsFromClipboard(const String& pasteboardName)
{
    auto sendResult = WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::ReadFilePaths(pasteboardName), 0);
    auto [result] = sendResult.takeReplyOr(Vector<String> { });
    return result;
}

RefPtr<SharedBuffer> WebPlatformStrategies::readBufferFromClipboard(const String& pasteboardName, const String& pasteboardType)
{
    auto sendResult = WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::ReadBuffer(pasteboardName, pasteboardType), 0);
    auto [buffer] = sendResult.takeReplyOr(nullptr);
    return buffer;
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
    auto sendResult = WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::GetPasteboardTypes(), 0);
    if (sendResult)
        std::tie(types) = sendResult.takeReply();
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
    auto sendResult = WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::TypesSafeForDOMToReadAndWrite(pasteboardName, origin, pageIdentifier(context)), 0);
    auto [types] = sendResult.takeReplyOr(Vector<String> { });
    return types;
}

int64_t WebPlatformStrategies::writeCustomData(const Vector<PasteboardCustomData>& data, const String& pasteboardName, const PasteboardContext* context)
{
    auto sendResult = WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::WriteCustomData(data, pasteboardName, pageIdentifier(context)), 0);
    auto [newChangeCount] = sendResult.takeReplyOr(0);
    return newChangeCount;
}

bool WebPlatformStrategies::containsStringSafeForDOMToReadForType(const String& type, const String& pasteboardName, const PasteboardContext* context)
{
    auto sendResult = WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::ContainsStringSafeForDOMToReadForType(type, pasteboardName, pageIdentifier(context)), 0);
    auto [result] = sendResult.takeReplyOr(false);
    return result;
}

int WebPlatformStrategies::getPasteboardItemsCount(const String& pasteboardName, const PasteboardContext* context)
{
    if (!WebPasteboardOverrides::sharedPasteboardOverrides().overriddenTypes(pasteboardName).isEmpty()) {
        // Override pasteboards currently only support single pasteboard items.
        return 1;
    }

    auto sendResult = WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::GetPasteboardItemsCount(pasteboardName, pageIdentifier(context)), 0);
    auto [itemsCount] = sendResult.takeReplyOr(0);
    return itemsCount;
}

std::optional<Vector<PasteboardItemInfo>> WebPlatformStrategies::allPasteboardItemInfo(const String& pasteboardName, int64_t changeCount, const PasteboardContext* context)
{
    if (auto info = WebPasteboardOverrides::sharedPasteboardOverrides().overriddenInfo(pasteboardName))
        return { { WTFMove(*info) } };

    auto sendResult = WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::AllPasteboardItemInfo(pasteboardName, changeCount, pageIdentifier(context)), 0);
    auto [allInfo] = sendResult.takeReplyOr(std::nullopt);
    return allInfo;
}

std::optional<PasteboardItemInfo> WebPlatformStrategies::informationForItemAtIndex(size_t index, const String& pasteboardName, int64_t changeCount, const PasteboardContext* context)
{
    if (auto info = WebPasteboardOverrides::sharedPasteboardOverrides().overriddenInfo(pasteboardName))
        return info;

    auto sendResult = WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::InformationForItemAtIndex(index, pasteboardName, changeCount, pageIdentifier(context)), 0);
    auto [info] = sendResult.takeReplyOr(std::nullopt);
    return info;
}

RefPtr<WebCore::SharedBuffer> WebPlatformStrategies::readBufferFromPasteboard(std::optional<size_t> index, const String& pasteboardType, const String& pasteboardName, const PasteboardContext* context)
{
    Vector<uint8_t> overrideBuffer;
    if (WebPasteboardOverrides::sharedPasteboardOverrides().getDataForOverride(pasteboardName, pasteboardType, overrideBuffer))
        return SharedBuffer::create(WTFMove(overrideBuffer));

    auto sendResult = WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::ReadBufferFromPasteboard(index, pasteboardType, pasteboardName, pageIdentifier(context)), 0);
    auto [buffer] = sendResult.takeReplyOr(nullptr);
    return buffer;
}

URL WebPlatformStrategies::readURLFromPasteboard(size_t index, const String& pasteboardName, String& title, const PasteboardContext* context)
{
    String urlString;
    auto sendResult = WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::ReadURLFromPasteboard(index, pasteboardName, pageIdentifier(context)), 0);
    if (sendResult)
        std::tie(urlString, title) = sendResult.takeReply();
    return URL({ }, urlString);
}

String WebPlatformStrategies::readStringFromPasteboard(size_t index, const String& pasteboardType, const String& pasteboardName, const PasteboardContext* context)
{
    auto sendResult = WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::ReadStringFromPasteboard(index, pasteboardType, pasteboardName, pageIdentifier(context)), 0);
    auto [value] = sendResult.takeReplyOr(String { });
    return value;
}

} // namespace WebKit
