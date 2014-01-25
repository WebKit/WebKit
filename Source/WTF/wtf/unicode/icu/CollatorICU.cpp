/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include <wtf/unicode/Collator.h>

#if !UCONFIG_NO_COLLATION

#include <mutex>
#include <wtf/Assertions.h>
#include <wtf/StringExtras.h>
#include <unicode/ucol.h>
#include <string.h>

#if OS(DARWIN)
#include <wtf/RetainPtr.h>
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace WTF {

static UCollator* cachedCollator;

static std::mutex& cachedCollatorMutex()
{
    static std::once_flag onceFlag;
    static std::mutex* mutex;
    std::call_once(onceFlag, []{
        mutex = std::make_unique<std::mutex>().release();
    });

    return *mutex;
}

Collator::Collator(const char* locale)
    : m_collator(0)
    , m_locale(locale ? strdup(locale) : 0)
    , m_lowerFirst(false)
{
}

std::unique_ptr<Collator> Collator::userDefault()
{
#if OS(DARWIN) && USE(CF)
    // Mac OS X doesn't set UNIX locale to match user-selected one, so ICU default doesn't work.
#if !OS(IOS)
    RetainPtr<CFLocaleRef> currentLocale = adoptCF(CFLocaleCopyCurrent());
    CFStringRef collationOrder = (CFStringRef)CFLocaleGetValue(currentLocale.get(), kCFLocaleCollatorIdentifier);
#else
    RetainPtr<CFStringRef> collationOrderRetainer = adoptCF((CFStringRef)CFPreferencesCopyValue(CFSTR("AppleCollationOrder"), kCFPreferencesAnyApplication, kCFPreferencesCurrentUser, kCFPreferencesAnyHost));
    CFStringRef collationOrder = collationOrderRetainer.get();
#endif
    char buf[256];
    if (!collationOrder)
        return std::make_unique<Collator>("");
    CFStringGetCString(collationOrder, buf, sizeof(buf), kCFStringEncodingASCII);
    return std::make_unique<Collator>(buf);
#else
    return std::make_unique<Collator>(static_cast<const char*>(0));
#endif
}

Collator::~Collator()
{
    releaseCollator();
    free(m_locale);
}

void Collator::setOrderLowerFirst(bool lowerFirst)
{
    m_lowerFirst = lowerFirst;
}

Collator::Result Collator::collate(const UChar* lhs, size_t lhsLength, const UChar* rhs, size_t rhsLength) const
{
    if (!m_collator)
        createCollator();

    return static_cast<Result>(ucol_strcoll(m_collator, lhs, lhsLength, rhs, rhsLength));
}

void Collator::createCollator() const
{
    ASSERT(!m_collator);
    UErrorCode status = U_ZERO_ERROR;

    {
        std::lock_guard<std::mutex> lock(cachedCollatorMutex());
        if (cachedCollator) {
            const char* cachedCollatorLocale = ucol_getLocaleByType(cachedCollator, ULOC_REQUESTED_LOCALE, &status);
            ASSERT(U_SUCCESS(status));
            ASSERT(cachedCollatorLocale);

            UColAttributeValue cachedCollatorLowerFirst = ucol_getAttribute(cachedCollator, UCOL_CASE_FIRST, &status);
            ASSERT(U_SUCCESS(status));

            // FIXME: default locale is never matched, because ucol_getLocaleByType returns the actual one used, not 0.
            if (m_locale && 0 == strcmp(cachedCollatorLocale, m_locale)
                && ((UCOL_LOWER_FIRST == cachedCollatorLowerFirst && m_lowerFirst) || (UCOL_UPPER_FIRST == cachedCollatorLowerFirst && !m_lowerFirst))) {
                m_collator = cachedCollator;
                cachedCollator = nullptr;
                return;
            }
        }
    }

    m_collator = ucol_open(m_locale, &status);
    if (U_FAILURE(status)) {
        status = U_ZERO_ERROR;
        m_collator = ucol_open("", &status); // Fallback to Unicode Collation Algorithm.
    }
    ASSERT(U_SUCCESS(status));

    ucol_setAttribute(m_collator, UCOL_CASE_FIRST, m_lowerFirst ? UCOL_LOWER_FIRST : UCOL_UPPER_FIRST, &status);
    ASSERT(U_SUCCESS(status));

    ucol_setAttribute(m_collator, UCOL_NORMALIZATION_MODE, UCOL_ON, &status);
    ASSERT(U_SUCCESS(status));
}

void Collator::releaseCollator()
{
    {
        std::lock_guard<std::mutex> lock(cachedCollatorMutex());
        if (cachedCollator)
            ucol_close(cachedCollator);
        cachedCollator = m_collator;
        m_collator = nullptr;
    }
}

} // namespace WTF

#endif
