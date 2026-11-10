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
#import <WebCore/Pasteboard.h>
#import <WebCore/PasteboardItemInfo.h>
#import <WebCore/PlatformPasteboard.h>
#import <WebCore/SharedBuffer.h>
#import <WebCore/SubframeLoader.h>
#import <wtf/NeverDestroyed.h>


WTF_MAKE_TZONE_ALLOCATED_IMPL(WebPlatformStrategies);

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

WebCore::LoaderStrategy* WebPlatformStrategies::createLoaderStrategy()
{
    return new WebResourceLoadScheduler;
}

WebCore::PasteboardStrategy* WebPlatformStrategies::createPasteboardStrategy()
{
    return this;
}

class WebMediaStrategy final : public WebCore::MediaStrategy {
private:
#if ENABLE(WEB_AUDIO)
    Ref<WebCore::AudioDestination> createAudioDestination(const WebCore::AudioDestinationCreationOptions& options) override
    {
        return WebCore::AudioDestination::create(options);
    }
#endif

    bool enableWebMMediaPlayer() const final { return false; }
};

WebCore::MediaStrategy* WebPlatformStrategies::createMediaStrategy()
{
    return new WebMediaStrategy;
}

class WebBlobRegistry final : public WebCore::BlobRegistry {
    WTF_MAKE_TZONE_ALLOCATED(WebBlobRegistry);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(WebBlobRegistry);
private:
    void registerInternalFileBlobURL(const URL& url, Ref<WebCore::BlobDataFileReference>&& reference, StringImpl*, StringImpl* contentType) final { m_blobRegistry.registerInternalFileBlobURL(url, WTF::move(reference), contentType); }
    void registerInternalBlobURL(const URL& url, Vector<WebCore::BlobPart>&& parts, StringImpl* contentType) final { m_blobRegistry.registerInternalBlobURL(url, WTF::move(parts), contentType); }
    void registerBlobURL(const URL& url, const URL& srcURL, const WebCore::PolicyContainer& policyContainer, const std::optional<WebCore::SecurityOriginData>& topOrigin) final { m_blobRegistry.registerBlobURL(url, srcURL, policyContainer, topOrigin); }
    void registerInternalBlobURLOptionallyFileBacked(const URL& url, const URL& srcURL, RefPtr<WebCore::BlobDataFileReference>&& reference, StringImpl* contentType) final { m_blobRegistry.registerInternalBlobURLOptionallyFileBacked(url, srcURL, WTF::move(reference), contentType, { }); }
    void registerInternalBlobURLForSlice(const URL& url, const URL& srcURL, long long start, long long end, StringImpl* contentType) final { m_blobRegistry.registerInternalBlobURLForSlice(url, srcURL, start, end, contentType); }
    void unregisterBlobURL(const URL& url, const std::optional<WebCore::SecurityOriginData>& topOrigin) final { m_blobRegistry.unregisterBlobURL(url, topOrigin); }
    RefPtr<StringImpl> blobType(const URL& url) final { return m_blobRegistry.blobType(url); }
    unsigned long long blobSize(const URL& url) final { return m_blobRegistry.blobSize(url); }
    void writeBlobsToTemporaryFilesForIndexedDB(const Vector<RefPtr<StringImpl>>& blobURLs, CompletionHandler<void(Vector<RefPtr<StringImpl>>&& filePaths)>&& completionHandler) final { m_blobRegistry.writeBlobsToTemporaryFilesForIndexedDB(blobURLs, WTF::move(completionHandler)); }
    void registerBlobURLHandle(const URL& url, const std::optional<WebCore::SecurityOriginData>& topOrigin) final { m_blobRegistry.registerBlobURLHandle(url, topOrigin); }
    void unregisterBlobURLHandle(const URL& url, const std::optional<WebCore::SecurityOriginData>& topOrigin) final { m_blobRegistry.unregisterBlobURLHandle(url, topOrigin); }

    WebCore::BlobRegistryImpl* blobRegistryImpl() final { return &m_blobRegistry; }

    WebCore::BlobRegistryImpl m_blobRegistry;
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebBlobRegistry);

WebCore::BlobRegistry* WebPlatformStrategies::createBlobRegistry()
{
    return new WebBlobRegistry;
}

void WebPlatformStrategies::getTypes(Vector<RefPtr<StringImpl>>& types, StringImpl* pasteboardName, const WebCore::PasteboardContext*)
{
    WebCore::PlatformPasteboard(pasteboardName).getTypes(types);
}

