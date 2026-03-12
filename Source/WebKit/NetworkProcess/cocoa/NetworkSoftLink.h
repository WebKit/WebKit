/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#if PLATFORM(COCOA)

#import <pal/spi/cocoa/NetworkSPI.h>
#import <wtf/SoftLinking.h>

SOFT_LINK_FRAMEWORK_FOR_HEADER(WebKit, Network)

SOFT_LINK_FUNCTION_MAY_FAIL_FOR_HEADER_WITH_NS_RETURNS_RETAINED(WebKit, Network, nw_parameters_create_webtransport_http, nw_parameters_t, (nw_parameters_configure_protocol_block_t configure_webtransport, nw_parameters_configure_protocol_block_t configure_tls, nw_parameters_configure_protocol_block_t configure_quic, nw_parameters_configure_protocol_block_t configure_tcp), (configure_webtransport, configure_tls, configure_quic, configure_tcp))

SOFT_LINK_FUNCTION_MAY_FAIL_FOR_HEADER_WITH_NS_RETURNS_RETAINED(WebKit, Network, nw_protocol_copy_webtransport_definition, nw_protocol_definition_t, (void), ())

SOFT_LINK_FUNCTION_MAY_FAIL_FOR_HEADER_WITH_NS_RETURNS_RETAINED(WebKit, Network, nw_webtransport_create_options, nw_protocol_options_t, (void), ())

SOFT_LINK_FUNCTION_MAY_FAIL_FOR_HEADER(WebKit, Network, nw_webtransport_options_set_is_unidirectional, void, (nw_protocol_options_t options, bool is_unidirectional), (options, is_unidirectional))

SOFT_LINK_FUNCTION_MAY_FAIL_FOR_HEADER(WebKit, Network, nw_webtransport_options_set_is_datagram, void, (nw_protocol_options_t options, bool is_datagram), (options, is_datagram))

SOFT_LINK_FUNCTION_MAY_FAIL_FOR_HEADER(WebKit, Network, nw_webtransport_options_add_connect_request_header, void, (nw_protocol_options_t options, const char* name, const char* value), (options, name, value))

SOFT_LINK_FUNCTION_MAY_FAIL_FOR_HEADER(WebKit, Network, nw_webtransport_options_set_allow_joining_before_ready, void, (nw_protocol_options_t options, bool allow), (options, allow))

SOFT_LINK_FUNCTION_MAY_FAIL_FOR_HEADER(WebKit, Network, nw_webtransport_options_set_initial_max_streams_uni, void, (nw_protocol_options_t options, uint64_t initial_max_streams_uni), (options, initial_max_streams_uni))

SOFT_LINK_FUNCTION_MAY_FAIL_FOR_HEADER(WebKit, Network, nw_webtransport_options_set_initial_max_streams_bidi, void, (nw_protocol_options_t options, uint64_t initial_max_streams_bidi), (options, initial_max_streams_bidi))

SOFT_LINK_FUNCTION_MAY_FAIL_FOR_HEADER(WebKit, Network, nw_webtransport_metadata_get_is_peer_initiated, bool, (nw_protocol_metadata_t metadata), (metadata))

SOFT_LINK_FUNCTION_MAY_FAIL_FOR_HEADER(WebKit, Network, nw_webtransport_metadata_get_is_unidirectional, bool, (nw_protocol_metadata_t metadata), (metadata))

SOFT_LINK_FUNCTION_MAY_FAIL_FOR_HEADER(WebKit, Network, nw_webtransport_metadata_get_session_error_code, uint32_t, (nw_protocol_metadata_t metadata), (metadata))

SOFT_LINK_FUNCTION_MAY_FAIL_FOR_HEADER(WebKit, Network, nw_webtransport_metadata_set_session_error_code, void, (nw_protocol_metadata_t metadata, uint32_t error_code), (metadata, error_code))

SOFT_LINK_FUNCTION_MAY_FAIL_FOR_HEADER(WebKit, Network, nw_webtransport_metadata_get_session_error_message, const char*, (nw_protocol_metadata_t metadata), (metadata))

SOFT_LINK_FUNCTION_MAY_FAIL_FOR_HEADER(WebKit, Network, nw_webtransport_metadata_set_session_error_message, void, (nw_protocol_metadata_t metadata, const char* error_message), (metadata, error_message))

SOFT_LINK_FUNCTION_MAY_FAIL_FOR_HEADER(WebKit, Network, nw_webtransport_metadata_get_session_closed, bool, (nw_protocol_metadata_t metadata), (metadata))

SOFT_LINK_FUNCTION_MAY_FAIL_FOR_HEADER(WebKit, Network, nw_webtransport_metadata_set_remote_drain_handler, void, (nw_protocol_metadata_t metadata, nw_webtransport_drain_handler_t handler, dispatch_queue_t queue), (metadata, handler, queue))

SOFT_LINK_FUNCTION_MAY_FAIL_FOR_HEADER_WITH_NS_RETURNS_RETAINED(WebKit, Network, nw_webtransport_metadata_copy_connect_response, nw_http_response_t, (nw_protocol_metadata_t metadata), (metadata))

SOFT_LINK_FUNCTION_MAY_FAIL_FOR_HEADER(WebKit, Network, nw_webtransport_metadata_get_transport_mode, nw_webtransport_transport_mode_t, (nw_protocol_metadata_t metadata), (metadata))

SOFT_LINK_FUNCTION_MAY_FAIL_FOR_HEADER(WebKit, Network, nw_webtransport_metadata_set_remote_receive_error_handler, void, (nw_protocol_metadata_t metadata, nw_webtransport_receive_error_handler_t handler, dispatch_queue_t queue), (metadata, handler, queue))

SOFT_LINK_FUNCTION_MAY_FAIL_FOR_HEADER(WebKit, Network, nw_webtransport_metadata_set_remote_send_error_handler, void, (nw_protocol_metadata_t metadata, nw_webtransport_send_error_handler_t handler, dispatch_queue_t queue), (metadata, handler, queue))

SOFT_LINK_FUNCTION_MAY_FAIL_FOR_HEADER(WebKit, Network, nw_connection_abort_reads, void, (nw_connection_t connection, uint64_t error_code), (connection, error_code))

SOFT_LINK_FUNCTION_MAY_FAIL_FOR_HEADER(WebKit, Network, nw_connection_abort_writes, void, (nw_connection_t connection, uint64_t error_code), (connection, error_code))

SOFT_LINK_FUNCTION_MAY_FAIL_FOR_HEADER(WebKit, Network, nw_http_fields_access_value_by_name, void, (nw_http_fields_t fields, const char* name, NS_NOESCAPE nw_http_optional_string_accessor_t accessor), (fields, name, accessor))

#if HAVE(NWSETTINGS_UNIFIED_HTTP_WEBKIT)

SOFT_LINK_FUNCTION_MAY_FAIL_FOR_HEADER(WebKit, Network, nw_settings_get_unified_http_enabled_webkit, bool, (), ())

#endif

#endif // PLATFORM(COCOA)
