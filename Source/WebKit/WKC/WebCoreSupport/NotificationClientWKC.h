/*
 * Web Notifications API bridge for the WKC embedder.
 *
 * The embedder owns permission persistence and native notification delivery.
 * This bridge keeps WebCore independent of Aroma-specific libraries.
 */
#pragma once

#include "config.h"

#if ENABLE(NOTIFICATIONS)
#include "Modules/notifications/NotificationClient.h"

namespace WKC {

class ChromeClientWKC;

class NotificationClientWKC final : public WebCore::NotificationClient {
public:
    explicit NotificationClientWKC(ChromeClientWKC&);

    bool show(WebCore::ScriptExecutionContext&, WebCore::NotificationData&&,
        RefPtr<WebCore::NotificationResources>&&, CompletionHandler<void()>&&) final;
    void cancel(WebCore::NotificationData&&) final;
    void notificationObjectDestroyed(WebCore::NotificationData&&) final;
    void notificationControllerDestroyed() final;
    void requestPermission(WebCore::ScriptExecutionContext&, PermissionHandler&&) final;
    Permission checkPermission(WebCore::ScriptExecutionContext*) final;

private:
    ChromeClientWKC& m_chromeClient;
};

} // namespace WKC
#endif // ENABLE(NOTIFICATIONS)
