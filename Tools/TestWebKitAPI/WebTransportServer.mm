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

namespace TestWebKitAPI {

WebTransportServer::WebTransportServer()
{
    auto configureTLS = [](nw_protocol_options_t options) {
        RetainPtr securityOptions = adoptNS(nw_quic_connection_copy_sec_protocol_options(options));
        sec_protocol_options_set_local_identity(securityOptions.get(), adoptNS(sec_identity_create(testIdentity().get())).get());
        sec_protocol_options_add_tls_application_protocol(securityOptions.get(), "h3");
    };

    RetainPtr parameters = adoptNS(nw_parameters_create_quic_stream(NW_PARAMETERS_DEFAULT_CONFIGURATION, configureTLS));

    m_listener = adoptNS(nw_listener_create(parameters.get()));

    // FIXME: This block unsafely captures this.
    nw_listener_set_new_connection_group_handler(m_listener.get(), ^(nw_connection_group_t connectionGroup) {
        constexpr uint32_t maximumMessageSize { std::numeric_limits<uint32_t>::max() };
        constexpr bool rejectOversizedMessages { false };
        nw_connection_group_set_receive_handler(connectionGroup, maximumMessageSize, rejectOversizedMessages, ^(dispatch_data_t data, nw_content_context_t, bool) {
            // FIXME: Implement and test datagrams with WebTransport.
        });

        // FIXME: This block unsafely captures this.
        nw_connection_group_set_new_connection_handler(connectionGroup, ^(nw_connection_t connection) {
            nw_connection_set_queue(connection, dispatch_get_main_queue());
            nw_connection_set_state_changed_handler(connection, ^(nw_connection_state_t state, nw_error_t error) {
                if (error) {
                    ASSERT_NOT_REACHED();
                    return;
                }
                if (state != nw_connection_state_ready)
                    return;
                nw_connection_receive(connection, 1, std::numeric_limits<uint32_t>::max(), ^(dispatch_data_t content, nw_content_context_t context, bool is_complete, nw_error_t error) {
                    nw_connection_send(connection, content, NW_CONNECTION_DEFAULT_MESSAGE_CONTEXT, true, ^(nw_error_t error) {
                        ASSERT_UNUSED(error, !error);
                    });
                });
            });
            nw_connection_start(connection);
            m_connections.append(connection);
        });

        nw_connection_group_set_queue(connectionGroup, dispatch_get_main_queue());
        nw_connection_group_start(connectionGroup);
        m_connectionGroups.append(connectionGroup);
    });
    nw_listener_set_queue(m_listener.get(), dispatch_get_main_queue());

    __block bool ready = false;
    nw_listener_set_state_changed_handler(m_listener.get(), ^(nw_listener_state_t state, nw_error_t error) {
        ASSERT_UNUSED(error, !error);
        if (state == nw_listener_state_ready)
            ready = true;
    });
    nw_listener_start(m_listener.get());
    Util::run(&ready);
}

uint16_t WebTransportServer::port() const
{
    return nw_listener_get_port(m_listener.get());
}

} // namespace TestWebKitAPI
