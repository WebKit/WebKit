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

namespace TestWebKitAPI {

void connectionLoop(WebTransportServer& server, Connection c)
{
    // FIXME: This unsafely captures a reference to the server.
    c.receiveBytes([&server, c](Vector<uint8_t>&& d) {
        if (d.size()) {
            server.setBytesReceived(d.size());
            d.append(0);
        }
        connectionLoop(server, c);
    });
}

WebTransportServer::WebTransportServer()
{
    RetainPtr parameters = adoptNS(nw_parameters_create_secure_tcp(^(nw_protocol_options_t options) {
        RetainPtr securityOptions = adoptNS(nw_tls_copy_sec_protocol_options(options));
        sec_protocol_options_set_local_identity(securityOptions.get(), adoptNS(sec_identity_create(testIdentity().get())).get());
        sec_protocol_options_add_tls_application_protocol(securityOptions.get(), "h2");
    }, NW_PARAMETERS_DEFAULT_CONFIGURATION));

    m_listener = adoptNS(nw_listener_create(parameters.get()));

    // FIXME: This block unsafely captures this.
    nw_listener_set_new_connection_handler(m_listener.get(), ^(nw_connection_t connection) {
        nw_connection_set_queue(connection, dispatch_get_main_queue());
        nw_connection_start(connection);
        m_connections.append(connection);
        connectionLoop(*this, connection);
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
