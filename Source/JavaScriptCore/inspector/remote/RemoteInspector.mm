/*
 * Copyright (C) 2013-2015 Apple Inc. All Rights Reserved.
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

namespace Inspector {

static bool canAccessWebInspectorMachPort()
{
    return sandbox_check(getpid(), "mach-lookup", static_cast<enum sandbox_filter_type>(SANDBOX_FILTER_GLOBAL_NAME | SANDBOX_CHECK_NO_REPORT), WIRXPCMachPortName) == 0;
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

bool RemoteInspector::startEnabled = true;

void RemoteInspector::startDisabled()
{
    RemoteInspector::startEnabled = false;
}

RemoteInspector& RemoteInspector::singleton()
{
    static NeverDestroyed<RemoteInspector> shared;

    static dispatch_once_t once;
    dispatch_once(&once, ^{
        if (canAccessWebInspectorMachPort()) {
            dispatch_block_t initialize = ^{
                WTF::initializeMainThread();
                JSC::initializeThreading();
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

unsigned RemoteInspector::nextAvailableIdentifier()
{
    unsigned nextValidIdentifier;
    do {
        nextValidIdentifier = m_nextAvailableIdentifier++;
    } while (!nextValidIdentifier || nextValidIdentifier == std::numeric_limits<unsigned>::max() || m_targetMap.contains(nextValidIdentifier));
    return nextValidIdentifier;
}

void RemoteInspector::registerTarget(RemoteControllableTarget* target)
{
    ASSERT_ARG(target, target);

    std::lock_guard<Lock> lock(m_mutex);

    unsigned identifier = nextAvailableIdentifier();
    target->setIdentifier(identifier);

    {
        auto result = m_targetMap.set(identifier, target);
        ASSERT_UNUSED(result, result.isNewEntry);
    }
    
    // If remote control is not allowed, a null listing is returned.
    if (RetainPtr<NSDictionary> listing = listingForTarget(*target)) {
        auto result = m_listingMap.set(identifier, listing);
        ASSERT_UNUSED(result, result.isNewEntry);
    }

    pushListingsSoon();
}

void RemoteInspector::unregisterTarget(RemoteControllableTarget* target)
{
    ASSERT_ARG(target, target);

    std::lock_guard<Lock> lock(m_mutex);

    unsigned identifier = target->identifier();
    if (!identifier)
        return;

    bool wasRemoved = m_targetMap.remove(identifier);
    ASSERT_UNUSED(wasRemoved, wasRemoved);

    // The listing may never have been added if remote control isn't allowed.
    m_listingMap.remove(identifier);

    if (auto connection = m_connectionMap.take(identifier))
        connection->targetClosed();

    pushListingsSoon();
}

void RemoteInspector::updateTarget(RemoteControllableTarget* target)
{
    ASSERT_ARG(target, target);

    std::lock_guard<Lock> lock(m_mutex);

    unsigned identifier = target->identifier();
    if (!identifier)
        return;

    {
        auto result = m_targetMap.set(identifier, target);
        ASSERT_UNUSED(result, !result.isNewEntry);
    }

    // If the target has just allowed remote control, then the listing won't exist yet.
    if (RetainPtr<NSDictionary> listing = listingForTarget(*target))
        m_listingMap.set(identifier, listing);

    pushListingsSoon();
}

void RemoteInspector::updateAutomaticInspectionCandidate(RemoteInspectionTarget* target)
{
    ASSERT_ARG(target, target);
    {
        std::lock_guard<Lock> lock(m_mutex);

        unsigned identifier = target->identifier();
        if (!identifier)
            return;

        auto result = m_targetMap.set(identifier, target);
        ASSERT_UNUSED(result, !result.isNewEntry);

        // If the target has just allowed remote control, then the listing won't exist yet.
        if (RetainPtr<NSDictionary> listing = listingForTarget(*target))
            m_listingMap.set(identifier, listing);

        // Don't allow automatic inspection unless it is allowed or we are stopped.
        if (!m_automaticInspectionEnabled || !m_enabled) {
            pushListingsSoon();
            return;
        }

        // FIXME: We should handle multiple debuggables trying to pause at the same time on different threads.
        // To make this work we will need to change m_automaticInspectionCandidateIdentifier to be a per-thread value.
        // Multiple attempts on the same thread should not be possible because our nested run loop is in a special RWI mode.
        if (m_automaticInspectionPaused) {
            LOG_ERROR("Skipping Automatic Inspection Candidate with pageId(%u) because we are already paused waiting for pageId(%u)", identifier, m_automaticInspectionCandidateIdentifier);
            pushListingsSoon();
            return;
        }

        m_automaticInspectionPaused = true;
        m_automaticInspectionCandidateIdentifier = identifier;

        // If we are pausing before we have connected to webinspectord the candidate message will be sent as soon as the connection is established.
        if (m_xpcConnection) {
            pushListingsNow();
            sendAutomaticInspectionCandidateMessage();
        }

        // In case debuggers fail to respond, or we cannot connect to webinspectord, automatically continue after a short period of time.
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 0.8 * NSEC_PER_SEC), dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
            std::lock_guard<Lock> lock(m_mutex);
            if (m_automaticInspectionCandidateIdentifier == identifier) {
                LOG_ERROR("Skipping Automatic Inspection Candidate with pageId(%u) because we failed to receive a response in time.", m_automaticInspectionCandidateIdentifier);
                m_automaticInspectionPaused = false;
            }
        });
    }

    target->pauseWaitingForAutomaticInspection();

    {
        std::lock_guard<Lock> lock(m_mutex);

        ASSERT(m_automaticInspectionCandidateIdentifier);
        m_automaticInspectionCandidateIdentifier = 0;
    }
}

void RemoteInspector::sendAutomaticInspectionCandidateMessage()
{
    ASSERT(m_enabled);
    ASSERT(m_automaticInspectionEnabled);
    ASSERT(m_automaticInspectionPaused);
    ASSERT(m_automaticInspectionCandidateIdentifier);
    ASSERT(m_xpcConnection);

    NSDictionary *details = @{WIRPageIdentifierKey: @(m_automaticInspectionCandidateIdentifier)};
    m_xpcConnection->sendMessage(WIRAutomaticInspectionCandidateMessage, details);
}

void RemoteInspector::sendMessageToRemote(unsigned identifier, const String& message)
{
    std::lock_guard<Lock> lock(m_mutex);

    if (!m_xpcConnection)
        return;

    auto connection = m_connectionMap.get(identifier);
    if (!connection)
        return;

    NSDictionary *userInfo = @{
        WIRRawDataKey: [static_cast<NSString *>(message) dataUsingEncoding:NSUTF8StringEncoding],
        WIRConnectionIdentifierKey: connection->connectionIdentifier(),
        WIRDestinationKey: connection->destination()
    };

    m_xpcConnection->sendMessage(WIRRawDataMessage, userInfo);
}

void RemoteInspector::setupFailed(unsigned identifier)
{
    std::lock_guard<Lock> lock(m_mutex);

    m_connectionMap.remove(identifier);

    updateHasActiveDebugSession();

    if (identifier == m_automaticInspectionCandidateIdentifier)
        m_automaticInspectionPaused = false;

    pushListingsSoon();
}

void RemoteInspector::setupCompleted(unsigned identifier)
{
    std::lock_guard<Lock> lock(m_mutex);

    if (identifier == m_automaticInspectionCandidateIdentifier)
        m_automaticInspectionPaused = false;
}

bool RemoteInspector::waitingForAutomaticInspection(unsigned)
{
    // We don't take the lock to check this because we assume it will be checked repeatedly.
    return m_automaticInspectionPaused;
}

void RemoteInspector::start()
{
    std::lock_guard<Lock> lock(m_mutex);

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

void RemoteInspector::stop()
{
    std::lock_guard<Lock> lock(m_mutex);

    stopInternal(StopSource::API);
}

void RemoteInspector::stopInternal(StopSource source)
{
    if (!m_enabled)
        return;

    m_enabled = false;

    m_pushScheduled = false;

    for (auto connection : m_connectionMap.values())
        connection->close();
    m_connectionMap.clear();

    updateHasActiveDebugSession();

    m_automaticInspectionPaused = false;

    if (m_xpcConnection) {
        switch (source) {
        case StopSource::API:
            m_xpcConnection->close();
            break;
        case StopSource::XPCMessage:
            m_xpcConnection->closeFromMessage();
            break;
        }

        m_xpcConnection = nullptr;
    }

    notify_cancel(m_notifyToken);
}

void RemoteInspector::setupXPCConnectionIfNeeded()
{
    std::lock_guard<Lock> lock(m_mutex);

    if (m_xpcConnection)
        return;

    xpc_connection_t connection = xpc_connection_create_mach_service(WIRXPCMachPortName, m_xpcQueue, 0);
    if (!connection)
        return;

    m_xpcConnection = adoptRef(new RemoteInspectorXPCConnection(connection, m_xpcQueue, this));
    m_xpcConnection->sendMessage(@"syn", nil); // Send a simple message to initialize the XPC connection.
    xpc_release(connection);

    if (m_automaticInspectionCandidateIdentifier) {
        // We already have a debuggable waiting to be automatically inspected.
        pushListingsNow();
        sendAutomaticInspectionCandidateMessage();
    } else
        pushListingsSoon();
}

#pragma mark - Proxy Application Information

void RemoteInspector::setParentProcessInformation(pid_t pid, RetainPtr<CFDataRef> auditData)
{
    std::lock_guard<Lock> lock(m_mutex);

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
    std::lock_guard<Lock> lock(m_mutex);

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
    else
        NSLog(@"Unrecognized RemoteInspector XPC Message: %@", messageName);
}

void RemoteInspector::xpcConnectionFailed(RemoteInspectorXPCConnection* connection)
{
    std::lock_guard<Lock> lock(m_mutex);

    ASSERT(connection == m_xpcConnection);
    if (connection != m_xpcConnection)
        return;

    m_pushScheduled = false;

    for (auto connection : m_connectionMap.values())
        connection->close();
    m_connectionMap.clear();

    updateHasActiveDebugSession();

    m_automaticInspectionPaused = false;

    // The connection will close itself.
    m_xpcConnection = nullptr;
}

void RemoteInspector::xpcConnectionUnhandledMessage(RemoteInspectorXPCConnection*, xpc_object_t)
{
    // Intentionally ignored.
}

#pragma mark - Listings

RetainPtr<NSDictionary> RemoteInspector::listingForTarget(const RemoteControllableTarget& target) const
{
    if (is<RemoteInspectionTarget>(target))
        return listingForInspectionTarget(downcast<RemoteInspectionTarget>(target));
    if (is<RemoteAutomationTarget>(target))
        return listingForAutomationTarget(downcast<RemoteAutomationTarget>(target));

    ASSERT_NOT_REACHED();
    return nil;
}

RetainPtr<NSDictionary> RemoteInspector::listingForInspectionTarget(const RemoteInspectionTarget& target) const
{
    // Must collect target information on the WebThread, Main, or Worker thread since RemoteTargets are
    // implemented by non-threadsafe JSC / WebCore classes such as JSGlobalObject or WebCore::Page.

    if (!target.remoteDebuggingAllowed())
        return nil;

    RetainPtr<NSMutableDictionary> listing = adoptNS([[NSMutableDictionary alloc] init]);
    [listing setObject:@(target.identifier()) forKey:WIRPageIdentifierKey];

    switch (target.type()) {
    case RemoteInspectionTarget::Type::JavaScript:
        [listing setObject:target.name() forKey:WIRTitleKey];
        [listing setObject:WIRTypeJavaScript forKey:WIRTypeKey];
        break;
    case RemoteInspectionTarget::Type::Web:
        [listing setObject:target.url() forKey:WIRURLKey];
        [listing setObject:target.name() forKey:WIRTitleKey];
        [listing setObject:WIRTypeWeb forKey:WIRTypeKey];
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }

    if (auto* connection = m_connectionMap.get(target.identifier()))
        [listing setObject:connection->connectionIdentifier() forKey:WIRConnectionIdentifierKey];

    if (target.hasLocalDebugger())
        [listing setObject:@YES forKey:WIRHasLocalDebuggerKey];

    return listing;
}

RetainPtr<NSDictionary> RemoteInspector::listingForAutomationTarget(const RemoteAutomationTarget& target) const
{
    // Must collect target information on the WebThread or Main thread since RemoteTargets are
    // implemented by non-threadsafe JSC / WebCore classes such as JSGlobalObject or WebCore::Page.
    ASSERT(isMainThread());

    if (!target.automationAllowed())
        return nil;

    RetainPtr<NSMutableDictionary> listing = adoptNS([[NSMutableDictionary alloc] init]);
    [listing setObject:@(target.identifier()) forKey:WIRPageIdentifierKey];
    [listing setObject:target.name() forKey:WIRTitleKey];
    [listing setObject:WIRTypeAutomation forKey:WIRTypeKey];

    if (auto connection = m_connectionMap.get(target.identifier()))
        [listing setObject:connection->connectionIdentifier() forKey:WIRConnectionIdentifierKey];

    return listing;
}

void RemoteInspector::pushListingsNow()
{
    ASSERT(m_xpcConnection);
    if (!m_xpcConnection)
        return;

    m_pushScheduled = false;

    RetainPtr<NSMutableDictionary> listings = adoptNS([[NSMutableDictionary alloc] init]);
    for (RetainPtr<NSDictionary> listing : m_listingMap.values()) {
        NSString *identifier = [[listing.get() objectForKey:WIRPageIdentifierKey] stringValue];
        [listings setObject:listing.get() forKey:identifier];
    }

    RetainPtr<NSMutableDictionary> message = adoptNS([[NSMutableDictionary alloc] init]);
    [message setObject:listings.get() forKey:WIRListingKey];

    m_xpcConnection->sendMessage(WIRListingMessage, message.get());
}

void RemoteInspector::pushListingsSoon()
{
    if (!m_xpcConnection)
        return;

    if (m_pushScheduled)
        return;

    m_pushScheduled = true;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 0.2 * NSEC_PER_SEC), dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        std::lock_guard<Lock> lock(m_mutex);
        if (m_pushScheduled)
            pushListingsNow();
    });
}

#pragma mark - Active Debugger Sessions

void RemoteInspector::updateHasActiveDebugSession()
{
    bool hasActiveDebuggerSession = !m_connectionMap.isEmpty();
    if (hasActiveDebuggerSession == m_hasActiveDebugSession)
        return;

    m_hasActiveDebugSession = hasActiveDebuggerSession;

    // FIXME: Expose some way to access this state in an embedder.
    // Legacy iOS WebKit 1 had a notification. This will need to be smarter with WebKit2.
}


#pragma mark - Received XPC Messages

void RemoteInspector::receivedSetupMessage(NSDictionary *userInfo)
{
    unsigned identifier = [[userInfo objectForKey:WIRPageIdentifierKey] unsignedIntegerValue];
    if (!identifier)
        return;

    NSString *connectionIdentifier = [userInfo objectForKey:WIRConnectionIdentifierKey];
    if (!connectionIdentifier)
        return;

    NSString *sender = [userInfo objectForKey:WIRSenderKey];
    if (!sender)
        return;

    if (m_connectionMap.contains(identifier))
        return;

    auto findResult = m_targetMap.find(identifier);
    if (findResult == m_targetMap.end())
        return;

    // Attempt to create a connection. This may fail if the page already has an inspector or if it disallows inspection.
    RemoteControllableTarget* target = findResult->value;
    RefPtr<RemoteConnectionToTarget> connection = adoptRef(new RemoteConnectionToTarget(downcast<RemoteInspectionTarget>(target), connectionIdentifier, sender));

    if (is<RemoteInspectionTarget>(target)) {
        bool isAutomaticInspection = m_automaticInspectionCandidateIdentifier == target->identifier();
        bool automaticallyPause = [[userInfo objectForKey:WIRAutomaticallyPause] boolValue];

        if (!connection->setup(isAutomaticInspection, automaticallyPause)) {
            connection->close();
            return;
        }
        m_connectionMap.set(identifier, connection.release());
        updateHasActiveDebugSession();
    } else if (is<RemoteAutomationTarget>(target)) {
        if (!connection->setup()) {
            connection->close();
            return;
        }
        m_connectionMap.set(identifier, connection.release());
        updateHasActiveDebugSession();
    } else
        ASSERT_NOT_REACHED();

    pushListingsSoon();
}

void RemoteInspector::receivedDataMessage(NSDictionary *userInfo)
{
    unsigned identifier = [[userInfo objectForKey:WIRPageIdentifierKey] unsignedIntegerValue];
    if (!identifier)
        return;

    auto connection = m_connectionMap.get(identifier);
    if (!connection)
        return;

    NSData *data = [userInfo objectForKey:WIRSocketDataKey];
    RetainPtr<NSString> message = adoptNS([[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding]);
    connection->sendMessageToTarget(message.get());
}

void RemoteInspector::receivedDidCloseMessage(NSDictionary *userInfo)
{
    unsigned identifier = [[userInfo objectForKey:WIRPageIdentifierKey] unsignedIntegerValue];
    if (!identifier)
        return;

    NSString *connectionIdentifier = [userInfo objectForKey:WIRConnectionIdentifierKey];
    if (!connectionIdentifier)
        return;

    auto connection = m_connectionMap.get(identifier);
    if (!connection)
        return;

    if (![connectionIdentifier isEqualToString:connection->connectionIdentifier()])
        return;

    connection->close();
    m_connectionMap.remove(identifier);

    updateHasActiveDebugSession();

    pushListingsSoon();
}

void RemoteInspector::receivedGetListingMessage(NSDictionary *)
{
    pushListingsNow();
}

void RemoteInspector::receivedIndicateMessage(NSDictionary *userInfo)
{
    unsigned identifier = [[userInfo objectForKey:WIRPageIdentifierKey] unsignedIntegerValue];
    if (!identifier)
        return;

    BOOL indicateEnabled = [[userInfo objectForKey:WIRIndicateEnabledKey] boolValue];

    callOnWebThreadOrDispatchAsyncOnMainThread(^{
        RemoteControllableTarget* target = nullptr;
        {
            std::lock_guard<Lock> lock(m_mutex);

            auto findResult = m_targetMap.find(identifier);
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
    ASSERT(m_xpcConnection);
    if (!m_xpcConnection)
        return;

    if (!m_parentProcessIdentifier || !m_parentProcessAuditData) {
        // We are a proxy application without parent process information.
        // Wait a bit for the information, but give up after a second.
        m_shouldSendParentProcessInformation = true;
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 1 * NSEC_PER_SEC), dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
            std::lock_guard<Lock> lock(m_mutex);
            if (m_shouldSendParentProcessInformation)
                stopInternal(StopSource::XPCMessage);
        });
        return;
    }

    m_shouldSendParentProcessInformation = false;

    m_xpcConnection->sendMessage(WIRProxyApplicationSetupResponseMessage, @{
        WIRProxyApplicationParentPIDKey: @(m_parentProcessIdentifier),
        WIRProxyApplicationParentAuditDataKey: (NSData *)m_parentProcessAuditData.get(),
    });
}

void RemoteInspector::receivedConnectionDiedMessage(NSDictionary *userInfo)
{
    NSString *connectionIdentifier = [userInfo objectForKey:WIRConnectionIdentifierKey];
    if (!connectionIdentifier)
        return;

    auto it = m_connectionMap.begin();
    auto end = m_connectionMap.end();
    for (; it != end; ++it) {
        if ([connectionIdentifier isEqualToString:it->value->connectionIdentifier()])
            break;
    }

    if (it == end)
        return;

    auto connection = it->value;
    connection->close();
    m_connectionMap.remove(it);

    updateHasActiveDebugSession();
}

void RemoteInspector::receivedAutomaticInspectionConfigurationMessage(NSDictionary *userInfo)
{
    m_automaticInspectionEnabled = [[userInfo objectForKey:WIRAutomaticInspectionEnabledKey] boolValue];

    if (!m_automaticInspectionEnabled && m_automaticInspectionPaused)
        m_automaticInspectionPaused = false;
}

void RemoteInspector::receivedAutomaticInspectionRejectMessage(NSDictionary *userInfo)
{
    unsigned rejectionIdentifier = [[userInfo objectForKey:WIRPageIdentifierKey] unsignedIntValue];

    ASSERT(rejectionIdentifier == m_automaticInspectionCandidateIdentifier);
    if (rejectionIdentifier == m_automaticInspectionCandidateIdentifier)
        m_automaticInspectionPaused = false;
}

} // namespace Inspector

#endif // ENABLE(REMOTE_INSPECTOR)
