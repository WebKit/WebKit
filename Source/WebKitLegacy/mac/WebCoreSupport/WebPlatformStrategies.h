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

#include <WebCore/LoaderStrategy.h>
#include <WebCore/PasteboardStrategy.h>
#include <WebCore/PlatformStrategies.h>

class PasteboardCustomData;
struct PasteboardImage;
struct PasteboardWebContent;

class WebPlatformStrategies : public WebCore::PlatformStrategies, private WebCore::PasteboardStrategy {
    WTF_MAKE_TZONE_ALLOCATED(WebPlatformStrategies);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(WebPlatformStrategies);
public:
    static void initializeIfNecessary();
    WebPlatformStrategies();

private:
    // WebCore::PlatformStrategies
    WebCore::LoaderStrategy* createLoaderStrategy() override;
    WebCore::PasteboardStrategy* createPasteboardStrategy() override;
    WebCore::MediaStrategy* createMediaStrategy() override;
    WebCore::BlobRegistry* createBlobRegistry() override;

#if ENABLE(DECLARATIVE_WEB_PUSH)
    WebCore::PushStrategy* createPushStrategy() override { return nullptr; }
#endif

    // WebCore::PasteboardStrategy
#if PLATFORM(IOS_FAMILY)
    void writeToPasteboard(const WebCore::PasteboardURL&, StringImpl* pasteboardName, const WebCore::PasteboardContext*) override;
    void writeToPasteboard(const WebCore::PasteboardWebContent&, StringImpl* pasteboardName, const WebCore::PasteboardContext*) override;
    void writeToPasteboard(const WebCore::PasteboardImage&, StringImpl* pasteboardName, const WebCore::PasteboardContext*) override;
    void writeToPasteboard(StringImpl* pasteboardType, StringImpl*, StringImpl* pasteboardName, const WebCore::PasteboardContext*) override;
    void updateSupportedTypeIdentifiers(const Vector<RefPtr<StringImpl>>& identifiers, StringImpl* pasteboardName, const WebCore::PasteboardContext*) override;
#endif
    RefPtr<StringImpl> readStringFromPasteboard(size_t index, StringImpl* pasteboardType, StringImpl* pasteboardName, const WebCore::PasteboardContext*) override;
    RefPtr<WebCore::SharedBuffer> readBufferFromPasteboard(std::optional<size_t> index, StringImpl* pasteboardType, StringImpl* pasteboardName, const WebCore::PasteboardContext*) override;
    URL readURLFromPasteboard(size_t index, StringImpl* pasteboardName, RefPtr<StringImpl>& title, const WebCore::PasteboardContext*) override;
    int getPasteboardItemsCount(StringImpl* pasteboardName, const WebCore::PasteboardContext*) override;
    std::optional<WebCore::PasteboardItemInfo> informationForItemAtIndex(size_t index, StringImpl* pasteboardName, int64_t changeCount, const WebCore::PasteboardContext*) override;
    std::optional<Vector<WebCore::PasteboardItemInfo>> allPasteboardItemInfo(StringImpl* pasteboardName, int64_t changeCount, const WebCore::PasteboardContext*) override;
    int getNumberOfFiles(StringImpl* pasteboardName, const WebCore::PasteboardContext*) override;
    void getTypes(Vector<RefPtr<StringImpl>>& types, StringImpl* pasteboardName, const WebCore::PasteboardContext*) override;
    RefPtr<WebCore::SharedBuffer> bufferForType(StringImpl* pasteboardType, StringImpl* pasteboardName, const WebCore::PasteboardContext*) override;
    void getPathnamesForType(Vector<RefPtr<StringImpl>>& pathnames, StringImpl* pasteboardType, StringImpl* pasteboardName, const WebCore::PasteboardContext*) override;
    RefPtr<StringImpl> stringForType(StringImpl* pasteboardType, StringImpl* pasteboardName, const WebCore::PasteboardContext*) override;
    Vector<RefPtr<StringImpl>> allStringsForType(StringImpl* pasteboardType, StringImpl* pasteboardName, const WebCore::PasteboardContext*) override;
    int64_t changeCount(StringImpl* pasteboardName, const WebCore::PasteboardContext*) override;
    WebCore::Color color(StringImpl* pasteboardName, const WebCore::PasteboardContext*) override;
    URL url(StringImpl* pasteboardName, const WebCore::PasteboardContext*) override;

    int64_t writeCustomData(const Vector<WebCore::PasteboardCustomData>&, StringImpl* pasteboardName, const WebCore::PasteboardContext*) override;
    Vector<RefPtr<StringImpl>> typesSafeForDOMToReadAndWrite(StringImpl* pasteboardName, StringImpl* origin, const WebCore::PasteboardContext*) override;

    int64_t addTypes(const Vector<RefPtr<StringImpl>>& pasteboardTypes, StringImpl* pasteboardName, const WebCore::PasteboardContext*) override;
    int64_t setTypes(const Vector<RefPtr<StringImpl>>& pasteboardTypes, StringImpl* pasteboardName, const WebCore::PasteboardContext*) override;
    int64_t setBufferForType(WebCore::SharedBuffer*, StringImpl* pasteboardType, StringImpl* pasteboardName, const WebCore::PasteboardContext*) override;
    int64_t setURL(const WebCore::PasteboardURL&, StringImpl* pasteboardName, const WebCore::PasteboardContext*) override;
    int64_t setColor(const WebCore::Color&, StringImpl* pasteboardName, const WebCore::PasteboardContext*) override;
    int64_t setStringForType(StringImpl*, StringImpl* pasteboardType, StringImpl* pasteboardName, const WebCore::PasteboardContext*) override;
    int64_t writeWebArchive(WebCore::LegacyWebArchive&, StringImpl* pasteboardName) override { return 0; }
    bool containsStringSafeForDOMToReadForType(StringImpl*, StringImpl* pasteboardName, const WebCore::PasteboardContext*) override;
    bool containsURLStringSuitableForLoading(StringImpl* pasteboardName, const WebCore::PasteboardContext*) override;
    RefPtr<StringImpl> urlStringSuitableForLoading(StringImpl* pasteboardName, RefPtr<StringImpl>& title, const WebCore::PasteboardContext*) override;
};

