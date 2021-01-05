/*
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

#ifndef PlatformStrategiesHaiku_h
#define PlatformStrategiesHaiku_h

#include "LoaderStrategy.h"
#include "NetworkStorageSession.h"
#include "PasteboardStrategy.h"
#include "PlatformStrategies.h"

#include <wtf/Forward.h>

#include "WebCore/PageIdentifier.h"

class PlatformStrategiesHaiku : public WebCore::PlatformStrategies {
public:
    static void initialize();

private:
    PlatformStrategiesHaiku();

    friend class NeverDestroyed<PlatformStrategiesHaiku>;

    // WebCore::PlatformStrategies
    WebCore::LoaderStrategy* createLoaderStrategy() override;
    WebCore::PasteboardStrategy* createPasteboardStrategy() override;
    WebCore::BlobRegistry* createBlobRegistry() override;
	WebCore::MediaStrategy* createMediaStrategy() override;

    // WebCore::CookiesStrategy
    virtual bool cookiesEnabled(const WebCore::NetworkStorageSession&);
    virtual bool getRawCookies(const WebCore::NetworkStorageSession&,
		const WTF::URL& firstParty, const WebCore::SameSiteInfo&, const WTF::URL&,
		WTF::Optional<WebCore::FrameIdentifier> frameID, WTF::Optional<WebCore::PageIdentifier> pageID,
		Vector<WebCore::Cookie>&);
    virtual void deleteCookie(const WebCore::NetworkStorageSession&, const WTF::URL&, const String&);
};

#endif // PlatformStrategiesHaiku_h
