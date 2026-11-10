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
#include "HangDetectionDisabler.h"
#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkProcessConnection.h"
#include "NetworkResourceLoadParameters.h"
#include "SharedBufferReference.h"
#include "WebErrors.h"
#include "WebFrame.h"
#include "WebLoaderStrategy.h"
#include "WebLocalFrameLoaderClient.h"
#include "WebMediaStrategy.h"
#include "WebPage.h"
#include "WebPasteboardOverrides.h"
#include "WebPasteboardProxyMessages.h"
#include "WebProcess.h"
#include "WebProcessProxyMessages.h"
#include "WriteWebArchiveToPasteBoardResult.h"
#include <WebCore/AudioDestination.h>
#include <WebCore/Color.h>
#include <WebCore/Document.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/ExceptionData.h>
#include <WebCore/ExceptionOr.h>
#include <WebCore/LoaderStrategy.h>
#include <WebCore/LocalFrameInlines.h>
#include <WebCore/MediaStrategy.h>
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

#if PLATFORM(GTK)
#include <WebCore/SelectionData.h>
#endif

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebPlatformStrategies);

class RemoteAudioDestination;

void WebPlatformStrategies::initialize()
{
    static NeverDestroyed<WebPlatformStrategies> platformStrategies;
    setPlatformStrategies(&platformStrategies.get());
}

WebPlatformStrategies::WebPlatformStrategies() = default;

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

#if ENABLE(DECLARATIVE_WEB_PUSH)
PushStrategy* WebPlatformStrategies::createPushStrategy()
{
    return this;
}
#endif

static std::optional<WebPageProxyIdentifier> pageIdentifier(const PasteboardContext* context)
{
    auto* pageContext = dynamicDowncast<PagePasteboardContext>(context);
    if (!pageContext)
        return std::nullopt;

    auto pageID = pageContext->pageID();
    if (!pageID)
        return std::nullopt;

    RefPtr webPage = WebProcess::singleton().webPage(*pageID);
    if (!webPage)
        return std::nullopt;

    return webPage->webPageProxyIdentifier();
}

#if PLATFORM(COCOA)
// PasteboardStrategy

void WebPlatformStrategies::getTypes(Vector<RefPtr<StringImpl>>& types, StringImpl* pasteboardName, const PasteboardContext* context)
{
    // First check the overrides.
    // The purpose of the overrides is to avoid messaging back to the UI process.
    // Therefore, if there are any overridden types, we return just those.
    types = WebPasteboardOverrides::sharedPasteboardOverrides().overriddenTypes(pasteboardName);
    if (!types.isEmpty())
        return;

    auto sendResult = protect(WebProcess::singleton().parentProcessConnection())->sendSync(Messages::WebPasteboardProxy::GetPasteboardTypes(pasteboardName, pageIdentifier(context)), 0);
    if (sendResult.succeeded())
        std::tie(types) = sendResult.takeReply();
}

RefPtr<WebCore::SharedBuffer> WebPlatformStrategies::bufferForType(StringImpl* pasteboardType, StringImpl* pasteboardName, const PasteboardContext* context)
{
    // First check the overrides.
    Vector<uint8_t> overrideBuffer;
    if (WebPasteboardOverrides::sharedPasteboardOverrides().getDataForOverride(pasteboardName, pasteboardType, overrideBuffer))
        return SharedBuffer::create(WTF::move(overrideBuffer));

    // Fallback to messaging the UI process for native pasteboard content.
    auto sendResult = protect(WebProcess::singleton().parentProcessConnection())->sendSync(Messages::WebPasteboardProxy::GetPasteboardBufferForType(pasteboardName, pasteboardType, pageIdentifier(context)), 0);
    auto [pasteboardBuffer] = sendResult.takeReplyOr(WebCore::PasteboardBuffer { });

    return Pasteboard::bufferConvertedToPasteboardType(pasteboardBuffer, pasteboardType);
}

