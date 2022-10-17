/*
 * Copyright (C) 2016-2022 Apple Inc. All rights reserved.
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

#include <wtf/text/ASCIILiteral.h>

namespace WebCore {

namespace ContentSecurityPolicyDirectiveNames {

ASCIILiteral baseURI = "base-uri"_s;
ASCIILiteral childSrc = "child-src"_s;
ASCIILiteral connectSrc = "connect-src"_s;
ASCIILiteral defaultSrc = "default-src"_s;
ASCIILiteral fontSrc = "font-src"_s;
ASCIILiteral formAction = "form-action"_s;
ASCIILiteral frameAncestors = "frame-ancestors"_s;
ASCIILiteral frameSrc = "frame-src"_s;
#if ENABLE(APPLICATION_MANIFEST)
ASCIILiteral manifestSrc = "manifest-src"_s;
#endif
ASCIILiteral imgSrc = "img-src"_s;
ASCIILiteral mediaSrc = "media-src"_s;
ASCIILiteral objectSrc = "object-src"_s;
ASCIILiteral pluginTypes = "plugin-types"_s;
ASCIILiteral prefetchSrc = "prefetch-src"_s;
ASCIILiteral reportTo = "report-to"_s;
ASCIILiteral reportURI = "report-uri"_s;
ASCIILiteral sandbox = "sandbox"_s;
ASCIILiteral scriptSrc = "script-src"_s;
ASCIILiteral scriptSrcAttr = "script-src-attr"_s;
ASCIILiteral scriptSrcElem = "script-src-elem"_s;
ASCIILiteral styleSrc = "style-src"_s;
ASCIILiteral styleSrcAttr = "style-src-attr"_s;
ASCIILiteral styleSrcElem = "style-src-elem"_s;
ASCIILiteral upgradeInsecureRequests = "upgrade-insecure-requests"_s;
ASCIILiteral blockAllMixedContent = "block-all-mixed-content"_s;
ASCIILiteral workerSrc = "worker-src"_s;

} // namespace ContentSecurityPolicyDirectiveNames

} // namespace WebCore
