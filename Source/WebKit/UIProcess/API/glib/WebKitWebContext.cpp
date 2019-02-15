/*
 * Copyright (C) 2011 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "WebKitWebContext.h"

#include "APIAutomationClient.h"
#include "APICustomProtocolManagerClient.h"
#include "APIDownloadClient.h"
#include "APIInjectedBundleClient.h"
#include "APIPageConfiguration.h"
#include "APIProcessPoolConfiguration.h"
#include "APIString.h"
#include "TextChecker.h"
#include "TextCheckerState.h"
#include "WebAutomationSession.h"
#include "WebCertificateInfo.h"
#include "WebGeolocationManagerProxy.h"
#include "WebKitAutomationSessionPrivate.h"
#include "WebKitCustomProtocolManagerClient.h"
#include "WebKitDownloadClient.h"
#include "WebKitDownloadPrivate.h"
#include "WebKitFaviconDatabasePrivate.h"
#include "WebKitGeolocationProvider.h"
#include "WebKitInjectedBundleClient.h"
#include "WebKitNetworkProxySettingsPrivate.h"
#include "WebKitNotificationProvider.h"
#include "WebKitPluginPrivate.h"
#include "WebKitPrivate.h"
#include "WebKitSecurityManagerPrivate.h"
#include "WebKitSecurityOriginPrivate.h"
#include "WebKitSettingsPrivate.h"
#include "WebKitURISchemeRequestPrivate.h"
#include "WebKitUserContentManagerPrivate.h"
#include "WebKitWebContextPrivate.h"
#include "WebKitWebViewPrivate.h"
#include "WebKitWebsiteDataManagerPrivate.h"
#include "WebNotificationManagerProxy.h"
#include "WebsiteDataType.h"
#include <JavaScriptCore/RemoteInspector.h>
#include <glib/gi18n-lib.h>
#include <libintl.h>
#include <memory>
#include <wtf/FileSystem.h>
#include <wtf/HashMap.h>
#include <wtf/Language.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/text/CString.h>

#if PLATFORM(GTK)
#include "WebKitRemoteInspectorProtocolHandler.h"
#endif

using namespace WebKit;

/**
 * SECTION: WebKitWebContext
 * @Short_description: Manages aspects common to all #WebKitWebView<!-- -->s
 * @Title: WebKitWebContext
 *
 * The #WebKitWebContext manages all aspects common to all
 * #WebKitWebView<!-- -->s.
 *
 * You can define the #WebKitCacheModel and #WebKitProcessModel with
 * webkit_web_context_set_cache_model() and
 * webkit_web_context_set_process_model(), depending on the needs of
 * your application. You can access the #WebKitSecurityManager to specify
 * the behaviour of your application regarding security using
 * webkit_web_context_get_security_manager().
 *
 * It is also possible to change your preferred language or enable
 * spell checking, using webkit_web_context_set_preferred_languages(),
 * webkit_web_context_set_spell_checking_languages() and
 * webkit_web_context_set_spell_checking_enabled().
 *
 * You can use webkit_web_context_register_uri_scheme() to register
 * custom URI schemes, and manage several other settings.
 *
 * TLS certificate validation failure is now treated as a transport
 * error by default. To handle TLS failures differently, you can
 * connect to #WebKitWebView::load-failed-with-tls-errors.
 * Alternatively, you can use webkit_web_context_set_tls_errors_policy()
 * to set the policy %WEBKIT_TLS_ERRORS_POLICY_IGNORE; however, this is
 * not appropriate for Internet applications.
 *
 */

enum {
    PROP_0,

#if PLATFORM(GTK)
    PROP_LOCAL_STORAGE_DIRECTORY,
#endif
    PROP_WEBSITE_DATA_MANAGER
};

enum {
    DOWNLOAD_STARTED,
    INITIALIZE_WEB_EXTENSIONS,
    INITIALIZE_NOTIFICATION_PERMISSIONS,
    AUTOMATION_STARTED,

    LAST_SIGNAL
};

class WebKitURISchemeHandler: public RefCounted<WebKitURISchemeHandler> {
public:
    WebKitURISchemeHandler(WebKitURISchemeRequestCallback callback, void* userData, GDestroyNotify destroyNotify)
        : m_callback(callback)
        , m_userData(userData)
        , m_destroyNotify(destroyNotify)
    {
    }

    ~WebKitURISchemeHandler()
    {
        if (m_destroyNotify)
            m_destroyNotify(m_userData);
    }

    bool hasCallback()
    {
        return m_callback;
    }

    void performCallback(WebKitURISchemeRequest* request)
    {
        ASSERT(m_callback);

        m_callback(request, m_userData);
    }

private:
    WebKitURISchemeRequestCallback m_callback { nullptr };
    void* m_userData { nullptr };
    GDestroyNotify m_destroyNotify { nullptr };
};

typedef HashMap<String, RefPtr<WebKitURISchemeHandler> > URISchemeHandlerMap;
typedef HashMap<uint64_t, GRefPtr<WebKitURISchemeRequest> > URISchemeRequestMap;

class WebKitAutomationClient;

struct _WebKitWebContextPrivate {
    RefPtr<WebProcessPool> processPool;
    bool clientsDetached;

    GRefPtr<WebKitFaviconDatabase> faviconDatabase;
    GRefPtr<WebKitSecurityManager> securityManager;
    URISchemeHandlerMap uriSchemeHandlers;
    URISchemeRequestMap uriSchemeRequests;
#if ENABLE(GEOLOCATION)
    std::unique_ptr<WebKitGeolocationProvider> geolocationProvider;
#endif
    std::unique_ptr<WebKitNotificationProvider> notificationProvider;
    GRefPtr<WebKitWebsiteDataManager> websiteDataManager;

    CString faviconDatabaseDirectory;
    WebKitTLSErrorsPolicy tlsErrorsPolicy;
    WebKitProcessModel processModel;
    unsigned processCountLimit;

    HashMap<uint64_t, WebKitWebView*> webViews;
    unsigned ephemeralPageCount;

    CString webExtensionsDirectory;
    GRefPtr<GVariant> webExtensionsInitializationUserData;

    CString localStorageDirectory;
#if ENABLE(REMOTE_INSPECTOR)
#if PLATFORM(GTK)
    std::unique_ptr<RemoteInspectorProtocolHandler> remoteInspectorProtocolHandler;
#endif
    std::unique_ptr<WebKitAutomationClient> automationClient;
    GRefPtr<WebKitAutomationSession> automationSession;
#endif
};

static guint signals[LAST_SIGNAL] = { 0, };

#if ENABLE(REMOTE_INSPECTOR)
class WebKitAutomationClient final : Inspector::RemoteInspector::Client {
public:
    explicit WebKitAutomationClient(WebKitWebContext* context)
        : m_webContext(context)
    {
        Inspector::RemoteInspector::singleton().setClient(this);
    }

    ~WebKitAutomationClient()
    {
        Inspector::RemoteInspector::singleton().setClient(nullptr);
    }

private:
    bool remoteAutomationAllowed() const override { return true; }

    String browserName() const override
    {
        if (!m_webContext->priv->automationSession)
            return { };

        return webkitAutomationSessionGetBrowserName(m_webContext->priv->automationSession.get());
    }

    String browserVersion() const override
    {
        if (!m_webContext->priv->automationSession)
            return { };

        return webkitAutomationSessionGetBrowserVersion(m_webContext->priv->automationSession.get());
    }

    void requestAutomationSession(const String& sessionIdentifier, const Inspector::RemoteInspector::Client::SessionCapabilities& capabilities) override
    {
        ASSERT(!m_webContext->priv->automationSession);
        m_webContext->priv->automationSession = adoptGRef(webkitAutomationSessionCreate(m_webContext, sessionIdentifier.utf8().data(), capabilities));
        g_signal_emit(m_webContext, signals[AUTOMATION_STARTED], 0, m_webContext->priv->automationSession.get());
        m_webContext->priv->processPool->setAutomationSession(&webkitAutomationSessionGetSession(m_webContext->priv->automationSession.get()));
    }

    WebKitWebContext* m_webContext;
};

void webkitWebContextWillCloseAutomationSession(WebKitWebContext* webContext)
{
    webContext->priv->processPool->setAutomationSession(nullptr);
    webContext->priv->automationSession = nullptr;
}
#endif // ENABLE(REMOTE_INSPECTOR)

WEBKIT_DEFINE_TYPE(WebKitWebContext, webkit_web_context, G_TYPE_OBJECT)

#if PLATFORM(GTK)
#define INJECTED_BUNDLE_FILENAME "libwebkit2gtkinjectedbundle.so"
#elif PLATFORM(WPE)
#define INJECTED_BUNDLE_FILENAME "libWPEInjectedBundle.so"
#endif

