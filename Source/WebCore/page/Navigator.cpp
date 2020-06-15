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
#include "DOMMimeType.h"
#include "DOMMimeTypeArray.h"
#include "DOMPlugin.h"
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
#include "ShareData.h"
#include "ShareDataReader.h"
#include "SharedBuffer.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/Language.h>
#include <wtf/StdLibExtras.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(Navigator);

Navigator::Navigator(ScriptExecutionContext* context, DOMWindow& window)
    : NavigatorBase(context)
    , DOMWindowProperty(&window)
{
}

Navigator::~Navigator() = default;

String Navigator::appVersion() const
{
    auto* frame = this->frame();
    if (!frame)
        return String();
    if (RuntimeEnabledFeatures::sharedFeatures().webAPIStatisticsEnabled())
        ResourceLoadObserver::shared().logNavigatorAPIAccessed(*frame->document(), ResourceLoadStatistics::NavigatorAPI::AppVersion);
    return NavigatorBase::appVersion();
}

const String& Navigator::userAgent() const
{
    auto* frame = this->frame();
    if (!frame || !frame->page())
        return m_userAgent;
    if (RuntimeEnabledFeatures::sharedFeatures().webAPIStatisticsEnabled())
        ResourceLoadObserver::shared().logNavigatorAPIAccessed(*frame->document(), ResourceLoadStatistics::NavigatorAPI::UserAgent);
    if (m_userAgent.isNull())
        m_userAgent = frame->loader().userAgent(frame->document()->url());
    return m_userAgent;
}
    
String Navigator::platform() const
{
    auto* frame = this->frame();
    if (!frame || !frame->page())
        return m_platform;

    if (m_platform.isNull())
        m_platform = frame->loader().navigatorPlatform();
    
    if (m_platform.isNull())
        m_platform = NavigatorBase::platform();
    return m_platform;
}

void Navigator::userAgentChanged()
{
    m_userAgent = String();
}

bool Navigator::onLine() const
{
    return platformStrategies()->loaderStrategy()->isOnLine();
}

bool Navigator::canShare(ScriptExecutionContext& context, const ShareData& data)
{
    auto* frame = this->frame();
    if (!frame || !frame->page())
        return false;
    if (data.title.isNull() && data.url.isNull() && data.text.isNull()) {
        if (!data.files.isEmpty()) {
#if ENABLE(FILE_SHARE)
            return true;
#else
            return false;
#endif
        }
        return false;
    }

    Optional<URL> url;
    if (!data.url.isNull()) {
        url = context.completeURL(data.url);
        if (!url->isValid())
            return false;
    }
    return true;
}

void Navigator::share(ScriptExecutionContext& context, const ShareData& data, Ref<DeferredPromise>&& promise)
{
    if (!canShare(context, data)) {
        promise->reject(TypeError);
        return;
    }
    
    Optional<URL> url;
    if (!data.url.isEmpty())
        url = context.completeURL(data.url);
    
    auto* window = this->window();
    // Note that the specification does not indicate we should consume user activation. We are intentionally stricter here.
    if (!window || !window->consumeTransientActivation() || m_hasPendingShare) {
        promise->reject(NotAllowedError);
        return;
    }
    
    ShareDataWithParsedURL shareData = {
        data,
        url,
        { },
    };
#if ENABLE(FILE_SHARE)
    if (!data.files.isEmpty()) {
        if (m_loader)
            m_loader->cancel();
        
        m_loader = ShareDataReader::create([this, promise = WTFMove(promise)] (ExceptionOr<ShareDataWithParsedURL&> readData) mutable {
            showShareData(readData, WTFMove(promise));
        });
        m_loader->start(frame()->document(), WTFMove(shareData));
        return;
    }
#endif
    this->showShareData(shareData, WTFMove(promise));
}

void Navigator::showShareData(ExceptionOr<ShareDataWithParsedURL&> readData, Ref<DeferredPromise>&& promise)
{
    if (readData.hasException()) {
        promise->reject(readData.releaseException());
        return;
    }
    
    auto* frame = this->frame();
    if (!frame || !frame->page())
        return;
    
    m_hasPendingShare = true;
    auto shareData = readData.returnValue();
    
    frame->page()->chrome().showShareSheet(shareData, [promise = WTFMove(promise), this] (bool completed) {
        m_hasPendingShare = false;
        if (completed) {
            promise->resolve();
            return;
        }
        promise->reject(Exception { AbortError, "Abort due to cancellation of share."_s });
    });
}

