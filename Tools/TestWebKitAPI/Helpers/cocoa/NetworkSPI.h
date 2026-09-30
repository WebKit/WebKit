/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#pragma once

#include <wtf/Compiler.h>
#include <wtf/Platform.h>

DECLARE_SYSTEM_HEADER

#import <pal/spi/cocoa/NetworkSPI.h>

#if USE(APPLE_INTERNAL_SDK) && HAVE(NETWORK_FRAMEWORK_HTTP_MESSAGING)
#import <Security/SecProtocolPriv.h>
#endif

#if HAVE(NETWORK_FRAMEWORK_HTTP_MESSAGING) && !USE(APPLE_INTERNAL_SDK)

typedef enum {
    sec_protocol_transport_any = 0,
    sec_protocol_transport_tcp,
    sec_protocol_transport_quic,
} sec_protocol_transport_t;

WTF_EXTERN_C_BEGIN
void sec_protocol_options_add_transport_specific_application_protocol(sec_protocol_options_t, const char *application_protocol, sec_protocol_transport_t);
WTF_EXTERN_C_END

WTF_EXTERN_C_BEGIN

void nw_parameters_set_attach_protocol_listener(nw_parameters_t, bool);

OS_OBJECT_RETURNS_RETAINED nw_protocol_options_t nw_http_messaging_create_options(void);
OS_OBJECT_RETURNS_RETAINED nw_protocol_options_t nw_http2_create_options(void);

OS_OBJECT_RETURNS_RETAINED nw_parameters_t nw_parameters_create_quic_stream(nw_parameters_configure_protocol_block_t configure_quic_stream, nw_parameters_configure_protocol_block_t configure_quic_connection);
OS_OBJECT_RETURNS_RETAINED sec_protocol_options_t nw_quic_connection_copy_sec_protocol_options(nw_protocol_options_t);
void nw_quic_stream_set_is_unidirectional(nw_protocol_options_t stream_options, bool is_unidirectional);

WTF_EXTERN_C_END

#endif // HAVE(NETWORK_FRAMEWORK_HTTP_MESSAGING) && !USE(APPLE_INTERNAL_SDK)
