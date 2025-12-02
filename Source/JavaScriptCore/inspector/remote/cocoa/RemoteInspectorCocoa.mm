/*
 * Copyright (C) 2013-2022 Apple Inc. All rights reserved.
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
#import <wtf/darwin/DispatchExtras.h>
#import <wtf/spi/darwin/SandboxSPI.h>
#import <wtf/spi/darwin/XPCSPI.h>
#import <wtf/text/WTFString.h>

#define BAIL_IF_UNEXPECTED_TYPE(expr, classExpr)          \
    do {                                                  \
        RetainPtr value = (expr);                         \
        id classValue = (classExpr);                      \
        if (![value isKindOfClass:classValue])            \
            return;                                       \
    } while (0);

#define BAIL_IF_UNEXPECTED_TYPE_ALLOWING_NIL(expr, classExpr)   \
    do {                                                        \
        RetainPtr value = (expr);                               \
        id classValue = (classExpr);                            \
        if (value && ![value isKindOfClass:classValue])         \
            return;                                             \
    } while (0);

namespace Inspector {

RemoteInspector::~RemoteInspector() = default;

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
                dispatch_async(mainDispatchQueueSingleton(), ^{
                    shared->initialize();
                });
            }
        }
    });

    return shared;
}

RemoteInspector::RemoteInspector()
    : m_xpcQueue(adoptOSObject(dispatch_queue_create("com.apple.JavaScriptCore.remote-inspector-xpc", DISPATCH_QUEUE_SERIAL)))
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
        m_automaticInspectionCandidates.clear();
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

        m_automaticInspectionCandidates.add(targetIdentifier);

        // If we are pausing before we have connected to webinspectord the candidate message will be sent as soon as the connection is established.
        if (m_relayConnection) {
            pushListingsNow();
            sendAutomaticInspectionCandidateMessage(targetIdentifier);
        }

        // In case debuggers fail to respond, or we cannot connect to webinspectord, assume a rejection for
        // automatic inspection after a short period of time.
        int64_t debuggerTimeoutDelay = 10;
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, debuggerTimeoutDelay * NSEC_PER_SEC), globalDispatchQueueSingleton(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
            Locker locker { m_mutex };
            if (m_automaticInspectionCandidates.remove(targetIdentifier)) {
                WTFLogAlways("Skipping Automatic Inspection Candidate with pageId(%u) because we failed to receive a response in time.", targetIdentifier);
                target->unpauseForResolvedAutomaticInspection();
            }
        });
    }

    target->pauseWaitingForAutomaticInspection();
}

void RemoteInspector::sendAutomaticInspectionCandidateMessage(TargetID targetID)
{
    ASSERT(m_enabled);
    ASSERT(m_automaticInspectionEnabled);
    ASSERT(m_relayConnection);
    ASSERT(m_automaticInspectionCandidates.contains(targetID));

    RefPtr target = dynamicDowncast<RemoteInspectionTarget>(m_targetMap.get(targetID));
    if (!target) {
        m_automaticInspectionCandidates.remove(targetID);
        return;
    }

    NSDictionary *details = @{
        WIRTargetIdentifierKey : @(targetID),
        WIRTargetAllowsAutomaticInspectionInSameProcessKey : @(target->automaticInspectionAllowedInSameProcess()),
    };
    Ref { *m_relayConnection }->sendMessage(WIRAutomaticInspectionCandidateMessage, details);
}

void RemoteInspector::sendMessageToRemote(TargetID targetIdentifier, const String& message)
{
    Locker locker { m_mutex };

    RefPtr relayConnection = m_relayConnection;
    if (!relayConnection)
        return;

    RefPtr targetConnection = m_targetConnectionMap.get(targetIdentifier);
    if (!targetConnection)
        return;

    RetainPtr messageData = [message.createNSString() dataUsingEncoding:NSUTF8StringEncoding];
    NSUInteger messageLength = [messageData length];
    const NSUInteger maxChunkSize = 2 * 1024 * 1024; // 2 Mebibytes

    if (!m_messageDataTypeChunkSupported || messageLength < maxChunkSize) {
        relayConnection->sendMessage(WIRRawDataMessage, @{
            WIRRawDataKey: messageData.get(),
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

            relayConnection->sendMessage(WIRRawDataMessage, @{
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

    notify_register_dispatch(WIRServiceAvailableNotification, &m_notifyToken, m_xpcQueue.get(), ^(int) {
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

    m_automaticInspectionCandidates.clear();

    if (RefPtr relayConnection = m_relayConnection) {
        switch (source) {
        case StopSource::API:
            relayConnection->close();
            break;
        case StopSource::XPCMessage:
            relayConnection->closeFromMessage();
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

    if (RefPtr relayConnection = m_relayConnection) {
        m_shouldReconnectToRelayOnFailure = true;
        relayConnection->sendMessage(WIRPingMessage, nil);
        return;
    }

    // FIXME: This is a false positive. <rdar://164843889>
    SUPPRESS_RETAINPTR_CTOR_ADOPT auto connection = adoptXPCObject(xpc_connection_create_mach_service(WIRXPCMachPortName, m_xpcQueue.get(), 0));
    if (!connection) {
        WTFLogAlways("RemoteInspector failed to create XPC connection.");
        return;
    }

    Ref relayConnection = RemoteInspectorXPCConnection::create(connection.get(), m_xpcQueue.get(), this);
    m_relayConnection = relayConnection.copyRef();
    relayConnection->sendMessage(@"syn", nil); // Send a simple message to initialize the XPC connection.

    if (m_automaticInspectionCandidates.size()) {
        // We already have a debuggable waiting to be automatically inspected.
        pushListingsNow();

        for (auto targetID : m_automaticInspectionCandidates)
            sendAutomaticInspectionCandidateMessage(targetID);
    } else
        pushListingsSoon();
}

void RemoteInspector::connectToWebInspector()
{
    dispatch_async(m_xpcQueue.get(), ^{
        RemoteInspector::singleton().setupXPCConnectionIfNeeded();
    });
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
    else if ([messageName isEqualToString:WIRPingSuccessMessage])
        receivedPingSuccessMessage();
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

    m_automaticInspectionCandidates.clear();

    // The XPC connection will close itself.
    m_relayConnection = nullptr;

    if (!m_shouldReconnectToRelayOnFailure)
        return;

    m_shouldReconnectToRelayOnFailure = false;

    // Schedule setting up a new connection, since we currently are holding a lock needed to create a new connection.
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 1 * NSEC_PER_SEC), globalDispatchQueueSingleton(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        RemoteInspector::singleton().setupXPCConnectionIfNeeded();
    });
}

void RemoteInspector::xpcConnectionUnhandledMessage(RemoteInspectorXPCConnection*, xpc_object_t)
{
    // Intentionally ignored.
}

#pragma mark - Listings

static RetainPtr<NSString> identifierForPID(ProcessID pid)
{
    // This format matches that used in the relay process.
    return adoptNS([[NSString alloc] initWithFormat:@"PID:%lu", static_cast<unsigned long>(pid)]);
}

RetainPtr<NSDictionary> RemoteInspector::listingForInspectionTarget(const RemoteInspectionTarget& target) const
{
    // Must collect target information on the WebThread, Main, or Worker thread since RemoteTargets are
    // implemented by non-threadsafe JSC / WebCore classes such as JSGlobalObject or WebCore::Page.

    if (!target.allowsInspectionByPolicy())
        return nil;

    RetainPtr<NSMutableDictionary> listing = adoptNS([[NSMutableDictionary alloc] init]);
    // FIXME: This is a safer cpp false positive (rdar://161698063).
    SUPPRESS_UNRETAINED_ARG [listing setObject:@(target.targetIdentifier()) forKey:WIRTargetIdentifierKey];

    switch (target.type()) {
    case RemoteInspectionTarget::Type::ITML:
        [listing setObject:target.name().createNSString().get() forKey:WIRTitleKey];
        [listing setObject:target.nameOverride().createNSString().get() forKey:WIROverrideNameKey];
        [listing setObject:WIRTypeITML forKey:WIRTypeKey];
        break;
    case RemoteInspectionTarget::Type::JavaScript:
        [listing setObject:target.name().createNSString().get() forKey:WIRTitleKey];
        [listing setObject:target.nameOverride().createNSString().get() forKey:WIROverrideNameKey];
        [listing setObject:WIRTypeJavaScript forKey:WIRTypeKey];
        break;
    case RemoteInspectionTarget::Type::Page:
        [listing setObject:target.url().createNSString().get() forKey:WIRURLKey];
        [listing setObject:target.name().createNSString().get() forKey:WIRTitleKey];
        [listing setObject:target.nameOverride().createNSString().get() forKey:WIROverrideNameKey];
        [listing setObject:WIRTypePage forKey:WIRTypeKey];
        break;
    case RemoteInspectionTarget::Type::ServiceWorker:
        [listing setObject:target.url().createNSString().get() forKey:WIRURLKey];
        [listing setObject:target.name().createNSString().get() forKey:WIRTitleKey];
        [listing setObject:target.nameOverride().createNSString().get() forKey:WIROverrideNameKey];
        [listing setObject:WIRTypeServiceWorker forKey:WIRTypeKey];
        break;
    case RemoteInspectionTarget::Type::WebPage:
        [listing setObject:target.url().createNSString().get() forKey:WIRURLKey];
        [listing setObject:target.name().createNSString().get() forKey:WIRTitleKey];
        [listing setObject:target.nameOverride().createNSString().get() forKey:WIROverrideNameKey];
        [listing setObject:WIRTypeWebPage forKey:WIRTypeKey];
        break;
    case RemoteInspectionTarget::Type::WasmDebugger:
        [listing setObject:target.url().createNSString().get() forKey:WIRURLKey];
        [listing setObject:target.name().createNSString().get() forKey:WIRTitleKey];
        [listing setObject:target.nameOverride().createNSString().get() forKey:WIROverrideNameKey];
        [listing setObject:WIRTypeWasmDebugger forKey:WIRTypeKey];
        if (auto pid = target.webContentProcessPID()) {
            RetainPtr<NSMutableDictionary> userInfo = adoptNS([[NSMutableDictionary alloc] init]);
            [userInfo setObject:@(*pid) forKey:@"WebContentProcessPID"];
            [listing setObject:userInfo.get() forKey:WIRUserInfoKey];
        }
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }

    if (auto presentingApplicationPID = target.presentingApplicationPID())
        [listing setObject:identifierForPID(presentingApplicationPID.value()).get() forKey:WIRHostApplicationIdentifierKey];

    if (RefPtr connectionToTarget = m_targetConnectionMap.get(target.targetIdentifier()))
        [listing setObject:connectionToTarget->protectedConnectionIdentifier().get() forKey:WIRConnectionIdentifierKey];

    if (target.hasLocalDebugger())
        [listing setObject:@YES forKey:WIRHasLocalDebuggerKey];

    return listing;
}

RetainPtr<NSDictionary> RemoteInspector::listingForAutomationTarget(const RemoteAutomationTarget& target) const
{
    // Must collect target information on the WebThread or Main thread since RemoteTargets are
    // implemented by non-threadsafe JSC / WebCore classes such as JSGlobalObject or WebCore::Page.
    ASSERT(isMainThread());

    if (target.isPendingTermination())
        return nullptr;

    RetainPtr<NSMutableDictionary> listing = adoptNS([[NSMutableDictionary alloc] init]);
    // FIXME: This is a safer cpp false positive (rdar://161698063).
    SUPPRESS_UNRETAINED_ARG [listing setObject:@(target.targetIdentifier()) forKey:WIRTargetIdentifierKey];
    [listing setObject:target.name().createNSString().get() forKey:WIRSessionIdentifierKey];
    [listing setObject:WIRTypeAutomation forKey:WIRTypeKey];
    // FIXME: This is a safer cpp false positive (rdar://161698063).
    SUPPRESS_UNRETAINED_ARG [listing setObject:@(target.isPaired()) forKey:WIRAutomationTargetIsPairedKey];
    if (m_clientCapabilities) {
        [listing setObject:m_clientCapabilities->browserName.createNSString().get() forKey:WIRAutomationTargetNameKey];
        [listing setObject:m_clientCapabilities->browserVersion.createNSString().get() forKey:WIRAutomationTargetVersionKey];
    }

    if (RefPtr connectionToTarget = m_targetConnectionMap.get(target.targetIdentifier()))
        [listing setObject:connectionToTarget->protectedConnectionIdentifier().get() forKey:WIRConnectionIdentifierKey];

    return listing;
}

void RemoteInspector::pushListingsNow()
{
    RefPtr relayConnection = m_relayConnection;
    ASSERT(relayConnection);
    if (!relayConnection)
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

    relayConnection->sendMessage(WIRListingMessage, message.get());
}

void RemoteInspector::pushListingsSoon()
{
    if (!m_relayConnection)
        return;

    if (m_pushScheduled)
        return;

    m_pushScheduled = true;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 0.2 * NSEC_PER_SEC), globalDispatchQueueSingleton(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
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
    RefPtr target = findResult->value;
    auto connectionToTarget = adoptRef(*new RemoteConnectionToTarget(target.get(), connectionIdentifier, sender));

    if (is<RemoteInspectionTarget>(target)) {
        bool isAutomaticInspection = m_automaticInspectionCandidates.contains(target->targetIdentifier());
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

    RefPtr connectionToTarget = m_targetConnectionMap.get(targetIdentifier);
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

    RefPtr connectionToTarget = m_targetConnectionMap.get(targetIdentifier);
    if (!connectionToTarget)
        return;

    if (![connectionIdentifier isEqualToString:connectionToTarget->protectedConnectionIdentifier().get()])
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
        RefPtr<RemoteControllableTarget> target;
        {
            Locker locker { m_mutex };

            auto findResult = m_targetMap.find(targetIdentifier);
            if (findResult == m_targetMap.end())
                return;

            target = findResult->value;
        }
        if (RefPtr inspectionTarget = dynamicDowncast<RemoteInspectionTarget>(WTFMove(target)))
            inspectionTarget->setIndicating(indicateEnabled);
    });
}

void RemoteInspector::receivedProxyApplicationSetupMessage(NSDictionary *)
{
    RefPtr relayConnection = m_relayConnection;
    ASSERT(relayConnection);
    if (!relayConnection)
        return;

    // Proxy applications using per-target `presentingApplicationPID`s do not require any additional setup. The remote
    // relay will complete its setup when the inspectable target listing is sent to it.
    if (m_usePerTargetPresentingApplicationPIDs)
        return;

    if (!m_parentProcessIdentifier || !m_parentProcessAuditData) {
        // We are a proxy application without parent process information.
        // Wait a bit for the information, but give up after a second.
        m_shouldSendParentProcessInformation = true;
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 1 * NSEC_PER_SEC), globalDispatchQueueSingleton(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
            Locker locker { m_mutex };
            if (m_shouldSendParentProcessInformation)
                stopInternal(StopSource::XPCMessage);
        });
        return;
    }

    m_shouldSendParentProcessInformation = false;

    relayConnection->sendMessage(WIRProxyApplicationSetupResponseMessage, @{
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
        if ([connectionIdentifier isEqualToString:RefPtr { it->value }->protectedConnectionIdentifier().get()])
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
        m_automaticInspectionCandidates.clear();
}

void RemoteInspector::receivedAutomaticInspectionRejectMessage(NSDictionary *userInfo)
{
    NSNumber *targetIdentifierNumber = userInfo[WIRTargetIdentifierKey];
    BAIL_IF_UNEXPECTED_TYPE(targetIdentifierNumber, [NSNumber class]);

    TargetID targetIdentifier = targetIdentifierNumber.unsignedIntValue;
    if (!targetIdentifier)
        return;

    ASSERT(m_automaticInspectionCandidates.contains(targetIdentifier));
    if (m_automaticInspectionCandidates.remove(targetIdentifier)) {
        if (RefPtr target = dynamicDowncast<RemoteInspectionTarget>(m_targetMap.get(targetIdentifier)))
            target->unpauseForResolvedAutomaticInspection();
    }
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

    if (NSNumber *value = forwardedCapabilities[WIRAlwaysAllowAutoplay]) {
        if ([value isKindOfClass:NSNumber.class])
            sessionCapabilities.alwaysAllowAutoplay = value.boolValue;
    }

    if (NSNumber *value = forwardedCapabilities[WIRSiteIsolationEnabled]) {
        if ([value isKindOfClass:NSNumber.class])
            sessionCapabilities.siteIsolationEnabled = value.boolValue;
    }

    if (!m_client)
        return;

    if (!m_clientCapabilities || !m_clientCapabilities->remoteAutomationAllowed)
        return;

    m_client->requestAutomationSession(suggestedSessionIdentifier, sessionCapabilities);
}

void RemoteInspector::receivedPingSuccessMessage()
{
    m_shouldReconnectToRelayOnFailure = false;
}

void RemoteInspector::setNeedMachSandboxExtension(bool needExtension)
{
    needMachSandboxExtension = needExtension;
}

} // namespace Inspector

#endif // ENABLE(REMOTE_INSPECTOR)
