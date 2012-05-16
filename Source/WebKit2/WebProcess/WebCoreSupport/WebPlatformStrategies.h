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

#if USE(PLATFORM_STRATEGIES)

#include <WebCore/CookiesStrategy.h>
#include <WebCore/PasteboardStrategy.h>
#include <WebCore/PlatformStrategies.h>
#include <WebCore/PluginStrategy.h>
#include <WebCore/VisitedLinkStrategy.h>

namespace WebKit {

class WebPlatformStrategies : public WebCore::PlatformStrategies, private WebCore::CookiesStrategy, private WebCore::PluginStrategy, private WebCore::VisitedLinkStrategy, private WebCore::PasteboardStrategy {
public:
    static void initialize();
    
private:
    WebPlatformStrategies();
    
    // WebCore::PlatformStrategies
    virtual WebCore::CookiesStrategy* createCookiesStrategy() OVERRIDE;
    virtual WebCore::PluginStrategy* createPluginStrategy() OVERRIDE;
    virtual WebCore::VisitedLinkStrategy* createVisitedLinkStrategy() OVERRIDE;
    virtual WebCore::PasteboardStrategy* createPasteboardStrategy() OVERRIDE;

    // WebCore::CookiesStrategy
    virtual void notifyCookiesChanged() OVERRIDE;

    // WebCore::PluginStrategy
    virtual void refreshPlugins() OVERRIDE;
    virtual void getPluginInfo(const WebCore::Page*, Vector<WebCore::PluginInfo>&) OVERRIDE;
    void populatePluginCache();

    // WebCore::VisitedLinkStrategy
    virtual bool isLinkVisited(WebCore::Page*, WebCore::LinkHash, const WebCore::KURL& baseURL, const WTF::AtomicString& attributeURL) OVERRIDE;
    virtual void addVisitedLink(WebCore::Page*, WebCore::LinkHash) OVERRIDE;

#if PLATFORM(MAC)
    // WebCore::PasteboardStrategy
    virtual void getTypes(Vector<String>& types, const String& pasteboardName) OVERRIDE;
    virtual PassRefPtr<WebCore::SharedBuffer> bufferForType(const String& pasteboardType, const String& pasteboardName) OVERRIDE;
    virtual void getPathnamesForType(Vector<String>& pathnames, const String& pasteboardType, const String& pasteboardName) OVERRIDE;
    virtual String stringForType(const String& pasteboardType, const String& pasteboardName) OVERRIDE;
    virtual int changeCount(const String& pasteboardName) OVERRIDE;
    virtual String uniqueName() OVERRIDE;
    virtual WebCore::Color color(const String& pasteboardName) OVERRIDE;
    virtual WebCore::KURL url(const String& pasteboardName) OVERRIDE;

    virtual void copy(const String& fromPasteboard, const String& toPasteboard) OVERRIDE;
    virtual void addTypes(const Vector<String>& pasteboardTypes, const String& pasteboardName) OVERRIDE;
    virtual void setTypes(const Vector<String>& pasteboardTypes, const String& pasteboardName) OVERRIDE;
    virtual void setBufferForType(PassRefPtr<WebCore::SharedBuffer>, const String& pasteboardType, const String& pasteboardName) OVERRIDE;
    virtual void setPathnamesForType(const Vector<String>&, const String& pasteboardType, const String& pasteboardName) OVERRIDE;
    virtual void setStringForType(const String&, const String& pasteboardType, const String& pasteboardName) OVERRIDE;
#endif

    bool m_pluginCacheIsPopulated;
    bool m_shouldRefreshPlugins;
    Vector<WebCore::PluginInfo> m_cachedPlugins;
};

void handleDidGetPlugins(uint64_t requestID, const Vector<WebCore::PluginInfo>&);

} // namespace WebKit

#endif // USE(PLATFORM_STRATEGIES)

#endif // WebPlatformStrategies_h
