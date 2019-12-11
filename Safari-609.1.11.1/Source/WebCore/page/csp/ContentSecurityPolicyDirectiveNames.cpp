/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "config.h"
#include "ContentSecurityPolicyDirectiveNames.h"

namespace WebCore {

namespace ContentSecurityPolicyDirectiveNames {

const char* const baseURI = "base-uri";
const char* const childSrc = "child-src";
const char* const connectSrc = "connect-src";
const char* const defaultSrc = "default-src";
const char* const fontSrc = "font-src";
const char* const formAction = "form-action";
const char* const frameAncestors = "frame-ancestors";
const char* const frameSrc = "frame-src";
#if ENABLE(APPLICATION_MANIFEST)
const char* const manifestSrc = "manifest-src";
#endif
const char* const imgSrc = "img-src";
const char* const mediaSrc = "media-src";
const char* const objectSrc = "object-src";
const char* const pluginTypes = "plugin-types";
const char* const reportURI = "report-uri";
const char* const sandbox = "sandbox";
const char* const scriptSrc = "script-src";
const char* const styleSrc = "style-src";
const char* const upgradeInsecureRequests = "upgrade-insecure-requests";
const char* const blockAllMixedContent = "block-all-mixed-content";
    
} // namespace ContentSecurityPolicyDirectiveNames

} // namespace WebCore
