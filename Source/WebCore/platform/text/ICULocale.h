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

#ifndef ICULocale_h
#define ICULocale_h

#include "DateComponents.h"
#include <unicode/udat.h>
#include <unicode/unum.h>
#include <wtf/Forward.h>
#include <wtf/OwnPtr.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

// We should use this class only for LocalizedNumberICU.cpp, LocalizedDateICU.cpp,
// LocalizedCalendarICU.cpp, and LocalizedNumberICUTest.cpp.
class ICULocale {
public:
    static PassOwnPtr<ICULocale> create(const char* localeString);
    static ICULocale* currentLocale();
    ~ICULocale();

    // For LocalizedNumber
    String convertToLocalizedNumber(const String&);
    String convertFromLocalizedNumber(const String&);

    // For LocalizedDate
    double parseLocalizedDate(const String&);
    String formatLocalizedDate(const DateComponents&);
#if ENABLE(CALENDAR_PICKER)
    String localizedDateFormatText();

    // For LocalizedCalendar
    const Vector<String>& monthLabels();
    const Vector<String>& weekDayShortLabels();
    unsigned firstDayOfWeek();
#endif

private:
    static PassOwnPtr<ICULocale> createForCurrentLocale();
    explicit ICULocale(const char*);
    void setDecimalSymbol(unsigned index, UNumberFormatSymbol);
    void setDecimalTextAttribute(String&, UNumberFormatTextAttribute);
    void initializeDecimalFormat();

    bool detectSignAndGetDigitRange(const String& input, bool& isNegative, unsigned& startIndex, unsigned& endIndex);
    unsigned matchedDecimalSymbolIndex(const String& input, unsigned& position);

    bool initializeShortDateFormat();

#if ENABLE(CALENDAR_PICKER)
    void initializeLocalizedDateFormatText();
    PassOwnPtr<Vector<String> > createLabelVector(UDateFormatSymbolType, int32_t startIndex, int32_t size);
    void initializeCalendar();
#endif

    CString m_locale;
    UNumberFormat* m_numberFormat;
    UDateFormat* m_shortDateFormat;
    enum {
        // 0-9 for digits.
        DecimalSeparatorIndex = 10,
        GroupSeparatorIndex = 11,
        DecimalSymbolsSize
    };
    String m_decimalSymbols[DecimalSymbolsSize];
    String m_positivePrefix;
    String m_positiveSuffix;
    String m_negativePrefix;
    String m_negativeSuffix;
    bool m_didCreateDecimalFormat;
    bool m_didCreateShortDateFormat;

#if ENABLE(CALENDAR_PICKER)
    String m_localizedDateFormatText;
    OwnPtr<Vector<String> > m_monthLabels;
    OwnPtr<Vector<String> > m_weekDayShortLabels;
    unsigned m_firstDayOfWeek;
#endif
};

}
#endif
