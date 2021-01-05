/*
    Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
    Copyright (C) 2008 Trolltech ASA
    Copyright (C) 2008 Collabora Ltd. All rights reserved.
    Copyright (C) 2008 INdT - Instituto Nokia de Tecnologia
    Copyright (C) 2009-2010 ProFUSION embedded systems
    Copyright (C) 2009-2011 Samsung Electronics
    Copyright (C) 2012 Intel Corporation

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "PlatformStrategiesHaiku.h"

#include <WebCore/MediaStrategy.h>

#include "BlobRegistryImpl.h"
#include "NetworkStorageSession.h"
#include "wtf/NeverDestroyed.h"
#include "NotImplemented.h"
#include "Page.h"
#include "PageGroup.h"
#include "WebResourceLoadScheduler.h"


using namespace WebCore;


void PlatformStrategiesHaiku::initialize()
{
    static NeverDestroyed<PlatformStrategiesHaiku> platformStrategies;
    setPlatformStrategies(&platformStrategies.get());
}

PlatformStrategiesHaiku::PlatformStrategiesHaiku()
{
}

LoaderStrategy* PlatformStrategiesHaiku::createLoaderStrategy()
{
    return new WebResourceLoadScheduler();
}

PasteboardStrategy* PlatformStrategiesHaiku::createPasteboardStrategy()
{
    notImplemented();
    return 0;
}

class WebBlobRegistry final : public BlobRegistry {
private:
    void registerFileBlobURL(const URL& url, Ref<BlobDataFileReference>&& reference, const String& contentType, const String&) final { m_blobRegistry.registerFileBlobURL(url, WTFMove(reference), contentType); }
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

WebCore::BlobRegistry* PlatformStrategiesHaiku::createBlobRegistry()
{
    return new WebBlobRegistry;
}


bool PlatformStrategiesHaiku::cookiesEnabled(const NetworkStorageSession& session)
{
	return true;
}


bool PlatformStrategiesHaiku::getRawCookies(const NetworkStorageSession& session, const URL& firstParty, const WebCore::SameSiteInfo& sameSite, const URL& url,
	WTF::Optional<FrameIdentifier> frameID, WTF::Optional<PageIdentifier> pageID,
	Vector<Cookie>& rawCookies)
{
    return session.getRawCookies(firstParty, sameSite, url, frameID, pageID, ShouldAskITP::No, ShouldRelaxThirdPartyCookieBlocking::Yes, rawCookies);
}

void PlatformStrategiesHaiku::deleteCookie(const NetworkStorageSession& session, const URL& url, const String& cookieName)
{
    session.deleteCookie(url, cookieName);
}


class WebMediaStrategy final : public MediaStrategy {
private:
#if ENABLE(WEB_AUDIO)
    std::unique_ptr<AudioDestination> createAudioDestination(AudioIOCallback& callback, const String& inputDeviceId,
        unsigned numberOfInputChannels, unsigned numberOfOutputChannels, float sampleRate) override
    {
        return AudioDestination::create(callback, inputDeviceId, numberOfInputChannels, numberOfOutputChannels, sampleRate);
    }
#endif
};

MediaStrategy* PlatformStrategiesHaiku::createMediaStrategy()
{
    return new WebMediaStrategy;
}

