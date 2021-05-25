/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
 * Copyright (C) 2012 Igalia S.L.
 * Copyright (C) 2013 Gustavo Noronha Silva <gns@gnome.org>.
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

#include "APINavigation.h"
#include "APINavigationAction.h"
#include "WKArray.h"
#include "WKContextMenuItem.h"
#include "WKMutableArray.h"
#include "WebFramePolicyListenerProxy.h"
#include "WebInspectorUIMessages.h"
#include "WebInspectorUIProxyClient.h"
#include "WebKitInspectorWindow.h"
#include "WebKitWebViewBasePrivate.h"
#include "WebPageGroup.h"
#include "WebProcessPool.h"
#include "WebProcessProxy.h"
#include "WebsiteDataStore.h"
#include <WebCore/CertificateInfo.h>
#include <WebCore/GtkUtilities.h>
#include <WebCore/GtkVersioning.h>
#include <WebCore/InspectorDebuggableType.h>
#include <WebCore/NotImplemented.h>
#include <wtf/FileSystem.h>
#include <wtf/text/Base64.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

static void inspectorViewDestroyed(GtkWidget*, gpointer userData)
{
    WebInspectorUIProxy* inspector = static_cast<WebInspectorUIProxy*>(userData);

    // Inform WebProcess about webinspector closure. Not doing so,
    // results in failure of subsequent invocation of webinspector.
    inspector->close();
}

void WebInspectorUIProxy::setClient(std::unique_ptr<WebInspectorUIProxyClient>&& client)
{
    m_client = WTFMove(client);
}

void WebInspectorUIProxy::updateInspectorWindowTitle() const
{
    ASSERT(m_inspectorWindow);
    webkitInspectorWindowSetSubtitle(WEBKIT_INSPECTOR_WINDOW(m_inspectorWindow), !m_inspectedURLString.isEmpty() ? m_inspectedURLString.utf8().data() : nullptr);
}

static unsigned long long exceededDatabaseQuota(WKPageRef, WKFrameRef, WKSecurityOriginRef, WKStringRef, WKStringRef, unsigned long long, unsigned long long, unsigned long long currentDatabaseUsage, unsigned long long expectedUsage, const void*)
{
    return std::max<unsigned long long>(expectedUsage, currentDatabaseUsage * 1.25);
}

static void webProcessDidCrash(WKPageRef, const void* clientInfo)
{
    WebInspectorUIProxy* inspector = static_cast<WebInspectorUIProxy*>(const_cast<void*>(clientInfo));
    ASSERT(inspector);
    inspector->closeForCrash();
}

static void decidePolicyForNavigationAction(WKPageRef pageRef, WKNavigationActionRef navigationActionRef, WKFramePolicyListenerRef listenerRef, WKTypeRef, const void* clientInfo)
{
    // Allow non-main frames to navigate anywhere.
    API::FrameInfo* sourceFrame = toImpl(navigationActionRef)->sourceFrame();
    if (sourceFrame && !sourceFrame->isMainFrame()) {
        toImpl(listenerRef)->use({ });
        return;
    }

    const WebInspectorUIProxy* inspector = static_cast<const WebInspectorUIProxy*>(clientInfo);
    ASSERT(inspector);

    WebCore::ResourceRequest request = toImpl(navigationActionRef)->request();

    // Allow loading of the main inspector file.
    if (WebInspectorUIProxy::isMainOrTestInspectorPage(request.url())) {
        toImpl(listenerRef)->use({ });
        return;
    }

    // Prevent everything else from loading in the inspector's page.
    toImpl(listenerRef)->ignore();

    // And instead load it in the inspected page.
    inspector->inspectedPage()->loadRequest(WTFMove(request));
}