static const char* injectedBundleDirectory()
{
#if ENABLE(DEVELOPER_MODE)
    const char* bundleDirectory = g_getenv("WEBKIT_INJECTED_BUNDLE_PATH");
    if (bundleDirectory && g_file_test(bundleDirectory, G_FILE_TEST_IS_DIR))
        return bundleDirectory;
#endif

#if PLATFORM(GTK)
    static const char* injectedBundlePath = LIBDIR G_DIR_SEPARATOR_S "webkit2gtk-" WEBKITGTK_API_VERSION_STRING
        G_DIR_SEPARATOR_S "injected-bundle" G_DIR_SEPARATOR_S;
    return injectedBundlePath;
#elif PLATFORM(WPE)
    static const char* injectedBundlePath = PKGLIBDIR G_DIR_SEPARATOR_S "injected-bundle" G_DIR_SEPARATOR_S;
    return injectedBundlePath;
#endif
}

static void webkitWebContextGetProperty(GObject* object, guint propID, GValue* value, GParamSpec* paramSpec)
{
    WebKitWebContext* context = WEBKIT_WEB_CONTEXT(object);

    switch (propID) {
#if PLATFORM(GTK)
    case PROP_LOCAL_STORAGE_DIRECTORY:
        g_value_set_string(value, context->priv->localStorageDirectory.data());
        break;
#endif
    case PROP_WEBSITE_DATA_MANAGER:
        g_value_set_object(value, webkit_web_context_get_website_data_manager(context));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propID, paramSpec);
    }
}

static void webkitWebContextSetProperty(GObject* object, guint propID, const GValue* value, GParamSpec* paramSpec)
{
    WebKitWebContext* context = WEBKIT_WEB_CONTEXT(object);

    switch (propID) {
#if PLATFORM(GTK)
    case PROP_LOCAL_STORAGE_DIRECTORY:
        context->priv->localStorageDirectory = g_value_get_string(value);
        break;
#endif
    case PROP_WEBSITE_DATA_MANAGER: {
        gpointer manager = g_value_get_object(value);
        context->priv->websiteDataManager = manager ? WEBKIT_WEBSITE_DATA_MANAGER(manager) : nullptr;
        break;
    }
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propID, paramSpec);
    }
}

static inline Ref<WebsiteDataStoreConfiguration> websiteDataStoreConfigurationForWebProcessPoolConfiguration(const API::ProcessPoolConfiguration& processPoolconfigurarion)
{
    auto configuration = WebsiteDataStoreConfiguration::create();
    configuration->setApplicationCacheDirectory(String(processPoolconfigurarion.applicationCacheDirectory()));
    configuration->setNetworkCacheDirectory(String(processPoolconfigurarion.diskCacheDirectory()));
    configuration->setWebSQLDatabaseDirectory(String(processPoolconfigurarion.webSQLDatabaseDirectory()));
    configuration->setLocalStorageDirectory(String(processPoolconfigurarion.localStorageDirectory()));
    configuration->setDeviceIdHashSaltsStorageDirectory(String(processPoolconfigurarion.deviceIdHashSaltsStorageDirectory()));
    configuration->setMediaKeysStorageDirectory(String(processPoolconfigurarion.mediaKeysStorageDirectory()));
    return configuration;
}

static void webkitWebContextConstructed(GObject* object)
{
    G_OBJECT_CLASS(webkit_web_context_parent_class)->constructed(object);

    GUniquePtr<char> bundleFilename(g_build_filename(injectedBundleDirectory(), INJECTED_BUNDLE_FILENAME, nullptr));

    API::ProcessPoolConfiguration configuration;
    configuration.setInjectedBundlePath(FileSystem::stringFromFileSystemRepresentation(bundleFilename.get()));
    configuration.setDiskCacheSpeculativeValidationEnabled(true);

    WebKitWebContext* webContext = WEBKIT_WEB_CONTEXT(object);
    WebKitWebContextPrivate* priv = webContext->priv;
    if (priv->websiteDataManager && !webkit_website_data_manager_is_ephemeral(priv->websiteDataManager.get())) {
        configuration.setLocalStorageDirectory(FileSystem::stringFromFileSystemRepresentation(webkit_website_data_manager_get_local_storage_directory(priv->websiteDataManager.get())));
        configuration.setDiskCacheDirectory(FileSystem::pathByAppendingComponent(FileSystem::stringFromFileSystemRepresentation(webkit_website_data_manager_get_disk_cache_directory(priv->websiteDataManager.get())), networkCacheSubdirectory));
        configuration.setApplicationCacheDirectory(FileSystem::stringFromFileSystemRepresentation(webkit_website_data_manager_get_offline_application_cache_directory(priv->websiteDataManager.get())));
        configuration.setIndexedDBDatabaseDirectory(FileSystem::stringFromFileSystemRepresentation(webkit_website_data_manager_get_indexeddb_directory(priv->websiteDataManager.get())));
        configuration.setWebSQLDatabaseDirectory(FileSystem::stringFromFileSystemRepresentation(webkit_website_data_manager_get_websql_directory(priv->websiteDataManager.get())));
    } else if (!priv->localStorageDirectory.isNull())
        configuration.setLocalStorageDirectory(FileSystem::stringFromFileSystemRepresentation(priv->localStorageDirectory.data()));

    priv->processPool = WebProcessPool::create(configuration);

    if (!priv->websiteDataManager)
        priv->websiteDataManager = adoptGRef(webkitWebsiteDataManagerCreate(websiteDataStoreConfigurationForWebProcessPoolConfiguration(configuration)));
    priv->processPool->setPrimaryDataStore(webkitWebsiteDataManagerGetDataStore(priv->websiteDataManager.get()));

    webkitWebsiteDataManagerAddProcessPool(priv->websiteDataManager.get(), *priv->processPool);

    priv->tlsErrorsPolicy = WEBKIT_TLS_ERRORS_POLICY_FAIL;
    priv->processPool->setIgnoreTLSErrors(false);

#if ENABLE(MEMORY_SAMPLER)
    if (getenv("WEBKIT_SAMPLE_MEMORY"))
        priv->processPool->startMemorySampler(0);
#endif

    attachInjectedBundleClientToContext(webContext);
    attachDownloadClientToContext(webContext);
    attachCustomProtocolManagerClientToContext(webContext);

#if ENABLE(GEOLOCATION)
    priv->geolocationProvider = std::make_unique<WebKitGeolocationProvider>(priv->processPool->supplement<WebGeolocationManagerProxy>());
#endif
    priv->notificationProvider = std::make_unique<WebKitNotificationProvider>(priv->processPool->supplement<WebNotificationManagerProxy>(), webContext);
#if PLATFORM(GTK) && ENABLE(REMOTE_INSPECTOR)
    priv->remoteInspectorProtocolHandler = std::make_unique<RemoteInspectorProtocolHandler>(webContext);
#endif
}

static void webkitWebContextDispose(GObject* object)
{
    WebKitWebContextPrivate* priv = WEBKIT_WEB_CONTEXT(object)->priv;
    if (!priv->clientsDetached) {
        priv->clientsDetached = true;
        priv->processPool->setInjectedBundleClient(nullptr);
        priv->processPool->setDownloadClient(nullptr);
        priv->processPool->setLegacyCustomProtocolManagerClient(nullptr);
    }

    if (priv->websiteDataManager) {
        webkitWebsiteDataManagerRemoveProcessPool(priv->websiteDataManager.get(), *priv->processPool);
        priv->websiteDataManager = nullptr;
    }

    G_OBJECT_CLASS(webkit_web_context_parent_class)->dispose(object);
}

static void webkit_web_context_class_init(WebKitWebContextClass* webContextClass)
{
    GObjectClass* gObjectClass = G_OBJECT_CLASS(webContextClass);

    bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");

    gObjectClass->get_property = webkitWebContextGetProperty;
    gObjectClass->set_property = webkitWebContextSetProperty;
    gObjectClass->constructed = webkitWebContextConstructed;
    gObjectClass->dispose = webkitWebContextDispose;

#if PLATFORM(GTK)
    /**
     * WebKitWebContext:local-storage-directory:
     *
     * The directory where local storage data will be saved.
     *
     * Since: 2.8
     *
     * Deprecated: 2.10. Use #WebKitWebsiteDataManager:local-storage-directory instead.
     */
    g_object_class_install_property(
        gObjectClass,
        PROP_LOCAL_STORAGE_DIRECTORY,
        g_param_spec_string(
            "local-storage-directory",
            _("Local Storage Directory"),
            _("The directory where local storage data will be saved"),
            nullptr,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)));
