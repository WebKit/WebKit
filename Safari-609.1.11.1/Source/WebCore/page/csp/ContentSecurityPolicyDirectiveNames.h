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

#pragma once

namespace WebCore {

namespace ContentSecurityPolicyDirectiveNames {

extern const char* const baseURI;
extern const char* const childSrc;
extern const char* const connectSrc;
extern const char* const defaultSrc;
extern const char* const fontSrc;
extern const char* const formAction;
extern const char* const frameAncestors;
extern const char* const frameSrc;
extern const char* const imgSrc;
#if ENABLE(APPLICATION_MANIFEST)
extern const char* const manifestSrc;
#endif
extern const char* const mediaSrc;
extern const char* const objectSrc;
extern const char* const pluginTypes;
extern const char* const reportURI;
extern const char* const sandbox;
extern const char* const scriptSrc;
extern const char* const styleSrc;
extern const char* const upgradeInsecureRequests;
extern const char* const blockAllMixedContent;

} // namespace ContentSecurityPolicyDirectiveNames

} // namespace WebCore

