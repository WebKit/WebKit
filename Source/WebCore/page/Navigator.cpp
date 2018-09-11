/*
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (c) 2000 Daniel Molkentin (molkentin@kde.org)
 *  Copyright (c) 2000 Stefan Schimanski (schimmi@kde.org)
 *  Copyright (C) 2003, 2004, 2005, 2006 Apple Inc.
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
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "Geolocation.h"
#include "JSDOMPromiseDeferred.h"
#include "LoaderStrategy.h"
#include "Page.h"
#include "PlatformStrategies.h"
#include "PluginData.h"
#include "ResourceLoadObserver.h"
#include "RuntimeEnabledFeatures.h"
#include "ScriptController.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include <wtf/Language.h>
#include <wtf/StdLibExtras.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
using namespace WTF;

Navigator::Navigator(ScriptExecutionContext& context, Frame& frame)
    : NavigatorBase(context)
    , DOMWindowProperty(&frame)
{
}

Navigator::~Navigator() = default;

// If this function returns true, we need to hide the substring "4." that would otherwise
// appear in the appVersion string. This is to avoid problems with old versions of a
// library called OpenCube QuickMenu, which as of this writing is still being used on
// sites such as nwa.com -- the library thinks Safari is Netscape 4 if we don't do this!
static bool shouldHideFourDot(Frame& frame)
{
    auto* sourceURL = frame.script().sourceURL();
    if (!sourceURL)
        return false;
    if (!(sourceURL->endsWith("/dqm_script.js") || sourceURL->endsWith("/dqm_loader.js") || sourceURL->endsWith("/tdqm_loader.js")))
        return false;
    return frame.settings().needsSiteSpecificQuirks();
}

String Navigator::appVersion() const
{
    if (!m_frame)
        return String();
    if (RuntimeEnabledFeatures::sharedFeatures().webAPIStatisticsEnabled())
        ResourceLoadObserver::shared().logNavigatorAPIAccessed(*m_frame->document(), ResourceLoadStatistics::NavigatorAPI::AppVersion);
    String appVersion = NavigatorBase::appVersion();
    if (shouldHideFourDot(*m_frame))
        appVersion.replace("4.", "4_");
    return appVersion;
}

const String& Navigator::userAgent() const
{
    if (!m_frame || !m_frame->page())
        return m_userAgent;
    if (RuntimeEnabledFeatures::sharedFeatures().webAPIStatisticsEnabled())
        ResourceLoadObserver::shared().logNavigatorAPIAccessed(*m_frame->document(), ResourceLoadStatistics::NavigatorAPI::UserAgent);
    if (m_userAgent.isNull())
        m_userAgent = m_frame->loader().userAgent(m_frame->document()->url());
    return m_userAgent;
}

void Navigator::userAgentChanged()
{
    m_userAgent = String();
}

bool Navigator::onLine() const
{
    return platformStrategies()->loaderStrategy()->isOnLine();
}

void Navigator::share(ScriptExecutionContext& context, ShareData data, Ref<DeferredPromise>&& promise)
{
    if (!m_frame || !m_frame->page()) {
        promise->reject(TypeError);
        return;
    }
    
    if (data.title.isEmpty() && data.url.isEmpty() && data.text.isEmpty()) {
        promise->reject(TypeError);
        return;
    }
    
    URL url = context.completeURL(data.url);
    if (!url.isValid()) {
        promise->reject(TypeError);
        return;
    }
    
    if (!UserGestureIndicator::processingUserGesture()) {
        promise->reject(NotAllowedError);
        return;
    }
    
    ShareDataWithParsedURL shareData = {
        data,
        url,
    };

    m_frame->page()->chrome().showShareSheet(shareData, [promise = WTFMove(promise)] (bool completed) {
        if (completed) {
            promise->resolve();
            return;
        }
        promise->reject(Exception { AbortError, "Abort due to cancellation of share."_s });
    });
}

DOMPluginArray& Navigator::plugins()
{
    if (RuntimeEnabledFeatures::sharedFeatures().webAPIStatisticsEnabled()) {
        if (m_frame)
            ResourceLoadObserver::shared().logNavigatorAPIAccessed(*m_frame->document(), ResourceLoadStatistics::NavigatorAPI::Plugins);
    }
    if (!m_plugins)
        m_plugins = DOMPluginArray::create(m_frame);
    return *m_plugins;
}

DOMMimeTypeArray& Navigator::mimeTypes()
{
    if (RuntimeEnabledFeatures::sharedFeatures().webAPIStatisticsEnabled()) {
        if (m_frame)
            ResourceLoadObserver::shared().logNavigatorAPIAccessed(*m_frame->document(), ResourceLoadStatistics::NavigatorAPI::MimeTypes);
    }
    if (!m_mimeTypes)
        m_mimeTypes = DOMMimeTypeArray::create(m_frame);
    return *m_mimeTypes;
}

bool Navigator::cookieEnabled() const
{
    if (!m_frame)
        return false;

    if (RuntimeEnabledFeatures::sharedFeatures().webAPIStatisticsEnabled())
        ResourceLoadObserver::shared().logNavigatorAPIAccessed(*m_frame->document(), ResourceLoadStatistics::NavigatorAPI::CookieEnabled);

    if (m_frame->page() && !m_frame->page()->settings().cookieEnabled())
        return false;

    auto* document = m_frame->document();
    if (!document)
        return false;

    return cookiesEnabled(*document);
}

bool Navigator::javaEnabled() const
{
    if (!m_frame)
        return false;

    if (RuntimeEnabledFeatures::sharedFeatures().webAPIStatisticsEnabled())
        ResourceLoadObserver::shared().logNavigatorAPIAccessed(*m_frame->document(), ResourceLoadStatistics::NavigatorAPI::JavaEnabled);

    if (!m_frame->settings().isJavaEnabled())
        return false;
    if (m_frame->document()->securityOrigin().isLocal() && !m_frame->settings().isJavaEnabledForLocalFiles())
        return false;

    return true;
}

#if PLATFORM(IOS)

bool Navigator::standalone() const
{
    return m_frame && m_frame->settings().standalone();
}

#endif

void Navigator::getStorageUpdates()
{
}

} // namespace WebCore