#endif

    /**
     * WebKitWebContext:website-data-manager:
     *
     * The #WebKitWebsiteDataManager associated with this context.
     *
     * Since: 2.10
     */
    g_object_class_install_property(
        gObjectClass,
        PROP_WEBSITE_DATA_MANAGER,
        g_param_spec_object(
            "website-data-manager",
            _("Website Data Manager"),
            _("The WebKitWebsiteDataManager associated with this context"),
            WEBKIT_TYPE_WEBSITE_DATA_MANAGER,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)));

    /**
     * WebKitWebContext::download-started:
     * @context: the #WebKitWebContext
     * @download: the #WebKitDownload associated with this event
     *
     * This signal is emitted when a new download request is made.
     */
    signals[DOWNLOAD_STARTED] =
        g_signal_new("download-started",
            G_TYPE_FROM_CLASS(gObjectClass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET(WebKitWebContextClass, download_started),
            nullptr, nullptr,
            g_cclosure_marshal_VOID__OBJECT,
            G_TYPE_NONE, 1,
            WEBKIT_TYPE_DOWNLOAD);

    /**
     * WebKitWebContext::initialize-web-extensions:
     * @context: the #WebKitWebContext
     *
     * This signal is emitted when a new web process is about to be
     * launched. It signals the most appropriate moment to use
     * webkit_web_context_set_web_extensions_initialization_user_data()
     * and webkit_web_context_set_web_extensions_directory().
     *
     * Since: 2.4
     */
    signals[INITIALIZE_WEB_EXTENSIONS] =
        g_signal_new("initialize-web-extensions",
            G_TYPE_FROM_CLASS(gObjectClass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET(WebKitWebContextClass, initialize_web_extensions),
            nullptr, nullptr,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE, 0);

    /**
     * WebKitWebContext::initialize-notification-permissions:
     * @context: the #WebKitWebContext
     *
     * This signal is emitted when a #WebKitWebContext needs to set
     * initial notification permissions for a web process. It is emitted
     * when a new web process is about to be launched, and signals the
     * most appropriate moment to use
     * webkit_web_context_initialize_notification_permissions(). If no
     * notification permissions have changed since the last time this
     * signal was emitted, then there is no need to call
     * webkit_web_context_initialize_notification_permissions() again.
     *
     * Since: 2.16
     */
    signals[INITIALIZE_NOTIFICATION_PERMISSIONS] =
        g_signal_new("initialize-notification-permissions",
            G_TYPE_FROM_CLASS(gObjectClass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET(WebKitWebContextClass, initialize_notification_permissions),
            nullptr, nullptr,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE, 0);

    /**
     * WebKitWebContext::automation-started:
     * @context: the #WebKitWebContext
     * @session: the #WebKitAutomationSession associated with this event
     *
     * This signal is emitted when a new automation request is made.
     * Note that it will never be emitted if automation is not enabled in @context,
     * see webkit_web_context_set_automation_allowed() for more details.
     *
     * Since: 2.18
     */
    signals[AUTOMATION_STARTED] =
        g_signal_new("automation-started",
            G_TYPE_FROM_CLASS(gObjectClass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET(WebKitWebContextClass, automation_started),
            nullptr, nullptr,
            g_cclosure_marshal_VOID__OBJECT,
            G_TYPE_NONE, 1,
            WEBKIT_TYPE_AUTOMATION_SESSION);
}

static gpointer createDefaultWebContext(gpointer)
{
    static GRefPtr<WebKitWebContext> webContext = adoptGRef(WEBKIT_WEB_CONTEXT(g_object_new(WEBKIT_TYPE_WEB_CONTEXT, nullptr)));
    return webContext.get();
}

/**
 * webkit_web_context_get_default:
 *
 * Gets the default web context
 *
 * Returns: (transfer none): a #WebKitWebContext
 */
WebKitWebContext* webkit_web_context_get_default(void)
{
    static GOnce onceInit = G_ONCE_INIT;
    return WEBKIT_WEB_CONTEXT(g_once(&onceInit, createDefaultWebContext, 0));
}

/**
 * webkit_web_context_new:
 *
 * Create a new #WebKitWebContext
 *
 * Returns: (transfer full): a newly created #WebKitWebContext
 *
 * Since: 2.8
 */
WebKitWebContext* webkit_web_context_new(void)
{
    return WEBKIT_WEB_CONTEXT(g_object_new(WEBKIT_TYPE_WEB_CONTEXT, nullptr));
}

/**
 * webkit_web_context_new_ephemeral:
 *
 * Create a new ephemeral #WebKitWebContext. An ephemeral #WebKitWebContext is a context
 * created with an ephemeral #WebKitWebsiteDataManager. This is just a convenient method
 * to create ephemeral contexts without having to create your own #WebKitWebsiteDataManager.
 * All #WebKitWebView<!-- -->s associated with this context will also be ephemeral. Websites will
 * not store any data in the client storage.
 * This is normally used to implement private instances.
 *
 * Returns: (transfer full): a new ephemeral #WebKitWebContext.
 *
 * Since: 2.16
 */
WebKitWebContext* webkit_web_context_new_ephemeral()
{
    GRefPtr<WebKitWebsiteDataManager> manager = adoptGRef(webkit_website_data_manager_new_ephemeral());
    return WEBKIT_WEB_CONTEXT(g_object_new(WEBKIT_TYPE_WEB_CONTEXT, "website-data-manager", manager.get(), nullptr));
}

/**
 * webkit_web_context_new_with_website_data_manager:
 * @manager: a #WebKitWebsiteDataManager
 *
 * Create a new #WebKitWebContext with a #WebKitWebsiteDataManager.
 *
 * Returns: (transfer full): a newly created #WebKitWebContext
 *
 * Since: 2.10
 */
WebKitWebContext* webkit_web_context_new_with_website_data_manager(WebKitWebsiteDataManager* manager)
{
    g_return_val_if_fail(WEBKIT_IS_WEBSITE_DATA_MANAGER(manager), nullptr);

    return WEBKIT_WEB_CONTEXT(g_object_new(WEBKIT_TYPE_WEB_CONTEXT, "website-data-manager", manager, nullptr));
}

/**
 * webkit_web_context_get_website_data_manager:
 * @context: the #WebKitWebContext
 *
 * Get the #WebKitWebsiteDataManager of @context.
 *
 * Returns: (transfer none): a #WebKitWebsiteDataManager
 *
 * Since: 2.10
 */
WebKitWebsiteDataManager* webkit_web_context_get_website_data_manager(WebKitWebContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_CONTEXT(context), nullptr);

    return context->priv->websiteDataManager.get();
}

/**
 * webkit_web_context_is_ephemeral:
 * @context: the #WebKitWebContext
 *
 * Get whether a #WebKitWebContext is ephemeral.
 *
 * Returns: %TRUE if @context is ephemeral or %FALSE otherwise.
 *
 * Since: 2.16
 */
gboolean webkit_web_context_is_ephemeral(WebKitWebContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_CONTEXT(context), FALSE);

    return webkit_website_data_manager_is_ephemeral(context->priv->websiteDataManager.get());
}

/**
 * webkit_web_context_is_automation_allowed:
 * @context: the #WebKitWebContext
 *
 * Get whether automation is allowed in @context.
 * See also webkit_web_context_set_automation_allowed().
 *
 * Returns: %TRUE if automation is allowed or %FALSE otherwise.
 *
 * Since: 2.18
 */
gboolean webkit_web_context_is_automation_allowed(WebKitWebContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_CONTEXT(context), FALSE);

#if ENABLE(REMOTE_INSPECTOR)
    return !!context->priv->automationClient;
#else
    return FALSE;
#endif
}

/**
 * webkit_web_context_set_automation_allowed:
 * @context: the #WebKitWebContext
 * @allowed: value to set
 *
 * Set whether automation is allowed in @context. When automation is enabled the browser could
 * be controlled by another process by requesting an automation session. When a new automation
 * session is requested the signal #WebKitWebContext::automation-started is emitted.
 * Automation is disabled by default, so you need to explicitly call this method passing %TRUE
 * to enable it.
 *
 * Note that only one #WebKitWebContext can have automation enabled, so this will do nothing
 * if there's another #WebKitWebContext with automation already enabled.
 *
 * Since: 2.18
 */
void webkit_web_context_set_automation_allowed(WebKitWebContext* context, gboolean allowed)
{
    g_return_if_fail(WEBKIT_IS_WEB_CONTEXT(context));

    if (webkit_web_context_is_automation_allowed(context) == allowed)
        return;
#if ENABLE(REMOTE_INSPECTOR)
    if (allowed) {
        if (Inspector::RemoteInspector::singleton().client()) {
            g_warning("Not enabling automation on WebKitWebContext because there's another context with automation enabled, only one is allowed");
            return;
        }
        context->priv->automationClient = std::make_unique<WebKitAutomationClient>(context);
    } else
        context->priv->automationClient = nullptr;
#endif
}

