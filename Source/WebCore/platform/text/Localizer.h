/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef Localizer_h
#define Localizer_h

#include <wtf/text/WTFString.h>

namespace WebCore {

class Localizer {
    WTF_MAKE_NONCOPYABLE(Localizer);

public:
    static PassOwnPtr<Localizer> create(const AtomicString&);
    String convertToLocalizedNumber(const String&);
    String convertFromLocalizedNumber(const String&);
#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
    String localizedDecimalSeparator();

    // Returns time format in Unicode TR35 LDML[1] containing hour, minute, and
    // second with optional period(AM/PM), e.g. "h:mm:ss a"
    // [1] LDML http://unicode.org/reports/tr35/tr35-6.html#Date_Format_Patterns
    virtual String timeFormat();

    // Returns time format in Unicode TR35 LDML containing hour, and minute
    // with optional period(AM/PM), e.g. "h:mm a"
    // Note: Some platforms return same value as timeFormat().
    virtual String shortTimeFormat();

    // Returns localized period field(AM/PM) strings.
    virtual const Vector<String>& timeAMPMLabels();
#endif

#if ENABLE(CALENDAR_PICKER)
    // Returns a vector of string of which size is 12. The first item is a
    // localized string of January, and the last item is a localized string of
    // December. These strings should not be abbreviations.
    virtual const Vector<String>& monthLabels() = 0;

    // Returns a vector of string of which size is 7. The first item is a
    // localized short string of Monday, and the last item is a localized
    // short string of Saturday. These strings should be short.
    virtual const Vector<String>& weekDayShortLabels() = 0;

    // The first day of a week. 0 is Sunday, and 6 is Saturday.
    virtual unsigned firstDayOfWeek() = 0;
#endif

    virtual ~Localizer();

protected:
    enum {
        // 0-9 for digits.
        DecimalSeparatorIndex = 10,
        GroupSeparatorIndex = 11,
        DecimalSymbolsSize
    };

#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
    String m_localizedTimeFormatText;
    String m_localizedShortTimeFormatText;
    Vector<String> m_timeAMPMLabels;
#endif

    Localizer() : m_hasLocalizerData(false) { }
    virtual void initializeLocalizerData() = 0;
    void setLocalizerData(const Vector<String, DecimalSymbolsSize>&, const String& positivePrefix, const String& positiveSuffix, const String& negativePrefix, const String& negativeSuffix);

private:
    bool detectSignAndGetDigitRange(const String& input, bool& isNegative, unsigned& startIndex, unsigned& endIndex);
    unsigned matchedDecimalSymbolIndex(const String& input, unsigned& position);

    String m_decimalSymbols[DecimalSymbolsSize];
    String m_positivePrefix;
    String m_positiveSuffix;
    String m_negativePrefix;
    String m_negativeSuffix;
    bool m_hasLocalizerData;
};

}
#endif
