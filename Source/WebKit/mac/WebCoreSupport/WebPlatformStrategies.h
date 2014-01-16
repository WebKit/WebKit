/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef WebPlatformStrategies_h
#define WebPlatformStrategies_h

#include <WebCore/CookiesStrategy.h>
#include <WebCore/DatabaseStrategy.h>
#include <WebCore/LoaderStrategy.h>
#include <WebCore/PasteboardStrategy.h>
#include <WebCore/PlatformStrategies.h>
#include <WebCore/PluginStrategy.h>
#include <WebCore/SharedWorkerStrategy.h>
#include <WebCore/StorageStrategy.h>
#include <WebCore/VisitedLinkStrategy.h>

struct PasteboardImage;
struct PasteboardWebContent;

class WebPlatformStrategies : public WebCore::PlatformStrategies, private WebCore::CookiesStrategy, private WebCore::DatabaseStrategy, private WebCore::LoaderStrategy, private WebCore::PasteboardStrategy, private WebCore::PluginStrategy, private WebCore::SharedWorkerStrategy, private WebCore::StorageStrategy, private WebCore::VisitedLinkStrategy {
public:
    static void initializeIfNecessary();
    
private:
    WebPlatformStrategies();
    
    // WebCore::PlatformStrategies
    virtual WebCore::CookiesStrategy* createCookiesStrategy() override;
    virtual WebCore::DatabaseStrategy* createDatabaseStrategy() override;
    virtual WebCore::LoaderStrategy* createLoaderStrategy() override;
    virtual WebCore::PasteboardStrategy* createPasteboardStrategy() override;
    virtual WebCore::PluginStrategy* createPluginStrategy() override;
    virtual WebCore::SharedWorkerStrategy* createSharedWorkerStrategy() override;
    virtual WebCore::StorageStrategy* createStorageStrategy() override;
    virtual WebCore::VisitedLinkStrategy* createVisitedLinkStrategy() override;

    // WebCore::CookiesStrategy
    virtual String cookiesForDOM(const WebCore::NetworkStorageSession&, const WebCore::URL& firstParty, const WebCore::URL&) override;
    virtual void setCookiesFromDOM(const WebCore::NetworkStorageSession&, const WebCore::URL& firstParty, const WebCore::URL&, const String&) override;
    virtual bool cookiesEnabled(const WebCore::NetworkStorageSession&, const WebCore::URL& firstParty, const WebCore::URL&) override;
    virtual String cookieRequestHeaderFieldValue(const WebCore::NetworkStorageSession&, const WebCore::URL& firstParty, const WebCore::URL&) override;
    virtual bool getRawCookies(const WebCore::NetworkStorageSession&, const WebCore::URL& firstParty, const WebCore::URL&, Vector<WebCore::Cookie>&) override;
    virtual void deleteCookie(const WebCore::NetworkStorageSession&, const WebCore::URL&, const String&) override;

    // WebCore::DatabaseStrategy
    // - Using default implementation.

    // WebCore::LoaderStrategy
    // - Using default implementation.

    // WebCore::PluginStrategy
    virtual void refreshPlugins() override;
    virtual void getPluginInfo(const WebCore::Page*, Vector<WebCore::PluginInfo>&) override;

    // WebCore::LoaderStrategy
    // - Using default implementation.

    // WebCore::VisitedLinkStrategy
    virtual bool isLinkVisited(WebCore::Page*, WebCore::LinkHash, const WebCore::URL& baseURL, const WTF::AtomicString& attributeURL) override;
    virtual void addVisitedLink(WebCore::Page*, WebCore::LinkHash) override;
    
    // WebCore::PasteboardStrategy
#if PLATFORM(IOS)
    virtual void writeToPasteboard(const WebCore::PasteboardWebContent&) override;
    virtual void writeToPasteboard(const WebCore::PasteboardImage&) override;
    virtual void writeToPasteboard(const String& pasteboardType, const String&) override;
    virtual int getPasteboardItemsCount() override;
    virtual String readStringFromPasteboard(int index, const String& pasteboardType) override;
    virtual PassRefPtr<WebCore::SharedBuffer> readBufferFromPasteboard(int index, const String& pasteboardType) override;
    virtual WebCore::URL readURLFromPasteboard(int index, const String& pasteboardType) override;
    virtual long changeCount() override;
#endif
    virtual void getTypes(Vector<String>& types, const String& pasteboardName) override;
    virtual PassRefPtr<WebCore::SharedBuffer> bufferForType(const String& pasteboardType, const String& pasteboardName) override;
    virtual void getPathnamesForType(Vector<String>& pathnames, const String& pasteboardType, const String& pasteboardName) override;
    virtual String stringForType(const String& pasteboardType, const String& pasteboardName) override;
    virtual long changeCount(const String& pasteboardName) override;
    virtual String uniqueName() override;
    virtual WebCore::Color color(const String& pasteboardName) override;
    virtual WebCore::URL url(const String& pasteboardName) override;

    virtual long addTypes(const Vector<String>& pasteboardTypes, const String& pasteboardName) override;
    virtual long setTypes(const Vector<String>& pasteboardTypes, const String& pasteboardName) override;
    virtual long copy(const String& fromPasteboard, const String& toPasteboard) override;
    virtual long setBufferForType(PassRefPtr<WebCore::SharedBuffer>, const String& pasteboardType, const String& pasteboardName) override;
    virtual long setPathnamesForType(const Vector<String>&, const String& pasteboardType, const String& pasteboardName) override;
    virtual long setStringForType(const String&, const String& pasteboardType, const String& pasteboardName) override;
};

#endif // WebPlatformStrategies_h
