/*
 * Copyright (C) 2013-2016 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "RemoteInspector.h"

#if ENABLE(REMOTE_INSPECTOR)

#import "InitializeThreading.h"
#import "RemoteAutomationTarget.h"
#import "RemoteConnectionToTarget.h"
#import "RemoteInspectionTarget.h"
#import "RemoteInspectorConstants.h"
#import <Foundation/Foundation.h>
#import <dispatch/dispatch.h>
#import <notify.h>
#import <wtf/Assertions.h>
#import <wtf/MainThread.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/spi/darwin/SandboxSPI.h>
#import <wtf/spi/darwin/XPCSPI.h>
#import <wtf/text/WTFString.h>

#define BAIL_IF_UNEXPECTED_TYPE(expr, classExpr)          \
    do {                                                  \
        id value = (expr);                                \
        id classValue = (classExpr);                      \
        if (![value isKindOfClass:classValue])            \
            return;                                       \
    } while (0);

#define BAIL_IF_UNEXPECTED_TYPE_ALLOWING_NIL(expr, classExpr)   \
    do {                                                        \
        id value = (expr);                                      \
        id classValue = (classExpr);                            \
        if (value && ![value isKindOfClass:classValue])         \
            return;                                             \
    } while (0);

namespace Inspector {

static void convertNSNullToNil(__strong NSNumber *& number)
{
    if ([number isEqual:[NSNull null]])
        number = nil;
}

static bool canAccessWebInspectorMachPort()
{
    return !sandbox_check(getpid(), "mach-lookup", static_cast<enum sandbox_filter_type>(SANDBOX_FILTER_GLOBAL_NAME | SANDBOX_CHECK_NO_REPORT), WIRXPCMachPortName);
}

static bool globalAutomaticInspectionState()
{
    int token = 0;
    if (notify_register_check(WIRAutomaticInspectionEnabledState, &token) != NOTIFY_STATUS_OK)
        return false;

    uint64_t automaticInspectionEnabled = 0;
    notify_get_state(token, &automaticInspectionEnabled);
    return automaticInspectionEnabled == 1;
}

RemoteInspector& RemoteInspector::singleton()
{
    static LazyNeverDestroyed<RemoteInspector> shared;
    static dispatch_once_t onceConstructKey;
    dispatch_once(&onceConstructKey, ^{
        shared.construct();
    });

#if PLATFORM(COCOA)
    if (needMachSandboxExtension)
        return shared;
#endif
    
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        if (canAccessWebInspectorMachPort()) {
            dispatch_block_t initialize = ^{
                WTF::initializeMainThread();
                JSC::initialize();
                if (RemoteInspector::startEnabled)
                    shared.get().start();
            };

            if ([NSThread isMainThread])
                initialize();
            else {
                // FIXME: This means that we may miss an auto-attach to a JSContext created on a non-main thread.
                // The main thread initialization is required for certain WTF values that need to be initialized
                // on the "real" main thread. We should investigate a better way to handle this.
                dispatch_async(dispatch_get_main_queue(), initialize);
            }
        }
    });

    return shared;
}

RemoteInspector::RemoteInspector()
    : m_xpcQueue(dispatch_queue_create("com.apple.JavaScriptCore.remote-inspector-xpc", DISPATCH_QUEUE_SERIAL))
{
}

void RemoteInspector::updateAutomaticInspectionCandidate(RemoteInspectionTarget* target)
{
    ASSERT_ARG(target, target);
    {
        Locker locker { m_mutex };

        if (!updateTargetMap(target))
            return;

        // Don't allow automatic inspection unless it is allowed or we are stopped.
        if (!m_automaticInspectionEnabled || !m_enabled) {
            pushListingsSoon();
            return;
        }

        auto targetIdentifier = target->targetIdentifier();

        // FIXME: We should handle multiple debuggables trying to pause at the same time on different threads.
        // To make this work we will need to change m_automaticInspectionCandidateTargetIdentifier to be a per-thread value.
        // Multiple attempts on the same thread should not be possible because our nested run loop is in a special RWI mode.
        if (m_automaticInspectionPaused) {
            WTFLogAlways("Skipping Automatic Inspection Candidate with pageId(%u) because we are already paused waiting for pageId(%u)", targetIdentifier, m_automaticInspectionCandidateTargetIdentifier);
            pushListingsSoon();
            return;
        }

        m_automaticInspectionPaused = true;
        m_automaticInspectionCandidateTargetIdentifier = targetIdentifier;

        // If we are pausing before we have connected to webinspectord the candidate message will be sent as soon as the connection is established.
        if (m_relayConnection) {
            pushListingsNow();
            sendAutomaticInspectionCandidateMessage();
        }

        // In case debuggers fail to respond, or we cannot connect to webinspectord, automatically continue after a short period of time.
        int64_t debuggerTimeoutDelay = 10;
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, debuggerTimeoutDelay * NSEC_PER_SEC), dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
            Locker locker { m_mutex };
            if (m_automaticInspectionCandidateTargetIdentifier == targetIdentifier) {
                WTFLogAlways("Skipping Automatic Inspection Candidate with pageId(%u) because we failed to receive a response in time.", m_automaticInspectionCandidateTargetIdentifier);
                m_automaticInspectionPaused = false;
            }
        });
    }

    target->pauseWaitingForAutomaticInspection();

    {
        Locker locker { m_mutex };

        ASSERT(m_automaticInspectionCandidateTargetIdentifier);
        m_automaticInspectionCandidateTargetIdentifier = 0;
    }
}

void RemoteInspector::sendAutomaticInspectionCandidateMessage()
{
    ASSERT(m_enabled);
    ASSERT(m_automaticInspectionEnabled);
    ASSERT(m_automaticInspectionPaused);
    ASSERT(m_automaticInspectionCandidateTargetIdentifier);
    ASSERT(m_relayConnection);

    NSDictionary *details = @{WIRTargetIdentifierKey: @(m_automaticInspectionCandidateTargetIdentifier)};
    m_relayConnection->sendMessage(WIRAutomaticInspectionCandidateMessage, details);
}

void RemoteInspector::sendMessageToRemote(TargetID targetIdentifier, const String& message)
{
    Locker locker { m_mutex };

    if (!m_relayConnection)
        return;

    auto targetConnection = m_targetConnectionMap.get(targetIdentifier);
    if (!targetConnection)
        return;

    NSDictionary *userInfo = @{
        WIRRawDataKey: [static_cast<NSString *>(message) dataUsingEncoding:NSUTF8StringEncoding],
        WIRConnectionIdentifierKey: targetConnection->connectionIdentifier(),
        WIRDestinationKey: targetConnection->destination()
    };

    m_relayConnection->sendMessage(WIRRawDataMessage, userInfo);
}

void RemoteInspector::start()
{
    Locker locker { m_mutex };

    if (m_enabled)
        return;

    m_enabled = true;

    // Load the initial automatic inspection state when first started, so we know it before we have even connected to webinspectord.
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        m_automaticInspectionEnabled = globalAutomaticInspectionState();
    });

    notify_register_dispatch(WIRServiceAvailableNotification, &m_notifyToken, m_xpcQueue, ^(int) {
        RemoteInspector::singleton().setupXPCConnectionIfNeeded();
    });

    notify_post(WIRServiceAvailabilityCheckNotification);
}

void RemoteInspector::stopInternal(StopSource source)
{
    if (!m_enabled)
        return;

    m_enabled = false;

    m_pushScheduled = false;

    for (const auto& targetConnection : m_targetConnectionMap.values())
        targetConnection->close();
    m_targetConnectionMap.clear();

    updateHasActiveDebugSession();

    m_automaticInspectionPaused = false;

    if (m_relayConnection) {
        switch (source) {
        case StopSource::API:
            m_relayConnection->close();
            break;
        case StopSource::XPCMessage:
            m_relayConnection->closeFromMessage();
            break;
        }

        m_relayConnection = nullptr;
    }

    m_shouldReconnectToRelayOnFailure = false;

    notify_cancel(m_notifyToken);
}

void RemoteInspector::setupXPCConnectionIfNeeded()
{
    Locker locker { m_mutex };

    if (m_relayConnection) {
        m_shouldReconnectToRelayOnFailure = true;

        // Send a simple message to make sure the connection is still open.
        m_relayConnection->sendMessage(@"check", nil);
        return;
    }

    auto connection = adoptOSObject(xpc_connection_create_mach_service(WIRXPCMachPortName, m_xpcQueue, 0));
    if (!connection) {
        WTFLogAlways("RemoteInspector failed to create XPC connection.");
        return;
    }

    m_relayConnection = adoptRef(new RemoteInspectorXPCConnection(connection.get(), m_xpcQueue, this));
    m_relayConnection->sendMessage(@"syn", nil); // Send a simple message to initialize the XPC connection.

    if (m_automaticInspectionCandidateTargetIdentifier) {
        // We already have a debuggable waiting to be automatically inspected.
        pushListingsNow();
        sendAutomaticInspectionCandidateMessage();
    } else
        pushListingsSoon();
}

#pragma mark - Proxy Application Information

void RemoteInspector::setParentProcessInformation(pid_t pid, RetainPtr<CFDataRef> auditData)
{
    Locker locker { m_mutex };

    if (m_parentProcessIdentifier || m_parentProcessAuditData)
        return;

    m_parentProcessIdentifier = pid;
    m_parentProcessAuditData = auditData;

    if (m_shouldSendParentProcessInformation)
        receivedProxyApplicationSetupMessage(nil);
}

#pragma mark - RemoteInspectorXPCConnection::Client

void RemoteInspector::xpcConnectionReceivedMessage(RemoteInspectorXPCConnection*, NSString *messageName, NSDictionary *userInfo)
{
    Locker locker { m_mutex };

    if ([messageName isEqualToString:WIRPermissionDenied]) {
        stopInternal(StopSource::XPCMessage);
        return;
    }

    if ([messageName isEqualToString:WIRSocketDataMessage])
        receivedDataMessage(userInfo);
    else if ([messageName isEqualToString:WIRSocketSetupMessage])
        receivedSetupMessage(userInfo);
    else if ([messageName isEqualToString:WIRWebPageCloseMessage])
        receivedDidCloseMessage(userInfo);
    else if ([messageName isEqualToString:WIRApplicationGetListingMessage])
        receivedGetListingMessage(userInfo);
    else if ([messageName isEqualToString:WIRApplicationWakeUpDebuggablesMessage])
        receivedWakeUpDebuggables(userInfo);
    else if ([messageName isEqualToString:WIRIndicateMessage])
        receivedIndicateMessage(userInfo);
    else if ([messageName isEqualToString:WIRProxyApplicationSetupMessage])
        receivedProxyApplicationSetupMessage(userInfo);
    else if ([messageName isEqualToString:WIRConnectionDiedMessage])
        receivedConnectionDiedMessage(userInfo);
    else if ([messageName isEqualToString:WIRAutomaticInspectionConfigurationMessage])
        receivedAutomaticInspectionConfigurationMessage(userInfo);
    else if ([messageName isEqualToString:WIRAutomaticInspectionRejectMessage])
        receivedAutomaticInspectionRejectMessage(userInfo);
    else if ([messageName isEqualToString:WIRAutomationSessionRequestMessage])
        receivedAutomationSessionRequestMessage(userInfo);
    else
        NSLog(@"Unrecognized RemoteInspector XPC Message: %@", messageName);
}

void RemoteInspector::xpcConnectionFailed(RemoteInspectorXPCConnection* relayConnection)
{
    Locker locker { m_mutex };

    ASSERT(relayConnection == m_relayConnection);
    if (relayConnection != m_relayConnection)
        return;

    m_pushScheduled = false;

    for (const auto& targetConnection : m_targetConnectionMap.values())
        targetConnection->close();
    m_targetConnectionMap.clear();

    updateHasActiveDebugSession();

    m_automaticInspectionPaused = false;

    // The XPC connection will close itself.
    m_relayConnection = nullptr;

    if (!m_shouldReconnectToRelayOnFailure)
        return;

    m_shouldReconnectToRelayOnFailure = false;

    // Schedule setting up a new connection, since we currently are holding a lock needed to create a new connection.
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 1 * NSEC_PER_SEC), dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        RemoteInspector::singleton().setupXPCConnectionIfNeeded();
    });
}

void RemoteInspector::xpcConnectionUnhandledMessage(RemoteInspectorXPCConnection*, xpc_object_t)
{
    // Intentionally ignored.
}

#pragma mark - Listings

RetainPtr<NSDictionary> RemoteInspector::listingForInspectionTarget(const RemoteInspectionTarget& target) const
{
    // Must collect target information on the WebThread, Main, or Worker thread since RemoteTargets are
    // implemented by non-threadsafe JSC / WebCore classes such as JSGlobalObject or WebCore::Page.

    if (!target.remoteDebuggingAllowed())
        return nil;

    RetainPtr<NSMutableDictionary> listing = adoptNS([[NSMutableDictionary alloc] init]);
    [listing setObject:@(target.targetIdentifier()) forKey:WIRTargetIdentifierKey];

    switch (target.type()) {
    case RemoteInspectionTarget::Type::ITML:
        [listing setObject:target.name() forKey:WIRTitleKey];
        [listing setObject:WIRTypeITML forKey:WIRTypeKey];
        break;
    case RemoteInspectionTarget::Type::JavaScript:
        [listing setObject:target.name() forKey:WIRTitleKey];
        [listing setObject:WIRTypeJavaScript forKey:WIRTypeKey];
        break;
    case RemoteInspectionTarget::Type::Page:
        [listing setObject:target.url() forKey:WIRURLKey];
        [listing setObject:target.name() forKey:WIRTitleKey];
        [listing setObject:WIRTypePage forKey:WIRTypeKey];
        break;
    case RemoteInspectionTarget::Type::ServiceWorker:
        [listing setObject:target.url() forKey:WIRURLKey];
        [listing setObject:target.name() forKey:WIRTitleKey];
        [listing setObject:WIRTypeServiceWorker forKey:WIRTypeKey];
        break;
    case RemoteInspectionTarget::Type::WebPage:
        [listing setObject:target.url() forKey:WIRURLKey];
        [listing setObject:target.name() forKey:WIRTitleKey];
        [listing setObject:WIRTypeWebPage forKey:WIRTypeKey];
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }

    if (auto* connectionToTarget = m_targetConnectionMap.get(target.targetIdentifier()))
        [listing setObject:connectionToTarget->connectionIdentifier() forKey:WIRConnectionIdentifierKey];

    if (target.hasLocalDebugger())
        [listing setObject:@YES forKey:WIRHasLocalDebuggerKey];

    return listing;
}

RetainPtr<NSDictionary> RemoteInspector::listingForAutomationTarget(const RemoteAutomationTarget& target) const
{
    // Must collect target information on the WebThread or Main thread since RemoteTargets are
    // implemented by non-threadsafe JSC / WebCore classes such as JSGlobalObject or WebCore::Page.
    ASSERT(isMainThread());

    RetainPtr<NSMutableDictionary> listing = adoptNS([[NSMutableDictionary alloc] init]);
    [listing setObject:@(target.targetIdentifier()) forKey:WIRTargetIdentifierKey];
    [listing setObject:target.name() forKey:WIRSessionIdentifierKey];
    [listing setObject:WIRTypeAutomation forKey:WIRTypeKey];
    [listing setObject:@(target.isPaired()) forKey:WIRAutomationTargetIsPairedKey];
    if (m_clientCapabilities) {
        [listing setObject:m_clientCapabilities->browserName forKey:WIRAutomationTargetNameKey];
        [listing setObject:m_clientCapabilities->browserVersion forKey:WIRAutomationTargetVersionKey];
    }

    if (auto connectionToTarget = m_targetConnectionMap.get(target.targetIdentifier()))
        [listing setObject:connectionToTarget->connectionIdentifier() forKey:WIRConnectionIdentifierKey];

    return listing;
}

void RemoteInspector::pushListingsNow()
{
    ASSERT(m_relayConnection);
    if (!m_relayConnection)
        return;

    m_pushScheduled = false;

    RetainPtr<NSMutableDictionary> listings = adoptNS([[NSMutableDictionary alloc] init]);
    for (const auto& listing : m_targetListingMap.values()) {
        NSString *targetIdentifierString = [[listing.get() objectForKey:WIRTargetIdentifierKey] stringValue];
        [listings setObject:listing.get() forKey:targetIdentifierString];
    }

    RetainPtr<NSMutableDictionary> message = adoptNS([[NSMutableDictionary alloc] init]);
    [message setObject:listings.get() forKey:WIRListingKey];

    if (!m_clientCapabilities)
        [message setObject:WIRAutomationAvailabilityUnknown forKey:WIRAutomationAvailabilityKey];
    else if (m_clientCapabilities->remoteAutomationAllowed)
        [message setObject:WIRAutomationAvailabilityAvailable forKey:WIRAutomationAvailabilityKey];
    else
        [message setObject:WIRAutomationAvailabilityNotAvailable forKey:WIRAutomationAvailabilityKey];

    // COMPATIBILITY(iOS 13): this key is deprecated and not used by newer versions of webinspectord.
    BOOL isAllowed = m_clientCapabilities && m_clientCapabilities->remoteAutomationAllowed;
    [message setObject:@(isAllowed) forKey:WIRRemoteAutomationEnabledKey];

    m_relayConnection->sendMessage(WIRListingMessage, message.get());
}

void RemoteInspector::pushListingsSoon()
{
    if (!m_relayConnection)
        return;

    if (m_pushScheduled)
        return;

    m_pushScheduled = true;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 0.2 * NSEC_PER_SEC), dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        Locker locker { m_mutex };
        if (m_pushScheduled)
            pushListingsNow();
    });
}

#pragma mark - Received XPC Messages

void RemoteInspector::receivedSetupMessage(NSDictionary *userInfo)
{
    NSNumber *targetIdentifierNumber = userInfo[WIRTargetIdentifierKey];
    BAIL_IF_UNEXPECTED_TYPE(targetIdentifierNumber, [NSNumber class]);

    NSString *connectionIdentifier = userInfo[WIRConnectionIdentifierKey];
    BAIL_IF_UNEXPECTED_TYPE(connectionIdentifier, [NSString class]);

    NSString *sender = userInfo[WIRSenderKey];
    BAIL_IF_UNEXPECTED_TYPE(sender, [NSString class]);

    NSNumber *automaticallyPauseNumber = userInfo[WIRAutomaticallyPause];
    convertNSNullToNil(automaticallyPauseNumber);
    BAIL_IF_UNEXPECTED_TYPE_ALLOWING_NIL(automaticallyPauseNumber, [NSNumber class]);
    BOOL automaticallyPause = automaticallyPauseNumber.boolValue;

    TargetID targetIdentifier = targetIdentifierNumber.unsignedIntValue;
    if (!targetIdentifier)
        return;

    if (m_targetConnectionMap.contains(targetIdentifier))
        return;

    auto findResult = m_targetMap.find(targetIdentifier);
    if (findResult == m_targetMap.end())
        return;

    // Attempt to create a connection. This may fail if the page already has an inspector or if it disallows inspection.
    RemoteControllableTarget* target = findResult->value;
    auto connectionToTarget = adoptRef(*new RemoteConnectionToTarget(target, connectionIdentifier, sender));

    if (is<RemoteInspectionTarget>(target)) {
        bool isAutomaticInspection = m_automaticInspectionCandidateTargetIdentifier == target->targetIdentifier();

        if (!connectionToTarget->setup(isAutomaticInspection, automaticallyPause)) {
            connectionToTarget->close();
            return;
        }
        m_targetConnectionMap.set(targetIdentifier, WTFMove(connectionToTarget));
    } else if (is<RemoteAutomationTarget>(target)) {
        if (!connectionToTarget->setup()) {
            connectionToTarget->close();
            return;
        }
        m_targetConnectionMap.set(targetIdentifier, WTFMove(connectionToTarget));
    } else
        ASSERT_NOT_REACHED();

    updateHasActiveDebugSession();
}

void RemoteInspector::receivedDataMessage(NSDictionary *userInfo)
{
    NSNumber *targetIdentifierNumber = userInfo[WIRTargetIdentifierKey];
    BAIL_IF_UNEXPECTED_TYPE(targetIdentifierNumber, [NSNumber class]);

    NSData *data = userInfo[WIRSocketDataKey];
    BAIL_IF_UNEXPECTED_TYPE(data, [NSData class]);

    TargetID targetIdentifier = targetIdentifierNumber.unsignedIntValue;
    if (!targetIdentifier)
        return;

    auto connectionToTarget = m_targetConnectionMap.get(targetIdentifier);
    if (!connectionToTarget)
        return;

    RetainPtr<NSString> message = adoptNS([[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding]);
    connectionToTarget->sendMessageToTarget(message.get());
}

void RemoteInspector::receivedDidCloseMessage(NSDictionary *userInfo)
{
    NSNumber *targetIdentifierNumber = userInfo[WIRTargetIdentifierKey];
    BAIL_IF_UNEXPECTED_TYPE(targetIdentifierNumber, [NSNumber class]);

    NSString *connectionIdentifier = userInfo[WIRConnectionIdentifierKey];
    BAIL_IF_UNEXPECTED_TYPE(connectionIdentifier, [NSString class]);

    TargetID targetIdentifier = targetIdentifierNumber.unsignedIntValue;
    if (!targetIdentifier)
        return;

    auto connectionToTarget = m_targetConnectionMap.get(targetIdentifier);
    if (!connectionToTarget)
        return;

    if (![connectionIdentifier isEqualToString:connectionToTarget->connectionIdentifier()])
        return;

    connectionToTarget->close();
    m_targetConnectionMap.remove(targetIdentifier);

    updateHasActiveDebugSession();
}

void RemoteInspector::receivedGetListingMessage(NSDictionary *)
{
    pushListingsNow();
}

void RemoteInspector::receivedWakeUpDebuggables(NSDictionary *)
{
    if (m_client)
        m_client->requestedDebuggablesToWakeUp();
}

void RemoteInspector::receivedIndicateMessage(NSDictionary *userInfo)
{
    NSNumber *targetIdentifierNumber = userInfo[WIRTargetIdentifierKey];
    BAIL_IF_UNEXPECTED_TYPE(targetIdentifierNumber, [NSNumber class]);

    NSNumber *indicateEnabledNumber = userInfo[WIRIndicateEnabledKey];
    BAIL_IF_UNEXPECTED_TYPE(indicateEnabledNumber, [NSNumber class]);
    BOOL indicateEnabled = indicateEnabledNumber.boolValue;

    TargetID targetIdentifier = targetIdentifierNumber.unsignedIntValue;
    if (!targetIdentifier)
        return;

    dispatchAsyncOnMainThreadWithWebThreadLockIfNeeded(^{
        RemoteControllableTarget* target = nullptr;
        {
            Locker locker { m_mutex };

            auto findResult = m_targetMap.find(targetIdentifier);
            if (findResult == m_targetMap.end())
                return;

            target = findResult->value;
        }
        if (is<RemoteInspectionTarget>(target))
            downcast<RemoteInspectionTarget>(target)->setIndicating(indicateEnabled);
    });
}

void RemoteInspector::receivedProxyApplicationSetupMessage(NSDictionary *)
{
    ASSERT(m_relayConnection);
    if (!m_relayConnection)
        return;

    if (!m_parentProcessIdentifier || !m_parentProcessAuditData) {
        // We are a proxy application without parent process information.
        // Wait a bit for the information, but give up after a second.
        m_shouldSendParentProcessInformation = true;
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 1 * NSEC_PER_SEC), dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
            Locker locker { m_mutex };
            if (m_shouldSendParentProcessInformation)
                stopInternal(StopSource::XPCMessage);
        });
        return;
    }

    m_shouldSendParentProcessInformation = false;

    m_relayConnection->sendMessage(WIRProxyApplicationSetupResponseMessage, @{
        WIRProxyApplicationParentPIDKey: @(m_parentProcessIdentifier),
        WIRProxyApplicationParentAuditDataKey: (__bridge NSData *)m_parentProcessAuditData.get(),
    });
}

void RemoteInspector::receivedConnectionDiedMessage(NSDictionary *userInfo)
{
    NSString *connectionIdentifier = userInfo[WIRConnectionIdentifierKey];
    BAIL_IF_UNEXPECTED_TYPE(connectionIdentifier, [NSString class]);

    auto it = m_targetConnectionMap.begin();
    auto end = m_targetConnectionMap.end();
    for (; it != end; ++it) {
        if ([connectionIdentifier isEqualToString:it->value->connectionIdentifier()])
            break;
    }

    if (it == end)
        return;

    auto connection = it->value;
    connection->close();
    m_targetConnectionMap.remove(it);

    updateHasActiveDebugSession();
}

void RemoteInspector::receivedAutomaticInspectionConfigurationMessage(NSDictionary *userInfo)
{
    NSNumber *automaticInspectionEnabledNumber = userInfo[WIRAutomaticInspectionEnabledKey];
    BAIL_IF_UNEXPECTED_TYPE(automaticInspectionEnabledNumber, [NSNumber class]);

    m_automaticInspectionEnabled = automaticInspectionEnabledNumber.boolValue;

    if (!m_automaticInspectionEnabled && m_automaticInspectionPaused)
        m_automaticInspectionPaused = false;
}

void RemoteInspector::receivedAutomaticInspectionRejectMessage(NSDictionary *userInfo)
{
    NSNumber *targetIdentifierNumber = userInfo[WIRTargetIdentifierKey];
    BAIL_IF_UNEXPECTED_TYPE(targetIdentifierNumber, [NSNumber class]);

    TargetID targetIdentifier = targetIdentifierNumber.unsignedIntValue;
    if (!targetIdentifier)
        return;

    ASSERT(targetIdentifier == m_automaticInspectionCandidateTargetIdentifier);
    if (targetIdentifier == m_automaticInspectionCandidateTargetIdentifier)
        m_automaticInspectionPaused = false;
}

void RemoteInspector::receivedAutomationSessionRequestMessage(NSDictionary *userInfo)
{
    NSString *suggestedSessionIdentifier = userInfo[WIRSessionIdentifierKey];
    BAIL_IF_UNEXPECTED_TYPE(suggestedSessionIdentifier, [NSString class]);

    NSDictionary *forwardedCapabilities = userInfo[WIRSessionCapabilitiesKey];
    BAIL_IF_UNEXPECTED_TYPE_ALLOWING_NIL(forwardedCapabilities, [NSDictionary class]);

    Client::SessionCapabilities sessionCapabilities;
    if (NSNumber *value = forwardedCapabilities[WIRAcceptInsecureCertificatesKey]) {
        if ([value isKindOfClass:[NSNumber class]])
            sessionCapabilities.acceptInsecureCertificates = value.boolValue;
    }

    if (NSNumber *value = forwardedCapabilities[WIRAllowInsecureMediaCaptureCapabilityKey]) {
        if ([value isKindOfClass:[NSNumber class]])
            sessionCapabilities.allowInsecureMediaCapture = value.boolValue;
    }

    if (NSNumber *value = forwardedCapabilities[WIRSuppressICECandidateFilteringCapabilityKey]) {
        if ([value isKindOfClass:[NSNumber class]])
            sessionCapabilities.suppressICECandidateFiltering = value.boolValue;
    }

    if (!m_client)
        return;

    if (!m_clientCapabilities || !m_clientCapabilities->remoteAutomationAllowed)
        return;

    m_client->requestAutomationSession(suggestedSessionIdentifier, sessionCapabilities);
}

} // namespace Inspector

#endif // ENABLE(REMOTE_INSPECTOR)
