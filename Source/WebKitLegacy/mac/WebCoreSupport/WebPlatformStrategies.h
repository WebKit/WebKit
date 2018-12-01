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

#pragma once

#include <WebCore/CookiesStrategy.h>
#include <WebCore/LoaderStrategy.h>
#include <WebCore/PasteboardStrategy.h>
#include <WebCore/PlatformStrategies.h>

struct PasteboardImage;
struct PasteboardWebContent;
struct PasteboardCustomData;

class WebPlatformStrategies : public WebCore::PlatformStrategies, private WebCore::CookiesStrategy, private WebCore::PasteboardStrategy {
public:
    static void initializeIfNecessary();
    
private:
    WebPlatformStrategies();
    
    // WebCore::PlatformStrategies
    WebCore::CookiesStrategy* createCookiesStrategy() override;
    WebCore::LoaderStrategy* createLoaderStrategy() override;
    WebCore::PasteboardStrategy* createPasteboardStrategy() override;
    WebCore::BlobRegistry* createBlobRegistry() override;

    // WebCore::CookiesStrategy
    std::pair<String, bool> cookiesForDOM(const WebCore::NetworkStorageSession&, const URL& firstParty, const WebCore::SameSiteInfo&, const URL&, std::optional<uint64_t> frameID, std::optional<uint64_t> pageID, WebCore::IncludeSecureCookies) override;
    void setCookiesFromDOM(const WebCore::NetworkStorageSession&, const URL& firstParty, const WebCore::SameSiteInfo&, const URL&, std::optional<uint64_t> frameID, std::optional<uint64_t> pageID, const String&) override;
    bool cookiesEnabled(const WebCore::NetworkStorageSession&) override;
    std::pair<String, bool> cookieRequestHeaderFieldValue(const WebCore::NetworkStorageSession&, const URL& firstParty, const WebCore::SameSiteInfo&, const URL&, std::optional<uint64_t> frameID, std::optional<uint64_t> pageID, WebCore::IncludeSecureCookies) override;
    std::pair<String, bool> cookieRequestHeaderFieldValue(PAL::SessionID, const URL& firstParty, const WebCore::SameSiteInfo&, const URL&, std::optional<uint64_t> frameID, std::optional<uint64_t> pageID, WebCore::IncludeSecureCookies) override;
    bool getRawCookies(const WebCore::NetworkStorageSession&, const URL& firstParty, const WebCore::SameSiteInfo&, const URL&, std::optional<uint64_t> frameID, std::optional<uint64_t> pageID, Vector<WebCore::Cookie>&) override;
    void deleteCookie(const WebCore::NetworkStorageSession&, const URL&, const String&) override;

    // WebCore::PasteboardStrategy
#if PLATFORM(IOS_FAMILY)
    void writeToPasteboard(const WebCore::PasteboardURL&, const String& pasteboardName) override;
    void writeToPasteboard(const WebCore::PasteboardWebContent&, const String& pasteboardName) override;
    void writeToPasteboard(const WebCore::PasteboardImage&, const String& pasteboardName) override;
    void writeToPasteboard(const String& pasteboardType, const String&, const String& pasteboardName) override;
    int getPasteboardItemsCount(const String& pasteboardName) override;
    String readStringFromPasteboard(int index, const String& pasteboardType, const String& pasteboardName) override;
    RefPtr<WebCore::SharedBuffer> readBufferFromPasteboard(int index, const String& pasteboardType, const String& pasteboardName) override;
    URL readURLFromPasteboard(int index, const String& pasteboardName, String& title) override;
    Vector<WebCore::PasteboardItemInfo> allPasteboardItemInfo(const String& pasteboardName) override;
    WebCore::PasteboardItemInfo informationForItemAtIndex(int index, const String& pasteboardName) override;
    void updateSupportedTypeIdentifiers(const Vector<String>& identifiers, const String& pasteboardName) override;
    void getTypesByFidelityForItemAtIndex(Vector<String>& types, uint64_t index, const String& pasteboardName) override;
#endif
    int getNumberOfFiles(const String& pasteboardName) override;
    void getTypes(Vector<String>& types, const String& pasteboardName) override;
    RefPtr<WebCore::SharedBuffer> bufferForType(const String& pasteboardType, const String& pasteboardName) override;
    void getPathnamesForType(Vector<String>& pathnames, const String& pasteboardType, const String& pasteboardName) override;
    String stringForType(const String& pasteboardType, const String& pasteboardName) override;
    Vector<String> allStringsForType(const String& pasteboardType, const String& pasteboardName) override;
    long changeCount(const String& pasteboardName) override;
    String uniqueName() override;
    WebCore::Color color(const String& pasteboardName) override;
    URL url(const String& pasteboardName) override;

    long writeCustomData(const WebCore::PasteboardCustomData&, const String& pasteboardName) override;
    Vector<String> typesSafeForDOMToReadAndWrite(const String& pasteboardName, const String& origin) override;

    long addTypes(const Vector<String>& pasteboardTypes, const String& pasteboardName) override;
    long setTypes(const Vector<String>& pasteboardTypes, const String& pasteboardName) override;
    long setBufferForType(WebCore::SharedBuffer*, const String& pasteboardType, const String& pasteboardName) override;
    long setURL(const WebCore::PasteboardURL&, const String& pasteboardName) override;
    long setColor(const WebCore::Color&, const String& pasteboardName) override;
    long setStringForType(const String&, const String& pasteboardType, const String& pasteboardName) override;
};

