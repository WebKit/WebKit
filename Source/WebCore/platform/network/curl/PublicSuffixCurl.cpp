/*
 * Copyright (C) 2018 Sony Interactive Entertainment Inc.
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
#include "PublicSuffix.h"

#if ENABLE(PUBLIC_SUFFIX_LIST)

#include "URL.h"
#include <libpsl.h>

namespace WebCore {

bool isPublicSuffix(const String& domain)
{
    if (domain.isEmpty())
        return false;

    const psl_ctx_t* psl = psl_builtin();
    ASSERT(psl);
    bool ret = psl_is_public_suffix2(psl, domain.convertToLowercaseWithoutLocale().utf8().data(), PSL_TYPE_ANY | PSL_TYPE_NO_STAR_RULE);
    return ret;
}

static String topPrivatelyControlledDomainInternal(const psl_ctx_t* psl, const char* domain)
{
    // psl_registerable_domain returns a pointer to domain's data or null if there is no private domain
    if (const char* topPrivateDomain = psl_registrable_domain(psl, domain))
        return topPrivateDomain;
    return String();
}

String topPrivatelyControlledDomain(const String& domain)
{
    if (URL::hostIsIPAddress(domain) || !domain.isAllASCII())
        return domain;

    String lowercaseDomain = domain.convertToASCIILowercase();
    if (lowercaseDomain == "localhost")
        return lowercaseDomain;

    if (isPublicSuffix(lowercaseDomain))
        return String();

    const psl_ctx_t* psl = psl_builtin();
    ASSERT(psl);
    return topPrivatelyControlledDomainInternal(psl, lowercaseDomain.utf8().data());
}

} // namespace WebCore

#endif // ENABLE(PUBLIC_SUFFIX_LIST)