static void getContextMenuFromProposedMenu(WKPageRef pageRef, WKArrayRef proposedMenuRef, WKArrayRef* newMenuRef, WKHitTestResultRef, WKTypeRef, const void*)
{
    WKMutableArrayRef menuItems = WKMutableArrayCreate();

    size_t count = WKArrayGetSize(proposedMenuRef);
    for (size_t i = 0; i < count; ++i) {
        WKContextMenuItemRef contextMenuItem = static_cast<WKContextMenuItemRef>(WKArrayGetItemAtIndex(proposedMenuRef, i));
        switch (WKContextMenuItemGetTag(contextMenuItem)) {
        case kWKContextMenuItemTagOpenLinkInNewWindow:
        case kWKContextMenuItemTagOpenImageInNewWindow:
        case kWKContextMenuItemTagOpenFrameInNewWindow:
        case kWKContextMenuItemTagOpenMediaInNewWindow:
        case kWKContextMenuItemTagDownloadLinkToDisk:
        case kWKContextMenuItemTagDownloadImageToDisk:
            break;
        default:
            WKArrayAppendItem(menuItems, contextMenuItem);
            break;
        }
    }

    *newMenuRef = menuItems;
}

static Ref<WebsiteDataStore> inspectorWebsiteDataStore()
{
    static const char* versionedDirectory = "webkitgtk-" WEBKITGTK_API_VERSION_STRING G_DIR_SEPARATOR_S "WebInspector" G_DIR_SEPARATOR_S;
    String baseCacheDirectory = FileSystem::pathByAppendingComponent(FileSystem::stringFromFileSystemRepresentation(g_get_user_cache_dir()), versionedDirectory);
    String baseDataDirectory = FileSystem::pathByAppendingComponent(FileSystem::stringFromFileSystemRepresentation(g_get_user_data_dir()), versionedDirectory);

    auto configuration = WebsiteDataStoreConfiguration::create(IsPersistent::Yes, WillCopyPathsFromExistingConfiguration::Yes);
    configuration->setNetworkCacheDirectory(FileSystem::pathByAppendingComponent(baseCacheDirectory, "WebKitCache"));
    configuration->setApplicationCacheDirectory(FileSystem::pathByAppendingComponent(baseCacheDirectory, "applications"));
    configuration->setHSTSStorageDirectory(String(baseCacheDirectory));
    configuration->setCacheStorageDirectory(FileSystem::pathByAppendingComponent(baseCacheDirectory, "CacheStorage"));
    configuration->setLocalStorageDirectory(FileSystem::pathByAppendingComponent(baseDataDirectory, "localstorage"));
    configuration->setIndexedDBDatabaseDirectory(FileSystem::pathByAppendingComponent(baseDataDirectory, "indexeddb"));
    configuration->setWebSQLDatabaseDirectory(FileSystem::pathByAppendingComponent(baseDataDirectory, "databases"));
    configuration->setResourceLoadStatisticsDirectory(FileSystem::pathByAppendingComponent(baseDataDirectory, "itp"));
    configuration->setServiceWorkerRegistrationDirectory(FileSystem::pathByAppendingComponent(baseDataDirectory, "serviceworkers"));
    configuration->setDeviceIdHashSaltsStorageDirectory(FileSystem::pathByAppendingComponent(baseDataDirectory, "deviceidhashsalts"));
    return WebsiteDataStore::create(WTFMove(configuration), PAL::SessionID::generatePersistentSessionID());
}

