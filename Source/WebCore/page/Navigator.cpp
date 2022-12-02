/*
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (c) 2000 Daniel Molkentin (molkentin@kde.org)
 *  Copyright (c) 2000 Stefan Schimanski (schimmi@kde.org)
 *  Copyright (C) 2003-2022 Apple Inc.
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
#include "DeprecatedGlobalSettings.h"
#include "Document.h"
#include "FeaturePolicy.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "GPU.h"
#include "Geolocation.h"
#include "JSDOMPromiseDeferred.h"
#include "LoaderStrategy.h"
#include "LocalizedStrings.h"
#include "Page.h"
#include "PlatformStrategies.h"
#include "PluginData.h"
#include "Quirks.h"
#include "ResourceLoadObserver.h"
#include "ScriptController.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "ShareData.h"
#include "ShareDataReader.h"
#include "SharedBuffer.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/Language.h>
#include <wtf/RunLoop.h>
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
    if (DeprecatedGlobalSettings::webAPIStatisticsEnabled())
        ResourceLoadObserver::shared().logNavigatorAPIAccessed(*frame->document(), NavigatorAPIsAccessed::AppVersion);
    return NavigatorBase::appVersion();
}

const String& Navigator::userAgent() const
{
    auto* frame = this->frame();
    if (!frame || !frame->page())
        return m_userAgent;
    if (DeprecatedGlobalSettings::webAPIStatisticsEnabled())
        ResourceLoadObserver::shared().logNavigatorAPIAccessed(*frame->document(), NavigatorAPIsAccessed::UserAgent);
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

static std::optional<URL> shareableURLForShareData(ScriptExecutionContext& context, const ShareData& data)
{
    if (data.url.isNull())
        return std::nullopt;

    auto url = context.completeURL(data.url);
    if (!url.isValid())
        return std::nullopt;
    if (!url.protocolIsInHTTPFamily())
        return std::nullopt;

    return url;
}

static bool validateWebSharePolicy(Document& document)
{
    return isFeaturePolicyAllowedByDocumentAndAllOwners(FeaturePolicy::Type::WebShare, document, LogFeaturePolicyFailure::Yes);
}

bool Navigator::canShare(Document& document, const ShareData& data)
{
    if (!document.isFullyActive() || !validateWebSharePolicy(document))
        return false;

#if ENABLE(FILE_SHARE)
    bool hasShareableFiles = document.settings().webShareFileAPIEnabled() && !data.files.isEmpty();
#else
    bool hasShareableFiles = false;
#endif

    if (data.title.isNull() && data.text.isNull() && data.url.isNull() && !hasShareableFiles)
        return false;

    return data.url.isNull() || shareableURLForShareData(document, data);
}

void Navigator::share(Document& document, const ShareData& data, Ref<DeferredPromise>&& promise)
{
    if (!document.isFullyActive()) {
        promise->reject(InvalidStateError);
        return;
    }

    if (!validateWebSharePolicy(document)) {
        promise->reject(NotAllowedError, "Third-party iframes are not allowed to call share() unless explicitly allowed via Feature-Policy (web-share)"_s);
        return;
    }

    if (m_hasPendingShare) {
        promise->reject(InvalidStateError, "share() is already in progress"_s);
        return;
    }

    auto* window = this->window();
    if (!window || !window->consumeTransientActivation()) {
        promise->reject(NotAllowedError);
        return;
    }

    if (!canShare(document, data)) {
        promise->reject(TypeError);
        return;
    }

    std::optional<URL> url = shareableURLForShareData(document, data);
    ShareDataWithParsedURL shareData = {
        data,
        url,
        { },
        ShareDataOriginator::Web,
    };
#if ENABLE(FILE_SHARE)
    if (document.settings().webShareFileAPIEnabled() && !data.files.isEmpty()) {
        if (m_loader)
            m_loader->cancel();

        m_loader = ShareDataReader::create([this, promise = WTFMove(promise)] (ExceptionOr<ShareDataWithParsedURL&> readData) mutable {
            showShareData(readData, WTFMove(promise));
        });
        m_loader->start(&document, WTFMove(shareData));
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

    if (frame->page()->isControlledByAutomation()) {
        RunLoop::main().dispatch([promise = WTFMove(promise), weakThis = WeakPtr { *this }] {
            if (weakThis)
                weakThis->m_hasPendingShare = false;
            promise->resolve();
        });
        return;
    }
    
    auto shareData = readData.returnValue();
    
    frame->page()->chrome().showShareSheet(shareData, [promise = WTFMove(promise), weakThis = WeakPtr { *this }] (bool completed) {
        if (weakThis)
            weakThis->m_hasPendingShare = false;
        if (completed) {
            promise->resolve();
            return;
        }
        promise->reject(Exception { AbortError, "Abort due to cancellation of share."_s });
    });
}

// https://html.spec.whatwg.org/multipage/system-state.html#pdf-viewing-support
// Section 8.9.1.6 states that if pdfViewerEnabled is true, we must return a list
// of exactly five PDF view plugins, in a particular order.
constexpr ASCIILiteral genericPDFViewerName { "PDF Viewer"_s };

static const Vector<String>& dummyPDFPluginNames()
{
    static NeverDestroyed<Vector<String>> dummyPluginNames(std::initializer_list<String> {
        genericPDFViewerName,
        "Chrome PDF Viewer"_s,
        "Chromium PDF Viewer"_s,
        "Microsoft Edge PDF Viewer"_s,
        "WebKit built-in PDF"_s,
    });
    return dummyPluginNames;
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

    m_pdfViewerEnabled = frame->loader().client().canShowMIMEType("application/pdf"_s);
    if (!m_pdfViewerEnabled) {
        m_plugins = DOMPluginArray::create(*this);
        m_mimeTypes = DOMMimeTypeArray::create(*this);
        return;
    }

    // macOS uses a PDF Plugin (which may be disabled). Other ports handle PDF's through native
    // platform views outside the engine, or use pdf.js.
    PluginInfo pdfPluginInfo = frame->page()->pluginData().builtInPDFPlugin().value_or(PluginData::dummyPDFPluginInfo());

    Vector<Ref<DOMPlugin>> domPlugins;
    Vector<Ref<DOMMimeType>> domMimeTypes;

    // https://html.spec.whatwg.org/multipage/system-state.html#pdf-viewing-support
    // Section 8.9.1.6 states that if pdfViewerEnabled is true, we must return a list
    // of exactly five PDF view plugins, in a particular order.
    for (auto& currentDummyName : dummyPDFPluginNames()) {
        pdfPluginInfo.name = currentDummyName;
        domPlugins.append(DOMPlugin::create(*this, pdfPluginInfo));

        // Register the copy of the PluginInfo using the generic 'PDF Viewer' name
        // as the handler for PDF MIME type to match the specification.
        if (currentDummyName == genericPDFViewerName)
            domMimeTypes.appendVector(domPlugins.last()->mimeTypes());
    }

    m_plugins = DOMPluginArray::create(*this, WTFMove(domPlugins));
    m_mimeTypes = DOMMimeTypeArray::create(*this, WTFMove(domMimeTypes));
}

DOMPluginArray& Navigator::plugins()
{
    if (DeprecatedGlobalSettings::webAPIStatisticsEnabled()) {
        if (auto* frame = this->frame())
            ResourceLoadObserver::shared().logNavigatorAPIAccessed(*frame->document(), NavigatorAPIsAccessed::Plugins);
    }
    initializePluginAndMimeTypeArrays();
    return *m_plugins;
}

DOMMimeTypeArray& Navigator::mimeTypes()
{
    if (DeprecatedGlobalSettings::webAPIStatisticsEnabled()) {
        if (auto* frame = this->frame())
            ResourceLoadObserver::shared().logNavigatorAPIAccessed(*frame->document(), NavigatorAPIsAccessed::MimeTypes);
    }
    initializePluginAndMimeTypeArrays();
    return *m_mimeTypes;
}

bool Navigator::pdfViewerEnabled()
{
    // https://html.spec.whatwg.org/multipage/system-state.html#pdf-viewing-support
    initializePluginAndMimeTypeArrays();
    return m_pdfViewerEnabled;
}

bool Navigator::cookieEnabled() const
{
    auto* frame = this->frame();
    if (!frame)
        return false;

    if (DeprecatedGlobalSettings::webAPIStatisticsEnabled())
        ResourceLoadObserver::shared().logNavigatorAPIAccessed(*frame->document(), NavigatorAPIsAccessed::CookieEnabled);

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

GPU* Navigator::gpu()
{
    if (!m_gpuForWebGPU) {
        auto* frame = this->frame();
        if (!frame)
            return nullptr;
        auto* page = frame->page();
        if (!page)
            return nullptr;
        auto gpu = page->chrome().createGPUForWebGPU();
        if (!gpu)
            return nullptr;

        m_gpuForWebGPU = GPU::create();
        m_gpuForWebGPU->setBacking(*gpu);
    }

    return m_gpuForWebGPU.get();
}

} // namespace WebCore
