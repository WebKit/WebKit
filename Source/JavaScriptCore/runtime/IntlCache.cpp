/*
 * Copyright (C) 2020-2023 Apple Inc. All rights reserved.
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
#include "IntlCache.h"

#include <wtf/TZoneMallocInlines.h>
#include <wtf/Vector.h>

namespace JSC {

WTF_MAKE_TZONE_ALLOCATED_IMPL(IntlCache);

UDateTimePatternGenerator* IntlCache::cacheSharedPatternGenerator(const CString& locale, UErrorCode& status)
{
    auto generator = std::unique_ptr<UDateTimePatternGenerator, ICUDeleter<udatpg_close>>(udatpg_open(locale.data(), &status));
    if (U_FAILURE(status))
        return nullptr;
    m_cachedDateTimePatternGeneratorLocale = locale;
    m_cachedDateTimePatternGenerator = WTFMove(generator);
    return m_cachedDateTimePatternGenerator.get();
}

Vector<UChar, 32> IntlCache::getBestDateTimePattern(const CString& locale, const UChar* skeleton, unsigned skeletonSize, UErrorCode& status)
{
    // Always use ICU date format generator, rather than our own pattern list and matcher.
    auto sharedGenerator = getSharedPatternGenerator(locale, status);
    if (U_FAILURE(status))
        return { };
    Vector<UChar, 32> patternBuffer;
    status = callBufferProducingFunction(udatpg_getBestPatternWithOptions, sharedGenerator, skeleton, skeletonSize, UDATPG_MATCH_HOUR_FIELD_LENGTH, patternBuffer);
    if (U_FAILURE(status))
        return { };
    return patternBuffer;
}

Vector<UChar, 32> IntlCache::getFieldDisplayName(const CString& locale, UDateTimePatternField field, UDateTimePGDisplayWidth width, UErrorCode& status)
{
    auto sharedGenerator = getSharedPatternGenerator(locale, status);
    if (U_FAILURE(status))
        return { };
    Vector<UChar, 32> buffer;
    status = callBufferProducingFunction(udatpg_getFieldDisplayName, sharedGenerator, field, width, buffer);
    if (U_FAILURE(status))
        return { };
    return buffer;
}

} // namespace JSC
