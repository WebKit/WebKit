/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#import "config.h"
#if ENABLE(WEB_PUSH_NOTIFICATIONS)

#import "PushClientConnection.h"

#import "CodeSigning.h"
#import "DaemonEncoder.h"
#import "DaemonUtilities.h"
#import "Logging.h"
#import "PushClientConnectionMessages.h"
#import "WebPushDaemon.h"
#import "WebPushDaemonConnectionConfiguration.h"
#import "WebPushDaemonConstants.h"
#import <JavaScriptCore/ConsoleTypes.h>
#import <WebCore/NotificationData.h>
#import <WebCore/PushPermissionState.h>
#import <wtf/ASCIICType.h>
#import <wtf/HexNumber.h>
#import <wtf/StdLibExtras.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/Vector.h>
#import <wtf/cocoa/Entitlements.h>
#import <wtf/text/TextStream.h>

#if PLATFORM(MAC)
#import <bsm/libbsm.h>
#import <pal/spi/cocoa/LaunchServicesSPI.h>
#endif

#if USE(EXTENSIONKIT)
#import "RunningBoardServicesSPI.h"
#endif

#if PLATFORM(IOS)
#import "UIKitSPI.h"
#endif

#if USE(APPLE_INTERNAL_SDK) && __has_include(<WebKitAdditions/PushClientConnectionAdditions.mm>)
#import <WebKitAdditions/PushClientConnectionAdditions.mm>
#endif

#if !defined(PUSH_CLIENT_CONNECTION_CREATE_ADDITIONS)
#define PUSH_CLIENT_CONNECTION_CREATE_ADDITIONS
#endif

using WebKit::Daemon::Encoder;

static const ASCIILiteral hostAppWebPushEntitlement = "com.apple.private.webkit.webpush"_s;

