/*
 * Copyright (C) 2015 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PublicSuffix.h"

#if ENABLE(PUBLIC_SUFFIX_LIST)

#include <libsoup/soup.h>
#include <wtf/glib/GUniquePtr.h>

namespace WebCore {

bool isPublicSuffix(StringView domain)
{
    if (domain.isEmpty())
        return false;

    return soup_tld_domain_is_public_suffix(domain.convertToASCIILowercase().utf8().data());
}

static String permissiveTopPrivateDomain(const String& domain)
{
    auto position = domain.length();
    bool foundDot = false;

    /* If a domain does not have a known public suffix we can just assume
     * the last pair of segments is probably the suffix.
     * Currently this is only used for web-platform.test. */
    while (position-- > 0) {
        if (domain[position] == '.') {
            if (foundDot)
                return domain.substring(position + 1);
            foundDot = true;
        }
    }

    return foundDot ? domain : String();
}

String topPrivatelyControlledDomain(const String& domain)
{
    if (domain.isEmpty())
        return String();
    if (!domain.isAllASCII())
        return domain;

    String lowercaseDomain = domain.convertToASCIILowercase();

    if (lowercaseDomain == "localhost"_s)
        return lowercaseDomain;

    CString domainUTF8 = lowercaseDomain.utf8();

    // This function is expected to work with the format used by cookies, so skip any leading dots.
    unsigned position = 0;
    while (domainUTF8.data()[position] == '.')
        position++;

    if (position == domainUTF8.length())
        return String();

    GUniqueOutPtr<GError> error;
    if (const char* baseDomain = soup_tld_get_base_domain(domainUTF8.data() + position, &error.outPtr()))
        return String::fromUTF8(baseDomain);

    if (g_error_matches(error.get(), SOUP_TLD_ERROR, SOUP_TLD_ERROR_NO_BASE_DOMAIN)) {
        if (isDomainForTesting(domain))
            return permissiveTopPrivateDomain(domain.substring(position));
        return String();
    }

    if (g_error_matches(error.get(), SOUP_TLD_ERROR, SOUP_TLD_ERROR_INVALID_HOSTNAME) || g_error_matches(error.get(), SOUP_TLD_ERROR, SOUP_TLD_ERROR_NOT_ENOUGH_DOMAINS))
        return String();

    if (g_error_matches(error.get(), SOUP_TLD_ERROR, SOUP_TLD_ERROR_IS_IP_ADDRESS))
        return domain;

    ASSERT_NOT_REACHED();
    return String();
}

void setTopPrivatelyControlledDomain(const String&, const String&)
{
}

} // namespace WebCore

#endif // ENABLE(PUBLIC_SUFFIX_LIST)