void WebPlatformStrategies::getPathnamesForType(Vector<RefPtr<StringImpl>>& pathnames, StringImpl* pasteboardType, StringImpl* pasteboardName, const PasteboardContext* context)
{
    Vector<SandboxExtension::Handle> sandboxExtensionsHandleArray;
    auto sendResult = protect(WebProcess::singleton().parentProcessConnection())->sendSync(Messages::WebPasteboardProxy::GetPasteboardPathnamesForType(pasteboardName, pasteboardType, pageIdentifier(context)), 0);
    if (sendResult.succeeded())
        std::tie(pathnames, sandboxExtensionsHandleArray) = sendResult.takeReply();
    ASSERT(pathnames.size() == sandboxExtensionsHandleArray.size());
    SandboxExtension::consumePermanently(sandboxExtensionsHandleArray);
}

RefPtr<StringImpl> WebPlatformStrategies::stringForType(StringImpl* pasteboardType, StringImpl* pasteboardName, const PasteboardContext* context)
{
    auto sendResult = protect(WebProcess::singleton().parentProcessConnection())->sendSync(Messages::WebPasteboardProxy::GetPasteboardStringForType(pasteboardName, pasteboardType, pageIdentifier(context)), 0);
    auto [value] = sendResult.takeReplyOr(nullString());
    return value;
}

Vector<RefPtr<StringImpl>> WebPlatformStrategies::allStringsForType(StringImpl* pasteboardType, StringImpl* pasteboardName, const PasteboardContext* context)
{
    auto sendResult = protect(WebProcess::singleton().parentProcessConnection())->sendSync(Messages::WebPasteboardProxy::GetPasteboardStringsForType(pasteboardName, pasteboardType, pageIdentifier(context)), 0);
    auto [values] = sendResult.takeReplyOr(Vector<RefPtr<StringImpl>> { });
    return values;
}

int64_t WebPlatformStrategies::changeCount(StringImpl* pasteboardName, const PasteboardContext* context)
{
    WebProcess::singleton().waitForPendingPasteboardWritesToFinish(pasteboardName);
    auto sendResult = protect(WebProcess::singleton().parentProcessConnection())->sendSync(Messages::WebPasteboardProxy::GetPasteboardChangeCount(pasteboardName, pageIdentifier(context)), 0);
    auto [changeCount] = sendResult.takeReplyOr(0);
    return changeCount;
}

Color WebPlatformStrategies::color(StringImpl* pasteboardName, const PasteboardContext* context)
{
    auto sendResult = protect(WebProcess::singleton().parentProcessConnection())->sendSync(Messages::WebPasteboardProxy::GetPasteboardColor(pasteboardName, pageIdentifier(context)), 0);
    auto [color] = sendResult.takeReplyOr(Color { });
    return color;
}

URL WebPlatformStrategies::url(StringImpl* pasteboardName, const PasteboardContext* context)
{
    auto sendResult = protect(WebProcess::singleton().parentProcessConnection())->sendSync(Messages::WebPasteboardProxy::GetPasteboardURL(pasteboardName, pageIdentifier(context)), 0);
    auto [urlString] = sendResult.takeReplyOr(nullString());
    return URL({ }, urlString);
}

int64_t WebPlatformStrategies::addTypes(const Vector<RefPtr<StringImpl>>& pasteboardTypes, StringImpl* pasteboardName, const PasteboardContext* context)
{
    auto sendResult = protect(WebProcess::singleton().parentProcessConnection())->sendSync(Messages::WebPasteboardProxy::AddPasteboardTypes(pasteboardName, pasteboardTypes, pageIdentifier(context)), 0);
    auto [newChangeCount] = sendResult.takeReplyOr(0);
    return newChangeCount;
}

int64_t WebPlatformStrategies::setTypes(const Vector<RefPtr<StringImpl>>& pasteboardTypes, StringImpl* pasteboardName, const PasteboardContext* context)
{
    auto sendResult = protect(WebProcess::singleton().parentProcessConnection())->sendSync(Messages::WebPasteboardProxy::SetPasteboardTypes(pasteboardName, pasteboardTypes, pageIdentifier(context)), 0);
    auto [newChangeCount] = sendResult.takeReplyOr(0);
    return newChangeCount;
}

int64_t WebPlatformStrategies::setBufferForType(SharedBuffer* buffer, StringImpl* pasteboardType, StringImpl* pasteboardName, const PasteboardContext* context)
{
    // FIXME: Null check prevents crashing, but it is not great that we will have empty pasteboard content for this type,
    // because we've already set the types.
    auto sendResult = protect(WebProcess::singleton().parentProcessConnection())->sendSync(Messages::WebPasteboardProxy::SetPasteboardBufferForType(pasteboardName, pasteboardType, buffer ? RefPtr { buffer } : SharedBuffer::create(), pageIdentifier(context)), 0);
    auto [newChangeCount] = sendResult.takeReplyOr(0);
    return newChangeCount;
}