WebPageProxy* WebInspectorUIProxy::platformCreateFrontendPage()
{
    ASSERT(inspectedPage());
    ASSERT(!m_inspectorView);

    auto preferences = WebPreferences::create(String(), "WebKit2.", "WebKit2.");
#if ENABLE(DEVELOPER_MODE)
    // Allow developers to inspect the Web Inspector in debug builds without changing settings.
    preferences->setDeveloperExtrasEnabled(true);
    preferences->setLogsPageMessagesToSystemConsoleEnabled(true);
#endif
    preferences->setAllowTopNavigationToDataURLs(true);
    preferences->setJavaScriptRuntimeFlags({
    });
    if (m_underTest)
        preferences->setHiddenPageDOMTimerThrottlingEnabled(false);
    auto pageGroup = WebPageGroup::create(WebKit::defaultInspectorPageGroupIdentifierForPage(inspectedPage()));
    auto websiteDataStore = inspectorWebsiteDataStore();
    auto& processPool = WebKit::defaultInspectorProcessPool(inspectionLevel());

    auto pageConfiguration = API::PageConfiguration::create();
    pageConfiguration->setProcessPool(&processPool);
    pageConfiguration->setPreferences(preferences.ptr());
    pageConfiguration->setPageGroup(pageGroup.ptr());
    pageConfiguration->setWebsiteDataStore(websiteDataStore.ptr());
    m_inspectorView = GTK_WIDGET(webkitWebViewBaseCreate(*pageConfiguration.ptr()));
    g_object_add_weak_pointer(G_OBJECT(m_inspectorView), reinterpret_cast<void**>(&m_inspectorView));
    g_signal_connect(m_inspectorView, "destroy", G_CALLBACK(inspectorViewDestroyed), this);

    WKPageUIClientV2 uiClient = {
        { 2, this },
        nullptr, // createNewPage_deprecatedForUseWithV0
        nullptr, // showPage
        nullptr, // closePage
        nullptr, // takeFocus
        nullptr, // focus
        nullptr, // unfocus
        nullptr, // runJavaScriptAlert
        nullptr, // runJavaScriptConfirm
        nullptr, // runJavaScriptPrompt
        nullptr, // setStatusText
        nullptr, // mouseDidMoveOverElement_deprecatedForUseWithV0
        nullptr, // missingPluginButtonClicked_deprecatedForUseWithV0
        nullptr, // didNotHandleKeyEvent
        nullptr, // didNotHandleWheelEvent
        nullptr, // areToolbarsVisible
        nullptr, // setToolbarsVisible
        nullptr, // isMenuBarVisible
        nullptr, // setMenuBarVisible
        nullptr, // isStatusBarVisible
        nullptr, // setStatusBarVisible
        nullptr, // isResizable
        nullptr, // setResizable
        nullptr, // getWindowFrame,
        nullptr, // setWindowFrame,
        nullptr, // runBeforeUnloadConfirmPanel
        nullptr, // didDraw
        nullptr, // pageDidScroll
        exceededDatabaseQuota,
        nullptr, // runOpenPanel,
        nullptr, // decidePolicyForGeolocationPermissionRequest
        nullptr, // headerHeight
        nullptr, // footerHeight
        nullptr, // drawHeader
        nullptr, // drawFooter
        nullptr, // printFrame
        nullptr, // runModal
        nullptr, // unused
        nullptr, // saveDataToFileInDownloadsFolder
        nullptr, // shouldInterruptJavaScript
        nullptr, // createPage
        nullptr, // mouseDidMoveOverElement
        nullptr, // decidePolicyForNotificationPermissionRequest
        nullptr, // unavailablePluginButtonClicked_deprecatedForUseWithV1
        nullptr, // showColorPicker
        nullptr, // hideColorPicker
        nullptr, // unavailablePluginButtonClicked
    };

    WKPageNavigationClientV0 navigationClient = {
        { 0, this },
        decidePolicyForNavigationAction,
        nullptr, // decidePolicyForNavigationResponse
        nullptr, // decidePolicyForPluginLoad
        nullptr, // didStartProvisionalNavigation
        nullptr, // didReceiveServerRedirectForProvisionalNavigation
        nullptr, // didFailProvisionalNavigation
        nullptr, // didCommitNavigation
        nullptr, // didFinishNavigation
        nullptr, // didFailNavigation
        nullptr, // didFailProvisionalLoadInSubframe
        nullptr, // didFinishDocumentLoad
        nullptr, // didSameDocumentNavigation
        nullptr, // renderingProgressDidChange
        nullptr, // canAuthenticateAgainstProtectionSpace
        nullptr, // didReceiveAuthenticationChallenge
        webProcessDidCrash,
        nullptr, // copyWebCryptoMasterKey

        nullptr, // didBeginNavigationGesture
        nullptr, // willEndNavigationGesture
        nullptr, // didEndNavigationGesture
        nullptr, // didRemoveNavigationGestureSnapshot
    };

    WKPageContextMenuClientV3 contextMenuClient = {
        { 3, this },
        nullptr, // getContextMenuFromProposedMenu_deprecatedForUseWithV0
        nullptr, // customContextMenuItemSelected
        nullptr, // contextMenuDismissed
        getContextMenuFromProposedMenu,
        nullptr, // showContextMenu
        nullptr, // hideContextMenu
    };

    WebPageProxy* inspectorPage = webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(m_inspectorView));
    ASSERT(inspectorPage);

    WKPageSetPageUIClient(toAPI(inspectorPage), &uiClient.base);
    WKPageSetPageNavigationClient(toAPI(inspectorPage), &navigationClient.base);
    WKPageSetPageContextMenuClient(toAPI(inspectorPage), &contextMenuClient.base);

    return inspectorPage;
}

