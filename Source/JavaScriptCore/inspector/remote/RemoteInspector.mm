/*
 * Copyright (C) 2013 Apple Inc. All Rights Reserved.
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
#import "RemoteInspectorConstants.h"
#import "RemoteInspectorDebuggable.h"
#import "RemoteInspectorDebuggableConnection.h"
#import <Foundation/Foundation.h>
#import <notify.h>
#import <wtf/Assertions.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/text/WTFString.h>
#import <xpc/xpc.h>

#if __has_include(<sandbox/private.h>)
#import <sandbox/private.h>
#else
enum sandbox_filter_type {
    SANDBOX_FILTER_GLOBAL_NAME,
};
#endif
extern "C" {
    int sandbox_check(pid_t, const char *operation, enum sandbox_filter_type, ...);
}

#if PLATFORM(IOS)
#import <wtf/ios/WebCoreThread.h>
#endif

namespace Inspector {

static bool canAccessWebInspectorMachPort()
{
    return sandbox_check(getpid(), "mach-lookup", SANDBOX_FILTER_GLOBAL_NAME, WIRXPCMachPortName) == 0;
}

static void dispatchAsyncOnQueueSafeForAnyDebuggable(void (^block)())
{
#if PLATFORM(IOS)
    if (WebCoreWebThreadIsEnabled && WebCoreWebThreadIsEnabled()) {
        WebCoreWebThreadRun(block);
        return;
    }
#endif

    dispatch_async(dispatch_get_main_queue(), block);
}

bool RemoteInspector::startEnabled = true;

void RemoteInspector::startDisabled()
{
    RemoteInspector::startEnabled = false;
}

RemoteInspector& RemoteInspector::shared()
{
    static NeverDestroyed<RemoteInspector> shared;

    static dispatch_once_t once;
    dispatch_once(&once, ^{
        if (canAccessWebInspectorMachPort()) {
            JSC::initializeThreading();
            if (RemoteInspector::startEnabled)
                shared.get().start();
        }
    });

    return shared;
}

RemoteInspector::RemoteInspector()
    : m_xpcQueue(dispatch_queue_create("com.apple.JavaScriptCore.remote-inspector-xpc", DISPATCH_QUEUE_SERIAL))
    , m_nextAvailableIdentifier(1)
    , m_notifyToken(0)
    , m_enabled(false)
    , m_hasActiveDebugSession(false)
    , m_pushScheduled(false)
    , m_parentProcessIdentifier(0)
    , m_shouldSendParentProcessInformation(false)
{
}

unsigned RemoteInspector::nextAvailableIdentifier()
{
    unsigned nextValidIdentifier;
    do {
        nextValidIdentifier = m_nextAvailableIdentifier++;
    } while (!nextValidIdentifier || nextValidIdentifier == std::numeric_limits<unsigned>::max() || m_debuggableMap.contains(nextValidIdentifier));
    return nextValidIdentifier;
}

void RemoteInspector::registerDebuggable(RemoteInspectorDebuggable* debuggable)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    unsigned identifier = nextAvailableIdentifier();
    debuggable->setIdentifier(identifier);

    auto result = m_debuggableMap.set(identifier, std::make_pair(debuggable, debuggable->info()));
    ASSERT_UNUSED(result, result.isNewEntry);

    if (debuggable->remoteDebuggingAllowed())
        pushListingSoon();
}

void RemoteInspector::unregisterDebuggable(RemoteInspectorDebuggable* debuggable)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    unsigned identifier = debuggable->identifier();
    if (!identifier)
        return;

    bool wasRemoved = m_debuggableMap.remove(identifier);
    ASSERT_UNUSED(wasRemoved, wasRemoved);

    if (RefPtr<RemoteInspectorDebuggableConnection> connection = m_connectionMap.take(identifier))
        connection->closeFromDebuggable();

    if (debuggable->remoteDebuggingAllowed())
        pushListingSoon();
}

void RemoteInspector::updateDebuggable(RemoteInspectorDebuggable* debuggable)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    unsigned identifier = debuggable->identifier();
    if (!identifier)
        return;

    auto result = m_debuggableMap.set(identifier, std::make_pair(debuggable, debuggable->info()));
    ASSERT_UNUSED(result, !result.isNewEntry);

    pushListingSoon();
}

void RemoteInspector::sendMessageToRemoteFrontend(unsigned identifier, const String& message)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_xpcConnection)
        return;

    RefPtr<RemoteInspectorDebuggableConnection> connection = m_connectionMap.get(identifier);
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
    std::lock_guard<std::mutex> lock(m_mutex);

    m_connectionMap.remove(identifier);

    updateHasActiveDebugSession();

    pushListingSoon();
}

void RemoteInspector::start()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_enabled)
        return;

    m_enabled = true;

    notify_register_dispatch(WIRServiceAvailableNotification, &m_notifyToken, m_xpcQueue, ^(int) {
        RemoteInspector::shared().setupXPCConnectionIfNeeded();
    });

    notify_post(WIRServiceAvailabilityCheckNotification);
}

void RemoteInspector::stop()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    stopInternal(StopSource::API);
}

void RemoteInspector::stopInternal(StopSource source)
{
    if (!m_enabled)
        return;

    m_enabled = false;

    m_pushScheduled = false;

    for (auto it = m_connectionMap.begin(), end = m_connectionMap.end(); it != end; ++it)
        it->value->close();
    m_connectionMap.clear();

    updateHasActiveDebugSession();

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
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_xpcConnection)
        return;

    xpc_connection_t connection = xpc_connection_create_mach_service(WIRXPCMachPortName, m_xpcQueue, 0);
    if (!connection)
        return;

    m_xpcConnection = adoptRef(new RemoteInspectorXPCConnection(connection, m_xpcQueue, this));
    m_xpcConnection->sendMessage(@"syn", nil); // Send a simple message to initialize the XPC connection.
    xpc_release(connection);

    pushListingSoon();
}

#pragma mark - Proxy Application Information

void RemoteInspector::setParentProcessInformation(pid_t pid, RetainPtr<CFDataRef> auditData)
{
    std::lock_guard<std::mutex> lock(m_mutex);

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
    std::lock_guard<std::mutex> lock(m_mutex);

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
    else
        NSLog(@"Unrecognized RemoteInspector XPC Message: %@", messageName);
}

void RemoteInspector::xpcConnectionFailed(RemoteInspectorXPCConnection* connection)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    ASSERT(connection == m_xpcConnection);
    if (connection != m_xpcConnection)
        return;

    m_pushScheduled = false;

    for (auto it = m_connectionMap.begin(), end = m_connectionMap.end(); it != end; ++it)
        it->value->close();
    m_connectionMap.clear();

    updateHasActiveDebugSession();

    // The connection will close itself.
    m_xpcConnection = nullptr;
}

void RemoteInspector::xpcConnectionUnhandledMessage(RemoteInspectorXPCConnection*, xpc_object_t)
{
    // Intentionally ignored.
}

#pragma mark - Listings

NSDictionary *RemoteInspector::listingForDebuggable(const RemoteInspectorDebuggableInfo& debuggableInfo) const
{
    NSMutableDictionary *debuggableDetails = [NSMutableDictionary dictionary];

    [debuggableDetails setObject:@(debuggableInfo.identifier) forKey:WIRPageIdentifierKey];

    switch (debuggableInfo.type) {
    case RemoteInspectorDebuggable::JavaScript: {
        NSString *name = debuggableInfo.name;
        [debuggableDetails setObject:name forKey:WIRTitleKey];
        [debuggableDetails setObject:WIRTypeJavaScript forKey:WIRTypeKey];
        break;
    }
    case RemoteInspectorDebuggable::Web: {
        NSString *url = debuggableInfo.url;
        NSString *title = debuggableInfo.name;
        [debuggableDetails setObject:url forKey:WIRURLKey];
        [debuggableDetails setObject:title forKey:WIRTitleKey];
        [debuggableDetails setObject:WIRTypeWeb forKey:WIRTypeKey];
        break;
    }
    default:
        ASSERT_NOT_REACHED();
        break;
    }

    if (RefPtr<RemoteInspectorDebuggableConnection> connection = m_connectionMap.get(debuggableInfo.identifier))
        [debuggableDetails setObject:connection->connectionIdentifier() forKey:WIRConnectionIdentifierKey];

    if (debuggableInfo.hasLocalDebugger)
        [debuggableDetails setObject:@YES forKey:WIRHasLocalDebuggerKey];

    return debuggableDetails;
}

void RemoteInspector::pushListingNow()
{
    ASSERT(m_xpcConnection);
    if (!m_xpcConnection)
        return;

    m_pushScheduled = false;

    RetainPtr<NSMutableDictionary> response = adoptNS([[NSMutableDictionary alloc] init]);
    for (auto it = m_debuggableMap.begin(), end = m_debuggableMap.end(); it != end; ++it) {
        const RemoteInspectorDebuggableInfo& debuggableInfo = it->value.second;
        if (debuggableInfo.remoteDebuggingAllowed) {
            NSDictionary *details = listingForDebuggable(debuggableInfo);
            [response setObject:details forKey:[NSString stringWithFormat:@"%u", debuggableInfo.identifier]];
        }
    }

    RetainPtr<NSMutableDictionary> outgoing = adoptNS([[NSMutableDictionary alloc] init]);
    [outgoing setObject:response.get() forKey:WIRListingKey];

    m_xpcConnection->sendMessage(WIRListingMessage, outgoing.get());
}

void RemoteInspector::pushListingSoon()
{
    if (!m_xpcConnection)
        return;

    if (m_pushScheduled)
        return;

    m_pushScheduled = true;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 0.2 * NSEC_PER_SEC), dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_pushScheduled)
            pushListingNow();
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
    NSNumber *pageId = [userInfo objectForKey:WIRPageIdentifierKey];
    if (!pageId)
        return;

    NSString *connectionIdentifier = [userInfo objectForKey:WIRConnectionIdentifierKey];
    if (!connectionIdentifier)
        return;

    NSString *sender = [userInfo objectForKey:WIRSenderKey];
    if (!sender)
        return;

    unsigned identifier = [pageId unsignedIntValue];
    if (m_connectionMap.contains(identifier))
        return;

    auto it = m_debuggableMap.find(identifier);
    if (it == m_debuggableMap.end())
        return;

    // Attempt to create a connection. This may fail if the page already has an inspector or if it disallows inspection.
    RemoteInspectorDebuggable* debuggable = it->value.first;
    RemoteInspectorDebuggableInfo debuggableInfo = it->value.second;
    RefPtr<RemoteInspectorDebuggableConnection> connection = adoptRef(new RemoteInspectorDebuggableConnection(debuggable, connectionIdentifier, sender, debuggableInfo.type));
    if (!connection->setup()) {
        connection->close();
        return;
    }

    m_connectionMap.set(identifier, connection.release());

    updateHasActiveDebugSession();

    pushListingSoon();
}

void RemoteInspector::receivedDataMessage(NSDictionary *userInfo)
{
    NSNumber *pageId = [userInfo objectForKey:WIRPageIdentifierKey];
    if (!pageId)
        return;

    unsigned pageIdentifier = [pageId unsignedIntValue];
    RefPtr<RemoteInspectorDebuggableConnection> connection = m_connectionMap.get(pageIdentifier);
    if (!connection)
        return;

    NSData *data = [userInfo objectForKey:WIRSocketDataKey];
    RetainPtr<NSString> message = adoptNS([[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding]);
    connection->sendMessageToBackend(message.get());
}

void RemoteInspector::receivedDidCloseMessage(NSDictionary *userInfo)
{
    NSNumber *pageId = [userInfo objectForKey:WIRPageIdentifierKey];
    if (!pageId)
        return;

    NSString *connectionIdentifier = [userInfo objectForKey:WIRConnectionIdentifierKey];
    if (!connectionIdentifier)
        return;

    unsigned identifier = [pageId unsignedIntValue];
    RefPtr<RemoteInspectorDebuggableConnection> connection = m_connectionMap.get(identifier);
    if (!connection)
        return;

    if (![connectionIdentifier isEqualToString:connection->connectionIdentifier()])
        return;

    connection->close();
    m_connectionMap.remove(identifier);

    updateHasActiveDebugSession();

    pushListingSoon();
}

void RemoteInspector::receivedGetListingMessage(NSDictionary *)
{
    pushListingNow();
}

void RemoteInspector::receivedIndicateMessage(NSDictionary *userInfo)
{
    NSNumber *pageId = [userInfo objectForKey:WIRPageIdentifierKey];
    if (!pageId)
        return;

    unsigned identifier = [pageId unsignedIntValue];
    BOOL indicateEnabled = [[userInfo objectForKey:WIRIndicateEnabledKey] boolValue];

    dispatchAsyncOnQueueSafeForAnyDebuggable(^{
        RemoteInspectorDebuggable* debuggable = nullptr;
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            auto it = m_debuggableMap.find(identifier);
            if (it == m_debuggableMap.end())
                return;

            debuggable = it->value.first;
        }
        debuggable->setIndicating(indicateEnabled);
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
            std::lock_guard<std::mutex> lock(m_mutex);
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

    RefPtr<RemoteInspectorDebuggableConnection> connection = it->value;
    connection->close();
    m_connectionMap.remove(it);

    updateHasActiveDebugSession();
}

} // namespace Inspector

#endif // ENABLE(REMOTE_INSPECTOR)
