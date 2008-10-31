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

#include "CookieJar.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "Geolocation.h"
#include "Language.h"
#include "MimeTypeArray.h"
#include "NetworkStateNotifier.h"
#include "Page.h"
#include "PlatformString.h"
#include "PluginArray.h"
#include "PluginData.h"
#include "ScriptController.h"
#include "Settings.h"

#ifndef WEBCORE_NAVIGATOR_PLATFORM
#if PLATFORM(MAC) && PLATFORM(PPC)
#define WEBCORE_NAVIGATOR_PLATFORM "MacPPC"
#elif PLATFORM(MAC) && PLATFORM(X86)
#define WEBCORE_NAVIGATOR_PLATFORM "MacIntel"
#elif PLATFORM(WIN_OS)
#define WEBCORE_NAVIGATOR_PLATFORM "Win32"
#else
#define WEBCORE_NAVIGATOR_PLATFORM ""
#endif
#endif // ifndef WEBCORE_NAVIGATOR_PLATFORM

#ifndef WEBCORE_NAVIGATOR_PRODUCT
#define WEBCORE_NAVIGATOR_PRODUCT "Gecko"
#endif // ifndef WEBCORE_NAVIGATOR_PRODUCT

#ifndef WEBCORE_NAVIGATOR_PRODUCT_SUB
#define WEBCORE_NAVIGATOR_PRODUCT_SUB "20030107"
#endif // ifndef WEBCORE_NAVIGATOR_PRODUCT_SUB

#ifndef WEBCORE_NAVIGATOR_VENDOR
#define WEBCORE_NAVIGATOR_VENDOR "Apple Computer, Inc."
#endif // ifndef WEBCORE_NAVIGATOR_VENDOR

#ifndef WEBCORE_NAVIGATOR_VENDOR_SUB
#define WEBCORE_NAVIGATOR_VENDOR_SUB ""
#endif // ifndef WEBCORE_NAVIGATOR_VENDOR_SUB


namespace WebCore {

Navigator::Navigator(Frame* frame)
    : m_frame(frame)
{
}

Navigator::~Navigator()
{
    disconnectFrame();
}

void Navigator::disconnectFrame()
{
    if (m_plugins) {
        m_plugins->disconnectFrame();
        m_plugins = 0;
    }
    if (m_mimeTypes) {
        m_mimeTypes->disconnectFrame();
        m_mimeTypes = 0;
    }
    if (m_geolocation) {
        m_geolocation->disconnectFrame();
        m_geolocation = 0;
    }
    m_frame = 0;
}

String Navigator::appCodeName() const
{
    return "Mozilla";
}

String Navigator::appName() const
{
    return "Netscape";
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
    if (!(sourceURL->endsWith("/dqm_script.js") || sourceURL->endsWith("/dqm_loader.js")))
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
    // Version is everything in the user agent string past the "Mozilla/" prefix.
    const String& userAgent = m_frame->loader()->userAgent(m_frame->document() ? m_frame->document()->url() : KURL());
    String appVersion = userAgent.substring(userAgent.find('/') + 1);
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
    return m_frame->loader()->userAgent(m_frame->document() ? m_frame->document()->url() : KURL());
}

String Navigator::platform() const
{
    return WEBCORE_NAVIGATOR_PLATFORM;
}

PluginArray* Navigator::plugins() const
{
    if (!m_plugins)
        m_plugins = PluginArray::create(m_frame);
    return m_plugins.get();
}

MimeTypeArray* Navigator::mimeTypes() const
{
    if (!m_mimeTypes)
        m_mimeTypes = MimeTypeArray::create(m_frame);
    return m_mimeTypes.get();
}

String Navigator::product() const
{
    return WEBCORE_NAVIGATOR_PRODUCT;
}

String Navigator::productSub() const
{
    return WEBCORE_NAVIGATOR_PRODUCT_SUB;
}

String Navigator::vendor() const
{
    return WEBCORE_NAVIGATOR_VENDOR;
}

String Navigator::vendorSub() const
{
    return WEBCORE_NAVIGATOR_VENDOR_SUB;
}

bool Navigator::cookieEnabled() const
{
    if (m_frame->page() && !m_frame->page()->cookieEnabled())
        return false;

    return cookiesEnabled(m_frame->document());
}

bool Navigator::javaEnabled() const
{
    if (!m_frame)
        return false;
    return m_frame->settings()->isJavaEnabled();
}
    
bool Navigator::onLine() const
{
    return networkStateNotifier().onLine();
}

Geolocation* Navigator::geolocation() const
{
    if (!m_geolocation)
        m_geolocation = Geolocation::create(m_frame);
    return m_geolocation.get();
}
    
} // namespace WebCore
