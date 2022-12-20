/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#import <Network/Network.h>

#if USE(APPLE_INTERNAL_SDK)

#import <nw/private.h>

#else

WTF_EXTERN_C_BEGIN

void nw_parameters_set_account_id(nw_parameters_t, const char * account_id);
void nw_parameters_set_source_application(nw_parameters_t, audit_token_t);
void nw_parameters_set_source_application_by_bundle_id(nw_parameters_t, const char*);
void nw_parameters_set_attributed_bundle_identifier(nw_parameters_t, const char*);
nw_endpoint_t nw_endpoint_create_host_with_numeric_port(const char* hostname, uint16_t port_host_order);
bool nw_nat64_does_interface_index_support_nat64(uint32_t ifindex);

#if HAVE(NWPARAMETERS_TRACKER_API)
void nw_parameters_set_is_third_party_web_content(nw_parameters_t, bool is_third_party_web_content);
void nw_parameters_set_is_known_tracker(nw_parameters_t, bool is_known_tracker);
void nw_parameters_allow_sharing_port_with_listener(nw_parameters_t, nw_listener_t);
#endif // HAVE(NWPARAMETERS_TRACKER_API)

WTF_EXTERN_C_END

#endif // USE(APPLE_INTERNAL_SDK)