int64_t WebPlatformStrategies::setURL(const PasteboardURL& pasteboardURL, StringImpl* pasteboardName, const PasteboardContext* context)
{
    auto sendResult = protect(WebProcess::singleton().parentProcessConnection())->sendSync(Messages::WebPasteboardProxy::SetPasteboardURL(pasteboardURL, pasteboardName, pageIdentifier(context)), 0);
    auto [newChangeCount] = sendResult.takeReplyOr(0);
    return newChangeCount;
}

int64_t WebPlatformStrategies::setColor(const Color& color, StringImpl* pasteboardName, const PasteboardContext* context)
{
    auto sendResult = protect(WebProcess::singleton().parentProcessConnection())->sendSync(Messages::WebPasteboardProxy::SetPasteboardColor(pasteboardName, color, pageIdentifier(context)), 0);
    auto [newChangeCount] = sendResult.takeReplyOr(0);
    return newChangeCount;
}

int64_t WebPlatformStrategies::setStringForType(StringImpl* string, StringImpl* pasteboardType, StringImpl* pasteboardName, const PasteboardContext* context)
{
    auto sendResult = protect(WebProcess::singleton().parentProcessConnection())->sendSync(Messages::WebPasteboardProxy::SetPasteboardStringForType(pasteboardName, pasteboardType, string, pageIdentifier(context)), 0);
    auto [newChangeCount] = sendResult.takeReplyOr(0);
    return newChangeCount;
}

static void collectFrameWebArchives(WebCore::FrameIdentifier frameIdentifier, HashMap<FrameIdentifier, Ref<WebCore::LegacyWebArchive>>& archives, Vector<WebCore::FrameIdentifier>& remoteFrameIdentifiers)
{
    RefPtr webFrame = WebFrame::webFrame(frameIdentifier);
    if (!webFrame)
        return;

    RefPtr rootFrame = webFrame->coreFrame();
    if (!rootFrame)
        return;

    for (RefPtr frame = rootFrame; frame; frame = frame->tree().traverseNext(rootFrame.get())) {
        RefPtr localFrame = dynamicDowncast<LocalFrame>(frame);
        if (!localFrame) {
            remoteFrameIdentifiers.append(frame->frameID());
            continue;
        }

        RefPtr document = localFrame->document();
        if (!document)
            continue;

        WebCore::LegacyWebArchive::ArchiveOptions options {
            LegacyWebArchive::ShouldSaveScriptsFromMemoryCache::Yes,
            LegacyWebArchive::ShouldArchiveSubframes::No
        };
        if (RefPtr archive = WebCore::LegacyWebArchive::create(*document, WTF::move(options))) {
            auto result = archives.add(localFrame->frameID(), archive.releaseNonNull());
            RELEASE_ASSERT(result.isNewEntry);
        }
    }
}

int64_t WebPlatformStrategies::writeWebArchive(WebCore::LegacyWebArchive& webArchive, StringImpl* pasteboardName)
{
    auto frameIdentifier = webArchive.frameIdentifier();
    if (!frameIdentifier)
        return 0;

    HashMap<WebCore::FrameIdentifier, Ref<WebCore::LegacyWebArchive>> localFrameWebArchives;
    Vector<WebCore::FrameIdentifier> remoteFrameIdentifiers;
    localFrameWebArchives.add(*frameIdentifier, webArchive);
    for (auto identifier : webArchive.subframeIdentifiers())
        collectFrameWebArchives(identifier, localFrameWebArchives, remoteFrameIdentifiers);

    auto sendResult = protect(WebProcess::singleton().parentProcessConnection())->sendSync(Messages::WebPasteboardProxy::WriteWebArchiveToPasteBoard(pasteboardName, *frameIdentifier, WTF::move(localFrameWebArchives), WTF::move(remoteFrameIdentifiers)), 0);
    auto [result, newChangeCount] = sendResult.takeReplyOr(WriteWebArchiveToPasteBoardResult::FailureOther, 0);
    return newChangeCount;
}