void WebInspectorUIProxy::platformCreateFrontendWindow()
{
    if (m_client && m_client->openWindow(*this))
        return;

    ASSERT(!m_inspectorWindow);
    m_inspectorWindow = webkitInspectorWindowNew();
#if USE(GTK4)
    gtk_window_set_child(GTK_WINDOW(m_inspectorWindow), m_inspectorView);
#else
    gtk_container_add(GTK_CONTAINER(m_inspectorWindow), m_inspectorView);
    gtk_widget_show(m_inspectorView);
#endif

    if (!m_inspectedURLString.isEmpty())
        updateInspectorWindowTitle();

    g_object_add_weak_pointer(G_OBJECT(m_inspectorWindow), reinterpret_cast<void**>(&m_inspectorWindow));
    gtk_window_present(GTK_WINDOW(m_inspectorWindow));
}

void WebInspectorUIProxy::platformCloseFrontendPageAndWindow()
{
    if (m_inspectorView) {
        g_signal_handlers_disconnect_by_func(m_inspectorView, reinterpret_cast<void*>(inspectorViewDestroyed), this);
        m_inspectorView = nullptr;
    }

    if (m_client)
        m_client->didClose(*this);

    if (m_inspectorWindow) {
        gtk_widget_destroy(m_inspectorWindow);
        m_inspectorWindow = nullptr;
    }
}

void WebInspectorUIProxy::platformDidCloseForCrash()
{
}

void WebInspectorUIProxy::platformInvalidate()
{
}

void WebInspectorUIProxy::platformHide()
{
    notImplemented();
}

void WebInspectorUIProxy::platformResetState()
{
}

void WebInspectorUIProxy::platformBringToFront()
{
    if (m_isOpening)
        return;

    if (m_client && m_client->bringToFront(*this))
        return;

    GtkWidget* parent = gtk_widget_get_toplevel(m_inspectorView);
    if (WebCore::widgetIsOnscreenToplevelWindow(parent))
        gtk_window_present(GTK_WINDOW(parent));
}

void WebInspectorUIProxy::platformBringInspectedPageToFront()
{
    notImplemented();
}

bool WebInspectorUIProxy::platformIsFront()
{
    GtkWidget* parent = gtk_widget_get_toplevel(m_inspectorView);
    if (WebCore::widgetIsOnscreenToplevelWindow(parent))
        return m_isVisible && gtk_window_is_active(GTK_WINDOW(parent));
    return false;
}

void WebInspectorUIProxy::platformSetForcedAppearance(WebCore::InspectorFrontendClient::Appearance)
{
    notImplemented();
}

void WebInspectorUIProxy::platformInspectedURLChanged(const String& url)
{
    m_inspectedURLString = url;
    if (m_client)
        m_client->inspectedURLChanged(*this, url);

    if (m_inspectorWindow)
        updateInspectorWindowTitle();
}

