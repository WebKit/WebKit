/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef LocaleICU_h
#define LocaleICU_h

#include "PlatformLocale.h"
#include <unicode/udat.h>
#include <unicode/unum.h>
#include <wtf/Forward.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

// We should use this class only for LocalizedNumberICU.cpp, LocalizedDateICU.cpp,
// and LocalizedNumberICUTest.cpp.
class LocaleICU : public Locale {
public:
    explicit LocaleICU(const char*);
    virtual ~LocaleICU();

#if ENABLE(DATE_AND_TIME_INPUT_TYPES)
    String dateFormat() override;
    String monthFormat() override;
    String shortMonthFormat() override;
    String timeFormat() override;
    String shortTimeFormat() override;
    String dateTimeFormatWithSeconds() override;
    String dateTimeFormatWithoutSeconds() override;
    const Vector<String>& monthLabels() override;
    const Vector<String>& shortMonthLabels() override;
    const Vector<String>& standAloneMonthLabels() override;
    const Vector<String>& shortStandAloneMonthLabels() override;
    const Vector<String>& timeAMPMLabels() override;
#endif

private:
#if !UCONFIG_NO_FORMATTING
    String decimalSymbol(UNumberFormatSymbol);
    String decimalTextAttribute(UNumberFormatTextAttribute);
#endif
    void initializeLocaleData() override;

#if ENABLE(DATE_AND_TIME_INPUT_TYPES)
    bool initializeShortDateFormat();
    UDateFormat* openDateFormat(UDateFormatStyle timeStyle, UDateFormatStyle dateStyle) const;

    std::unique_ptr<Vector<String>> createLabelVector(const UDateFormat*, UDateFormatSymbolType, int32_t startIndex, int32_t size);
    void initializeDateTimeFormat();
#endif

    CString m_locale;

#if !UCONFIG_NO_FORMATTING
    UNumberFormat* m_numberFormat { nullptr };
    bool m_didCreateDecimalFormat { false };
#endif

#if ENABLE(DATE_AND_TIME_INPUT_TYPES)
    std::unique_ptr<Vector<String>> m_monthLabels;
    String m_dateFormat;
    String m_monthFormat;
    String m_shortMonthFormat;
    String m_timeFormatWithSeconds;
    String m_timeFormatWithoutSeconds;
    String m_dateTimeFormatWithSeconds;
    String m_dateTimeFormatWithoutSeconds;
    UDateFormat* m_shortDateFormat { nullptr };
    UDateFormat* m_mediumTimeFormat { nullptr };
    UDateFormat* m_shortTimeFormat { nullptr };
    Vector<String> m_shortMonthLabels;
    Vector<String> m_standAloneMonthLabels;
    Vector<String> m_shortStandAloneMonthLabels;
    Vector<String> m_timeAMPMLabels;
    bool m_didCreateShortDateFormat { false };
    bool m_didCreateTimeFormat { false };
#endif
};

} // namespace WebCore
#endif