int WebPlatformStrategies::getNumberOfFiles(StringImpl* pasteboardName, const PasteboardContext* context)
{
    auto sendResult = protect(WebProcess::singleton().parentProcessConnection())->sendSync(Messages::WebPasteboardProxy::GetNumberOfFiles(pasteboardName, pageIdentifier(context)), 0);
    auto [numberOfFiles] = sendResult.takeReplyOr(0);
    return numberOfFiles;
}

bool WebPlatformStrategies::containsURLStringSuitableForLoading(StringImpl* pasteboardName, const PasteboardContext* context)
{
    auto sendResult = protect(WebProcess::singleton().parentProcessConnection())->sendSync(Messages::WebPasteboardProxy::ContainsURLStringSuitableForLoading(pasteboardName, pageIdentifier(context)), 0);
    auto [result] = sendResult.takeReplyOr(false);
    return result;
}

RefPtr<StringImpl> WebPlatformStrategies::urlStringSuitableForLoading(StringImpl* pasteboardName, RefPtr<StringImpl>& title, const PasteboardContext* context)
{
    RefPtr<StringImpl> url;
    auto sendResult = protect(WebProcess::singleton().parentProcessConnection())->sendSync(Messages::WebPasteboardProxy::URLStringSuitableForLoading(pasteboardName, pageIdentifier(context)), 0);
    if (sendResult.succeeded())
        std::tie(url, title) = sendResult.takeReply();
    return url;
}

#if PLATFORM(IOS_FAMILY)

void WebPlatformStrategies::writeToPasteboard(const PasteboardURL& url, StringImpl* pasteboardName, const PasteboardContext* context)
{
    WebProcess::singleton().willWriteToPasteboardAsynchronously(pasteboardName);
    protect(WebProcess::singleton().parentProcessConnection())->send(Messages::WebPasteboardProxy::WriteURLToPasteboard(url, pasteboardName, pageIdentifier(context)), 0);
}

static std::optional<WebCore::PasteboardWebContent> updateContentForWebArchive(const WebCore::PasteboardWebContent& content)
{
    RefPtr webArchive = content.webArchive;
    if (!webArchive)
        return std::nullopt;

    auto updatedContent = content;
    auto frameIdentifier = webArchive->frameIdentifier();
    if (!frameIdentifier) {
        updatedContent.webArchive = nullptr;
        return updatedContent;
    }

    auto subFrameIdentifiers = webArchive->subframeIdentifiers();
    if (subFrameIdentifiers.isEmpty()) {
        updatedContent.webArchive = nullptr;
        return updatedContent;
    }

    updatedContent.localFrameArchives.add(*frameIdentifier, *webArchive);
    for (auto identifier : subFrameIdentifiers)
        collectFrameWebArchives(identifier, updatedContent.localFrameArchives, updatedContent.remoteFrameIdentifiers);

    return updatedContent;
}

void WebPlatformStrategies::writeToPasteboard(const WebCore::PasteboardWebContent& content, StringImpl* pasteboardName, const PasteboardContext* context)
{
    WebProcess::singleton().willWriteToPasteboardAsynchronously(pasteboardName);
    if (auto updatedContent = updateContentForWebArchive(content)) {
        protect(WebProcess::singleton().parentProcessConnection())->send(Messages::WebPasteboardProxy::WriteWebContentToPasteboard(*updatedContent, pasteboardName, pageIdentifier(context)), 0);
        return;
    }
    protect(WebProcess::singleton().parentProcessConnection())->send(Messages::WebPasteboardProxy::WriteWebContentToPasteboard(content, pasteboardName, pageIdentifier(context)), 0);
}

void WebPlatformStrategies::writeToPasteboard(const WebCore::PasteboardImage& image, StringImpl* pasteboardName, const PasteboardContext* context)
{
    WebProcess::singleton().willWriteToPasteboardAsynchronously(pasteboardName);
    protect(WebProcess::singleton().parentProcessConnection())->send(Messages::WebPasteboardProxy::WriteImageToPasteboard(image, pasteboardName, pageIdentifier(context)), 0);
}