/**
 * webkit_web_context_set_cache_model:
 * @context: the #WebKitWebContext
 * @cache_model: a #WebKitCacheModel
 *
 * Specifies a usage model for WebViews, which WebKit will use to
 * determine its caching behavior. All web views follow the cache
 * model. This cache model determines the RAM and disk space to use
 * for caching previously viewed content .
 *
 * Research indicates that users tend to browse within clusters of
 * documents that hold resources in common, and to revisit previously
 * visited documents. WebKit and the frameworks below it include
 * built-in caches that take advantage of these patterns,
 * substantially improving document load speed in browsing
 * situations. The WebKit cache model controls the behaviors of all of
 * these caches, including various WebCore caches.
 *
 * Browsers can improve document load speed substantially by
 * specifying %WEBKIT_CACHE_MODEL_WEB_BROWSER. Applications without a
 * browsing interface can reduce memory usage substantially by
 * specifying %WEBKIT_CACHE_MODEL_DOCUMENT_VIEWER. The default value is
 * %WEBKIT_CACHE_MODEL_WEB_BROWSER.
 */
void webkit_web_context_set_cache_model(WebKitWebContext* context, WebKitCacheModel model)
{
    CacheModel cacheModel;

    g_return_if_fail(WEBKIT_IS_WEB_CONTEXT(context));

    switch (model) {
    case WEBKIT_CACHE_MODEL_DOCUMENT_VIEWER:
        cacheModel = CacheModel::DocumentViewer;
        break;
    case WEBKIT_CACHE_MODEL_WEB_BROWSER:
        cacheModel = CacheModel::PrimaryWebBrowser;
        break;
    case WEBKIT_CACHE_MODEL_DOCUMENT_BROWSER:
        cacheModel = CacheModel::DocumentBrowser;
        break;
    default:
        g_assert_not_reached();
    }

    if (cacheModel != context->priv->processPool->cacheModel())
        context->priv->processPool->setCacheModel(cacheModel);
}

/**
 * webkit_web_context_get_cache_model:
 * @context: the #WebKitWebContext
 *
 * Returns the current cache model. For more information about this
 * value check the documentation of the function
 * webkit_web_context_set_cache_model().
 *
 * Returns: the current #WebKitCacheModel
 */
WebKitCacheModel webkit_web_context_get_cache_model(WebKitWebContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_CONTEXT(context), WEBKIT_CACHE_MODEL_WEB_BROWSER);

    switch (context->priv->processPool->cacheModel()) {
    case CacheModel::DocumentViewer:
        return WEBKIT_CACHE_MODEL_DOCUMENT_VIEWER;
    case CacheModel::PrimaryWebBrowser:
        return WEBKIT_CACHE_MODEL_WEB_BROWSER;
    case CacheModel::DocumentBrowser:
        return WEBKIT_CACHE_MODEL_DOCUMENT_BROWSER;
    default:
        g_assert_not_reached();
    }

    return WEBKIT_CACHE_MODEL_WEB_BROWSER;
}

/**
 * webkit_web_context_clear_cache:
 * @context: a #WebKitWebContext
 *
 * Clears all resources currently cached.
 * See also webkit_web_context_set_cache_model().
 */
void webkit_web_context_clear_cache(WebKitWebContext* context)
{
    g_return_if_fail(WEBKIT_IS_WEB_CONTEXT(context));

    OptionSet<WebsiteDataType> websiteDataTypes;
    websiteDataTypes.add(WebsiteDataType::MemoryCache);
    websiteDataTypes.add(WebsiteDataType::DiskCache);
    auto& websiteDataStore = webkitWebsiteDataManagerGetDataStore(context->priv->websiteDataManager.get()).websiteDataStore();
    websiteDataStore.removeData(websiteDataTypes, -WallTime::infinity(), [] { });
}

/**
 * webkit_web_context_set_network_proxy_settings:
 * @context: a #WebKitWebContext
 * @proxy_mode: a #WebKitNetworkProxyMode
 * @proxy_settings: (allow-none): a #WebKitNetworkProxySettings, or %NULL
 *
 * Set the network proxy settings to be used by connections started in @context.
 * By default %WEBKIT_NETWORK_PROXY_MODE_DEFAULT is used, which means that the
 * system settings will be used (g_proxy_resolver_get_default()).
 * If you want to override the system default settings, you can either use
 * %WEBKIT_NETWORK_PROXY_MODE_NO_PROXY to make sure no proxies are used at all,
 * or %WEBKIT_NETWORK_PROXY_MODE_CUSTOM to provide your own proxy settings.
 * When @proxy_mode is %WEBKIT_NETWORK_PROXY_MODE_CUSTOM @proxy_settings must be
 * a valid #WebKitNetworkProxySettings; otherwise, @proxy_settings must be %NULL.
 *
 * Since: 2.16
 */
void webkit_web_context_set_network_proxy_settings(WebKitWebContext* context, WebKitNetworkProxyMode proxyMode, WebKitNetworkProxySettings* proxySettings)
{
    g_return_if_fail(WEBKIT_IS_WEB_CONTEXT(context));
    g_return_if_fail((proxyMode != WEBKIT_NETWORK_PROXY_MODE_CUSTOM && !proxySettings) || (proxyMode == WEBKIT_NETWORK_PROXY_MODE_CUSTOM && proxySettings));

    WebKitWebContextPrivate* priv = context->priv;
    switch (proxyMode) {
    case WEBKIT_NETWORK_PROXY_MODE_DEFAULT:
        priv->processPool->setNetworkProxySettings({ });
        break;
    case WEBKIT_NETWORK_PROXY_MODE_NO_PROXY:
        priv->processPool->setNetworkProxySettings(WebCore::SoupNetworkProxySettings(WebCore::SoupNetworkProxySettings::Mode::NoProxy));
        break;
    case WEBKIT_NETWORK_PROXY_MODE_CUSTOM:
        const auto& settings = webkitNetworkProxySettingsGetNetworkProxySettings(proxySettings);
        if (settings.isEmpty()) {
            g_warning("Invalid attempt to set custom network proxy settings with an empty WebKitNetworkProxySettings. Use "
                "WEBKIT_NETWORK_PROXY_MODE_NO_PROXY to not use any proxy or WEBKIT_NETWORK_PROXY_MODE_DEFAULT to use the default system settings");
        } else
            priv->processPool->setNetworkProxySettings(settings);
        break;
    }
}

typedef HashMap<DownloadProxy*, GRefPtr<WebKitDownload> > DownloadsMap;

static DownloadsMap& downloadsMap()
{
    static NeverDestroyed<DownloadsMap> downloads;
    return downloads;
}

/**
 * webkit_web_context_download_uri:
 * @context: a #WebKitWebContext
 * @uri: the URI to download
 *
 * Requests downloading of the specified URI string. The download operation
 * will not be associated to any #WebKitWebView, if you are interested in
 * starting a download from a particular #WebKitWebView use
 * webkit_web_view_download_uri() instead.
 *
 * Returns: (transfer full): a new #WebKitDownload representing
 *    the download operation.
 */
WebKitDownload* webkit_web_context_download_uri(WebKitWebContext* context, const gchar* uri)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_CONTEXT(context), nullptr);
    g_return_val_if_fail(uri, nullptr);

    GRefPtr<WebKitDownload> download = webkitWebContextStartDownload(context, uri, nullptr);
    return download.leakRef();
}

/**
 * webkit_web_context_get_cookie_manager:
 * @context: a #WebKitWebContext
 *
 * Get the #WebKitCookieManager of the @context's #WebKitWebsiteDataManager.
 *
 * Returns: (transfer none): the #WebKitCookieManager of @context.
 */
WebKitCookieManager* webkit_web_context_get_cookie_manager(WebKitWebContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_CONTEXT(context), nullptr);

    return webkit_website_data_manager_get_cookie_manager(context->priv->websiteDataManager.get());
}

static void ensureFaviconDatabase(WebKitWebContext* context)
{
    WebKitWebContextPrivate* priv = context->priv;
    if (priv->faviconDatabase)
        return;

    priv->faviconDatabase = adoptGRef(webkitFaviconDatabaseCreate());
}

static void webkitWebContextEnableIconDatabasePrivateBrowsingIfNeeded(WebKitWebContext* context, WebKitWebView* webView)
{
    if (webkit_web_context_is_ephemeral(context))
        return;
    if (!webkit_web_view_is_ephemeral(webView))
        return;

    if (!context->priv->ephemeralPageCount && context->priv->faviconDatabase)
        webkitFaviconDatabaseSetPrivateBrowsingEnabled(context->priv->faviconDatabase.get(), true);
    context->priv->ephemeralPageCount++;
}

