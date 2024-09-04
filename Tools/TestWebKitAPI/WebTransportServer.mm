/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#import "WebTransportServer.h"

#import "HTTPServer.h"
#import "Utilities.h"
#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/BlockPtr.h>

namespace TestWebKitAPI {

struct WebTransportServer::Data : public RefCounted<WebTransportServer::Data> {
    static Ref<Data> create(Function<Task(Connection)>&& connectionHandler) { return adoptRef(*new Data(WTFMove(connectionHandler))); }
    Data(Function<Task(Connection)>&& connectionHandler)
        : connectionHandler(WTFMove(connectionHandler)) { }

    Function<Task(Connection)> connectionHandler;
    RetainPtr<nw_listener_t> listener;
    Vector<RetainPtr<nw_connection_group_t>> connectionGroups;
    Vector<RetainPtr<nw_connection_t>> connections;
    Vector<CoroutineHandle<Task::promise_type>> coroutineHandles;
};

WebTransportServer::WebTransportServer(Function<Task(Connection)>&& connectionHandler)
    : m_data(Data::create(WTFMove(connectionHandler)))
{
    auto configureTLS = [](nw_protocol_options_t options) {
        RetainPtr securityOptions = adoptNS(nw_quic_connection_copy_sec_protocol_options(options));
        sec_protocol_options_set_local_identity(securityOptions.get(), adoptNS(sec_identity_create(testIdentity().get())).get());
        sec_protocol_options_add_tls_application_protocol(securityOptions.get(), "h3");
    };

    RetainPtr parameters = adoptNS(nw_parameters_create_quic_stream(NW_PARAMETERS_DEFAULT_CONFIGURATION, configureTLS));

    RetainPtr listener = adoptNS(nw_listener_create(parameters.get()));

    nw_listener_set_new_connection_group_handler(listener.get(), [data = m_data] (nw_connection_group_t connectionGroup) {
        constexpr uint32_t maximumMessageSize { std::numeric_limits<uint32_t>::max() };
        constexpr bool rejectOversizedMessages { false };
        nw_connection_group_set_receive_handler(connectionGroup, maximumMessageSize, rejectOversizedMessages, ^(dispatch_data_t, nw_content_context_t, bool) {
            // FIXME: Implement and test datagrams with WebTransport.
        });

        nw_connection_group_set_new_connection_handler(connectionGroup, [data] (nw_connection_t connection) {
            data->connections.append(connection);
            nw_connection_set_queue(connection, dispatch_get_main_queue());
            nw_connection_start(connection);
            data->coroutineHandles.append(data->connectionHandler(connection).handle);
        });

        nw_connection_group_set_queue(connectionGroup, dispatch_get_main_queue());
        nw_connection_group_start(connectionGroup);
        data->connectionGroups.append(connectionGroup);
    });
    nw_listener_set_queue(listener.get(), dispatch_get_main_queue());

    __block bool ready = false;
    nw_listener_set_state_changed_handler(listener.get(), ^(nw_listener_state_t state, nw_error_t error) {
        ASSERT_UNUSED(error, !error);
        if (state == nw_listener_state_ready)
            ready = true;
    });
    nw_listener_start(listener.get());
    Util::run(&ready);

    m_data->listener = WTFMove(listener);
}

WebTransportServer::~WebTransportServer() = default;

uint16_t WebTransportServer::port() const
{
    return nw_listener_get_port(m_data->listener.get());
}

} // namespace TestWebKitAPI