void WebPlatformStrategies::writeToPasteboard(StringImpl* pasteboardType, StringImpl* text, StringImpl* pasteboardName, const PasteboardContext* context)
{
    WebProcess::singleton().willWriteToPasteboardAsynchronously(pasteboardName);
    protect(WebProcess::singleton().parentProcessConnection())->send(Messages::WebPasteboardProxy::WriteStringToPasteboard(pasteboardType, text, pasteboardName, pageIdentifier(context)), 0);
}

void WebPlatformStrategies::updateSupportedTypeIdentifiers(const Vector<RefPtr<StringImpl>>& identifiers, StringImpl* pasteboardName, const PasteboardContext* context)
{
    protect(WebProcess::singleton().parentProcessConnection())->send(Messages::WebPasteboardProxy::UpdateSupportedTypeIdentifiers(identifiers, pasteboardName, pageIdentifier(context)), 0);
}
#endif // PLATFORM(IOS_FAMILY)

#endif // PLATFORM(COCOA)

#if PLATFORM(GTK) || PLATFORM(WPE)
// PasteboardStrategy

Vector<RefPtr<StringImpl>> WebPlatformStrategies::types(StringImpl* pasteboardName)
{
    auto sendResult = protect(WebProcess::singleton().parentProcessConnection())->sendSync(Messages::WebPasteboardProxy::GetTypes(pasteboardName), 0);
    auto [result] = sendResult.takeReplyOr(Vector<RefPtr<StringImpl>> { });
    return result;
}

RefPtr<StringImpl> WebPlatformStrategies::readTextFromClipboard(StringImpl* pasteboardName, StringImpl* pasteboardType)
{
    auto sendResult = protect(WebProcess::singleton().parentProcessConnection())->sendSync(Messages::WebPasteboardProxy::ReadText(pasteboardName, pasteboardType), 0);
    auto [result] = sendResult.takeReplyOr(nullptr);
    return result;
}

Vector<RefPtr<StringImpl>> WebPlatformStrategies::readFilePathsFromClipboard(StringImpl* pasteboardName)
{
    auto sendResult = protect(WebProcess::singleton().parentProcessConnection())->sendSync(Messages::WebPasteboardProxy::ReadFilePaths(pasteboardName), 0);
    auto [result] = sendResult.takeReplyOr(Vector<RefPtr<StringImpl>> { });
    return result;
}

RefPtr<SharedBuffer> WebPlatformStrategies::readBufferFromClipboard(StringImpl* pasteboardName, StringImpl* pasteboardType)
{
    auto sendResult = protect(WebProcess::singleton().parentProcessConnection())->sendSync(Messages::WebPasteboardProxy::ReadBuffer(pasteboardName, pasteboardType), 0);
    auto [buffer] = sendResult.takeReplyOr(nullptr);
    return buffer;
}

void WebPlatformStrategies::writeToClipboard(StringImpl* pasteboardName, SelectionData&& selectionData)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebPasteboardProxy::WriteToClipboard(pasteboardName, WTF::move(selectionData)), 0);
}

void WebPlatformStrategies::clearClipboard(StringImpl* pasteboardName)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebPasteboardProxy::ClearClipboard(pasteboardName), 0);
}

#elif USE(LIBWPE)
// PasteboardStrategy

void WebPlatformStrategies::getTypes(Vector<RefPtr<StringImpl>>& types)
{
    auto sendResult = protect(WebProcess::singleton().parentProcessConnection())->sendSync(Messages::WebPasteboardProxy::GetPasteboardTypes(), 0);
    if (sendResult.succeeded())
        std::tie(types) = sendResult.takeReply();
}

void WebPlatformStrategies::writeToPasteboard(const WebCore::PasteboardWebContent& content)
{
    protect(WebProcess::singleton().parentProcessConnection())->send(Messages::WebPasteboardProxy::WriteWebContentToPasteboard(content), 0);
}

void WebPlatformStrategies::writeToPasteboard(StringImpl* pasteboardType, StringImpl* text)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebPasteboardProxy::WriteStringToPasteboard(pasteboardType, text), 0);
}

#endif // USE(LIBWPE)

#if PLATFORM(GTK) || PLATFORM(WPE) || PLATFORM(WIN)

int64_t WebPlatformStrategies::changeCount(StringImpl* pasteboardName)
{
    auto sendResult = protect(WebProcess::singleton().parentProcessConnection())->sendSync(Messages::WebPasteboardProxy::GetPasteboardChangeCount(pasteboardName), 0);
    auto [changeCount] = sendResult.takeReplyOr(0);
    return changeCount;
}

