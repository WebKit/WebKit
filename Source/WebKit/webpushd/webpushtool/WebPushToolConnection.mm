/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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
#import "WebPushToolConnection.h"

#import "DaemonEncoder.h"
#import "DaemonUtilities.h"
#import "PushClientConnectionMessages.h"
#import "WebPushDaemonConnectionConfiguration.h"
#import "WebPushDaemonConstants.h"
#import <WebCore/SecurityOriginData.h>
#import <mach/mach_init.h>
#import <mach/task.h>
#import <pal/spi/cocoa/ServersSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/MainThread.h>
#import <wtf/RetainPtr.h>
#import <wtf/StdLibExtras.h>
#import <wtf/TZoneMallocInlines.h>

namespace WebPushTool {

WTF_MAKE_TZONE_ALLOCATED_IMPL(Connection);

Ref<Connection> Connection::create(PreferTestService preferTestService, String bundleIdentifier, String pushPartition)
{
    return adoptRef(*new Connection(preferTestService, bundleIdentifier, pushPartition));
}

static mach_port_t maybeConnectToService(const char* serviceName)
{
    mach_port_t bsPort;
    task_get_special_port(mach_task_self(), TASK_BOOTSTRAP_PORT, &bsPort);

    mach_port_t servicePort;
    kern_return_t err = bootstrap_look_up(bsPort, serviceName, &servicePort);

    if (err == KERN_SUCCESS)
        return servicePort;

    return MACH_PORT_NULL;
}

Connection::Connection(PreferTestService preferTestService, String bundleIdentifier, String pushPartition)
    : m_bundleIdentifier(bundleIdentifier)
    , m_pushPartition(pushPartition)
{
    if (preferTestService == PreferTestService::Yes)
        m_serviceName = "org.webkit.webpushtestdaemon.service"_s;
    else
        m_serviceName = "com.apple.webkit.webpushd.service"_s;
}

void Connection::connectToService(WaitForServiceToExist waitForServiceToExist)
{

    m_connection = adoptNS(xpc_connection_create_mach_service(m_serviceName, dispatch_get_main_queue(), 0));

    xpc_connection_set_event_handler(m_connection.get(), [](xpc_object_t event) {
        if (event == XPC_ERROR_CONNECTION_INVALID || event == XPC_ERROR_CONNECTION_INTERRUPTED) {
            fprintf(stderr, "Unexpected XPC connection issue: %s\n", event.debugDescription.UTF8String);
            return;
        }

        RELEASE_ASSERT_NOT_REACHED();
    });

    if (waitForServiceToExist == WaitForServiceToExist::Yes) {
        auto result = maybeConnectToService(m_serviceName);
        if (result == MACH_PORT_NULL)
            printf("Waiting for service '%s' to be available\n", m_serviceName.characters());

        while (result == MACH_PORT_NULL) {
            usleep(1000);
            result = maybeConnectToService(m_serviceName);
        }
    }

    printf("Connecting to service '%s'\n", m_serviceName.characters());
    xpc_connection_activate(m_connection.get());

    sendAuditToken();
}

void Connection::sendPushMessage(PushMessageForTesting&& message, CompletionHandler<void(String)>&& completionHandler)
{
    printf("Injecting push message\n");

    sendWithAsyncReplyWithoutUsingIPCConnection(Messages::PushClientConnection::InjectPushMessageForTesting(WTFMove(message)), WTFMove(completionHandler));
}

void Connection::getPushPermissionState(const String& scope, CompletionHandler<void(WebCore::PushPermissionState)>&& completionHandler)
{
    printf("Getting push permission state\n");

    sendWithAsyncReplyWithoutUsingIPCConnection(Messages::PushClientConnection::GetPushPermissionState(WebCore::SecurityOriginData::fromURL(URL { scope })), WTFMove(completionHandler));
}

void Connection::requestPushPermission(const String& scope, CompletionHandler<void(bool)>&& completionHandler)
{
    printf("Request push permission state for %s\n", scope.utf8().data());

    sendWithAsyncReplyWithoutUsingIPCConnection(Messages::PushClientConnection::RequestPushPermission(WebCore::SecurityOriginData::fromURL(URL { scope })), WTFMove(completionHandler));
}

void Connection::sendAuditToken()
{
    audit_token_t token = { 0, 0, 0, 0, 0, 0, 0, 0 };
    mach_msg_type_number_t auditTokenCount = TASK_AUDIT_TOKEN_COUNT;
    kern_return_t result = task_info(mach_task_self(), TASK_AUDIT_TOKEN, (task_info_t)(&token), &auditTokenCount);
    if (result != KERN_SUCCESS) {
        printf("Unable to get audit token to send\n");
        return;
    }

    WebKit::WebPushD::WebPushDaemonConnectionConfiguration configuration;
    configuration.bundleIdentifierOverride = m_bundleIdentifier;
    configuration.pushPartitionString = m_pushPartition;

    Vector<uint8_t> tokenVector;
    tokenVector.resize(32);
    memcpySpan(tokenVector.mutableSpan(), asByteSpan(token));
    configuration.hostAppAuditTokenData = WTFMove(tokenVector);

    sendWithoutUsingIPCConnection(Messages::PushClientConnection::InitializeConnection(WTFMove(configuration)));
}

static OSObjectPtr<xpc_object_t> messageDictionaryFromEncoder(UniqueRef<IPC::Encoder>&& encoder)
{
    auto xpcData = WebKit::encoderToXPCData(WTFMove(encoder));
    auto dictionary = adoptOSObject(xpc_dictionary_create(nullptr, nullptr, 0));
    xpc_dictionary_set_uint64(dictionary.get(), WebKit::WebPushD::protocolVersionKey, WebKit::WebPushD::protocolVersionValue);
    xpc_dictionary_set_value(dictionary.get(), WebKit::WebPushD::protocolEncodedMessageKey, xpcData.get());

    return dictionary;
}

bool Connection::performSendWithoutUsingIPCConnection(UniqueRef<IPC::Encoder>&& encoder) const
{
    auto dictionary = messageDictionaryFromEncoder(WTFMove(encoder));
    xpc_connection_send_message(m_connection.get(), dictionary.get());

    return true;
}

bool Connection::performSendWithAsyncReplyWithoutUsingIPCConnection(UniqueRef<IPC::Encoder>&& encoder, CompletionHandler<void(IPC::Decoder*)>&& completionHandler) const
{
    auto dictionary = messageDictionaryFromEncoder(WTFMove(encoder));
    xpc_connection_send_message_with_reply(m_connection.get(), dictionary.get(), dispatch_get_main_queue(), makeBlockPtr([completionHandler = WTFMove(completionHandler)] (xpc_object_t reply) mutable {
        if (xpc_get_type(reply) != XPC_TYPE_DICTIONARY) {
            ASSERT_NOT_REACHED();
            return completionHandler(nullptr);
        }
        if (xpc_dictionary_get_uint64(reply, WebKit::WebPushD::protocolVersionKey) != WebKit::WebPushD::protocolVersionValue) {
            ASSERT_NOT_REACHED();
            return completionHandler(nullptr);
        }

        auto data = xpc_dictionary_get_data_span(reply, WebKit::WebPushD::protocolEncodedMessageKey);
        auto decoder = IPC::Decoder::create(data, { });
        ASSERT(decoder);

        completionHandler(decoder.get());
    }).get());

    return true;
}


} // namespace WebPushTool

#endif // ENABLE(WEB_PUSH_NOTIFICATIONS)