namespace WebPushD {

static bool hostAppHasEntitlement(audit_token_t hostAppAuditToken, ASCIILiteral entitlement)
{
#if PLATFORM(MAC) && !USE(APPLE_INTERNAL_SDK)
    // Skip entitlement check on Mac open source builds.
    return true;
#else
    return WTF::hasEntitlement(hostAppAuditToken, entitlement);
#endif
}

static String bundleIdentifierFromAuditToken(audit_token_t token)
{
#if PLATFORM(MAC) && !USE(APPLE_INTERNAL_SDK)
    // This isn't great, but currently the only user of webpushd in open source builds is TestWebKitAPI and codeSigningIdentifier returns the null String on x86_64 Macs.
    UNUSED_PARAM(token);
    return "com.apple.WebKit.TestWebKitAPI"_s;
#elif PLATFORM(MAC)
    LSSessionID sessionID = (LSSessionID)audit_token_to_asid(token);
    auto auditTokenDataRef = adoptCF(CFDataCreate(kCFAllocatorDefault, (const UInt8 *)(&token), sizeof(token)));
    CFTypeRef keys[] = { _kLSAuditTokenKey };
    CFTypeRef values[] = { auditTokenDataRef.get() };
    auto matchingAppsRef = adoptCF(_LSCopyMatchingApplicationsWithItems(sessionID, 1, keys, values));
    if (matchingAppsRef && CFArrayGetCount(matchingAppsRef.get())) {
        LSASNRef asnRef = (LSASNRef)CFArrayGetValueAtIndex(matchingAppsRef.get(), 0);
        auto bundleIdentifierRef = adoptCF((CFStringRef)_LSCopyApplicationInformationItem(sessionID, asnRef, kCFBundleIdentifierKey));
        if (bundleIdentifierRef && CFStringGetLength(bundleIdentifierRef.get()))
            return bundleIdentifierRef.get();
    }
#endif // PLATFORM(MAC)

    return WebKit::codeSigningIdentifier(token);
}

static bool isValidPushPartition(String partition)
{
#if PLATFORM(IOS)
    // We allow the client to not specify a partition at all at connection setup time. In this case,
    // we select the partition dynamically (see subscriptionSetIdentifierForOrigin).
    if (partition.isEmpty())
        return true;

    // On iOS, the push partition is expected to match the format of a web clip identifier.
    auto isASCIIDigitOrUpper = [](UChar character) {
        return isASCIIDigit(character) || isASCIIUpper(character);
    };
    return partition.length() == 32 && partition.containsOnly<isASCIIDigitOrUpper>();
#else
    UNUSED_PARAM(partition);
    return true;
#endif
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(PushClientConnection);

RefPtr<PushClientConnection> PushClientConnection::create(xpc_connection_t connection, IPC::Decoder& initialMessageDecoder)
{
    if (initialMessageDecoder.messageName() != Messages::PushClientConnection::InitializeConnection::name()) {
        RELEASE_LOG_ERROR(Push, "PushClientConnection::create failed: first message must be InitializeConnection");
        return nullptr;
    }

    auto maybeConfiguration = initialMessageDecoder.decode<WebPushDaemonConnectionConfiguration>();
    if (!maybeConfiguration) {
        RELEASE_LOG_ERROR(Push, "PushClientConnection::create failed: could not decode InitializeConnection arguments");
        return nullptr;
    }
    auto& configuration = *maybeConfiguration;

    // This is to handle webpushtool, which is a direct peer that has the injection entitlement but no host app.
    audit_token_t peerAuditToken;
    xpc_connection_get_audit_token(connection, &peerAuditToken);
    if (bool peerHasPushInjectEntitlement = WTF::hasEntitlement(peerAuditToken, "com.apple.private.webkit.webpush.inject"_s))
        return adoptRef(new PushClientConnection(connection, WTFMove(configuration.bundleIdentifierOverride), peerHasPushInjectEntitlement, WTFMove(configuration.pushPartitionString), WTFMove(configuration.dataStoreIdentifier)));

#if USE(EXTENSIONKIT)
    pid_t pid = xpc_connection_get_pid(connection);
    NSError *error = nil;
    RBSProcessHandle *handle = [RBSProcessHandle handleForIdentifier:[RBSProcessIdentifier identifierWithPid:pid] error:&error];
    if (error) {
        RELEASE_LOG_ERROR(Push, "PushClientConnection::create failed: couldn't look up remote pid %d: %{public}@", pid, error);
        return nullptr;
    }

    if (!handle.hostProcess) {
        RELEASE_LOG_ERROR(Push, "PushClientConnection::create failed: remote pid %d has no host process", pid);
        return nullptr;
    }

    audit_token_t hostAppAuditToken = handle.hostProcess.auditToken;
#else
    audit_token_t hostAppAuditToken { };
    if (configuration.hostAppAuditTokenData.size() != sizeof(audit_token_t)) {
        RELEASE_LOG_ERROR(Push, "PushClientConnection::create failed: client attempted to set audit token with incorrect size");
        return nullptr;
    }

    memcpySpan(asMutableByteSpan(hostAppAuditToken), configuration.hostAppAuditTokenData.span());
#endif

    bool hostAppHasWebPushEntitlement = hostAppHasEntitlement(hostAppAuditToken, hostAppWebPushEntitlement);
    String hostAppCodeSigningIdentifier = bundleIdentifierFromAuditToken(hostAppAuditToken);
    bool hostAppHasPushInjectEntitlement = hostAppHasEntitlement(hostAppAuditToken, "com.apple.private.webkit.webpush.inject"_s);
    auto pushPartition = WTFMove(configuration.pushPartitionString);
    bool hasValidPushPartition = isValidPushPartition(pushPartition);

    if (hostAppHasPushInjectEntitlement && !configuration.bundleIdentifierOverride.isEmpty())
        hostAppCodeSigningIdentifier = configuration.bundleIdentifierOverride;

    PUSH_CLIENT_CONNECTION_CREATE_ADDITIONS;

    if (!hostAppHasWebPushEntitlement) {
        RELEASE_LOG_ERROR(Push, "PushClientConnection::create failed: client is missing Web Push entitlement");
        return nullptr;
    }

    if (hostAppCodeSigningIdentifier.isEmpty()) {
        RELEASE_LOG_ERROR(Push, "PushClientConnection::create failed: cannot determine code signing identifier of client");
        return nullptr;
    }

    if (!hasValidPushPartition) {
        RELEASE_LOG_ERROR(Push, "PushClientConnection::create failed: invalid push partition %{public}s", pushPartition.utf8().data());
        return nullptr;
    }

    return adoptRef(new PushClientConnection(connection, WTFMove(hostAppCodeSigningIdentifier), hostAppHasPushInjectEntitlement, WTFMove(pushPartition), WTFMove(configuration.dataStoreIdentifier)));
}

PushClientConnection::PushClientConnection(xpc_connection_t connection, String&& hostAppCodeSigningIdentifier, bool hostAppHasPushInjectEntitlement, String&& pushPartitionString, std::optional<WTF::UUID>&& dataStoreIdentifier)
    : m_xpcConnection(connection)
    , m_hostAppCodeSigningIdentifier(WTFMove(hostAppCodeSigningIdentifier))
    , m_hostAppHasPushInjectEntitlement(hostAppHasPushInjectEntitlement)
    , m_pushPartitionString(pushPartitionString)
    , m_dataStoreIdentifier(WTFMove(dataStoreIdentifier))
{
}

void PushClientConnection::initializeConnection(WebPushDaemonConnectionConfiguration&&)
{
    RELEASE_LOG_ERROR(Push, "PushClientConnection::initializeConnection(%p): ignoring duplicate message", this);
}

void PushClientConnection::getPushTopicsForTesting(CompletionHandler<void(Vector<String>, Vector<String>)>&& completionHandler)
{
    WebPushDaemon::singleton().getPushTopicsForTesting(*this, WTFMove(completionHandler));
}

std::optional<WebCore::PushSubscriptionSetIdentifier> PushClientConnection::subscriptionSetIdentifierForOrigin(const WebCore::SecurityOriginData& origin) const
{
    String pushPartitionString = m_pushPartitionString;

#if PLATFORM(IOS)
    if (pushPartitionString.isEmpty()) {
        // On iOS, when the client doesn't explicitly specify a push partition (web clip id), we
        // implicitly choose the oldest web clip associated with that origin.
        auto& webClipCache = WebPushDaemon::singleton().ensureWebClipCache();
        pushPartitionString = webClipCache.preferredWebClipIdentifier(m_hostAppCodeSigningIdentifier, origin);

        if (pushPartitionString.isEmpty())
            return std::nullopt;
    }
#endif

    return WebCore::PushSubscriptionSetIdentifier { hostAppCodeSigningIdentifier(), pushPartitionString, dataStoreIdentifier() };
}

String PushClientConnection::debugDescription() const
{
    TextStream textStream;
    textStream << m_hostAppCodeSigningIdentifier;
    if (!m_pushPartitionString.isEmpty())
        textStream << " | part: " << m_pushPartitionString;
    if (m_dataStoreIdentifier)
        textStream << " | ds: " << m_dataStoreIdentifier->toString();
    return textStream.release();
}

void PushClientConnection::connectionClosed()
{
    RELEASE_ASSERT(m_xpcConnection);
    m_xpcConnection = nullptr;
}

void PushClientConnection::setPushAndNotificationsEnabledForOrigin(const String& originString, bool enabled, CompletionHandler<void()>&& replySender)
{
    WebPushDaemon::singleton().setPushAndNotificationsEnabledForOrigin(*this, originString, enabled, WTFMove(replySender));
}

void PushClientConnection::injectPushMessageForTesting(PushMessageForTesting&& message, CompletionHandler<void(const String&)>&& replySender)
{
    WebPushDaemon::singleton().injectPushMessageForTesting(*this, WTFMove(message), WTFMove(replySender));
}

void PushClientConnection::injectEncryptedPushMessageForTesting(const String& message, CompletionHandler<void(bool)>&& replySender)
{
    WebPushDaemon::singleton().injectEncryptedPushMessageForTesting(*this, message, WTFMove(replySender));
}

void PushClientConnection::getPendingPushMessage(CompletionHandler<void(const std::optional<WebKit::WebPushMessage>&)>&& replySender)
{
    WebPushDaemon::singleton().getPendingPushMessage(*this, WTFMove(replySender));
}

void PushClientConnection::getPendingPushMessages(CompletionHandler<void(const Vector<WebKit::WebPushMessage>&)>&& replySender)
{
    WebPushDaemon::singleton().getPendingPushMessages(*this, WTFMove(replySender));
}

void PushClientConnection::subscribeToPushService(URL&& scopeURL, const Vector<uint8_t>& applicationServerKey, CompletionHandler<void(const Expected<WebCore::PushSubscriptionData, WebCore::ExceptionData>&)>&& replySender)
{
    WebPushDaemon::singleton().subscribeToPushService(*this, WTFMove(scopeURL), applicationServerKey, WTFMove(replySender));
}

void PushClientConnection::unsubscribeFromPushService(URL&& scopeURL, std::optional<WebCore::PushSubscriptionIdentifier> identifier, CompletionHandler<void(const Expected<bool, WebCore::ExceptionData>&)>&& replySender)
{
    WebPushDaemon::singleton().unsubscribeFromPushService(*this, WTFMove(scopeURL), WTFMove(identifier), WTFMove(replySender));
}

void PushClientConnection::getPushSubscription(URL&& scopeURL, CompletionHandler<void(const Expected<std::optional<WebCore::PushSubscriptionData>, WebCore::ExceptionData>&)>&& replySender)
{
    WebPushDaemon::singleton().getPushSubscription(*this, WTFMove(scopeURL), WTFMove(replySender));
}

void PushClientConnection::incrementSilentPushCount(WebCore::SecurityOriginData&& origin, CompletionHandler<void(unsigned)>&& replySender)
{
    WebPushDaemon::singleton().incrementSilentPushCount(*this, WTFMove(origin), WTFMove(replySender));
}

void PushClientConnection::removeAllPushSubscriptions(CompletionHandler<void(unsigned)>&& replySender)
{
    WebPushDaemon::singleton().removeAllPushSubscriptions(*this, WTFMove(replySender));
}

void PushClientConnection::removePushSubscriptionsForOrigin(WebCore::SecurityOriginData&& origin, CompletionHandler<void(unsigned)>&& replySender)
{
    WebPushDaemon::singleton().removePushSubscriptionsForOrigin(*this, WTFMove(origin), WTFMove(replySender));
}

void PushClientConnection::setPublicTokenForTesting(const String& publicToken, CompletionHandler<void()>&& replySender)
{
    WebPushDaemon::singleton().setPublicTokenForTesting(*this, publicToken, WTFMove(replySender));
}

void PushClientConnection::getPushPermissionState(WebCore::SecurityOriginData&& origin, CompletionHandler<void(WebCore::PushPermissionState)>&& replySender)
{
#if HAVE(FULL_FEATURED_USER_NOTIFICATIONS)
    WebPushDaemon::singleton().getPushPermissionState(*this, WTFMove(origin), WTFMove(replySender));
#else
    UNUSED_PARAM(origin);
    replySender({ });
#endif
}

void PushClientConnection::requestPushPermission(WebCore::SecurityOriginData&& origin, CompletionHandler<void(bool)>&& replySender)
{
#if HAVE(FULL_FEATURED_USER_NOTIFICATIONS)
    WebPushDaemon::singleton().requestPushPermission(*this, WTFMove(origin), WTFMove(replySender));
#else
    UNUSED_PARAM(origin);
    replySender(false);
#endif
}

void PushClientConnection::showNotification(const WebCore::NotificationData& notificationData, RefPtr<WebCore::NotificationResources> notificationResources, CompletionHandler<void()>&& completionHandler)
{
#if HAVE(FULL_FEATURED_USER_NOTIFICATIONS)
    WebPushDaemon::singleton().showNotification(*this, notificationData, notificationResources, WTFMove(completionHandler));
#else
    UNUSED_PARAM(notificationData);
    UNUSED_PARAM(notificationResources);
    completionHandler();
#endif
}

void PushClientConnection::getNotifications(const URL& registrationURL, const String& tag, CompletionHandler<void(Expected<Vector<WebCore::NotificationData>, WebCore::ExceptionData>&&)>&& completionHandler)
{
#if HAVE(FULL_FEATURED_USER_NOTIFICATIONS)
    WebPushDaemon::singleton().getNotifications(*this, registrationURL, tag, WTFMove(completionHandler));
#else
    UNUSED_PARAM(registrationURL);
    UNUSED_PARAM(tag);
    completionHandler({ });
#endif
}

void PushClientConnection::cancelNotification(WebCore::SecurityOriginData&& origin, const WTF::UUID& notificationID)
{
#if HAVE(FULL_FEATURED_USER_NOTIFICATIONS)
    WebPushDaemon::singleton().cancelNotification(*this, WTFMove(origin), notificationID);
#endif
}

void PushClientConnection::setAppBadge(WebCore::SecurityOriginData&& origin, std::optional<uint64_t> badge)
{
#if HAVE(FULL_FEATURED_USER_NOTIFICATIONS)
    WebPushDaemon::singleton().setAppBadge(*this, WTFMove(origin), badge);
#endif
}

void PushClientConnection::getAppBadgeForTesting(CompletionHandler<void(std::optional<uint64_t>)>&& completionHandler)
{
#if HAVE(FULL_FEATURED_USER_NOTIFICATIONS)
    WebPushDaemon::singleton().getAppBadgeForTesting(*this, WTFMove(completionHandler));
#else
    completionHandler(std::nullopt);
#endif
}

void PushClientConnection::setProtocolVersionForTesting(unsigned version, CompletionHandler<void()>&& completionHandler)
{
    WebPushDaemon::singleton().setProtocolVersionForTesting(*this, version, WTFMove(completionHandler));
}

} // namespace WebPushD

#endif // ENABLE(WEB_PUSH_NOTIFICATIONS)
