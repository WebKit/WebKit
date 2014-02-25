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

#import "WebPlatformStrategies.h"

#import "WebFrameNetworkingContext.h"
#import "WebPluginDatabase.h"
#import "WebPluginPackage.h"
#import <WebCore/BlockExceptions.h>
#import <WebCore/Color.h>
#import <WebCore/MainFrame.h>
#import <WebCore/Page.h>
#import <WebCore/PageGroup.h>
#import <WebCore/PlatformCookieJar.h>
#import <WebCore/PlatformPasteboard.h>
#import <WebCore/SharedBuffer.h>
#import <WebCore/SubframeLoader.h>
#import <WebKitSystemInterface.h>

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

CookiesStrategy* WebPlatformStrategies::createCookiesStrategy()
{
    return this;
}

DatabaseStrategy* WebPlatformStrategies::createDatabaseStrategy()
{
    return this;
}

LoaderStrategy* WebPlatformStrategies::createLoaderStrategy()
{
    return this;
}

PasteboardStrategy* WebPlatformStrategies::createPasteboardStrategy()
{
    return this;
}

PluginStrategy* WebPlatformStrategies::createPluginStrategy()
{
    return this;
}

SharedWorkerStrategy* WebPlatformStrategies::createSharedWorkerStrategy()
{
    return this;
}

StorageStrategy* WebPlatformStrategies::createStorageStrategy()
{
    return this;
}

String WebPlatformStrategies::cookiesForDOM(const NetworkStorageSession& session, const URL& firstParty, const URL& url)
{
    return WebCore::cookiesForDOM(session, firstParty, url);
}

void WebPlatformStrategies::setCookiesFromDOM(const NetworkStorageSession& session, const URL& firstParty, const URL& url, const String& cookieString)
{
    WebCore::setCookiesFromDOM(session, firstParty, url, cookieString);
}

bool WebPlatformStrategies::cookiesEnabled(const NetworkStorageSession& session, const URL& firstParty, const URL& url)
{
    return WebCore::cookiesEnabled(session, firstParty, url);
}

String WebPlatformStrategies::cookieRequestHeaderFieldValue(const NetworkStorageSession& session, const URL& firstParty, const URL& url)
{
    return WebCore::cookieRequestHeaderFieldValue(session, firstParty, url);
}

bool WebPlatformStrategies::getRawCookies(const NetworkStorageSession& session, const URL& firstParty, const URL& url, Vector<Cookie>& rawCookies)
{
    return WebCore::getRawCookies(session, firstParty, url, rawCookies);
}

void WebPlatformStrategies::deleteCookie(const NetworkStorageSession& session, const URL& url, const String& cookieName)
{
    WebCore::deleteCookie(session, url, cookieName);
}

void WebPlatformStrategies::refreshPlugins()
{
    [[WebPluginDatabase sharedDatabaseIfExists] refresh];
}

void WebPlatformStrategies::getPluginInfo(const Page* page, Vector<PluginInfo>& plugins)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    // WebKit1 has no application plug-ins, so we don't need to add them here.
    if (!page->mainFrame().loader().subframeLoader().allowPlugins(NotAboutToInstantiatePlugin))
        return;

    NSArray* pluginsArray = [[WebPluginDatabase sharedDatabase] plugins];
    for (unsigned int i = 0; i < [pluginsArray count]; ++i) {
        WebPluginPackage *plugin = [pluginsArray objectAtIndex:i];

        plugins.append([plugin pluginInfo]);
    }
    
    END_BLOCK_OBJC_EXCEPTIONS;
}

void WebPlatformStrategies::getTypes(Vector<String>& types, const String& pasteboardName)
{
    PlatformPasteboard(pasteboardName).getTypes(types);
}

PassRefPtr<SharedBuffer> WebPlatformStrategies::bufferForType(const String& pasteboardType, const String& pasteboardName)
{
    return PlatformPasteboard(pasteboardName).bufferForType(pasteboardType);
}

void WebPlatformStrategies::getPathnamesForType(Vector<String>& pathnames, const String& pasteboardType, const String& pasteboardName)
{
    PlatformPasteboard(pasteboardName).getPathnamesForType(pathnames, pasteboardType);
}

String WebPlatformStrategies::stringForType(const String& pasteboardType, const String& pasteboardName)
{
    return PlatformPasteboard(pasteboardName).stringForType(pasteboardType);
}

long WebPlatformStrategies::copy(const String& fromPasteboard, const String& toPasteboard)
{
    return PlatformPasteboard(toPasteboard).copy(fromPasteboard);
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

long WebPlatformStrategies::setBufferForType(PassRefPtr<SharedBuffer> buffer, const String& pasteboardType, const String& pasteboardName)
{
    return PlatformPasteboard(pasteboardName).setBufferForType(buffer, pasteboardType);
}

long WebPlatformStrategies::setPathnamesForType(const Vector<String>& pathnames, const String& pasteboardType, const String& pasteboardName)
{
    return PlatformPasteboard(pasteboardName).setPathnamesForType(pathnames, pasteboardType);
}

long WebPlatformStrategies::setStringForType(const String& string, const String& pasteboardType, const String& pasteboardName)
{
    return PlatformPasteboard(pasteboardName).setStringForType(string, pasteboardType);
}

#if PLATFORM(IOS)
void WebPlatformStrategies::writeToPasteboard(const WebCore::PasteboardWebContent& content)
{
    PlatformPasteboard().write(content);
}

void WebPlatformStrategies::writeToPasteboard(const WebCore::PasteboardImage& image)
{
    PlatformPasteboard().write(image);
}

void WebPlatformStrategies::writeToPasteboard(const String& pasteboardType, const String& text)
{
    PlatformPasteboard().write(pasteboardType, text);
}

int WebPlatformStrategies::getPasteboardItemsCount()
{
    return PlatformPasteboard().count();
}

PassRefPtr<WebCore::SharedBuffer> WebPlatformStrategies::readBufferFromPasteboard(int index, const String& type)
{
    return PlatformPasteboard().readBuffer(index, type);
}

WebCore::URL WebPlatformStrategies::readURLFromPasteboard(int index, const String& type)
{
    return PlatformPasteboard().readURL(index, type);
}

String WebPlatformStrategies::readStringFromPasteboard(int index, const String& type)
{
    return PlatformPasteboard().readString(index, type);
}

long WebPlatformStrategies::changeCount()
{
    return PlatformPasteboard().changeCount();
}

#endif
