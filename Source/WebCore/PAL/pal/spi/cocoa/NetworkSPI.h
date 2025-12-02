/*
 * Copyright (C) 2021-2025 Apple Inc. All rights reserved.
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
 * BE LIABLE FOR ANY DIRECT, I NDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

DECLARE_SYSTEM_HEADER

#import <Network/Network.h>

typedef void (^nw_webtransport_drain_handler_t)(void);
typedef void (^nw_webtransport_receive_error_handler_t)(uint64_t receive_error_code);
typedef void (^nw_webtransport_send_error_handler_t)(uint64_t send_error_code);
typedef void (^nw_http_optional_string_accessor_t)(const char * _Nullable string);

#if OS_OBJECT_USE_OBJC
NW_OBJECT_DECL(nw_http_fields);
NW_OBJECT_DECL_SUBCLASS(nw_http_response, nw_http_fields);
#else
struct nw_http_fields;
typedef struct nw_http_fields *nw_http_fields_t;
typedef nw_http_fields_t nw_http_response_t;
#endif // OS_OBJECT_USE_OBJC

#if USE(APPLE_INTERNAL_SDK)

#import <nw/private.h>

#if __has_include(<sys/socket_private.h>)
#import <sys/socket_private.h>
#else
#define SO_TC_BK_SYS 100
#define SO_TC_BE 0
#define SO_TC_VI 700
#define SO_TC_VO 800
#endif

#if PLATFORM(MAC) && defined(__OBJC__)
// Only needed for running tests.
#import <NetworkExtension/NEPolicySession.h>
#endif

#endif // USE(APPLE_INTERNAL_SDK)

#if !defined(NW_HAS_WEBTRANSPORT_TRANSPORT_MODE)
typedef enum : uint8_t {
    nw_webtransport_transport_mode_unknown,
    nw_webtransport_transport_mode_http2,
    nw_webtransport_transport_mode_http3,
} nw_webtransport_transport_mode_t;
#endif

#if !USE(APPLE_INTERNAL_SDK)

WTF_EXTERN_C_BEGIN

void nw_parameters_set_account_id(nw_parameters_t, const char * account_id);
void nw_parameters_set_source_application(nw_parameters_t, audit_token_t);
void nw_parameters_set_source_application_by_bundle_id(nw_parameters_t, const char*);
void nw_parameters_set_attributed_bundle_identifier(nw_parameters_t, const char*);
OS_OBJECT_RETURNS_RETAINED nw_endpoint_t nw_endpoint_create_host_with_numeric_port(const char* hostname, uint16_t port_host_order);
const char* nw_endpoint_get_known_tracker_name(nw_endpoint_t);
bool nw_nat64_does_interface_index_support_nat64(uint32_t ifindex);

void nw_parameters_set_is_third_party_web_content(nw_parameters_t, bool is_third_party_web_content);
void nw_parameters_set_is_known_tracker(nw_parameters_t, bool is_known_tracker);
void nw_parameters_allow_sharing_port_with_listener(nw_parameters_t, nw_listener_t);

#define SO_TC_BK_SYS 100
#define SO_TC_BE 0
#define SO_TC_VI 700
#define SO_TC_VO 800

void nw_connection_reset_traffic_class(nw_connection_t, uint32_t traffic_class);
void nw_parameters_set_traffic_class(nw_parameters_t, uint32_t traffic_class);

OS_OBJECT_RETURNS_RETAINED nw_interface_t nw_path_copy_interface(nw_path_t);

bool nw_settings_get_unified_http_enabled(void);

void nw_parameters_set_server_mode(nw_parameters_t, bool);
OS_OBJECT_RETURNS_RETAINED nw_parameters_t nw_parameters_create_webtransport_http(nw_parameters_configure_protocol_block_t, nw_parameters_configure_protocol_block_t, nw_parameters_configure_protocol_block_t, nw_parameters_configure_protocol_block_t);
OS_OBJECT_RETURNS_RETAINED nw_protocol_definition_t nw_protocol_copy_webtransport_definition(void);
OS_OBJECT_RETURNS_RETAINED nw_protocol_options_t nw_webtransport_create_options(void);
bool nw_protocol_options_is_webtransport(nw_protocol_options_t);
void nw_webtransport_options_set_is_unidirectional(nw_protocol_options_t, bool);
void nw_webtransport_options_set_is_datagram(nw_protocol_options_t, bool);
void nw_webtransport_options_set_connection_max_sessions(nw_protocol_options_t, uint64_t);
void nw_webtransport_options_add_connect_request_header(nw_protocol_options_t, const char*, const char*);
void nw_webtransport_options_set_allow_joining_before_ready(nw_protocol_options_t, bool);
void nw_webtransport_options_set_initial_max_streams_uni(nw_protocol_options_t, uint64_t);
void nw_webtransport_options_set_initial_max_streams_bidi(nw_protocol_options_t, uint64_t);

bool nw_webtransport_metadata_get_is_unidirectional(nw_protocol_metadata_t);
uint32_t nw_webtransport_metadata_get_session_error_code(nw_protocol_metadata_t);
void nw_webtransport_metadata_set_session_error_code(nw_protocol_metadata_t, uint32_t);
const char* nw_webtransport_metadata_get_session_error_message(nw_protocol_metadata_t);
void nw_webtransport_metadata_set_session_error_message(nw_protocol_metadata_t, const char*);
bool nw_webtransport_metadata_get_session_closed(nw_protocol_metadata_t);
void nw_webtransport_metadata_set_remote_drain_handler(nw_protocol_metadata_t, nw_webtransport_drain_handler_t, dispatch_queue_t);
void nw_webtransport_metadata_set_local_draining(nw_protocol_metadata_t);
OS_OBJECT_RETURNS_RETAINED nw_http_response_t nw_webtransport_metadata_copy_connect_response(nw_protocol_metadata_t);
nw_webtransport_transport_mode_t nw_webtransport_metadata_get_transport_mode(nw_protocol_metadata_t);
void nw_webtransport_metadata_set_remote_receive_error_handler(nw_protocol_metadata_t, nw_webtransport_receive_error_handler_t, dispatch_queue_t);
void nw_webtransport_metadata_set_remote_send_error_handler(nw_protocol_metadata_t, nw_webtransport_send_error_handler_t, dispatch_queue_t);

void nw_connection_abort_reads(nw_connection_t, uint64_t);
void nw_connection_abort_writes(nw_connection_t, uint64_t);

void nw_http_fields_access_value_by_name(nw_http_fields_t, const char*, nw_http_optional_string_accessor_t);

WTF_EXTERN_C_END

// ------------------------------------------------------------
// The following declarations are only needed for running tests.

WTF_EXTERN_C_BEGIN

typedef enum {
    nw_resolver_protocol_dns53 = 0,
} nw_resolver_protocol_t;

typedef enum {
    nw_resolver_class_designated_direct = 2,
} nw_resolver_class_t;

OS_OBJECT_RETURNS_RETAINED nw_resolver_config_t nw_resolver_config_create(void);
void nw_resolver_config_set_protocol(nw_resolver_config_t, nw_resolver_protocol_t);
void nw_resolver_config_set_class(nw_resolver_config_t, nw_resolver_class_t);
void nw_resolver_config_add_match_domain(nw_resolver_config_t, const char *);
void nw_resolver_config_add_name_server(nw_resolver_config_t, const char *name_server);
void nw_resolver_config_set_identifier(nw_resolver_config_t, const uuid_t identifier);
bool nw_resolver_config_publish(nw_resolver_config_t);
void nw_resolver_config_unpublish(nw_resolver_config_t);

WTF_EXTERN_C_END

#if defined(__OBJC__)
typedef NS_ENUM(NSInteger, NEPolicySessionPriority) {
    NEPolicySessionPriorityHigh = 300,
};

@interface NEPolicyCondition : NSObject
+ (NEPolicyCondition *)domain:(NSString *)domain;
@end

@interface NEPolicyResult : NSObject
+ (NEPolicyResult *)netAgentUUID:(NSUUID *)agentUUID;
@end

@interface NEPolicy : NSObject
- (instancetype)initWithOrder:(uint32_t)order result:(NEPolicyResult *)result conditions:(NSArray<NEPolicyCondition *> *)conditions;
@end

@interface NEPolicySession : NSObject
@property NEPolicySessionPriority priority;
- (NSUInteger)addPolicy:(NEPolicy *)policy;
- (BOOL)apply;
@end
#endif // defined(__OBJC__)
// ------------------------------------------------------------

#endif // !USE(APPLE_INTERNAL_SDK)