#endif // PLATFORM(GTK) || PLATFORM(WPE) || PLATFORM(WIN)

Vector<RefPtr<StringImpl>> WebPlatformStrategies::typesSafeForDOMToReadAndWrite(StringImpl* pasteboardName, StringImpl* origin, const PasteboardContext* context)
{
    auto sendResult = protect(WebProcess::singleton().parentProcessConnection())->sendSync(Messages::WebPasteboardProxy::TypesSafeForDOMToReadAndWrite(pasteboardName, origin, pageIdentifier(context)), 0);
    auto [types] = sendResult.takeReplyOr(Vector<RefPtr<StringImpl>> { });
    return types;
}

int64_t WebPlatformStrategies::writeCustomData(const Vector<PasteboardCustomData>& data, StringImpl* pasteboardName, const PasteboardContext* context)
{
    auto sendResult = protect(WebProcess::singleton().parentProcessConnection())->sendSync(Messages::WebPasteboardProxy::WriteCustomData(data, pasteboardName, pageIdentifier(context)), 0);
    auto [newChangeCount] = sendResult.takeReplyOr(0);
    return newChangeCount;
}

bool WebPlatformStrategies::containsStringSafeForDOMToReadForType(StringImpl* type, StringImpl* pasteboardName, const PasteboardContext* context)
{
    auto sendResult = protect(WebProcess::singleton().parentProcessConnection())->sendSync(Messages::WebPasteboardProxy::ContainsStringSafeForDOMToReadForType(type, pasteboardName, pageIdentifier(context)), 0);
    auto [result] = sendResult.takeReplyOr(false);
    return result;
}

int WebPlatformStrategies::getPasteboardItemsCount(StringImpl* pasteboardName, const PasteboardContext* context)
{
    if (!WebPasteboardOverrides::sharedPasteboardOverrides().overriddenTypes(pasteboardName).isEmpty()) {
        // Override pasteboards currently only support single pasteboard items.
        return 1;
    }

    auto sendResult = protect(WebProcess::singleton().parentProcessConnection())->sendSync(Messages::WebPasteboardProxy::GetPasteboardItemsCount(pasteboardName, pageIdentifier(context)), 0);
    auto [itemsCount] = sendResult.takeReplyOr(0);
    return itemsCount;
}

std::optional<Vector<PasteboardItemInfo>> WebPlatformStrategies::allPasteboardItemInfo(StringImpl* pasteboardName, int64_t changeCount, const PasteboardContext* context)
{
    if (auto info = WebPasteboardOverrides::sharedPasteboardOverrides().overriddenInfo(pasteboardName))
        return { { WTF::move(*info) } };

    auto sendResult = protect(WebProcess::singleton().parentProcessConnection())->sendSync(Messages::WebPasteboardProxy::AllPasteboardItemInfo(pasteboardName, changeCount, pageIdentifier(context)), 0);
    auto [allInfo] = sendResult.takeReplyOr(std::nullopt);
    return allInfo;
}

std::optional<PasteboardItemInfo> WebPlatformStrategies::informationForItemAtIndex(size_t index, StringImpl* pasteboardName, int64_t changeCount, const PasteboardContext* context)
{
    if (auto info = WebPasteboardOverrides::sharedPasteboardOverrides().overriddenInfo(pasteboardName))
        return info;

    auto sendResult = protect(WebProcess::singleton().parentProcessConnection())->sendSync(Messages::WebPasteboardProxy::InformationForItemAtIndex(index, pasteboardName, changeCount, pageIdentifier(context)), 0);
    auto [info] = sendResult.takeReplyOr(std::nullopt);
    return info;
}

RefPtr<WebCore::SharedBuffer> WebPlatformStrategies::readBufferFromPasteboard(std::optional<size_t> index, StringImpl* pasteboardType, StringImpl* pasteboardName, const PasteboardContext* context)
{
    Vector<uint8_t> overrideBuffer;
    if (WebPasteboardOverrides::sharedPasteboardOverrides().getDataForOverride(pasteboardName, pasteboardType, overrideBuffer))
        return SharedBuffer::create(WTF::move(overrideBuffer));

    auto sendResult = protect(WebProcess::singleton().parentProcessConnection())->sendSync(Messages::WebPasteboardProxy::ReadBufferFromPasteboard(index, pasteboardType, pasteboardName, pageIdentifier(context)), 0);
    auto [buffer] = sendResult.takeReplyOr(nullptr);
    return buffer;
}