static void webkitWebContextDisableIconDatabasePrivateBrowsingIfNeeded(WebKitWebContext* context, WebKitWebView* webView)
{
    if (webkit_web_context_is_ephemeral(context))
        return;
    if (!webkit_web_view_is_ephemeral(webView))
        return;

    ASSERT(context->priv->ephemeralPageCount);
    context->priv->ephemeralPageCount--;
    if (!context->priv->ephemeralPageCount && context->priv->faviconDatabase)
        webkitFaviconDatabaseSetPrivateBrowsingEnabled(context->priv->faviconDatabase.get(), false);
}

/**
 * webkit_web_context_set_favicon_database_directory:
 * @context: a #WebKitWebContext
 * @path: (allow-none): an absolute path to the icon database
 * directory or %NULL to use the defaults
 *
 * Set the directory path to be used to store the favicons database
 * for @context on disk. Passing %NULL as @path means using the
 * default directory for the platform (see g_get_user_cache_dir()).
 *
 * Calling this method also means enabling the favicons database for
 * its use from the applications, so that's why it's expected to be
 * called only once. Further calls for the same instance of
 * #WebKitWebContext won't cause any effect.
 */
void webkit_web_context_set_favicon_database_directory(WebKitWebContext* context, const gchar* path)
{
    g_return_if_fail(WEBKIT_IS_WEB_CONTEXT(context));

    WebKitWebContextPrivate* priv = context->priv;
    ensureFaviconDatabase(context);

    String directoryPath = FileSystem::stringFromFileSystemRepresentation(path);
    // Use default if nullptr is passed as parameter.
    if (directoryPath.isEmpty()) {
#if PLATFORM(GTK)
        const char* portDirectory = "webkitgtk";
#elif PLATFORM(WPE)
        const char* portDirectory = "wpe";
#endif
        GUniquePtr<gchar> databaseDirectory(g_build_filename(g_get_user_cache_dir(), portDirectory, "icondatabase", nullptr));
        directoryPath = FileSystem::stringFromFileSystemRepresentation(databaseDirectory.get());
    }
    priv->faviconDatabaseDirectory = directoryPath.utf8();

    // Build the full path to the icon database file on disk.
    GUniquePtr<gchar> faviconDatabasePath(g_build_filename(priv->faviconDatabaseDirectory.data(),
        "WebpageIcons.db", nullptr));

    // Setting the path will cause the icon database to be opened.
    webkitFaviconDatabaseOpen(priv->faviconDatabase.get(), FileSystem::stringFromFileSystemRepresentation(faviconDatabasePath.get()));

    if (webkit_web_context_is_ephemeral(context))
        webkitFaviconDatabaseSetPrivateBrowsingEnabled(priv->faviconDatabase.get(), true);
}

/**
 * webkit_web_context_get_favicon_database_directory:
 * @context: a #WebKitWebContext
 *
 * Get the directory path being used to store the favicons database
 * for @context, or %NULL if
 * webkit_web_context_set_favicon_database_directory() hasn't been
 * called yet.
 *
 * This function will always return the same path after having called
 * webkit_web_context_set_favicon_database_directory() for the first
 * time.
 *
 * Returns: (transfer none): the path of the directory of the favicons
 * database associated with @context, or %NULL.
 */
const gchar* webkit_web_context_get_favicon_database_directory(WebKitWebContext *context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_CONTEXT(context), 0);

    WebKitWebContextPrivate* priv = context->priv;
    if (priv->faviconDatabaseDirectory.isNull())
        return 0;

    return priv->faviconDatabaseDirectory.data();
}

/**
 * webkit_web_context_get_favicon_database:
 * @context: a #WebKitWebContext
 *
 * Get the #WebKitFaviconDatabase associated with @context.
 *
 * To initialize the database you need to call
 * webkit_web_context_set_favicon_database_directory().
 *
 * Returns: (transfer none): the #WebKitFaviconDatabase of @context.
 */
WebKitFaviconDatabase* webkit_web_context_get_favicon_database(WebKitWebContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_CONTEXT(context), 0);

    ensureFaviconDatabase(context);
    return context->priv->faviconDatabase.get();
}

/**
 * webkit_web_context_get_security_manager:
 * @context: a #WebKitWebContext
 *
 * Get the #WebKitSecurityManager of @context.
 *
 * Returns: (transfer none): the #WebKitSecurityManager of @context.
 */
WebKitSecurityManager* webkit_web_context_get_security_manager(WebKitWebContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_CONTEXT(context), 0);

    WebKitWebContextPrivate* priv = context->priv;
    if (!priv->securityManager)
        priv->securityManager = adoptGRef(webkitSecurityManagerCreate(context));

    return priv->securityManager.get();
}

/**
 * webkit_web_context_set_additional_plugins_directory:
 * @context: a #WebKitWebContext
 * @directory: the directory to add
 *
 * Set an additional directory where WebKit will look for plugins.
 */
void webkit_web_context_set_additional_plugins_directory(WebKitWebContext* context, const char* directory)
{
    g_return_if_fail(WEBKIT_IS_WEB_CONTEXT(context));
    g_return_if_fail(directory);

#if ENABLE(NETSCAPE_PLUGIN_API)
    context->priv->processPool->setAdditionalPluginsDirectory(FileSystem::stringFromFileSystemRepresentation(directory));
#endif
}

static void destroyPluginList(GList* plugins)
{
    g_list_free_full(plugins, g_object_unref);
}

static void webkitWebContextGetPluginThread(GTask* task, gpointer object, gpointer /* taskData */, GCancellable*)
{
    GList* returnValue = 0;
#if ENABLE(NETSCAPE_PLUGIN_API)
    Vector<PluginModuleInfo> plugins = WEBKIT_WEB_CONTEXT(object)->priv->processPool->pluginInfoStore().plugins();
    for (size_t i = 0; i < plugins.size(); ++i)
        returnValue = g_list_prepend(returnValue, webkitPluginCreate(plugins[i]));
#endif
    g_task_return_pointer(task, returnValue, reinterpret_cast<GDestroyNotify>(destroyPluginList));
}

/**
 * webkit_web_context_get_plugins:
 * @context: a #WebKitWebContext
 * @cancellable: (allow-none): a #GCancellable or %NULL to ignore
 * @callback: (scope async): a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * Asynchronously get the list of installed plugins.
 *
 * When the operation is finished, @callback will be called. You can then call
 * webkit_web_context_get_plugins_finish() to get the result of the operation.
 */
void webkit_web_context_get_plugins(WebKitWebContext* context, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer userData)
{
    g_return_if_fail(WEBKIT_IS_WEB_CONTEXT(context));

    GRefPtr<GTask> task = adoptGRef(g_task_new(context, cancellable, callback, userData));
    g_task_run_in_thread(task.get(), webkitWebContextGetPluginThread);
}

/**
 * webkit_web_context_get_plugins_finish:
 * @context: a #WebKitWebContext
 * @result: a #GAsyncResult
 * @error: return location for error or %NULL to ignore
 *
 * Finish an asynchronous operation started with webkit_web_context_get_plugins.
 *
 * Returns: (element-type WebKitPlugin) (transfer full): a #GList of #WebKitPlugin. You must free the #GList with
 *    g_list_free() and unref the #WebKitPlugin<!-- -->s with g_object_unref() when you're done with them.
 */
GList* webkit_web_context_get_plugins_finish(WebKitWebContext* context, GAsyncResult* result, GError** error)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_CONTEXT(context), 0);
    g_return_val_if_fail(g_task_is_valid(result, context), 0);

    return static_cast<GList*>(g_task_propagate_pointer(G_TASK(result), error));
}

