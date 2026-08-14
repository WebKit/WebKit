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

DECLARE_SYSTEM_HEADER

#import <pal/spi/cocoa/NetworkSPI.h>

#if USE(APPLE_INTERNAL_SDK) && HAVE(NETWORK_FRAMEWORK_HTTP_MESSAGING)
#import <Security/SecProtocolPriv.h>
#endif

#if HAVE(NETWORK_FRAMEWORK_HTTP_MESSAGING) && !USE(APPLE_INTERNAL_SDK)

#if OS_OBJECT_USE_OBJC
NW_OBJECT_DECL_SUBCLASS(nw_http_request, nw_http_fields);
#else
typedef nw_http_fields_t nw_http_request_t;
#endif // OS_OBJECT_USE_OBJC

typedef enum {
    nw_http_metadata_type_unknown = 0,
    nw_http_metadata_type_request = 1,
    nw_http_metadata_type_response = 2,
    nw_http_metadata_type_informational_response = 3,
} nw_http_metadata_type_t;

typedef void (^nw_http_string_accessor_t)(const char *string);
typedef bool (^nw_http_field_content_enumerator_t)(const char *name, size_t name_length, const char *value, size_t value_length);

typedef enum {
    sec_protocol_transport_any = 0,
    sec_protocol_transport_tcp,
    sec_protocol_transport_quic,
} sec_protocol_transport_t;

WTF_EXTERN_C_BEGIN
void sec_protocol_options_add_transport_specific_application_protocol(sec_protocol_options_t, const char *application_protocol, sec_protocol_transport_t);
WTF_EXTERN_C_END

#ifndef NW_NOESCAPE
#if __has_attribute(noescape)
#define NW_NOESCAPE __attribute__((__noescape__))
#else
#define NW_NOESCAPE
#endif
#endif

WTF_EXTERN_C_BEGIN

void nw_parameters_set_attach_protocol_listener(nw_parameters_t, bool);

OS_OBJECT_RETURNS_RETAINED nw_protocol_definition_t nw_protocol_copy_http_definition(void);
OS_OBJECT_RETURNS_RETAINED nw_protocol_options_t nw_http_messaging_create_options(void);
OS_OBJECT_RETURNS_RETAINED nw_protocol_options_t nw_http2_create_options(void);

OS_OBJECT_RETURNS_RETAINED nw_parameters_t nw_parameters_create_quic_stream(nw_parameters_configure_protocol_block_t configure_quic_stream, nw_parameters_configure_protocol_block_t configure_quic_connection);
OS_OBJECT_RETURNS_RETAINED sec_protocol_options_t nw_quic_connection_copy_sec_protocol_options(nw_protocol_options_t);
void nw_quic_stream_set_is_unidirectional(nw_protocol_options_t stream_options, bool is_unidirectional);

nw_http_metadata_type_t nw_http_metadata_get_type(nw_protocol_metadata_t);
OS_OBJECT_RETURNS_RETAINED nw_http_request_t _Nullable nw_http_metadata_copy_request(nw_protocol_metadata_t);
OS_OBJECT_RETURNS_RETAINED nw_protocol_metadata_t nw_http_create_metadata_for_response(nw_http_response_t);

OS_OBJECT_RETURNS_RETAINED nw_http_fields_t nw_http_fields_create(void);
void nw_http_fields_append(nw_http_fields_t, const char *name, const char *value);
bool nw_http_fields_enumerate(nw_http_fields_t, NW_NOESCAPE nw_http_field_content_enumerator_t);

void nw_http_request_access_method(nw_http_request_t, NW_NOESCAPE nw_http_string_accessor_t);
void nw_http_request_access_path(nw_http_request_t, NW_NOESCAPE nw_http_optional_string_accessor_t);
OS_OBJECT_RETURNS_RETAINED nw_http_fields_t nw_http_request_copy_header_fields(nw_http_request_t);

OS_OBJECT_RETURNS_RETAINED nw_http_response_t nw_http_response_create(uint16_t status_code, const char * _Nullable reason_phrase);
void nw_http_response_set_header_fields(nw_http_response_t, nw_http_fields_t);

WTF_EXTERN_C_END

#endif // HAVE(NETWORK_FRAMEWORK_HTTP_MESSAGING) && !USE(APPLE_INTERNAL_SDK)
