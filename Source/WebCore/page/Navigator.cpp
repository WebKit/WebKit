/*
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (c) 2000 Daniel Molkentin (molkentin@kde.org)
 *  Copyright (c) 2000 Stefan Schimanski (schimmi@kde.org)
 *  Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
 *  Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "Navigator.h"

#include "Chrome.h"
#include "CookieJar.h"
#include "DOMMimeTypeArray.h"
#include "DOMPluginArray.h"
#include "Document.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "Geolocation.h"
#include "PointerLock.h"
#include "KURL.h"
#include "Language.h"
#include "Page.h"
#include "PageGroup.h"
#include "PlatformString.h"
#include "PluginData.h"
#include "Settings.h"
#include "StorageNamespace.h"
#include <wtf/HashSet.h>
#include <wtf/StdLibExtras.h>

#if ENABLE(GAMEPAD)
#include "GamepadList.h"
#include "Gamepads.h"
#endif

#if ENABLE(MEDIA_STREAM)
#include "NavigatorUserMediaErrorCallback.h"
#include "NavigatorUserMediaSuccessCallback.h"
#include "UserMediaRequest.h"
#endif

namespace WebCore {

Navigator::Navigator(Frame* frame)
    : DOMWindowProperty(frame)
{
}

Navigator::~Navigator()
{
}

void Navigator::resetGeolocation()
{
    if (m_geolocation)
        m_geolocation->reset();
}

// If this function returns true, we need to hide the substring "4." that would otherwise
// appear in the appVersion string. This is to avoid problems with old versions of a
// library called OpenCube QuickMenu, which as of this writing is still being used on
// sites such as nwa.com -- the library thinks Safari is Netscape 4 if we don't do this!
static bool shouldHideFourDot(Frame* frame)
{
    const String* sourceURL = frame->script()->sourceURL();
    if (!sourceURL)
        return false;
    if (!(sourceURL->endsWith("/dqm_script.js") || sourceURL->endsWith("/dqm_loader.js") || sourceURL->endsWith("/tdqm_loader.js")))
        return false;
    Settings* settings = frame->settings();
    if (!settings)
        return false;
    return settings->needsSiteSpecificQuirks();
}

String Navigator::appVersion() const
{
    if (!m_frame)
        return String();
    String appVersion = NavigatorBase::appVersion();
    if (shouldHideFourDot(m_frame))
        appVersion.replace("4.", "4_");
    return appVersion;
}

String Navigator::language() const
{
    return defaultLanguage();
}

String Navigator::userAgent() const
{
    if (!m_frame)
        return String();
        
    // If the frame is already detached, FrameLoader::userAgent may malfunction, because it calls a client method
    // that uses frame's WebView (at least, in Mac WebKit).
    if (!m_frame->page())
        return String();
        
    return m_frame->loader()->userAgent(m_frame->document()->url());
}

DOMPluginArray* Navigator::plugins() const
{
    if (!m_plugins)
        m_plugins = DOMPluginArray::create(m_frame);
    return m_plugins.get();
}

DOMMimeTypeArray* Navigator::mimeTypes() const
{
    if (!m_mimeTypes)
        m_mimeTypes = DOMMimeTypeArray::create(m_frame);
    return m_mimeTypes.get();
}

bool Navigator::cookieEnabled() const
{
    if (!m_frame)
        return false;
        
    if (m_frame->page() && !m_frame->page()->cookieEnabled())
        return false;

    return cookiesEnabled(m_frame->document());
}

bool Navigator::javaEnabled() const
{
    if (!m_frame || !m_frame->settings())
        return false;

    return m_frame->settings()->isJavaEnabled();
}

Geolocation* Navigator::geolocation() const
{
    if (!m_geolocation)
        m_geolocation = Geolocation::create(m_frame);
    return m_geolocation.get();
}

#if ENABLE(POINTER_LOCK)
PointerLock* Navigator::webkitPointer() const
{
    if (!m_pointer)
        m_pointer = PointerLock::create();
    return m_pointer.get();
}
#endif

void Navigator::getStorageUpdates()
{
    // FIXME: Remove this method or rename to yieldForStorageUpdates.
}

#if ENABLE(REGISTER_PROTOCOL_HANDLER)
static HashSet<String>* protocolWhitelist;

static void initProtocolHandlerWhitelist()
{
    protocolWhitelist = new HashSet<String>;
    static const char* protocols[] = {
        "irc",
        "mailto",
        "mms",
        "news",
        "nntp",
        "sms",
        "smsto",
        "tel",
        "urn",
        "webcal",
    };
    for (size_t i = 0; i < WTF_ARRAY_LENGTH(protocols); ++i)
        protocolWhitelist->add(protocols[i]);
}

static bool verifyCustomHandlerURL(const String& baseURL, const String& url, ExceptionCode& ec)
{
    // The specification requires that it is a SYNTAX_ERR if the "%s" token is
    // not present.
    static const char token[] = "%s";
    int index = url.find(token);
    if (-1 == index) {
        ec = SYNTAX_ERR;
        return false;
    }

    // It is also a SYNTAX_ERR if the custom handler URL, as created by removing
    // the "%s" token and prepending the base url, does not resolve.
    String newURL = url;
    newURL.remove(index, WTF_ARRAY_LENGTH(token) - 1);

    KURL base(ParsedURLString, baseURL);
    KURL kurl(base, newURL);

    if (kurl.isEmpty() || !kurl.isValid()) {
        ec = SYNTAX_ERR;
        return false;
    }

    return true;
}

static bool isProtocolWhitelisted(const String& scheme)
{
    if (!protocolWhitelist)
        initProtocolHandlerWhitelist();
    return protocolWhitelist->contains(scheme);
}

static bool verifyProtocolHandlerScheme(const String& scheme, ExceptionCode& ec)
{
    if (scheme.startsWith("web+")) {
        if (isValidProtocol(scheme))
            return true;
        ec = SECURITY_ERR;
        return false;
    }

    if (isProtocolWhitelisted(scheme))
        return true;
    ec = SECURITY_ERR;
    return false;
}

void Navigator::registerProtocolHandler(const String& scheme, const String& url, const String& title, ExceptionCode& ec)
{
    if (!m_frame)
        return;

    Document* document = m_frame->document();
    if (!document)
        return;

    String baseURL = document->baseURL().baseAsString();

    if (!verifyCustomHandlerURL(baseURL, url, ec))
        return;

    if (!verifyProtocolHandlerScheme(scheme, ec))
        return;

    Page* page = m_frame->page();
    if (!page)
        return;

    page->chrome()->registerProtocolHandler(scheme, baseURL, url, m_frame->displayStringModifiedByEncoding(title));
}
#endif

#if ENABLE(MEDIA_STREAM)
void Navigator::webkitGetUserMedia(const String& options, PassRefPtr<NavigatorUserMediaSuccessCallback> successCallback, PassRefPtr<NavigatorUserMediaErrorCallback> errorCallback, ExceptionCode& ec)
{
    if (!successCallback)
        return;

    if (!m_frame)
        return;

    Page* page = m_frame->page();
    if (!page)
        return;

    RefPtr<UserMediaRequest> request = UserMediaRequest::create(m_frame->document(), page->userMediaClient(), options, successCallback, errorCallback);
    if (!request) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }

    request->start();
}
#endif

#if ENABLE(GAMEPAD)
GamepadList* Navigator::gamepads()
{
    if (!m_gamepads)
        m_gamepads = GamepadList::create();
    sampleGamepads(m_gamepads.get());
    return m_gamepads.get();
}
#endif

} // namespace WebCore
