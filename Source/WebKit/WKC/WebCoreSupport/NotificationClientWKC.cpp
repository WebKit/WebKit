/*
 * Web Notifications API bridge for the WKC embedder.
 */
#include "config.h"

#if ENABLE(NOTIFICATIONS)
#include "NotificationClientWKC.h"

#include "ChromeClientWKC.h"
#include "NotificationData.h"
#include "NotificationResources.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"

#include <memory>

namespace WKC {
namespace {

WebCore::NotificationPermission permissionFromEmbedder(int state)
{
    switch (state) {
    case 1:
        return WebCore::NotificationPermission::Granted;
    case 0:
        return WebCore::NotificationPermission::Denied;
    default:
        return WebCore::NotificationPermission::Default;
    }
}

struct PendingNotificationPermission {
    explicit PendingNotificationPermission(WebCore::NotificationClient::PermissionHandler&& handler)
        : completion(WTFMove(handler))
    {
    }
    WebCore::NotificationClient::PermissionHandler completion;
};

void notificationPermissionCompleted(void* context, int permission)
{
    std::unique_ptr<PendingNotificationPermission> pending(static_cast<PendingNotificationPermission*>(context));
    pending->completion(permissionFromEmbedder(permission));
}

} // namespace

NotificationClientWKC::NotificationClientWKC(ChromeClientWKC& chromeClient)
    : m_chromeClient(chromeClient)
{
}

bool
NotificationClientWKC::show(WebCore::ScriptExecutionContext&, WebCore::NotificationData&& data,
    RefPtr<WebCore::NotificationResources>&&, CompletionHandler<void()>&& completion)
{
    // Text delivery is the minimum portable contract. The embedder decides whether
    // to use a system popup, the official inbox, or another supported destination.
    if (!m_chromeClient.showWebNotification(data.originString, data.title, data.body))
        return false;

    completion();
    return true;
}

void
NotificationClientWKC::cancel(WebCore::NotificationData&& data)
{
    m_chromeClient.cancelWebNotification(data.originString, data.title);
}

void
NotificationClientWKC::notificationObjectDestroyed(WebCore::NotificationData&&)
{
    // A host may let an already delivered system notification remain visible.
}

void
NotificationClientWKC::notificationControllerDestroyed()
{
    // WKCWebViewPrivate keeps this object alive through Page teardown.
}

void
NotificationClientWKC::requestPermission(WebCore::ScriptExecutionContext& context, PermissionHandler&& completion)
{
    auto origin = context.securityOrigin()->toString();
    auto pending = std::make_unique<PendingNotificationPermission>(WTFMove(completion));
    auto* pendingContext = pending.release();
    m_chromeClient.requestWebNotificationPermission(origin, notificationPermissionCompleted, pendingContext);
}

WebCore::NotificationPermission
NotificationClientWKC::checkPermission(WebCore::ScriptExecutionContext* context)
{
    if (!context)
        return WebCore::NotificationPermission::Denied;

    auto origin = context->securityOrigin()->toString();
    return permissionFromEmbedder(m_chromeClient.checkWebNotificationPermission(origin));
}

} // namespace WKC
#endif // ENABLE(NOTIFICATIONS)
