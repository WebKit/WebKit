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
#import <WebCore/BlobRegistryImpl.h>
#import <WebCore/Color.h>
#import <WebCore/Frame.h>
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

class WebBlobRegistry final : public BlobRegistry {
private:
    void registerFileBlobURL(PAL::SessionID, const URL& url, Ref<BlobDataFileReference>&& reference, const String& contentType) final { m_blobRegistry.registerFileBlobURL(url, WTFMove(reference), contentType); }
    void registerBlobURL(PAL::SessionID, const URL& url, Vector<BlobPart>&& parts, const String& contentType) final { m_blobRegistry.registerBlobURL(url, WTFMove(parts), contentType); }
    void registerBlobURL(PAL::SessionID, const URL& url, const URL& srcURL) final { m_blobRegistry.registerBlobURL(url, srcURL); }
    void registerBlobURLOptionallyFileBacked(PAL::SessionID, const URL& url, const URL& srcURL, RefPtr<BlobDataFileReference>&& reference, const String& contentType) final { m_blobRegistry.registerBlobURLOptionallyFileBacked(url, srcURL, WTFMove(reference), contentType); }
    void registerBlobURLForSlice(PAL::SessionID, const URL& url, const URL& srcURL, long long start, long long end) final { m_blobRegistry.registerBlobURLForSlice(url, srcURL, start, end); }
    void unregisterBlobURL(PAL::SessionID, const URL& url) final { m_blobRegistry.unregisterBlobURL(url); }
    unsigned long long blobSize(PAL::SessionID, const URL& url) final { return m_blobRegistry.blobSize(url); }
    void writeBlobsToTemporaryFiles(PAL::SessionID, const Vector<String>& blobURLs, CompletionHandler<void(Vector<String>&& filePaths)>&& completionHandler) final { m_blobRegistry.writeBlobsToTemporaryFiles(blobURLs, WTFMove(completionHandler)); }

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

long WebPlatformStrategies::changeCount(const String &pasteboardName)
{
    return PlatformPasteboard(pasteboardName).changeCount();
}

String WebPlatformStrategies::uniqueName()
{
    return PlatformPasteboard::uniqueName();
}

Color WebPlatformStrategies::color(const String& pasteboardName)
{
    return PlatformPasteboard(pasteboardName).color();    
}

URL WebPlatformStrategies::url(const String& pasteboardName)
{
    return PlatformPasteboard(pasteboardName).url();
}

long WebPlatformStrategies::addTypes(const Vector<String>& pasteboardTypes, const String& pasteboardName)
{
    return PlatformPasteboard(pasteboardName).addTypes(pasteboardTypes);
}

long WebPlatformStrategies::setTypes(const Vector<String>& pasteboardTypes, const String& pasteboardName)
{
    return PlatformPasteboard(pasteboardName).setTypes(pasteboardTypes);
}

long WebPlatformStrategies::setBufferForType(SharedBuffer* buffer, const String& pasteboardType, const String& pasteboardName)
{
    return PlatformPasteboard(pasteboardName).setBufferForType(buffer, pasteboardType);
}

long WebPlatformStrategies::setURL(const PasteboardURL& pasteboardURL, const String& pasteboardName)
{
    return PlatformPasteboard(pasteboardName).setURL(pasteboardURL);
}

long WebPlatformStrategies::setColor(const Color& color, const String& pasteboardName)
{
    return PlatformPasteboard(pasteboardName).setColor(color);
}

long WebPlatformStrategies::setStringForType(const String& string, const String& pasteboardType, const String& pasteboardName)
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

long WebPlatformStrategies::writeCustomData(const WebCore::PasteboardCustomData& data, const String& pasteboardName)
{
    return PlatformPasteboard(pasteboardName).write(data);
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

int WebPlatformStrategies::getPasteboardItemsCount(const String& pasteboardName)
{
    return PlatformPasteboard(pasteboardName).count();
}

void WebPlatformStrategies::updateSupportedTypeIdentifiers(const Vector<String>& identifiers, const String& pasteboardName)
{
    PlatformPasteboard(pasteboardName).updateSupportedTypeIdentifiers(identifiers);
}

RefPtr<WebCore::SharedBuffer> WebPlatformStrategies::readBufferFromPasteboard(int index, const String& type, const String& pasteboardName)
{
    return PlatformPasteboard(pasteboardName).readBuffer(index, type);
}

URL WebPlatformStrategies::readURLFromPasteboard(int index, const String& pasteboardName, String& title)
{
    return PlatformPasteboard(pasteboardName).readURL(index, title);
}

String WebPlatformStrategies::readStringFromPasteboard(int index, const String& type, const String& pasteboardName)
{
    return PlatformPasteboard(pasteboardName).readString(index, type);
}

Vector<WebCore::PasteboardItemInfo> WebPlatformStrategies::allPasteboardItemInfo(const String& pasteboardName)
{
    return PlatformPasteboard(pasteboardName).allPasteboardItemInfo();
}

WebCore::PasteboardItemInfo WebPlatformStrategies::informationForItemAtIndex(int index, const String& pasteboardName)
{
    return PlatformPasteboard(pasteboardName).informationForItemAtIndex(index);
}
#endif // PLATFORM(IOS_FAMILY)