void Navigator::initializePluginAndMimeTypeArrays()
{
    if (m_plugins)
        return;

    auto* frame = this->frame();
    if (!frame || !frame->page()) {
        m_plugins = DOMPluginArray::create(*this);
        m_mimeTypes = DOMMimeTypeArray::create(*this);
        return;
    }

    auto [publiclyVisiblePlugins, additionalWebVisiblePlugins] = frame->page()->pluginData().publiclyVisiblePluginsAndAdditionalWebVisiblePlugins();

    Vector<Ref<DOMPlugin>> publiclyVisibleDOMPlugins;
    Vector<Ref<DOMPlugin>> additionalWebVisibleDOMPlugins;
    Vector<Ref<DOMMimeType>> webVisibleDOMMimeTypes;

    publiclyVisibleDOMPlugins.reserveInitialCapacity(publiclyVisiblePlugins.size());
    for (auto& plugin : publiclyVisiblePlugins) {
        auto wrapper = DOMPlugin::create(*this, plugin);
        webVisibleDOMMimeTypes.appendVector(wrapper->mimeTypes());
        publiclyVisibleDOMPlugins.uncheckedAppend(WTFMove(wrapper));
    }

    additionalWebVisibleDOMPlugins.reserveInitialCapacity(additionalWebVisiblePlugins.size());
    for (auto& plugin : additionalWebVisiblePlugins) {
        auto wrapper = DOMPlugin::create(*this, plugin);
        webVisibleDOMMimeTypes.appendVector(wrapper->mimeTypes());
        additionalWebVisibleDOMPlugins.uncheckedAppend(WTFMove(wrapper));
    }

    std::sort(publiclyVisibleDOMPlugins.begin(), publiclyVisibleDOMPlugins.end(), [](const Ref<DOMPlugin>& a, const Ref<DOMPlugin>& b) {
        if (auto nameComparison = codePointCompare(a->info().name, b->info().name))
            return nameComparison < 0;
        return codePointCompareLessThan(a->info().bundleIdentifier, b->info().bundleIdentifier);
    });

    std::sort(webVisibleDOMMimeTypes.begin(), webVisibleDOMMimeTypes.end(), [](const Ref<DOMMimeType>& a, const Ref<DOMMimeType>& b) {
        if (auto typeComparison = codePointCompare(a->type(), b->type()))
            return typeComparison < 0;
        return codePointCompareLessThan(a->enabledPlugin()->info().bundleIdentifier, b->enabledPlugin()->info().bundleIdentifier);
    });

    // NOTE: It is not necessary to sort additionalWebVisibleDOMPlugins, as they are only accessible via
    // named property look up, so their order is not exposed.

    m_plugins = DOMPluginArray::create(*this, WTFMove(publiclyVisibleDOMPlugins), WTFMove(additionalWebVisibleDOMPlugins));
    m_mimeTypes = DOMMimeTypeArray::create(*this, WTFMove(webVisibleDOMMimeTypes));
}

DOMPluginArray& Navigator::plugins()
{
    if (RuntimeEnabledFeatures::sharedFeatures().webAPIStatisticsEnabled()) {
        if (auto* frame = this->frame())
            ResourceLoadObserver::shared().logNavigatorAPIAccessed(*frame->document(), ResourceLoadStatistics::NavigatorAPI::Plugins);
    }
    initializePluginAndMimeTypeArrays();
    return *m_plugins;
}

DOMMimeTypeArray& Navigator::mimeTypes()
{
    if (RuntimeEnabledFeatures::sharedFeatures().webAPIStatisticsEnabled()) {
        if (auto* frame = this->frame())
            ResourceLoadObserver::shared().logNavigatorAPIAccessed(*frame->document(), ResourceLoadStatistics::NavigatorAPI::MimeTypes);
    }
    initializePluginAndMimeTypeArrays();
    return *m_mimeTypes;
}

bool Navigator::cookieEnabled() const
{
    auto* frame = this->frame();
    if (!frame)
        return false;

    if (RuntimeEnabledFeatures::sharedFeatures().webAPIStatisticsEnabled())
        ResourceLoadObserver::shared().logNavigatorAPIAccessed(*frame->document(), ResourceLoadStatistics::NavigatorAPI::CookieEnabled);

    auto* page = frame->page();
    if (!page)
        return false;
    
    if (!page->settings().cookieEnabled())
        return false;

    auto* document = frame->document();
    if (!document)
        return false;

    return page->cookieJar().cookiesEnabled(*document);
}

bool Navigator::javaEnabled() const
{
    auto* frame = this->frame();
    if (!frame)
        return false;

    if (RuntimeEnabledFeatures::sharedFeatures().webAPIStatisticsEnabled())
        ResourceLoadObserver::shared().logNavigatorAPIAccessed(*frame->document(), ResourceLoadStatistics::NavigatorAPI::JavaEnabled);

    if (!frame->settings().isJavaEnabled())
        return false;
    if (frame->document()->securityOrigin().isLocal() && !frame->settings().isJavaEnabledForLocalFiles())
        return false;

    return true;
}

#if PLATFORM(IOS_FAMILY)

bool Navigator::standalone() const
{
    auto* frame = this->frame();
    return frame && frame->settings().standalone();
}

#endif

void Navigator::getStorageUpdates()
{
}

} // namespace WebCore
