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

#import <wtf/SoftLinking.h>
#import <wtf/text/WTFString.h>

#if HAVE(NWPARAMETERS_TRACKER_API)
SOFT_LINK_LIBRARY_OPTIONAL(libnetwork)
SOFT_LINK_OPTIONAL(libnetwork, nw_parameters_set_attributed_bundle_identifier, void, __cdecl, (nw_parameters_t, const char*))
#endif

namespace WebKit {

void setNWParametersApplicationIdentifiers(nw_parameters_t parameters, const char* sourceApplicationBundleIdentifier, std::optional<audit_token_t> sourceApplicationAuditToken, const String& attributedBundleIdentifier)
{
    if (sourceApplicationBundleIdentifier && *sourceApplicationBundleIdentifier)
        nw_parameters_set_source_application_by_bundle_id(parameters, sourceApplicationBundleIdentifier);
    else if (sourceApplicationAuditToken)
        nw_parameters_set_source_application(parameters, *sourceApplicationAuditToken);

#if HAVE(NWPARAMETERS_TRACKER_API)
    if (!attributedBundleIdentifier.isEmpty() && nw_parameters_set_attributed_bundle_identifierPtr())
        nw_parameters_set_attributed_bundle_identifierPtr()(parameters, attributedBundleIdentifier.utf8().data());
#endif
}

} // namespace WebKit

#endif // USE(LIBWEBRTC)