RefPtr<WebCore::SharedBuffer> WebPlatformStrategies::bufferForType(StringImpl* pasteboardType, StringImpl* pasteboardName, const WebCore::PasteboardContext*)
{
    auto pasteboardBuffer = WebCore::PlatformPasteboard(pasteboardName).bufferForType(pasteboardType);
    return WebCore::Pasteboard::bufferConvertedToPasteboardType(pasteboardBuffer, pasteboardType);
}

void WebPlatformStrategies::getPathnamesForType(Vector<RefPtr<StringImpl>>& pathnames, StringImpl* pasteboardType, StringImpl* pasteboardName, const WebCore::PasteboardContext*)
{
    WebCore::PlatformPasteboard(pasteboardName).getPathnamesForType(pathnames, pasteboardType);
}

Vector<RefPtr<StringImpl>> WebPlatformStrategies::allStringsForType(StringImpl* pasteboardType, StringImpl* pasteboardName, const WebCore::PasteboardContext*)
{
    return WebCore::PlatformPasteboard(pasteboardName).allStringsForType(pasteboardType);
}

RefPtr<StringImpl> WebPlatformStrategies::stringForType(StringImpl* pasteboardType, StringImpl* pasteboardName, const WebCore::PasteboardContext*)
{
    return WebCore::PlatformPasteboard(pasteboardName).stringForType(pasteboardType);
}

int64_t WebPlatformStrategies::changeCount(StringImpl* pasteboardName, const WebCore::PasteboardContext*)
{
    return WebCore::PlatformPasteboard(pasteboardName).changeCount();
}

WebCore::Color WebPlatformStrategies::color(StringImpl* pasteboardName, const WebCore::PasteboardContext*)
{
    return WebCore::PlatformPasteboard(pasteboardName).color();    
}

URL WebPlatformStrategies::url(StringImpl* pasteboardName, const WebCore::PasteboardContext*)
{
    return WebCore::PlatformPasteboard(pasteboardName).url();
}

int64_t WebPlatformStrategies::addTypes(const Vector<RefPtr<StringImpl>>& pasteboardTypes, StringImpl* pasteboardName, const WebCore::PasteboardContext*)
{
    return WebCore::PlatformPasteboard(pasteboardName).addTypes(pasteboardTypes);
}

int64_t WebPlatformStrategies::setTypes(const Vector<RefPtr<StringImpl>>& pasteboardTypes, StringImpl* pasteboardName, const WebCore::PasteboardContext*)
{
    return WebCore::PlatformPasteboard(pasteboardName).setTypes(pasteboardTypes);
}

int64_t WebPlatformStrategies::setBufferForType(WebCore::SharedBuffer* buffer, StringImpl* pasteboardType, StringImpl* pasteboardName, const WebCore::PasteboardContext*)
{
    return WebCore::PlatformPasteboard(pasteboardName).setBufferForType(buffer, pasteboardType);
}

int64_t WebPlatformStrategies::setURL(const WebCore::PasteboardURL& pasteboardURL, StringImpl* pasteboardName, const WebCore::PasteboardContext*)
{
    return WebCore::PlatformPasteboard(pasteboardName).setURL(pasteboardURL);
}

int64_t WebPlatformStrategies::setColor(const WebCore::Color& color, StringImpl* pasteboardName, const WebCore::PasteboardContext*)
{
    return WebCore::PlatformPasteboard(pasteboardName).setColor(color);
}

int64_t WebPlatformStrategies::setStringForType(StringImpl* string, StringImpl* pasteboardType, StringImpl* pasteboardName, const WebCore::PasteboardContext*)
{
    return WebCore::PlatformPasteboard(pasteboardName).setStringForType(string, pasteboardType);
}

int WebPlatformStrategies::getNumberOfFiles(StringImpl* pasteboardName, const WebCore::PasteboardContext*)
{
    return WebCore::PlatformPasteboard(pasteboardName).numberOfFiles();
}

Vector<RefPtr<StringImpl>> WebPlatformStrategies::typesSafeForDOMToReadAndWrite(StringImpl* pasteboardName, StringImpl* origin, const WebCore::PasteboardContext*)
{
    return WebCore::PlatformPasteboard(pasteboardName).typesSafeForDOMToReadAndWrite(origin);
}

