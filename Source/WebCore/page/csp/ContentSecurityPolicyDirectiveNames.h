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

#pragma once

#include <wtf/Forward.h>

namespace WebCore {

namespace ContentSecurityPolicyDirectiveNames {

extern ASCIILiteral baseURI;
extern ASCIILiteral childSrc;
extern ASCIILiteral connectSrc;
extern ASCIILiteral defaultSrc;
extern ASCIILiteral fontSrc;
extern ASCIILiteral formAction;
extern ASCIILiteral frameAncestors;
extern ASCIILiteral frameSrc;
extern ASCIILiteral imgSrc;
#if ENABLE(APPLICATION_MANIFEST)
extern ASCIILiteral manifestSrc;
#endif
extern ASCIILiteral mediaSrc;
extern ASCIILiteral objectSrc;
extern ASCIILiteral pluginTypes;
extern ASCIILiteral prefetchSrc;
extern ASCIILiteral reportURI;
extern ASCIILiteral reportTo;
extern ASCIILiteral sandbox;
extern ASCIILiteral scriptSrc;
extern ASCIILiteral scriptSrcElem;
extern ASCIILiteral scriptSrcAttr;
extern ASCIILiteral styleSrc;
extern ASCIILiteral styleSrcAttr;
extern ASCIILiteral styleSrcElem;
extern ASCIILiteral upgradeInsecureRequests;
extern ASCIILiteral blockAllMixedContent;
extern ASCIILiteral workerSrc;

} // namespace ContentSecurityPolicyDirectiveNames

} // namespace WebCore

