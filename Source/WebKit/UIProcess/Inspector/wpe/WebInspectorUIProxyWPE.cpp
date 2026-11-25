/*
 * Copyright (C) 2024 Igalia S.L.
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

#include "config.h"
#include "WebInspectorUIProxy.h"

#if ENABLE(WPE_PLATFORM)

#include "APINavigationAction.h"
#include "APINavigationClient.h"
#include "APIPageConfiguration.h"
#include "WPEWebViewPlatform.h"
#include "WebFramePolicyListenerProxy.h"
#include "WebInspectorUIProxyGLib.h"
#include "WebPageGroup.h"
#include "WebPageProxy.h"
#include "WebPreferences.h"
#include "WebProcessPool.h"
#include "WebsiteDataStore.h"
#include <WebCore/CertificateInfo.h>
#include <WebCore/FloatRect.h>
#include <WebCore/InspectorFrontendClient.h>
#include <WebCore/NotImplemented.h>
#include <wpe/wpe-platform.h>
#include <wtf/FileSystem.h>
#include <wtf/glib/GMallocString.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

class InspectorNavigationClient final : public API::NavigationClient {
public:
    explicit InspectorNavigationClient(WebInspectorUIProxy& proxy)
        : m_proxy(proxy)
    {
    }

    bool processDidTerminate(WebPageProxy&, ProcessTerminationReason reason) override
    {
        if (reason == ProcessTerminationReason::Crash)
            m_proxy.closeForCrash();
        return true;
    }

    void decidePolicyForNavigationAction(WebPageProxy&, Ref<API::NavigationAction>&& navigationAction, Ref<WebFramePolicyListenerProxy>&& listener) override
    {
        // Allow non-main frames to navigate anywhere.
        if (!navigationAction->targetFrame()->isMainFrame()) {
            listener->use();
            return;
        }

        // Allow loading of the main inspector file.
        if (WebInspectorUIProxy::isMainOrTestInspectorPage(navigationAction->request().url())) {
            listener->use();
            return;
        }

        // Prevent everything else.
        listener->ignore();

        // Try to load the request in the inspected page.
        if (RefPtr page = m_proxy.inspectedPage()) {
            auto request = navigationAction->request();
            page->loadRequest(WTF::move(request));
        }
    }

private:
    WebInspectorUIProxy& m_proxy;
};

static Ref<WebsiteDataStore> inspectorWebsiteDataStore()
{
    static constexpr auto versionedDirectory = "wpewebkit-" WPE_API_VERSION G_DIR_SEPARATOR_S "WebInspector" G_DIR_SEPARATOR_S ""_s;
    RefPtr<StringImpl> baseCacheDirectory = FileSystem::pathByAppendingComponent(FileSystem::userCacheDirectory(), versionedDirectory);
    RefPtr<StringImpl> baseDataDirectory = FileSystem::pathByAppendingComponent(FileSystem::userDataDirectory(), versionedDirectory);

    auto configuration = WebsiteDataStoreConfiguration::createWithBaseDirectories(baseCacheDirectory, baseDataDirectory);
    return WebsiteDataStore::create(WTF::move(configuration), PAL::SessionID::generatePersistentSessionID());
}

RefPtr<WebPageProxy> WebInspectorUIProxy::platformCreateFrontendPage()
{
    auto* inspectedWPEView = m_inspectedPage->wpeView();
    if (!inspectedWPEView)
        return nullptr;

    RELEASE_ASSERT(m_inspectedPage);
    RELEASE_ASSERT(!m_inspectorView);

    auto preferences = WebPreferences::create(nullptr, "WebKit2."_si, "WebKit2."_si);
#if ENABLE(DEVELOPER_MODE)
    // Allow developers to inspect the Web Inspector in debug builds without changing settings.
    preferences->setDeveloperExtrasEnabled(true);
    preferences->setLogsPageMessagesToSystemConsoleEnabled(true);
#endif
    // The Find banner and Search sidebar use the legacy results=N attribute for recent searches.
    preferences->setSearchInputResultsAttributeEnabled(true);
    preferences->setAllowTopNavigationToDataURLs(true);
    preferences->setJavaScriptRuntimeFlags({ });
    preferences->setAcceleratedCompositingEnabled(true);
    preferences->setForceCompositingMode(true);
    preferences->setThreadedScrollingEnabled(true);
    preferences->setHiddenPageDOMTimerThrottlingEnabled(false);

    auto pageGroup = WebPageGroup::create(WebKit::defaultInspectorPageGroupIdentifierForPage(protect(inspectedPage()).get()));
    auto websiteDataStore = inspectorWebsiteDataStore();
    auto& processPool = WebKit::defaultInspectorProcessPool(inspectionLevel());

    auto pageConfiguration = API::PageConfiguration::create();
    pageConfiguration->setProcessPool(&processPool);
    pageConfiguration->setPreferences(preferences.ptr());
    pageConfiguration->setPageGroup(pageGroup.ptr());
    pageConfiguration->setWebsiteDataStore(websiteDataStore.ptr());
    auto inspectorView = WKWPE::ViewPlatform::create(wpe_view_get_display(inspectedWPEView), *pageConfiguration.ptr());

    auto* wpeView = inspectorView->wpeView();
    if (auto* toplevel = wpe_view_get_toplevel(wpeView)) {
        m_inspectorWindow = toplevel;
        wpe_view_set_toplevel(wpeView, nullptr);
    } else
        m_inspectorWindow = adoptGRef(wpe_display_create_toplevel(wpe_view_get_display(wpeView), 1));
    if (!m_inspectorWindow)
        return nullptr;

    m_inspectorView = WTF::move(inspectorView);
    g_signal_connect(wpeView, "closed", G_CALLBACK(+[](WPEView* wpeView, WebInspectorUIProxy* proxy) {
        proxy->close();
    }), this);

    Ref page = m_inspectorView->page();
    page->setNavigationClient(makeUniqueRef<InspectorNavigationClient>(*this));
    return page;
}

void WebInspectorUIProxy::platformCreateFrontendWindow()
{
    wpe_toplevel_resize(m_inspectorWindow.get(), initialWindowWidth, initialWindowHeight);
    wpe_view_set_toplevel(m_inspectorView->wpeView(), m_inspectorWindow.get());
}

void WebInspectorUIProxy::platformCloseFrontendPageAndWindow()
{
    if (m_inspectorView)
        g_signal_handlers_disconnect_by_data(m_inspectorView->wpeView(), this);
    m_inspectorView = nullptr;
    m_inspectorWindow = nullptr;
}

void WebInspectorUIProxy::platformDidCloseForCrash()
{
    notImplemented();
}

void WebInspectorUIProxy::platformInvalidate()
{
    if (m_inspectorView)
        g_signal_handlers_disconnect_by_data(m_inspectorView->wpeView(), this);
}

void WebInspectorUIProxy::platformResetState()
{
    notImplemented();
}

void WebInspectorUIProxy::platformBringToFront()
{
    notImplemented();
}

void WebInspectorUIProxy::platformBringInspectedPageToFront()
{
    notImplemented();
}

void WebInspectorUIProxy::platformHide()
{
    notImplemented();
}

bool WebInspectorUIProxy::platformIsFront()
{
    notImplemented();
    return false;
}

void WebInspectorUIProxy::platformSetForcedAppearance(WebCore::InspectorFrontendClient::Appearance)
{
    notImplemented();
}

void WebInspectorUIProxy::platformRevealFileExternally(StringImpl*)
{
    notImplemented();
}

void WebInspectorUIProxy::platformInspectedURLChanged(StringImpl* url)
{
    if (!m_inspectorWindow)
        return;

    auto title = utf8(makeString(u8"Web Inspector — "_span, url));
    wpe_toplevel_set_title(m_inspectorWindow.get(), title.legacyCStringPointer());
}

void WebInspectorUIProxy::platformShowCertificate(const WebCore::CertificateInfo&)
{
    notImplemented();
}

static RefPtr<StringImpl> computeContentHash(StringImpl* content, bool base64Encoded)
{
    GMallocString digest;
    if (base64Encoded) {
        auto decoded = base64Decode(content);
        if (decoded)
            digest = GMallocString::unsafeAdoptFromUTF8(g_compute_checksum_for_data(G_CHECKSUM_SHA256, decoded->span().data(), decoded->size()));
    } else {
        auto utf8Data = utf8(content);
        auto characters = byteCast<char>(utf8Data.span());
        digest = GMallocString::unsafeAdoptFromUTF8(g_compute_checksum_for_string(G_CHECKSUM_SHA256, characters.data(), characters.size()));
    }

    return StringImpl::fromUTF8(digest.span());
}

void WebInspectorUIProxy::platformSave(Vector<WebCore::InspectorFrontendClient::SaveData>&& saveDatas, bool forceSaveAs)
{
    ASSERT(saveDatas.size() == 1);
    UNUSED_PARAM(forceSaveAs);

    // Some inspector views (Audits for instance) use a custom URI scheme, such
    // as web-inspector. So we can't rely on the URL being a valid file:/// URL
    // unfortunately.
    URL url { saveDatas[0].url };
    auto filename = url.path().substring(1).utf8();

    const gchar* downloadsDir = g_get_user_special_dir(G_USER_DIRECTORY_DOWNLOAD);
    if (!downloadsDir) {
        // If we don't have XDG user dirs info, set just to HOME.
        downloadsDir = g_get_home_dir();
    }

    auto hash = computeContentHash(saveDatas[0].content, saveDatas[0].base64Encoded);

    auto updatedFilename = utf8(makeString(filename, "-"_s, left(hash, 8)));

    GRefPtr<GFile> file = adoptGRef(g_file_new_build_filename(downloadsDir, updatedFilename.legacyCStringPointer(), nullptr));

    platformSaveDataToFile(WTF::move(file), saveDatas[0].content, saveDatas[0].base64Encoded);
}

void WebInspectorUIProxy::platformLoad(StringImpl*, CompletionHandler<void(StringImpl*)>&& completionHandler)
{
    notImplemented();
    completionHandler(nullString());
}

void WebInspectorUIProxy::platformPickColorFromScreen(CompletionHandler<void(const std::optional<WebCore::Color>&)>&& completionHandler)
{
    notImplemented();
    completionHandler({ });
}

void WebInspectorUIProxy::platformAttach()
{
    notImplemented();
}

void WebInspectorUIProxy::platformDetach()
{
    notImplemented();
}

void WebInspectorUIProxy::platformSetAttachedWindowHeight(unsigned)
{
    notImplemented();
}

void WebInspectorUIProxy::platformSetSheetRect(const WebCore::FloatRect&)
{
    notImplemented();
}

void WebInspectorUIProxy::platformStartWindowDrag()
{
    notImplemented();
}

RefPtr<StringImpl> WebInspectorUIProxy::inspectorPageURL()
{
    return "resource:///org/webkit/inspector/UserInterface/Main.html"_si;
}

RefPtr<StringImpl> WebInspectorUIProxy::inspectorTestPageURL()
{
    return "resource:///org/webkit/inspector/UserInterface/Test.html"_si;
}

DebuggableInfoData WebInspectorUIProxy::infoForLocalDebuggable()
{
    auto data = DebuggableInfoData::empty();
    data.debuggableType = Inspector::DebuggableType::WebPage;
    return data;
}

void WebInspectorUIProxy::platformSetAttachedWindowWidth(unsigned)
{
    notImplemented();
}

void WebInspectorUIProxy::platformAttachAvailabilityChanged(bool)
{
    notImplemented();
}

} // namespace WebKit

#endif // ENABLE(WPE_PLATFORM)