int64_t WebPlatformStrategies::writeCustomData(const Vector<WebCore::PasteboardCustomData>& data, StringImpl* pasteboardName, const WebCore::PasteboardContext*)
{
    return WebCore::PlatformPasteboard(pasteboardName).write(data);
}

bool WebPlatformStrategies::containsStringSafeForDOMToReadForType(StringImpl* pasteboardName, StringImpl* type, const WebCore::PasteboardContext*)
{
    return WebCore::PlatformPasteboard(pasteboardName).containsStringSafeForDOMToReadForType(type);
}

std::optional<WebCore::PasteboardItemInfo> WebPlatformStrategies::informationForItemAtIndex(size_t index, StringImpl* pasteboardName, int64_t changeCount, const WebCore::PasteboardContext*)
{
    return WebCore::PlatformPasteboard(pasteboardName).informationForItemAtIndex(index, changeCount);
}

std::optional<Vector<WebCore::PasteboardItemInfo>> WebPlatformStrategies::allPasteboardItemInfo(StringImpl* pasteboardName, int64_t changeCount, const WebCore::PasteboardContext*)
{
    return WebCore::PlatformPasteboard(pasteboardName).allPasteboardItemInfo(changeCount);
}

int WebPlatformStrategies::getPasteboardItemsCount(StringImpl* pasteboardName, const WebCore::PasteboardContext*)
{
    return WebCore::PlatformPasteboard(pasteboardName).count();
}

RefPtr<WebCore::SharedBuffer> WebPlatformStrategies::readBufferFromPasteboard(std::optional<size_t> index, StringImpl* type, StringImpl* pasteboardName, const WebCore::PasteboardContext*)
{
    return WebCore::PlatformPasteboard(pasteboardName).readBuffer(index, type);
}

URL WebPlatformStrategies::readURLFromPasteboard(size_t index, StringImpl* pasteboardName, RefPtr<StringImpl>& title, const WebCore::PasteboardContext*)
{
    return WebCore::PlatformPasteboard(pasteboardName).readURL(index, title);
}

RefPtr<StringImpl> WebPlatformStrategies::readStringFromPasteboard(size_t index, StringImpl* type, StringImpl* pasteboardName, const WebCore::PasteboardContext*)
{
    return WebCore::PlatformPasteboard(pasteboardName).readString(index, type);
}

bool WebPlatformStrategies::containsURLStringSuitableForLoading(StringImpl* pasteboardName, const WebCore::PasteboardContext*)
{
    return WebCore::PlatformPasteboard(pasteboardName).containsURLStringSuitableForLoading();
}

RefPtr<StringImpl> WebPlatformStrategies::urlStringSuitableForLoading(StringImpl* pasteboardName, RefPtr<StringImpl>& title, const WebCore::PasteboardContext*)
{
    return WebCore::PlatformPasteboard(pasteboardName).urlStringSuitableForLoading(title);
}

#if PLATFORM(IOS_FAMILY)

void WebPlatformStrategies::writeToPasteboard(const WebCore::PasteboardURL& url, StringImpl* pasteboardName, const WebCore::PasteboardContext*)
{
    WebCore::PlatformPasteboard(pasteboardName).write(url);
}

void WebPlatformStrategies::writeToPasteboard(const WebCore::PasteboardWebContent& content, StringImpl* pasteboardName, const WebCore::PasteboardContext*)
{
    WebCore::PlatformPasteboard(pasteboardName).write(content);
}

void WebPlatformStrategies::writeToPasteboard(const WebCore::PasteboardImage& image, StringImpl* pasteboardName, const WebCore::PasteboardContext*)
{
    WebCore::PlatformPasteboard(pasteboardName).write(image);
}

void WebPlatformStrategies::writeToPasteboard(StringImpl* pasteboardType, StringImpl* text, StringImpl* pasteboardName, const WebCore::PasteboardContext*)
{
    WebCore::PlatformPasteboard(pasteboardName).write(pasteboardType, text);
}

void WebPlatformStrategies::updateSupportedTypeIdentifiers(const Vector<RefPtr<StringImpl>>& identifiers, StringImpl* pasteboardName, const WebCore::PasteboardContext*)
{
    WebCore::PlatformPasteboard(pasteboardName).updateSupportedTypeIdentifiers(identifiers);
}
#endif // PLATFORM(IOS_FAMILY)