/**
 * webkit_web_context_register_uri_scheme:
 * @context: a #WebKitWebContext
 * @scheme: the network scheme to register
 * @callback: (scope async): a #WebKitURISchemeRequestCallback
 * @user_data: data to pass to callback function
 * @user_data_destroy_func: destroy notify for @user_data
 *
 * Register @scheme in @context, so that when an URI request with @scheme is made in the
 * #WebKitWebContext, the #WebKitURISchemeRequestCallback registered will be called with a
 * #WebKitURISchemeRequest.
 * It is possible to handle URI scheme requests asynchronously, by calling g_object_ref() on the
 * #WebKitURISchemeRequest and calling webkit_uri_scheme_request_finish() later
 * when the data of the request is available or
 * webkit_uri_scheme_request_finish_error() in case of error.
 *
 * <informalexample><programlisting>
 * static void
 * about_uri_scheme_request_cb (WebKitURISchemeRequest *request,
 *                              gpointer                user_data)
 * {
 *     GInputStream *stream;
 *     gsize         stream_length;
 *     const gchar  *path;
 *
 *     path = webkit_uri_scheme_request_get_path (request);
 *     if (!g_strcmp0 (path, "plugins")) {
 *         /<!-- -->* Create a GInputStream with the contents of plugins about page, and set its length to stream_length *<!-- -->/
 *     } else if (!g_strcmp0 (path, "memory")) {
 *         /<!-- -->* Create a GInputStream with the contents of memory about page, and set its length to stream_length *<!-- -->/
 *     } else if (!g_strcmp0 (path, "applications")) {
 *         /<!-- -->* Create a GInputStream with the contents of applications about page, and set its length to stream_length *<!-- -->/
 *     } else if (!g_strcmp0 (path, "example")) {
 *         gchar *contents;
 *
 *         contents = g_strdup_printf ("&lt;html&gt;&lt;body&gt;&lt;p&gt;Example about page&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;");
 *         stream_length = strlen (contents);
 *         stream = g_memory_input_stream_new_from_data (contents, stream_length, g_free);
 *     } else {
 *         GError *error;
 *
 *         error = g_error_new (ABOUT_HANDLER_ERROR, ABOUT_HANDLER_ERROR_INVALID, "Invalid about:%s page.", path);
 *         webkit_uri_scheme_request_finish_error (request, error);
 *         g_error_free (error);
 *         return;
 *     }
 *     webkit_uri_scheme_request_finish (request, stream, stream_length, "text/html");
 *     g_object_unref (stream);
 * }
 * </programlisting></informalexample>
 */
void webkit_web_context_register_uri_scheme(WebKitWebContext* context, const char* scheme, WebKitURISchemeRequestCallback callback, gpointer userData, GDestroyNotify destroyNotify)
{
    g_return_if_fail(WEBKIT_IS_WEB_CONTEXT(context));
    g_return_if_fail(scheme);
    g_return_if_fail(callback);

    RefPtr<WebKitURISchemeHandler> handler = adoptRef(new WebKitURISchemeHandler(callback, userData, destroyNotify));
    auto addResult = context->priv->uriSchemeHandlers.set(String::fromUTF8(scheme), handler.get());
    if (addResult.isNewEntry)
        context->priv->processPool->registerSchemeForCustomProtocol(String::fromUTF8(scheme));
}

/**
 * webkit_web_context_set_sandbox_enabled:
 * @context: a #WebKitWebContext
 * @enabled: if %TRUE enable sandboxing
 *
 * Set whether WebKit subprocesses will be sandboxed, limiting access to the system.
 *
 * This method **must be called before any web process has been created**,
 * as early as possible in your application. Calling it later is a fatal error.
 *
 * This is only implemented on Linux and is a no-op otherwise.
 *
 * Since: 2.26
 */
void webkit_web_context_set_sandbox_enabled(WebKitWebContext* context, gboolean enabled)
{
    g_return_if_fail(WEBKIT_IS_WEB_CONTEXT(context));

    if (context->priv->processPool->processes().size())
        g_error("Sandboxing cannot be changed after subprocesses were spawned.");

    context->priv->processPool->setSandboxEnabled(enabled);
}

/**
 * webkit_web_context_add_path_to_sandbox:
 * @context: a #WebKitWebContext
 * @path: (type filename): an absolute path to mount in the sandbox
 * @read_only: if %TRUE the path will be read-only
 *
 * Adds a path to be mounted in the sandbox. @path must exist before any web process
 * has been created otherwise it will be silently ignored. It is a fatal error to
 * add paths after a web process has been spawned.
 *
 * See also webkit_web_context_set_sandbox_enabled()
 *
 * Since: 2.26
 */
void webkit_web_context_add_path_to_sandbox(WebKitWebContext* context, const char* path, gboolean readOnly)
{
    g_return_if_fail(WEBKIT_IS_WEB_CONTEXT(context));
    g_return_if_fail(g_path_is_absolute(path));

    if (context->priv->processPool->processes().size())
        g_error("Sandbox paths cannot be changed after subprocesses were spawned.");

    auto permission = readOnly ? SandboxPermission::ReadOnly : SandboxPermission::ReadWrite;
    context->priv->processPool->addSandboxPath(path, permission);
}

/**
 * webkit_web_context_get_sandbox_enabled:
 * @context: a #WebKitWebContext
 *
 * Get whether sandboxing is currently enabled.
 *
 * Returns: %TRUE if sandboxing is enabled, or %FALSE otherwise.
 *
 * Since: 2.26
 */
gboolean webkit_web_context_get_sandbox_enabled(WebKitWebContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_CONTEXT(context), FALSE);

    return context->priv->processPool->sandboxEnabled();
}

/**
 * webkit_web_context_get_spell_checking_enabled:
 * @context: a #WebKitWebContext
 *
 * Get whether spell checking feature is currently enabled.
 *
 * Returns: %TRUE If spell checking is enabled, or %FALSE otherwise.
 */
gboolean webkit_web_context_get_spell_checking_enabled(WebKitWebContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_CONTEXT(context), FALSE);

#if ENABLE(SPELLCHECK)
    return TextChecker::state().isContinuousSpellCheckingEnabled;
#else
    return false;
#endif
}

/**
 * webkit_web_context_set_spell_checking_enabled:
 * @context: a #WebKitWebContext
 * @enabled: Value to be set
 *
 * Enable or disable the spell checking feature.
 */
void webkit_web_context_set_spell_checking_enabled(WebKitWebContext* context, gboolean enabled)
{
    g_return_if_fail(WEBKIT_IS_WEB_CONTEXT(context));

#if ENABLE(SPELLCHECK)
    TextChecker::setContinuousSpellCheckingEnabled(enabled);
#endif
}

/**
 * webkit_web_context_get_spell_checking_languages:
 * @context: a #WebKitWebContext
 *
 * Get the the list of spell checking languages associated with
 * @context, or %NULL if no languages have been previously set.
 *
 * See webkit_web_context_set_spell_checking_languages() for more
 * details on the format of the languages in the list.
 *
 * Returns: (array zero-terminated=1) (element-type utf8) (transfer none): A %NULL-terminated
 *    array of languages if available, or %NULL otherwise.
 */
const gchar* const* webkit_web_context_get_spell_checking_languages(WebKitWebContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_CONTEXT(context), nullptr);

#if ENABLE(SPELLCHECK)
    Vector<String> spellCheckingLanguages = TextChecker::loadedSpellCheckingLanguages();
    if (spellCheckingLanguages.isEmpty())
        return nullptr;

    static GRefPtr<GPtrArray> languagesToReturn;
    languagesToReturn = adoptGRef(g_ptr_array_new_with_free_func(g_free));
    for (const auto& language : spellCheckingLanguages)
        g_ptr_array_add(languagesToReturn.get(), g_strdup(language.utf8().data()));
    g_ptr_array_add(languagesToReturn.get(), nullptr);

    return reinterpret_cast<char**>(languagesToReturn->pdata);
#else
    return 0;
#endif
}

/**
 * webkit_web_context_set_spell_checking_languages:
 * @context: a #WebKitWebContext
 * @languages: (array zero-terminated=1) (transfer none): a %NULL-terminated list of spell checking languages
 *
 * Set the list of spell checking languages to be used for spell
 * checking.
 *
 * The locale string typically is in the form lang_COUNTRY, where lang
 * is an ISO-639 language code, and COUNTRY is an ISO-3166 country code.
 * For instance, sv_FI for Swedish as written in Finland or pt_BR
 * for Portuguese as written in Brazil.
 *
 * You need to call this function with a valid list of languages at
 * least once in order to properly enable the spell checking feature
 * in WebKit.
 */
void webkit_web_context_set_spell_checking_languages(WebKitWebContext* context, const gchar* const* languages)
{
    g_return_if_fail(WEBKIT_IS_WEB_CONTEXT(context));
    g_return_if_fail(languages);

#if ENABLE(SPELLCHECK)
    Vector<String> spellCheckingLanguages;
    for (size_t i = 0; languages[i]; ++i)
        spellCheckingLanguages.append(String::fromUTF8(languages[i]));
    TextChecker::setSpellCheckingLanguages(spellCheckingLanguages);
#endif
}

/**
 * webkit_web_context_set_preferred_languages:
 * @context: a #WebKitWebContext
 * @languages: (allow-none) (array zero-terminated=1) (element-type utf8) (transfer none): a %NULL-terminated list of language identifiers
 *
 * Set the list of preferred languages, sorted from most desirable
 * to least desirable. The list will be used to build the "Accept-Language"
 * header that will be included in the network requests started by
 * the #WebKitWebContext.
 */