void WebInspectorUIProxy::platformShowCertificate(const WebCore::CertificateInfo&)
{
    notImplemented();
}

String WebInspectorUIProxy::inspectorPageURL()
{
    return "resource:///org/webkit/inspector/UserInterface/Main.html"_s;
}

String WebInspectorUIProxy::inspectorTestPageURL()
{
    return "resource:///org/webkit/inspector/UserInterface/Test.html"_s;
}

DebuggableInfoData WebInspectorUIProxy::infoForLocalDebuggable()
{
    // FIXME <https://webkit.org/b/205536>: this should infer more useful data about the debug target.
    auto data = DebuggableInfoData::empty();
    data.debuggableType = Inspector::DebuggableType::WebPage;
    return data;
}

unsigned WebInspectorUIProxy::platformInspectedWindowHeight()
{
    return gtk_widget_get_allocated_height(inspectedPage()->viewWidget());
}

unsigned WebInspectorUIProxy::platformInspectedWindowWidth()
{
    return gtk_widget_get_allocated_width(inspectedPage()->viewWidget());
}

void WebInspectorUIProxy::platformAttach()
{
    GRefPtr<GtkWidget> inspectorView = m_inspectorView;
    if (m_inspectorWindow) {
#if USE(GTK4)
        gtk_window_set_child(GTK_WINDOW(m_inspectorWindow), nullptr);
#else
        gtk_container_remove(GTK_CONTAINER(m_inspectorWindow), m_inspectorView);
#endif
        gtk_widget_destroy(m_inspectorWindow);
        m_inspectorWindow = nullptr;
    }

    // Set a default sizes based on InspectorFrontendClientLocal.
    static const unsigned defaultAttachedSize = 300;
    static const unsigned minimumAttachedWidth = 750;
    static const unsigned minimumAttachedHeight = 250;

    if (m_attachmentSide == AttachmentSide::Bottom) {
        unsigned maximumAttachedHeight = platformInspectedWindowHeight() * 3 / 4;
        platformSetAttachedWindowHeight(std::max(minimumAttachedHeight, std::min(defaultAttachedSize, maximumAttachedHeight)));
    } else {
        unsigned maximumAttachedWidth = platformInspectedWindowWidth() * 3 / 4;
        platformSetAttachedWindowWidth(std::max(minimumAttachedWidth, std::min(defaultAttachedSize, maximumAttachedWidth)));
    }

    if (m_client && m_client->attach(*this))
        return;

    webkitWebViewBaseAddWebInspector(WEBKIT_WEB_VIEW_BASE(inspectedPage()->viewWidget()), m_inspectorView, m_attachmentSide);
    gtk_widget_show(m_inspectorView);
}

void WebInspectorUIProxy::platformDetach()
{
    if (!inspectedPage()->hasRunningProcess())
        return;

    GRefPtr<GtkWidget> inspectorView = m_inspectorView;
    if (!m_client || !m_client->detach(*this)) {
        // Detach is called when m_isAttached is true, but it could called before
        // the inspector is opened if the inspector is shown/closed quickly. So,
        // we might not have a parent yet.
        if (GtkWidget* parent = gtk_widget_get_parent(m_inspectorView))
            webkitWebViewBaseRemoveWebInspector(WEBKIT_WEB_VIEW_BASE(parent), m_inspectorView);
    }

    // Return early if we are not visible. This means the inspector was closed while attached
    // and we should not create and show the inspector window.
    if (!m_isVisible) {
        // The inspector view will be destroyed, but we don't need to notify the web process to close the
        // inspector in this case, since it's already closed.
        g_signal_handlers_disconnect_by_func(m_inspectorView, reinterpret_cast<void*>(inspectorViewDestroyed), this);
        m_inspectorView = nullptr;
        return;
    }

    open();
}

