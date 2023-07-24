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

#include <libpsl.h>
#include <wtf/URL.h>

namespace WebCore {

bool isPublicSuffix(StringView domain)
{
    if (domain.isEmpty())
        return false;

    const psl_ctx_t* psl = psl_builtin();
    ASSERT(psl);
    bool ret = psl_is_public_suffix2(psl, domain.toStringWithoutCopying().convertToLowercaseWithoutLocale().utf8().data(), PSL_TYPE_ANY | PSL_TYPE_NO_STAR_RULE);
    return ret;
}

static String topPrivatelyControlledDomainInternal(const psl_ctx_t* psl, const char* domain)
{
    // psl_registerable_domain returns a pointer to domain's data or null if there is no private domain
    if (const char* topPrivateDomain = psl_registrable_domain(psl, domain))
        return String::fromLatin1(topPrivateDomain);
    return String();
}

String topPrivatelyControlledDomain(const String& domain)
{
    if (domain.isEmpty())
        return String();
    if (URL::hostIsIPAddress(domain) || !domain.containsOnlyASCII())
        return domain;

    String lowercaseDomain = domain.convertToASCIILowercase();
    if (lowercaseDomain == "localhost"_s)
        return lowercaseDomain;

    if (isPublicSuffix(lowercaseDomain))
        return String();

    // This function is expected to work with the format used by cookies, so skip any leading dots.
    auto domainUTF8 = lowercaseDomain.utf8();

    unsigned position = 0;
    while (domainUTF8.data()[position] == '.')
        position++;

    if (position == domainUTF8.length())
        return String();

    const psl_ctx_t* psl = psl_builtin();
    ASSERT(psl);
    return topPrivatelyControlledDomainInternal(psl, domainUTF8.data() + position);
}

void setTopPrivatelyControlledDomain(const String&, const String&)
{
}

} // namespace WebCore

#endif // ENABLE(PUBLIC_SUFFIX_LIST)
