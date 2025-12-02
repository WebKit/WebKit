/*
 * Copyright (C) 2010-2025 Apple Inc. All rights reserved.
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

#import "WebPlatformStrategies.h"

#import "WebFrameNetworkingContext.h"
#import "WebPluginPackage.h"
#import "WebResourceLoadScheduler.h"
#import <WebCore/AudioDestination.h>
#import <WebCore/BlobRegistryImpl.h>
#import <WebCore/CDMFactory.h>
#import <WebCore/Color.h>
#import <WebCore/LocalFrame.h>
#import <WebCore/MediaStrategy.h>
#import <WebCore/NetworkStorageSession.h>
#import <WebCore/Pasteboard.h>
#import <WebCore/PasteboardItemInfo.h>
#import <WebCore/PlatformPasteboard.h>
#import <WebCore/SharedBuffer.h>
#import <WebCore/SubframeLoader.h>
#import <wtf/NeverDestroyed.h>

using namespace WebCore;

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(WebPlatformStrategies);

void WebPlatformStrategies::initializeIfNecessary()
{
    static NeverDestroyed<std::unique_ptr<WebPlatformStrategies>> platformStrategies = [] {
        auto platformStrategies = makeUnique<WebPlatformStrategies>();
        setPlatformStrategies(platformStrategies.get());
        return platformStrategies;
    }();
    UNUSED_PARAM(platformStrategies);
}

WebPlatformStrategies::WebPlatformStrategies() = default;

LoaderStrategy* WebPlatformStrategies::createLoaderStrategy()
{
    return new WebResourceLoadScheduler;
}

PasteboardStrategy* WebPlatformStrategies::createPasteboardStrategy()
{
    return this;
}

class WebMediaStrategy final : public MediaStrategy {
private:
#if ENABLE(WEB_AUDIO)
    Ref<AudioDestination> createAudioDestination(const AudioDestinationCreationOptions& options) override
    {
        return AudioDestination::create(options);
    }
#endif

    bool enableWebMMediaPlayer() const final { return false; }
};

MediaStrategy* WebPlatformStrategies::createMediaStrategy()
{
    return new WebMediaStrategy;
}

class WebBlobRegistry final : public BlobRegistry {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(WebBlobRegistry);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(WebBlobRegistry);
private:
    void registerInternalFileBlobURL(const URL& url, Ref<BlobDataFileReference>&& reference, const String&, const String& contentType) final { m_blobRegistry.registerInternalFileBlobURL(url, WTFMove(reference), contentType); }
    void registerInternalBlobURL(const URL& url, Vector<BlobPart>&& parts, const String& contentType) final { m_blobRegistry.registerInternalBlobURL(url, WTFMove(parts), contentType); }
    void registerBlobURL(const URL& url, const URL& srcURL, const PolicyContainer& policyContainer, const std::optional<WebCore::SecurityOriginData>& topOrigin) final { m_blobRegistry.registerBlobURL(url, srcURL, policyContainer, topOrigin); }
    void registerInternalBlobURLOptionallyFileBacked(const URL& url, const URL& srcURL, RefPtr<BlobDataFileReference>&& reference, const String& contentType) final { m_blobRegistry.registerInternalBlobURLOptionallyFileBacked(url, srcURL, WTFMove(reference), contentType, { }); }
    void registerInternalBlobURLForSlice(const URL& url, const URL& srcURL, long long start, long long end, const String& contentType) final { m_blobRegistry.registerInternalBlobURLForSlice(url, srcURL, start, end, contentType); }
    void unregisterBlobURL(const URL& url, const std::optional<WebCore::SecurityOriginData>& topOrigin) final { m_blobRegistry.unregisterBlobURL(url, topOrigin); }
    String blobType(const URL& url) final { return m_blobRegistry.blobType(url); }
    unsigned long long blobSize(const URL& url) final { return m_blobRegistry.blobSize(url); }
    void writeBlobsToTemporaryFilesForIndexedDB(const Vector<String>& blobURLs, CompletionHandler<void(Vector<String>&& filePaths)>&& completionHandler) final { m_blobRegistry.writeBlobsToTemporaryFilesForIndexedDB(blobURLs, WTFMove(completionHandler)); }
    void registerBlobURLHandle(const URL& url, const std::optional<SecurityOriginData>& topOrigin) final { m_blobRegistry.registerBlobURLHandle(url, topOrigin); }
    void unregisterBlobURLHandle(const URL& url, const std::optional<SecurityOriginData>& topOrigin) final { m_blobRegistry.unregisterBlobURLHandle(url, topOrigin); }

    BlobRegistryImpl* blobRegistryImpl() final { return &m_blobRegistry; }

    BlobRegistryImpl m_blobRegistry;
};

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(WebBlobRegistry);

BlobRegistry* WebPlatformStrategies::createBlobRegistry()
{
    return new WebBlobRegistry;
}

void WebPlatformStrategies::getTypes(Vector<String>& types, const String& pasteboardName, const PasteboardContext*)
{
    PlatformPasteboard(pasteboardName).getTypes(types);
}

RefPtr<WebCore::SharedBuffer> WebPlatformStrategies::bufferForType(const String& pasteboardType, const String& pasteboardName, const PasteboardContext*)
{
    auto pasteboardBuffer = PlatformPasteboard(pasteboardName).bufferForType(pasteboardType);
    return Pasteboard::bufferConvertedToPasteboardType(pasteboardBuffer, pasteboardType);
}

void WebPlatformStrategies::getPathnamesForType(Vector<String>& pathnames, const String& pasteboardType, const String& pasteboardName, const PasteboardContext*)
{
    PlatformPasteboard(pasteboardName).getPathnamesForType(pathnames, pasteboardType);
}

Vector<String> WebPlatformStrategies::allStringsForType(const String& pasteboardType, const String& pasteboardName, const PasteboardContext*)
{
    return PlatformPasteboard(pasteboardName).allStringsForType(pasteboardType);
}

String WebPlatformStrategies::stringForType(const String& pasteboardType, const String& pasteboardName, const PasteboardContext*)
{
    return PlatformPasteboard(pasteboardName).stringForType(pasteboardType);
}

int64_t WebPlatformStrategies::changeCount(const String& pasteboardName, const PasteboardContext*)
{
    return PlatformPasteboard(pasteboardName).changeCount();
}

Color WebPlatformStrategies::color(const String& pasteboardName, const PasteboardContext*)
{
    return PlatformPasteboard(pasteboardName).color();    
}

URL WebPlatformStrategies::url(const String& pasteboardName, const PasteboardContext*)
{
    return PlatformPasteboard(pasteboardName).url();
}

int64_t WebPlatformStrategies::addTypes(const Vector<String>& pasteboardTypes, const String& pasteboardName, const PasteboardContext*)
{
    return PlatformPasteboard(pasteboardName).addTypes(pasteboardTypes);
}

int64_t WebPlatformStrategies::setTypes(const Vector<String>& pasteboardTypes, const String& pasteboardName, const PasteboardContext*)
{
    return PlatformPasteboard(pasteboardName).setTypes(pasteboardTypes);
}

int64_t WebPlatformStrategies::setBufferForType(SharedBuffer* buffer, const String& pasteboardType, const String& pasteboardName, const PasteboardContext*)
{
    return PlatformPasteboard(pasteboardName).setBufferForType(buffer, pasteboardType);
}

int64_t WebPlatformStrategies::setURL(const PasteboardURL& pasteboardURL, const String& pasteboardName, const PasteboardContext*)
{
    return PlatformPasteboard(pasteboardName).setURL(pasteboardURL);
}

int64_t WebPlatformStrategies::setColor(const Color& color, const String& pasteboardName, const PasteboardContext*)
{
    return PlatformPasteboard(pasteboardName).setColor(color);
}

int64_t WebPlatformStrategies::setStringForType(const String& string, const String& pasteboardType, const String& pasteboardName, const PasteboardContext*)
{
    return PlatformPasteboard(pasteboardName).setStringForType(string, pasteboardType);
}

int WebPlatformStrategies::getNumberOfFiles(const String& pasteboardName, const PasteboardContext*)
{
    return PlatformPasteboard(pasteboardName).numberOfFiles();
}

Vector<String> WebPlatformStrategies::typesSafeForDOMToReadAndWrite(const String& pasteboardName, const String& origin, const PasteboardContext*)
{
    return PlatformPasteboard(pasteboardName).typesSafeForDOMToReadAndWrite(origin);
}

int64_t WebPlatformStrategies::writeCustomData(const Vector<WebCore::PasteboardCustomData>& data, const String& pasteboardName, const PasteboardContext*)
{
    return PlatformPasteboard(pasteboardName).write(data);
}

bool WebPlatformStrategies::containsStringSafeForDOMToReadForType(const String& pasteboardName, const String& type, const PasteboardContext*)
{
    return PlatformPasteboard(pasteboardName).containsStringSafeForDOMToReadForType(type);
}

std::optional<WebCore::PasteboardItemInfo> WebPlatformStrategies::informationForItemAtIndex(size_t index, const String& pasteboardName, int64_t changeCount, const PasteboardContext*)
{
    return PlatformPasteboard(pasteboardName).informationForItemAtIndex(index, changeCount);
}

std::optional<Vector<WebCore::PasteboardItemInfo>> WebPlatformStrategies::allPasteboardItemInfo(const String& pasteboardName, int64_t changeCount, const PasteboardContext*)
{
    return PlatformPasteboard(pasteboardName).allPasteboardItemInfo(changeCount);
}

int WebPlatformStrategies::getPasteboardItemsCount(const String& pasteboardName, const PasteboardContext*)
{
    return PlatformPasteboard(pasteboardName).count();
}

RefPtr<WebCore::SharedBuffer> WebPlatformStrategies::readBufferFromPasteboard(std::optional<size_t> index, const String& type, const String& pasteboardName, const PasteboardContext*)
{
    return PlatformPasteboard(pasteboardName).readBuffer(index, type);
}

URL WebPlatformStrategies::readURLFromPasteboard(size_t index, const String& pasteboardName, String& title, const PasteboardContext*)
{
    return PlatformPasteboard(pasteboardName).readURL(index, title);
}

String WebPlatformStrategies::readStringFromPasteboard(size_t index, const String& type, const String& pasteboardName, const PasteboardContext*)
{
    return PlatformPasteboard(pasteboardName).readString(index, type);
}

bool WebPlatformStrategies::containsURLStringSuitableForLoading(const String& pasteboardName, const PasteboardContext*)
{
    return PlatformPasteboard(pasteboardName).containsURLStringSuitableForLoading();
}

String WebPlatformStrategies::urlStringSuitableForLoading(const String& pasteboardName, String& title, const PasteboardContext*)
{
    return PlatformPasteboard(pasteboardName).urlStringSuitableForLoading(title);
}

#if PLATFORM(IOS_FAMILY)

void WebPlatformStrategies::writeToPasteboard(const PasteboardURL& url, const String& pasteboardName, const PasteboardContext*)
{
    PlatformPasteboard(pasteboardName).write(url);
}

void WebPlatformStrategies::writeToPasteboard(const WebCore::PasteboardWebContent& content, const String& pasteboardName, const PasteboardContext*)
{
    PlatformPasteboard(pasteboardName).write(content);
}

void WebPlatformStrategies::writeToPasteboard(const WebCore::PasteboardImage& image, const String& pasteboardName, const PasteboardContext*)
{
    PlatformPasteboard(pasteboardName).write(image);
}

void WebPlatformStrategies::writeToPasteboard(const String& pasteboardType, const String& text, const String& pasteboardName, const PasteboardContext*)
{
    PlatformPasteboard(pasteboardName).write(pasteboardType, text);
}

void WebPlatformStrategies::updateSupportedTypeIdentifiers(const Vector<String>& identifiers, const String& pasteboardName, const PasteboardContext*)
{
    PlatformPasteboard(pasteboardName).updateSupportedTypeIdentifiers(identifiers);
}
#endif // PLATFORM(IOS_FAMILY)