void webkit_web_context_set_preferred_languages(WebKitWebContext* context, const gchar* const* languageList)
{
    g_return_if_fail(WEBKIT_IS_WEB_CONTEXT(context));

    if (!languageList || !g_strv_length(const_cast<char**>(languageList)))
        return;

    Vector<String> languages;
    for (size_t i = 0; languageList[i]; ++i) {
        // Do not propagate the C locale to WebCore.
        if (!g_ascii_strcasecmp(languageList[i], "C") || !g_ascii_strcasecmp(languageList[i], "POSIX"))
            languages.append("en-US"_s);
        else
            languages.append(String::fromUTF8(languageList[i]).replace("_", "-"));
    }
    overrideUserPreferredLanguages(languages);
}

/**
 * webkit_web_context_set_tls_errors_policy:
 * @context: a #WebKitWebContext
 * @policy: a #WebKitTLSErrorsPolicy
 *
 * Set the TLS errors policy of @context as @policy
 */
void webkit_web_context_set_tls_errors_policy(WebKitWebContext* context, WebKitTLSErrorsPolicy policy)
{
    g_return_if_fail(WEBKIT_IS_WEB_CONTEXT(context));

    if (context->priv->tlsErrorsPolicy == policy)
        return;

    context->priv->tlsErrorsPolicy = policy;
    bool ignoreTLSErrors = policy == WEBKIT_TLS_ERRORS_POLICY_IGNORE;
    if (context->priv->processPool->ignoreTLSErrors() != ignoreTLSErrors)
        context->priv->processPool->setIgnoreTLSErrors(ignoreTLSErrors);
}

/**
 * webkit_web_context_get_tls_errors_policy:
 * @context: a #WebKitWebContext
 *
 * Get the TLS errors policy of @context
 *
 * Returns: a #WebKitTLSErrorsPolicy
 */
WebKitTLSErrorsPolicy webkit_web_context_get_tls_errors_policy(WebKitWebContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_CONTEXT(context), WEBKIT_TLS_ERRORS_POLICY_IGNORE);

    return context->priv->tlsErrorsPolicy;
}

/**
 * webkit_web_context_set_web_extensions_directory:
 * @context: a #WebKitWebContext
 * @directory: the directory to add
 *
 * Set the directory where WebKit will look for Web Extensions.
 * This method must be called before loading anything in this context,
 * otherwise it will not have any effect. You can connect to
 * #WebKitWebContext::initialize-web-extensions to call this method
 * before anything is loaded.
 */
void webkit_web_context_set_web_extensions_directory(WebKitWebContext* context, const char* directory)
{
    g_return_if_fail(WEBKIT_IS_WEB_CONTEXT(context));
    g_return_if_fail(directory);

    context->priv->webExtensionsDirectory = directory;
}

/**
 * webkit_web_context_set_web_extensions_initialization_user_data:
 * @context: a #WebKitWebContext
 * @user_data: a #GVariant
 *
 * Set user data to be passed to Web Extensions on initialization.
 * The data will be passed to the
 * #WebKitWebExtensionInitializeWithUserDataFunction.
 * This method must be called before loading anything in this context,
 * otherwise it will not have any effect. You can connect to
 * #WebKitWebContext::initialize-web-extensions to call this method
 * before anything is loaded.
 *
 * Since: 2.4
 */
void webkit_web_context_set_web_extensions_initialization_user_data(WebKitWebContext* context, GVariant* userData)
{
    g_return_if_fail(WEBKIT_IS_WEB_CONTEXT(context));
    g_return_if_fail(userData);

    context->priv->webExtensionsInitializationUserData = userData;
}

#if PLATFORM(GTK)
/**
 * webkit_web_context_set_disk_cache_directory:
 * @context: a #WebKitWebContext
 * @directory: the directory to set
 *
 * Set the directory where disk cache files will be stored
 * This method must be called before loading anything in this context, otherwise
 * it will not have any effect.
 *
 * Note that this method overrides the directory set in the #WebKitWebsiteDataManager,
 * but it doesn't change the value returned by webkit_website_data_manager_get_disk_cache_directory()
 * since the #WebKitWebsiteDataManager is immutable.
 *
 * Deprecated: 2.10. Use webkit_web_context_new_with_website_data_manager() instead.
 */
void webkit_web_context_set_disk_cache_directory(WebKitWebContext* context, const char* directory)
{
    g_return_if_fail(WEBKIT_IS_WEB_CONTEXT(context));
    g_return_if_fail(directory);

    context->priv->processPool->configuration().setDiskCacheDirectory(FileSystem::pathByAppendingComponent(FileSystem::stringFromFileSystemRepresentation(directory), networkCacheSubdirectory));
}
#endif

/**
 * webkit_web_context_prefetch_dns:
 * @context: a #WebKitWebContext
 * @hostname: a hostname to be resolved
 *
 * Resolve the domain name of the given @hostname in advance, so that if a URI
 * of @hostname is requested the load will be performed more quickly.
 */
void webkit_web_context_prefetch_dns(WebKitWebContext* context, const char* hostname)
{
    g_return_if_fail(WEBKIT_IS_WEB_CONTEXT(context));
    g_return_if_fail(hostname);

    API::Dictionary::MapType message;
    message.set(String::fromUTF8("Hostname"), API::String::create(String::fromUTF8(hostname)));
    context->priv->processPool->postMessageToInjectedBundle(String::fromUTF8("PrefetchDNS"), API::Dictionary::create(WTFMove(message)).ptr());
}

/**
 * webkit_web_context_allow_tls_certificate_for_host:
 * @context: a #WebKitWebContext
 * @certificate: a #GTlsCertificate
 * @host: the host for which a certificate is to be allowed
 *
 * Ignore further TLS errors on the @host for the certificate present in @info.
 *
 * Since: 2.6
 */
void webkit_web_context_allow_tls_certificate_for_host(WebKitWebContext* context, GTlsCertificate* certificate, const gchar* host)
{
    g_return_if_fail(WEBKIT_IS_WEB_CONTEXT(context));
    g_return_if_fail(G_IS_TLS_CERTIFICATE(certificate));
    g_return_if_fail(host);

    auto webCertificateInfo = WebCertificateInfo::create(WebCore::CertificateInfo(certificate, static_cast<GTlsCertificateFlags>(0)));
    context->priv->processPool->allowSpecificHTTPSCertificateForHost(webCertificateInfo.ptr(), String::fromUTF8(host));
}

/**
 * webkit_web_context_set_process_model:
 * @context: the #WebKitWebContext
 * @process_model: a #WebKitProcessModel
 *
 * Specifies a process model for WebViews, which WebKit will use to
 * determine how auxiliary processes are handled. The default setting
 * (%WEBKIT_PROCESS_MODEL_SHARED_SECONDARY_PROCESS) is suitable for most
 * applications which embed a small amount of WebViews, or are used to
 * display documents which are considered safe — like local files.
 *
 * Applications which may potentially use a large amount of WebViews
 * —for example a multi-tabbed web browser— may want to use
 * %WEBKIT_PROCESS_MODEL_MULTIPLE_SECONDARY_PROCESSES, which will use
 * one process per view most of the time, while still allowing for web
 * views to share a process when needed (for example when different
 * views interact with each other). Using this model, when a process
 * hangs or crashes, only the WebViews using it stop working, while
 * the rest of the WebViews in the application will still function
 * normally.
 *
 * This method **must be called before any web process has been created**,
 * as early as possible in your application. Calling it later will make
 * your application crash.
 *
 * Since: 2.4
 */
void webkit_web_context_set_process_model(WebKitWebContext* context, WebKitProcessModel processModel)
{
    g_return_if_fail(WEBKIT_IS_WEB_CONTEXT(context));

    if (processModel == context->priv->processModel)
        return;

    context->priv->processModel = processModel;
}

/**
 * webkit_web_context_get_process_model:
 * @context: the #WebKitWebContext
 *
 * Returns the current process model. For more information about this value
 * see webkit_web_context_set_process_model().
 *
 * Returns: the current #WebKitProcessModel
 *
 * Since: 2.4
 */
WebKitProcessModel webkit_web_context_get_process_model(WebKitWebContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_CONTEXT(context), WEBKIT_PROCESS_MODEL_SHARED_SECONDARY_PROCESS);

    return context->priv->processModel;
}

/**
 * webkit_web_context_set_web_process_count_limit:
 * @context: the #WebKitWebContext
 * @limit: the maximum number of web processes
 *
 * Sets the maximum number of web processes that can be created at the same time for the @context.
 * The default value is 0 and means no limit.
 *
 * This method **must be called before any web process has been created**,
 * as early as possible in your application. Calling it later will make
 * your application crash.
 *
 * Since: 2.10
 */
