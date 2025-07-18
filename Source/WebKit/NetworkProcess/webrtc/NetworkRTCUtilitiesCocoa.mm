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
#import "NetworkRTCUtilitiesCocoa.h"

#if USE(LIBWEBRTC)

#import <WebCore/RegistrableDomain.h>
#import <wtf/SoftLinking.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/text/WTFString.h>

SOFT_LINK_LIBRARY(libnetworkextension)
SOFT_LINK_CLASS(libnetworkextension, NEHelperTrackerDisposition_t)
SOFT_LINK_CLASS(libnetworkextension, NEHelperTrackerAppInfoRef)
SOFT_LINK_CLASS(libnetworkextension, NEHelperTrackerDomainContextRef)
SOFT_LINK(libnetworkextension, NEHelperTrackerGetDisposition, NEHelperTrackerDisposition_t*, (NEHelperTrackerAppInfoRef *app_info_ref, CFArrayRef domains, NEHelperTrackerDomainContextRef *trackerDomainContextRef, CFIndex *trackerDomainIndex), (app_info_ref, domains, trackerDomainContextRef, trackerDomainIndex))

SOFT_LINK_LIBRARY_OPTIONAL(libnetwork)
SOFT_LINK_OPTIONAL(libnetwork, nw_parameters_set_attributed_bundle_identifier, void, __cdecl, (nw_parameters_t, const char*))

namespace WebKit {

void setNWParametersApplicationIdentifiers(nw_parameters_t parameters, const char* sourceApplicationBundleIdentifier, std::optional<audit_token_t> sourceApplicationAuditToken, const String& attributedBundleIdentifier)
{
    if (sourceApplicationBundleIdentifier && *sourceApplicationBundleIdentifier)
        nw_parameters_set_source_application_by_bundle_id(parameters, sourceApplicationBundleIdentifier);
    else if (sourceApplicationAuditToken)
        nw_parameters_set_source_application(parameters, *sourceApplicationAuditToken);

    if (!attributedBundleIdentifier.isEmpty() && nw_parameters_set_attributed_bundle_identifierPtr())
        nw_parameters_set_attributed_bundle_identifierPtr()(parameters, attributedBundleIdentifier.utf8().data());
}

void setNWParametersTrackerOptions(nw_parameters_t parameters, bool shouldBypassRelay, bool isFirstParty, bool isKnownTracker)
{
    if (shouldBypassRelay)
        nw_parameters_set_account_id(parameters, "com.apple.safari.peertopeer");
    nw_parameters_set_is_third_party_web_content(parameters, !isFirstParty);
    nw_parameters_set_is_known_tracker(parameters, isKnownTracker);
}

bool isKnownTracker(const WebCore::RegistrableDomain& domain)
{
    RetainPtr domains = @[domain.string().createNSString().get()];
    NEHelperTrackerDomainContextRef *context = nil;
    CFIndex index = 0;
    return !!NEHelperTrackerGetDisposition(nullptr, bridge_cast(domains.get()), context, &index);
}

std::optional<uint32_t> trafficClassFromDSCP(webrtc::DiffServCodePoint dscpValue)
{
    switch (dscpValue) {
    case webrtc::DiffServCodePoint::DSCP_NO_CHANGE:
        return { };
    case webrtc::DiffServCodePoint::DSCP_CS0:
        return SO_TC_BE;
    case webrtc::DiffServCodePoint::DSCP_CS1:
        return SO_TC_BK_SYS;
    case webrtc::DiffServCodePoint::DSCP_AF41:
        return SO_TC_VI;
    case webrtc::DiffServCodePoint::DSCP_AF42:
        return SO_TC_VI;
    case webrtc::DiffServCodePoint::DSCP_EF:
        return SO_TC_VO;
    case webrtc::DiffServCodePoint::DSCP_AF11:
    case webrtc::DiffServCodePoint::DSCP_AF12:
    case webrtc::DiffServCodePoint::DSCP_AF13:
    case webrtc::DiffServCodePoint::DSCP_CS2:
    case webrtc::DiffServCodePoint::DSCP_AF21:
    case webrtc::DiffServCodePoint::DSCP_AF22:
    case webrtc::DiffServCodePoint::DSCP_AF23:
    case webrtc::DiffServCodePoint::DSCP_CS3:
    case webrtc::DiffServCodePoint::DSCP_AF31:
    case webrtc::DiffServCodePoint::DSCP_AF32:
    case webrtc::DiffServCodePoint::DSCP_AF33:
    case webrtc::DiffServCodePoint::DSCP_CS4:
    case webrtc::DiffServCodePoint::DSCP_AF43:
    case webrtc::DiffServCodePoint::DSCP_CS5:
    case webrtc::DiffServCodePoint::DSCP_CS6:
    case webrtc::DiffServCodePoint::DSCP_CS7:
        break;
    };
    return { };
}

} // namespace WebKit

#endif // USE(LIBWEBRTC)