void WebInspectorUIProxy::platformSetAttachedWindowHeight(unsigned height)
{
    if (!m_isAttached)
        return;

    if (m_client)
        m_client->didChangeAttachedHeight(*this, height);
    webkitWebViewBaseSetInspectorViewSize(WEBKIT_WEB_VIEW_BASE(inspectedPage()->viewWidget()), height);
}

void WebInspectorUIProxy::platformSetAttachedWindowWidth(unsigned width)
{
    if (!m_isAttached)
        return;

    if (m_client)
        m_client->didChangeAttachedWidth(*this, width);
    webkitWebViewBaseSetInspectorViewSize(WEBKIT_WEB_VIEW_BASE(inspectedPage()->viewWidget()), width);
}

void WebInspectorUIProxy::platformSetSheetRect(const WebCore::FloatRect&)
{
    notImplemented();
}

void WebInspectorUIProxy::platformStartWindowDrag()
{
    notImplemented();
}

static void fileReplaceContentsCallback(GObject* sourceObject, GAsyncResult* result, gpointer userData)
{
    GFile* file = G_FILE(sourceObject);
    if (!g_file_replace_contents_finish(file, result, nullptr, nullptr))
        return;

    auto* page = static_cast<WebPageProxy*>(userData);
    GUniquePtr<char> path(g_file_get_path(file));
    page->send(Messages::WebInspectorUI::DidSave(path.get()));
}

void WebInspectorUIProxy::platformSave(const String& suggestedURL, const String& content, bool base64Encoded, bool forceSaveDialog)
{
    UNUSED_PARAM(forceSaveDialog);

    GtkWidget* parent = gtk_widget_get_toplevel(m_inspectorView);
    if (!WebCore::widgetIsOnscreenToplevelWindow(parent))
        return;

    GRefPtr<GtkFileChooserNative> dialog = adoptGRef(gtk_file_chooser_native_new("Save File",
        GTK_WINDOW(parent), GTK_FILE_CHOOSER_ACTION_SAVE, "Save", "Cancel"));

    GtkFileChooser* chooser = GTK_FILE_CHOOSER(dialog.get());
#if !USE(GTK4)
    gtk_file_chooser_set_do_overwrite_confirmation(chooser, TRUE);
#endif

    // Some inspector views (Audits for instance) use a custom URI scheme, such
    // as web-inspector. So we can't rely on the URL being a valid file:/// URL
    // unfortunately.
    URL url(URL(), suggestedURL);
    // Strip leading / character.
    gtk_file_chooser_set_current_name(chooser, url.path().substring(1).utf8().data());

    if (gtk_native_dialog_run(GTK_NATIVE_DIALOG(dialog.get())) != GTK_RESPONSE_ACCEPT)
        return;

    Vector<uint8_t> dataVector;
    CString dataString;
    if (base64Encoded) {
        auto decodedData = base64Decode(content, Base64DecodeOptions::ValidatePadding);
        if (!decodedData)
            return;
        decodedData->shrinkToFit();
        dataVector = WTFMove(*decodedData);
    } else
        dataString = content.utf8();

    const char* data = !dataString.isNull() ? dataString.data() : reinterpret_cast<char*>(dataVector.data());
    size_t dataLength = !dataString.isNull() ? dataString.length() : dataVector.size();
    GRefPtr<GFile> file = adoptGRef(gtk_file_chooser_get_file(chooser));
    GUniquePtr<char> path(g_file_get_path(file.get()));
    g_file_replace_contents_async(file.get(), data, dataLength, nullptr, false,
        G_FILE_CREATE_REPLACE_DESTINATION, nullptr, fileReplaceContentsCallback, m_inspectorPage);
}

void WebInspectorUIProxy::platformAppend(const String&, const String&)
{
    notImplemented();
}

void WebInspectorUIProxy::platformAttachAvailabilityChanged(bool available)
{
    if (m_client)
        m_client->didChangeAttachAvailability(*this, available);
}

} // namespace WebKit