URL WebPlatformStrategies::readURLFromPasteboard(size_t index, StringImpl* pasteboardName, RefPtr<StringImpl>& title, const PasteboardContext* context)
{
    RefPtr<StringImpl> urlString;
    auto sendResult = protect(WebProcess::singleton().parentProcessConnection())->sendSync(Messages::WebPasteboardProxy::ReadURLFromPasteboard(index, pasteboardName, pageIdentifier(context)), 0);
    if (sendResult.succeeded())
        std::tie(urlString, title) = sendResult.takeReply();
    return URL({ }, urlString);
}

RefPtr<StringImpl> WebPlatformStrategies::readStringFromPasteboard(size_t index, StringImpl* pasteboardType, StringImpl* pasteboardName, const PasteboardContext* context)
{
    auto sendResult = protect(WebProcess::singleton().parentProcessConnection())->sendSync(Messages::WebPasteboardProxy::ReadStringFromPasteboard(index, pasteboardType, pasteboardName, pageIdentifier(context)), 0);
    auto [value] = sendResult.takeReplyOr(nullString());
    return value;
}

#if ENABLE(DECLARATIVE_WEB_PUSH)

void WebPlatformStrategies::windowSubscribeToPushService(const URL& scope, const Vector<uint8_t>& applicationServerKey, SubscribeToPushServiceCallback&& callback)
{
    auto completionHandler = [callback = WTF::move(callback)](auto&& valueOrException) mutable {
        if (!valueOrException.has_value()) {
            callback(valueOrException.error().toException());
            return;
        }
        callback(WTF::move(*valueOrException));
    };

    protect(WebProcess::singleton().ensureNetworkProcessConnection().connection())->sendWithAsyncReply(Messages::NetworkConnectionToWebProcess::NavigatorSubscribeToPushService(scope, applicationServerKey), WTF::move(completionHandler));
}

void WebPlatformStrategies::windowUnsubscribeFromPushService(const URL& scope, std::optional<PushSubscriptionIdentifier> subscriptionIdentifier, UnsubscribeFromPushServiceCallback&& callback)
{
    auto completionHandler = [callback = WTF::move(callback)](auto&& valueOrException) mutable {
        if (!valueOrException.has_value()) {
            callback(valueOrException.error().toException());
            return;
        }
        callback(WTF::move(*valueOrException));
    };

    protect(WebProcess::singleton().ensureNetworkProcessConnection().connection())->sendWithAsyncReply(Messages::NetworkConnectionToWebProcess::NavigatorUnsubscribeFromPushService(scope, *subscriptionIdentifier), WTF::move(completionHandler));
}

void WebPlatformStrategies::windowGetPushSubscription(const URL& scope, GetPushSubscriptionCallback&& callback)
{
    auto completionHandler = [callback = WTF::move(callback)](auto&& valueOrException) mutable {
        if (!valueOrException.has_value()) {
            callback(valueOrException.error().toException());
            return;
        }
        callback(WTF::move(*valueOrException));
    };

    protect(WebProcess::singleton().ensureNetworkProcessConnection().connection())->sendWithAsyncReply(Messages::NetworkConnectionToWebProcess::NavigatorGetPushSubscription(scope), WTF::move(completionHandler));
}

void WebPlatformStrategies::windowGetPushPermissionState(const URL& scope, GetPushPermissionStateCallback&& callback)
{
    auto completionHandler = [callback = WTF::move(callback)](auto&& valueOrException) mutable {
        if (!valueOrException.has_value())
            return callback(valueOrException.error().toException());
        callback(static_cast<PushPermissionState>(*valueOrException));
    };

    protect(WebProcess::singleton().ensureNetworkProcessConnection().connection())->sendWithAsyncReply(Messages::NetworkConnectionToWebProcess::NavigatorGetPushPermissionState(scope), WTF::move(completionHandler));
}

#endif // ENABLE(DECLARATIVE_WEB_PUSH)
} // namespace WebKit
