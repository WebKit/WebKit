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
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE, PAL_EXPORT)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"

#if HAVE(WEB_TRANSPORT)

#import <pal/spi/cocoa/NetworkSPI.h>
#import <wtf/SoftLinking.h>

SOFT_LINK_FRAMEWORK_FOR_SOURCE(WebKit, Network)

SOFT_LINK_FUNCTION_MAY_FAIL_FOR_SOURCE(WebKit, Network, nw_webtransport_options_set_allow_joining_before_ready, void, (nw_protocol_options_t options, bool allow), (options, allow))

SOFT_LINK_FUNCTION_MAY_FAIL_FOR_SOURCE(WebKit, Network, nw_webtransport_options_set_initial_max_streams_uni, void, (nw_protocol_options_t options, uint64_t initial_max_streams_uni), (options, initial_max_streams_uni))

SOFT_LINK_FUNCTION_MAY_FAIL_FOR_SOURCE(WebKit, Network, nw_webtransport_options_set_initial_max_streams_bidi, void, (nw_protocol_options_t options, uint64_t initial_max_streams_bidi), (options, initial_max_streams_bidi))

SOFT_LINK_FUNCTION_MAY_FAIL_FOR_SOURCE(WebKit, Network, nw_webtransport_metadata_get_session_closed, bool, (nw_protocol_metadata_t metadata), (metadata))

SOFT_LINK_FUNCTION_MAY_FAIL_FOR_SOURCE(WebKit, Network, nw_webtransport_metadata_set_remote_drain_handler, void, (nw_protocol_metadata_t metadata, nw_webtransport_drain_handler_t handler, dispatch_queue_t queue), (metadata, handler, queue))

SOFT_LINK_FUNCTION_MAY_FAIL_FOR_SOURCE(WebKit, Network, nw_webtransport_metadata_copy_connect_response, nw_http_response_t, (nw_protocol_metadata_t metadata), (metadata))

SOFT_LINK_FUNCTION_MAY_FAIL_FOR_SOURCE(WebKit, Network, nw_webtransport_metadata_get_transport_mode, nw_webtransport_transport_mode_t, (nw_protocol_metadata_t metadata), (metadata))

SOFT_LINK_FUNCTION_MAY_FAIL_FOR_SOURCE(WebKit, Network, nw_webtransport_metadata_set_remote_receive_error_handler, void, (nw_protocol_metadata_t metadata, nw_webtransport_receive_error_handler_t handler, dispatch_queue_t queue), (metadata, handler, queue))

SOFT_LINK_FUNCTION_MAY_FAIL_FOR_SOURCE(WebKit, Network, nw_webtransport_metadata_set_remote_send_error_handler, void, (nw_protocol_metadata_t metadata, nw_webtransport_send_error_handler_t handler, dispatch_queue_t queue), (metadata, handler, queue))

SOFT_LINK_FUNCTION_MAY_FAIL_FOR_SOURCE(WebKit, Network, nw_connection_abort_reads, void, (nw_connection_t connection, uint64_t error_code), (connection, error_code))

SOFT_LINK_FUNCTION_MAY_FAIL_FOR_SOURCE(WebKit, Network, nw_connection_abort_writes, void, (nw_connection_t connection, uint64_t error_code), (connection, error_code))

#endif // HAVE(WEB_TRANSPORT)
