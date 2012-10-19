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

#include "DateComponents.h"
#include "Language.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class Localizer {
    WTF_MAKE_NONCOPYABLE(Localizer);

public:
    static PassOwnPtr<Localizer> create(const AtomicString& localeIdentifier);
    static PassOwnPtr<Localizer> createDefault();

    // Converts the specified number string to another number string localized
    // for this Localizer locale. The input string must conform to HTML
    // floating-point numbers, and is not empty.
    String convertToLocalizedNumber(const String&);

    // Converts the specified localized number string to a number string in the
    // HTML floating-point number format. The input string is provided by a end
    // user, and might not be a number string. It's ok that the function returns
    // a string which is not conforms to the HTML floating-point number format,
    // callers of this function are responsible to check the format of the
    // resultant string.
    String convertFromLocalizedNumber(const String&);

#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
    // Returns localized decimal separator, e.g. "." for English, "," for French.
    String localizedDecimalSeparator();

    // Returns date format in Unicode TR35 LDML[1] containing day of month,
    // month, and year, e.g. "dd/mm/yyyy"
    // [1] LDML http://unicode.org/reports/tr35/#Date_Format_Patterns
    virtual String dateFormat() = 0;

    // Returns a year-month format in Unicode TR35 LDML.
    virtual String monthFormat() = 0;

    // Returns time format in Unicode TR35 LDML[1] containing hour, minute, and
    // second with optional period(AM/PM), e.g. "h:mm:ss a"
    // [1] LDML http://unicode.org/reports/tr35/#Date_Format_Patterns
    virtual String timeFormat();

    // Returns time format in Unicode TR35 LDML containing hour, and minute
    // with optional period(AM/PM), e.g. "h:mm a"
    // Note: Some platforms return same value as timeFormat().
    virtual String shortTimeFormat();

    // Returns a date-time format in Unicode TR35 LDML. It should have a seconds
    // field.
    String dateTimeFormatWithSeconds();

    // Returns a date-time format in Unicode TR35 LDML. It should have no seconds
    // field.
    String dateTimeFormatWithoutSeconds();

    // Returns a vector of string of which size is 12. The first item is a
    // localized string of Jan and the last item is a localized string of
    // Dec. These strings should be short.
    virtual const Vector<String>& shortMonthLabels() = 0;

    // Stand-alone month version of shortMonthLabels.
    virtual const Vector<String>& shortStandAloneMonthLabels() = 0;

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

    virtual String dateFormatText() = 0;

    // Returns true if people use right-to-left writing in the locale for this
    // object.
    virtual bool isRTL() = 0;
#endif

    // Parses a string representation of a date/time string localized
    // for this Localizer locale. If the input string is not valid or
    // an implementation doesn't support localized dates, this
    // function returns NaN. If the input string is valid this
    // function returns the number of milliseconds since 1970-01-01
    // 00:00:00.000 UTC.
    virtual double parseDateTime(const String&, DateComponents::Type) = 0;

    enum FormatType { FormatTypeUnspecified, FormatTypeShort, FormatTypeMedium };

    // Serializes the specified date into a formatted date string to
    // display to the user. If an implementation doesn't support
    // localized dates the function should return an empty string.
    String formatDateTime(const DateComponents&, FormatType = FormatTypeUnspecified);

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
    String m_dateTimeFormatWithSeconds;
    String m_dateTimeFormatWithoutSeconds;
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

inline PassOwnPtr<Localizer> Localizer::createDefault()
{
    return Localizer::create(defaultLanguage());
}

}
#endif
