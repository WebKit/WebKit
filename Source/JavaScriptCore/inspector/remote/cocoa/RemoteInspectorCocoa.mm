/*
 * Copyright (C) 2013-2022 Apple Inc. All Rights Reserved.
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

void RemoteInspector::updateFromGlobalNotifyState()
{
    int token = 0;
    if (notify_register_check(WIRGlobalNotifyStateName, &token) != NOTIFY_STATUS_OK)
        return;

    uint64_t state = 0;
    notify_get_state(token, &state);

    m_automaticInspectionEnabled = state & WIRGlobalNotifyStateAutomaticInspection;
    m_simulateCustomerInstall = state & WIRGlobalNotifyStateSimulateCustomerInstall;
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
            {
                Locker locker { shared->m_mutex };

                // Acquire global state so we can determine inspectability and automatic inspection state before
                // connecting to `webinspectord`.
                shared->updateFromGlobalNotifyState();
            }

            shared->setPendingMainThreadInitialization(true);
            if ([NSThread isMainThread])
                shared->initialize();
            else {
                dispatch_async(dispatch_get_main_queue(), ^{
                    shared->initialize();
                });
            }
        }
    });

    return shared;
}

RemoteInspector::RemoteInspector()
    : m_xpcQueue(dispatch_queue_create("com.apple.JavaScriptCore.remote-inspector-xpc", DISPATCH_QUEUE_SERIAL))
{
}

void RemoteInspector::initialize()
{
    ASSERT(pthread_main_np());

    {
        Locker locker { m_mutex };
        if (!m_pendingMainThreadInitialization)
            return;
    }

    WTF::initializeMainThread();
    JSC::initialize();

    if (RemoteInspector::startEnabled)
        start();
    else
        setPendingMainThreadInitialization(false);
}

void RemoteInspector::setPendingMainThreadInitialization(bool pendingInitialization)
{
    Locker locker { m_mutex };

    m_pendingMainThreadInitialization = pendingInitialization;
    if (!m_pendingMainThreadInitialization && !m_automaticInspectionEnabled)
        m_pausedAutomaticInspectionCandidates.clear();
}

void RemoteInspector::updateAutomaticInspectionCandidate(RemoteInspectionTarget* target)
{
    ASSERT_ARG(target, target);

    // If the inspection candidate is on the main thread we can not pause it while we are still pending initialization,
    // otherwise initialization may never occur. Take advantage of the fact we have the thread now and initialize.
    auto singletonShouldImmediatelyInitialize = isMainThread();

    if (singletonShouldImmediatelyInitialize) {
        Locker locker { m_mutex };
        singletonShouldImmediatelyInitialize &= m_pendingMainThreadInitialization;
    }

    // It is safe to not hold a lock for this work because it is only ever done on the main thread and will eventually
    // acquire a lock itself for `start`ing the singleton.
    if (singletonShouldImmediatelyInitialize)
        initialize();

    auto targetIdentifier = target->targetIdentifier();

    {
        Locker locker { m_mutex };

        if (!updateTargetMap(target))
            return;

        if (!m_automaticInspectionEnabled || (!m_enabled && !m_pendingMainThreadInitialization)) {
            pushListingsSoon();
            return;
        }

        m_pausedAutomaticInspectionCandidates.add(targetIdentifier);

        // If we are pausing before we have connected to webinspectord the candidate message will be sent as soon as the connection is established.
        if (m_relayConnection) {
            pushListingsNow();
            sendAutomaticInspectionCandidateMessage(targetIdentifier);
        }

        // In case debuggers fail to respond, or we cannot connect to webinspectord, automatically continue after a
        // short period of time.
        int64_t debuggerTimeoutDelay = 10;
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, debuggerTimeoutDelay * NSEC_PER_SEC), dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
            Locker locker { m_mutex };
            if (m_pausedAutomaticInspectionCandidates.remove(targetIdentifier))
                WTFLogAlways("Skipping Automatic Inspection Candidate with pageId(%u) because we failed to receive a response in time.", targetIdentifier);
        });
    }

    target->pauseWaitingForAutomaticInspection();

    {
        Locker locker { m_mutex };
        m_pausedAutomaticInspectionCandidates.remove(targetIdentifier);
    }
}

void RemoteInspector::sendAutomaticInspectionCandidateMessage(TargetID targetID)
{
    ASSERT(m_enabled);
    ASSERT(m_automaticInspectionEnabled);
    ASSERT(m_relayConnection);
    ASSERT(m_pausedAutomaticInspectionCandidates.contains(targetID));

    NSDictionary *details = @{ WIRTargetIdentifierKey: @(targetID) };
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

    NSData *messageData = [static_cast<NSString *>(message) dataUsingEncoding:NSUTF8StringEncoding];
    NSUInteger messageLength = messageData.length;
    const NSUInteger maxChunkSize = 2 * 1024 * 1024; // 2 Mebibytes

    if (!m_messageDataTypeChunkSupported || messageLength < maxChunkSize) {
        m_relayConnection->sendMessage(WIRRawDataMessage, @{
            WIRRawDataKey: messageData,
            WIRMessageDataTypeKey: WIRMessageDataTypeFull,
            WIRConnectionIdentifierKey: targetConnection->connectionIdentifier(),
            WIRDestinationKey: targetConnection->destination()
        });
        return;
    }

    for (NSUInteger offset = 0; offset < messageLength; offset += maxChunkSize) {
        @autoreleasepool {
            NSUInteger currentChunkSize = std::min(messageLength - offset, maxChunkSize);
            NSString *type = offset + currentChunkSize == messageLength ? WIRMessageDataTypeFinalChunk : WIRMessageDataTypeChunk;

            m_relayConnection->sendMessage(WIRRawDataMessage, @{
                WIRRawDataKey: [messageData subdataWithRange:NSMakeRange(offset, currentChunkSize)],
                WIRMessageDataTypeKey: type,
                WIRConnectionIdentifierKey: targetConnection->connectionIdentifier(),
                WIRDestinationKey: targetConnection->destination()
            });
        }
    }
}

void RemoteInspector::start()
{
    Locker locker { m_mutex };

    if (m_enabled)
        return;

    m_enabled = true;

    notify_register_dispatch(WIRServiceAvailableNotification, &m_notifyToken, m_xpcQueue, ^(int) {
        RemoteInspector::singleton().setupXPCConnectionIfNeeded();
    });

    notify_post(WIRServiceAvailabilityCheckNotification);

    m_pendingMainThreadInitialization = false;
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

    m_pausedAutomaticInspectionCandidates.clear();

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

    if (m_pausedAutomaticInspectionCandidates.size()) {
        // We already have a debuggable waiting to be automatically inspected.
        pushListingsNow();

        for (auto targetID : m_pausedAutomaticInspectionCandidates)
            sendAutomaticInspectionCandidateMessage(targetID);
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

std::optional<audit_token_t> RemoteInspector::parentProcessAuditToken()
{
    Locker locker { m_mutex };

    if (!m_parentProcessAuditData)
        return std::nullopt;

    if (CFDataGetLength(m_parentProcessAuditData.get()) != sizeof(audit_token_t))
        return std::nullopt;

    return *(const audit_token_t *)CFDataGetBytePtr(m_parentProcessAuditData.get());
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

    m_pausedAutomaticInspectionCandidates.clear();

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

    if (!target.allowsInspectionByPolicy())
        return nil;

    NSMutableDictionary *listing = [[NSMutableDictionary alloc] init];
    listing[WIRTargetIdentifierKey] = @(target.targetIdentifier());

    switch (target.type()) {
    case RemoteInspectionTarget::Type::ITML:
        listing[WIRTitleKey] = target.name();
        listing[WIRTypeKey] = WIRTypeITML;
        break;
    case RemoteInspectionTarget::Type::JavaScript:
        listing[WIRTitleKey] = target.name();
        listing[WIRTypeKey] = WIRTypeJavaScript;
        break;
    case RemoteInspectionTarget::Type::Page:
        listing[WIRURLKey] = target.url();
        listing[WIRTitleKey] = target.name();
        listing[WIRTypeKey] = WIRTypePage;
        break;
    case RemoteInspectionTarget::Type::ServiceWorker:
        listing[WIRURLKey] = target.url();
        listing[WIRTitleKey] = target.name();
        listing[WIRTypeKey] = WIRTypeServiceWorker;
        break;
    case RemoteInspectionTarget::Type::WebPage:
        listing[WIRURLKey] = target.url();
        listing[WIRTitleKey] = target.name();
        listing[WIRTypeKey] = WIRTypeWebPage;
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }

    if (auto* connectionToTarget = m_targetConnectionMap.get(target.targetIdentifier()))
        listing[WIRConnectionIdentifierKey] = connectionToTarget->connectionIdentifier();

    if (target.hasLocalDebugger())
        listing[WIRHasLocalDebuggerKey] = @YES;

    return adoptNS(listing);
}

RetainPtr<NSDictionary> RemoteInspector::listingForAutomationTarget(const RemoteAutomationTarget& target) const
{
    // Must collect target information on the WebThread or Main thread since RemoteTargets are
    // implemented by non-threadsafe JSC / WebCore classes such as JSGlobalObject or WebCore::Page.
    ASSERT(isMainThread());

    if (target.isPendingTermination())
        return nullptr;

    NSMutableDictionary *listing = [[NSMutableDictionary alloc] init];
    listing[WIRTargetIdentifierKey] = @(target.targetIdentifier());
    listing[WIRSessionIdentifierKey] = target.name();
    listing[WIRTypeKey] = WIRTypeAutomation;
    listing[WIRAutomationTargetIsPairedKey] = @(target.isPaired());
    if (m_clientCapabilities) {
        listing[WIRAutomationTargetNameKey] = m_clientCapabilities->browserName;
        listing[WIRAutomationTargetVersionKey] = m_clientCapabilities->browserVersion;
    }

    if (auto connectionToTarget = m_targetConnectionMap.get(target.targetIdentifier()))
        listing[WIRConnectionIdentifierKey] = connectionToTarget->connectionIdentifier();

    return adoptNS(listing);
}

void RemoteInspector::pushListingsNow()
{
    ASSERT(m_relayConnection);
    if (!m_relayConnection)
        return;

    m_pushScheduled = false;

    NSMutableDictionary *listings = [[NSMutableDictionary alloc] init];
    for (const auto& listing : m_targetListingMap.values()) {
        NSString *targetIdentifierString = [listing.get()[WIRTargetIdentifierKey] stringValue];
        listings[targetIdentifierString] = listing.get();
    }

    NSMutableDictionary *message = [[NSMutableDictionary alloc] init];
    message[WIRListingKey] = listings;

#if !__has_feature(objc_arc)
    [listings release];
#endif

    if (!m_clientCapabilities)
        message[WIRAutomationAvailabilityKey] = WIRAutomationAvailabilityUnknown;
    else if (m_clientCapabilities->remoteAutomationAllowed)
        message[WIRAutomationAvailabilityKey] = WIRAutomationAvailabilityAvailable;
    else
        message[WIRAutomationAvailabilityKey] = WIRAutomationAvailabilityNotAvailable;

    // COMPATIBILITY(iOS 13): this key is deprecated and not used by newer versions of webinspectord.
    BOOL isAllowed = m_clientCapabilities && m_clientCapabilities->remoteAutomationAllowed;
    message[WIRRemoteAutomationEnabledKey] = @(isAllowed);

    m_relayConnection->sendMessage(WIRListingMessage, message);

#if !__has_feature(objc_arc)
    [message release];
#endif
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

    NSNumber *messageDataTypeChunkSupportedNumber = userInfo[WIRMessageDataTypeChunkSupportedKey];
    convertNSNullToNil(messageDataTypeChunkSupportedNumber);
    BAIL_IF_UNEXPECTED_TYPE_ALLOWING_NIL(messageDataTypeChunkSupportedNumber, [NSNumber class]);
    m_messageDataTypeChunkSupported = messageDataTypeChunkSupportedNumber.boolValue;

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
        bool isAutomaticInspection = m_pausedAutomaticInspectionCandidates.contains(target->targetIdentifier());
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

    if (!m_automaticInspectionEnabled)
        m_pausedAutomaticInspectionCandidates.clear();
}

void RemoteInspector::receivedAutomaticInspectionRejectMessage(NSDictionary *userInfo)
{
    NSNumber *targetIdentifierNumber = userInfo[WIRTargetIdentifierKey];
    BAIL_IF_UNEXPECTED_TYPE(targetIdentifierNumber, [NSNumber class]);

    TargetID targetIdentifier = targetIdentifierNumber.unsignedIntValue;
    if (!targetIdentifier)
        return;

    ASSERT(m_pausedAutomaticInspectionCandidates.contains(targetIdentifier));
    m_pausedAutomaticInspectionCandidates.remove(targetIdentifier);
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
