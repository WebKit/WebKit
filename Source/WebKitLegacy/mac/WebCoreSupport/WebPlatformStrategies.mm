/*
 * Copyright (C) 2010-2017 Apple Inc. All rights reserved.
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
#import <WebCore/Frame.h>
#import <WebCore/MediaSessionManagerCocoa.h>
#import <WebCore/MediaStrategy.h>
#import <WebCore/NetworkStorageSession.h>
#import <WebCore/PasteboardItemInfo.h>
#import <WebCore/PlatformPasteboard.h>
#import <WebCore/SharedBuffer.h>
#import <WebCore/SubframeLoader.h>

using namespace WebCore;

void WebPlatformStrategies::initializeIfNecessary()
{
    static WebPlatformStrategies* platformStrategies;
    if (!platformStrategies) {
        platformStrategies = new WebPlatformStrategies;
        setPlatformStrategies(platformStrategies);
    }
}

WebPlatformStrategies::WebPlatformStrategies()
{
}

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
    Ref<AudioDestination> createAudioDestination(AudioIOCallback& callback, const String& inputDeviceId,
        unsigned numberOfInputChannels, unsigned numberOfOutputChannels, float sampleRate) override
    {
        return AudioDestination::create(callback, inputDeviceId, numberOfInputChannels, numberOfOutputChannels, sampleRate);
    }
#endif
    void clearNowPlayingInfo() final
    {
        MediaSessionManagerCocoa::clearNowPlayingInfo();
    }

    void setNowPlayingInfo(bool setAsNowPlayingApplication, const NowPlayingInfo& nowPlayingInfo) final
    {
        MediaSessionManagerCocoa::setNowPlayingInfo(setAsNowPlayingApplication, nowPlayingInfo);
    }
};

MediaStrategy* WebPlatformStrategies::createMediaStrategy()
{
    return new WebMediaStrategy;
}

class WebBlobRegistry final : public BlobRegistry {
private:
    void registerFileBlobURL(const URL& url, Ref<BlobDataFileReference>&& reference, const String&, const String& contentType) final { m_blobRegistry.registerFileBlobURL(url, WTFMove(reference), contentType); }
    void registerBlobURL(const URL& url, Vector<BlobPart>&& parts, const String& contentType) final { m_blobRegistry.registerBlobURL(url, WTFMove(parts), contentType); }
    void registerBlobURL(const URL& url, const URL& srcURL) final { m_blobRegistry.registerBlobURL(url, srcURL); }
    void registerBlobURLOptionallyFileBacked(const URL& url, const URL& srcURL, RefPtr<BlobDataFileReference>&& reference, const String& contentType) final { m_blobRegistry.registerBlobURLOptionallyFileBacked(url, srcURL, WTFMove(reference), contentType); }
    void registerBlobURLForSlice(const URL& url, const URL& srcURL, long long start, long long end) final { m_blobRegistry.registerBlobURLForSlice(url, srcURL, start, end); }
    void unregisterBlobURL(const URL& url) final { m_blobRegistry.unregisterBlobURL(url); }
    unsigned long long blobSize(const URL& url) final { return m_blobRegistry.blobSize(url); }
    void writeBlobsToTemporaryFiles(const Vector<String>& blobURLs, CompletionHandler<void(Vector<String>&& filePaths)>&& completionHandler) final { m_blobRegistry.writeBlobsToTemporaryFiles(blobURLs, WTFMove(completionHandler)); }

    BlobRegistryImpl* blobRegistryImpl() final { return &m_blobRegistry; }

    BlobRegistryImpl m_blobRegistry;
};

BlobRegistry* WebPlatformStrategies::createBlobRegistry()
{
    return new WebBlobRegistry;
}

void WebPlatformStrategies::getTypes(Vector<String>& types, const String& pasteboardName)
{
    PlatformPasteboard(pasteboardName).getTypes(types);
}

RefPtr<SharedBuffer> WebPlatformStrategies::bufferForType(const String& pasteboardType, const String& pasteboardName)
{
    return PlatformPasteboard(pasteboardName).bufferForType(pasteboardType);
}

void WebPlatformStrategies::getPathnamesForType(Vector<String>& pathnames, const String& pasteboardType, const String& pasteboardName)
{
    PlatformPasteboard(pasteboardName).getPathnamesForType(pathnames, pasteboardType);
}

Vector<String> WebPlatformStrategies::allStringsForType(const String& pasteboardType, const String& pasteboardName)
{
    return PlatformPasteboard(pasteboardName).allStringsForType(pasteboardType);
}

String WebPlatformStrategies::stringForType(const String& pasteboardType, const String& pasteboardName)
{
    return PlatformPasteboard(pasteboardName).stringForType(pasteboardType);
}

int64_t WebPlatformStrategies::changeCount(const String& pasteboardName)
{
    return PlatformPasteboard(pasteboardName).changeCount();
}

Color WebPlatformStrategies::color(const String& pasteboardName)
{
    return PlatformPasteboard(pasteboardName).color();    
}

URL WebPlatformStrategies::url(const String& pasteboardName)
{
    return PlatformPasteboard(pasteboardName).url();
}

int64_t WebPlatformStrategies::addTypes(const Vector<String>& pasteboardTypes, const String& pasteboardName)
{
    return PlatformPasteboard(pasteboardName).addTypes(pasteboardTypes);
}

int64_t WebPlatformStrategies::setTypes(const Vector<String>& pasteboardTypes, const String& pasteboardName)
{
    return PlatformPasteboard(pasteboardName).setTypes(pasteboardTypes);
}

int64_t WebPlatformStrategies::setBufferForType(SharedBuffer* buffer, const String& pasteboardType, const String& pasteboardName)
{
    return PlatformPasteboard(pasteboardName).setBufferForType(buffer, pasteboardType);
}

int64_t WebPlatformStrategies::setURL(const PasteboardURL& pasteboardURL, const String& pasteboardName)
{
    return PlatformPasteboard(pasteboardName).setURL(pasteboardURL);
}

int64_t WebPlatformStrategies::setColor(const Color& color, const String& pasteboardName)
{
    return PlatformPasteboard(pasteboardName).setColor(color);
}

int64_t WebPlatformStrategies::setStringForType(const String& string, const String& pasteboardType, const String& pasteboardName)
{
    return PlatformPasteboard(pasteboardName).setStringForType(string, pasteboardType);
}

int WebPlatformStrategies::getNumberOfFiles(const String& pasteboardName)
{
    return PlatformPasteboard(pasteboardName).numberOfFiles();
}

Vector<String> WebPlatformStrategies::typesSafeForDOMToReadAndWrite(const String& pasteboardName, const String& origin)
{
    return PlatformPasteboard(pasteboardName).typesSafeForDOMToReadAndWrite(origin);
}

int64_t WebPlatformStrategies::writeCustomData(const Vector<WebCore::PasteboardCustomData>& data, const String& pasteboardName)
{
    return PlatformPasteboard(pasteboardName).write(data);
}

bool WebPlatformStrategies::containsStringSafeForDOMToReadForType(const String& pasteboardName, const String& type)
{
    return PlatformPasteboard(pasteboardName).containsStringSafeForDOMToReadForType(type);
}

Optional<WebCore::PasteboardItemInfo> WebPlatformStrategies::informationForItemAtIndex(size_t index, const String& pasteboardName, int64_t changeCount)
{
    return PlatformPasteboard(pasteboardName).informationForItemAtIndex(index, changeCount);
}

Optional<Vector<WebCore::PasteboardItemInfo>> WebPlatformStrategies::allPasteboardItemInfo(const String& pasteboardName, int64_t changeCount)
{
    return PlatformPasteboard(pasteboardName).allPasteboardItemInfo(changeCount);
}

int WebPlatformStrategies::getPasteboardItemsCount(const String& pasteboardName)
{
    return PlatformPasteboard(pasteboardName).count();
}

RefPtr<WebCore::SharedBuffer> WebPlatformStrategies::readBufferFromPasteboard(size_t index, const String& type, const String& pasteboardName)
{
    return PlatformPasteboard(pasteboardName).readBuffer(index, type);
}

URL WebPlatformStrategies::readURLFromPasteboard(size_t index, const String& pasteboardName, String& title)
{
    return PlatformPasteboard(pasteboardName).readURL(index, title);
}

String WebPlatformStrategies::readStringFromPasteboard(size_t index, const String& type, const String& pasteboardName)
{
    return PlatformPasteboard(pasteboardName).readString(index, type);
}

bool WebPlatformStrategies::containsURLStringSuitableForLoading(const String& pasteboardName)
{
    return PlatformPasteboard(pasteboardName).containsURLStringSuitableForLoading();
}

String WebPlatformStrategies::urlStringSuitableForLoading(const String& pasteboardName, String& title)
{
    return PlatformPasteboard(pasteboardName).urlStringSuitableForLoading(title);
}

#if PLATFORM(IOS_FAMILY)

void WebPlatformStrategies::writeToPasteboard(const PasteboardURL& url, const String& pasteboardName)
{
    PlatformPasteboard(pasteboardName).write(url);
}

void WebPlatformStrategies::writeToPasteboard(const WebCore::PasteboardWebContent& content, const String& pasteboardName)
{
    PlatformPasteboard(pasteboardName).write(content);
}

void WebPlatformStrategies::writeToPasteboard(const WebCore::PasteboardImage& image, const String& pasteboardName)
{
    PlatformPasteboard(pasteboardName).write(image);
}

void WebPlatformStrategies::writeToPasteboard(const String& pasteboardType, const String& text, const String& pasteboardName)
{
    PlatformPasteboard(pasteboardName).write(pasteboardType, text);
}

void WebPlatformStrategies::updateSupportedTypeIdentifiers(const Vector<String>& identifiers, const String& pasteboardName)
{
    PlatformPasteboard(pasteboardName).updateSupportedTypeIdentifiers(identifiers);
}
#endif // PLATFORM(IOS_FAMILY)