void webkit_web_context_set_web_process_count_limit(WebKitWebContext* context, guint limit)
{
    g_return_if_fail(WEBKIT_IS_WEB_CONTEXT(context));

    if (context->priv->processCountLimit == limit)
        return;

    context->priv->processCountLimit = limit;
}

/**
 * webkit_web_context_get_web_process_count_limit:
 * @context: the #WebKitWebContext
 *
 * Gets the maximum number of web processes that can be created at the same time for the @context.
 *
 * Returns: the maximum limit of web processes, or 0 if there isn't a limit.
 *
 * Since: 2.10
 */
guint webkit_web_context_get_web_process_count_limit(WebKitWebContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_CONTEXT(context), 0);

    return context->priv->processCountLimit;
}

static void addOriginToMap(WebKitSecurityOrigin* origin, HashMap<String, bool>* map, bool allowed)
{
    String string = webkitSecurityOriginGetSecurityOrigin(origin).toString();
    if (string != "null")
        map->set(string, allowed);
}

/**
 * webkit_web_context_initialize_notification_permissions:
 * @context: the #WebKitWebContext
 * @allowed_origins: (element-type WebKitSecurityOrigin): a #GList of security origins
 * @disallowed_origins: (element-type WebKitSecurityOrigin): a #GList of security origins
 *
 * Sets initial desktop notification permissions for the @context.
 * @allowed_origins and @disallowed_origins must each be #GList of
 * #WebKitSecurityOrigin objects representing origins that will,
 * respectively, either always or never have permission to show desktop
 * notifications. No #WebKitNotificationPermissionRequest will ever be
 * generated for any of the security origins represented in
 * @allowed_origins or @disallowed_origins. This function is necessary
 * because some webpages proactively check whether they have permission
 * to display notifications without ever creating a permission request.
 *
 * This function only affects web processes that have not already been
 * created. The best time to call it is when handling
 * #WebKitWebContext::initialize-notification-permissions so as to
 * ensure that new web processes receive the most recent set of
 * permissions.
 *
 * Since: 2.16
 */
void webkit_web_context_initialize_notification_permissions(WebKitWebContext* context, GList* allowedOrigins, GList* disallowedOrigins)
{
    HashMap<String, bool> map;
    g_list_foreach(allowedOrigins, [](gpointer data, gpointer userData) {
        addOriginToMap(static_cast<WebKitSecurityOrigin*>(data), static_cast<HashMap<String, bool>*>(userData), true);
    }, &map);
    g_list_foreach(disallowedOrigins, [](gpointer data, gpointer userData) {
        addOriginToMap(static_cast<WebKitSecurityOrigin*>(data), static_cast<HashMap<String, bool>*>(userData), false);
    }, &map);
    context->priv->notificationProvider->setNotificationPermissions(WTFMove(map));
}

void webkitWebContextInitializeNotificationPermissions(WebKitWebContext* context)
{
    g_signal_emit(context, signals[INITIALIZE_NOTIFICATION_PERMISSIONS], 0);
}

WebKitDownload* webkitWebContextGetOrCreateDownload(DownloadProxy* downloadProxy)
{
    GRefPtr<WebKitDownload> download = downloadsMap().get(downloadProxy);
    if (download)
        return download.get();

    download = adoptGRef(webkitDownloadCreate(downloadProxy));
    downloadsMap().set(downloadProxy, download.get());
    return download.get();
}

WebKitDownload* webkitWebContextStartDownload(WebKitWebContext* context, const char* uri, WebPageProxy* initiatingPage)
{
    WebCore::ResourceRequest request(String::fromUTF8(uri));
    return webkitWebContextGetOrCreateDownload(context->priv->processPool->download(initiatingPage, request));
}

void webkitWebContextRemoveDownload(DownloadProxy* downloadProxy)
{
    downloadsMap().remove(downloadProxy);
}

void webkitWebContextDownloadStarted(WebKitWebContext* context, WebKitDownload* download)
{
    g_signal_emit(context, signals[DOWNLOAD_STARTED], 0, download);
}

GVariant* webkitWebContextInitializeWebExtensions(WebKitWebContext* context)
{
    g_signal_emit(context, signals[INITIALIZE_WEB_EXTENSIONS], 0);
    return g_variant_new("(msmv)",
        context->priv->webExtensionsDirectory.data(),
        context->priv->webExtensionsInitializationUserData.get());
}

WebProcessPool& webkitWebContextGetProcessPool(WebKitWebContext* context)
{
    g_assert(WEBKIT_IS_WEB_CONTEXT(context));

    return *context->priv->processPool;
}

void webkitWebContextStartLoadingCustomProtocol(WebKitWebContext* context, uint64_t customProtocolID, const WebCore::ResourceRequest& resourceRequest, LegacyCustomProtocolManagerProxy& manager)
{
    GRefPtr<WebKitURISchemeRequest> request = adoptGRef(webkitURISchemeRequestCreate(customProtocolID, context, resourceRequest, manager));
    String scheme(String::fromUTF8(webkit_uri_scheme_request_get_scheme(request.get())));
    RefPtr<WebKitURISchemeHandler> handler = context->priv->uriSchemeHandlers.get(scheme);
    ASSERT(handler.get());
    if (!handler->hasCallback())
        return;

    context->priv->uriSchemeRequests.set(customProtocolID, request.get());
    handler->performCallback(request.get());
}

void webkitWebContextStopLoadingCustomProtocol(WebKitWebContext* context, uint64_t customProtocolID)
{
    GRefPtr<WebKitURISchemeRequest> request = context->priv->uriSchemeRequests.get(customProtocolID);
    if (!request.get())
        return;
    webkitURISchemeRequestCancel(request.get());
}

void webkitWebContextInvalidateCustomProtocolRequests(WebKitWebContext* context, LegacyCustomProtocolManagerProxy& manager)
{
    for (auto& request : copyToVector(context->priv->uriSchemeRequests.values())) {
        if (webkitURISchemeRequestGetManager(request.get()) == &manager)
            webkitURISchemeRequestInvalidate(request.get());
    }
}

void webkitWebContextDidFinishLoadingCustomProtocol(WebKitWebContext* context, uint64_t customProtocolID)
{
    context->priv->uriSchemeRequests.remove(customProtocolID);
}

bool webkitWebContextIsLoadingCustomProtocol(WebKitWebContext* context, uint64_t customProtocolID)
{
    return context->priv->uriSchemeRequests.get(customProtocolID);
}

void webkitWebContextCreatePageForWebView(WebKitWebContext* context, WebKitWebView* webView, WebKitUserContentManager* userContentManager, WebKitWebView* relatedView)
{
    // FIXME: icon database private mode is global, not per page, so while there are
    // pages in private mode we need to enable the private mode in the icon database.
    webkitWebContextEnableIconDatabasePrivateBrowsingIfNeeded(context, webView);

    auto pageConfiguration = API::PageConfiguration::create();
    pageConfiguration->setProcessPool(context->priv->processPool.get());
    pageConfiguration->setPreferences(webkitSettingsGetPreferences(webkit_web_view_get_settings(webView)));
    pageConfiguration->setRelatedPage(relatedView ? &webkitWebViewGetPage(relatedView) : nullptr);
    pageConfiguration->setUserContentController(userContentManager ? webkitUserContentManagerGetUserContentControllerProxy(userContentManager) : nullptr);
    pageConfiguration->setControlledByAutomation(webkit_web_view_is_controlled_by_automation(webView));

    WebKitWebsiteDataManager* manager = webkitWebViewGetWebsiteDataManager(webView);
    if (!manager)
        manager = context->priv->websiteDataManager.get();
    pageConfiguration->setWebsiteDataStore(&webkitWebsiteDataManagerGetDataStore(manager));
    pageConfiguration->setSessionID(pageConfiguration->websiteDataStore()->websiteDataStore().sessionID());
    webkitWebViewCreatePage(webView, WTFMove(pageConfiguration));

    context->priv->webViews.set(webkit_web_view_get_page_id(webView), webView);
}

void webkitWebContextWebViewDestroyed(WebKitWebContext* context, WebKitWebView* webView)
{
    webkitWebContextDisableIconDatabasePrivateBrowsingIfNeeded(context, webView);
    context->priv->webViews.remove(webkit_web_view_get_page_id(webView));
}

WebKitWebView* webkitWebContextGetWebViewForPage(WebKitWebContext* context, WebPageProxy* page)
{
    return page ? context->priv->webViews.get(page->pageID()) : 0;
}
