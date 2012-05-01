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

#ifndef LocaleWin_h
#define LocaleWin_h

#include <windows.h>
#include <wtf/Forward.h>
#include <wtf/Vector.h>

namespace WebCore {

class DateComponents;
struct DateFormatToken;

class LocaleWin {
public:
    static PassOwnPtr<LocaleWin> create(LCID);
    static LocaleWin* currentLocale();
    ~LocaleWin();
    double parseDate(const String&);
    String formatDate(const DateComponents&);
#if ENABLE(CALENDAR_PICKER)
    String dateFormatText();
    const Vector<String>& monthLabels();
    const Vector<String>& weekDayShortLabels();
    unsigned firstDayOfWeek() { return m_firstDayOfWeek; }
#endif

    // For testing.
    double parseDate(const String& format, int baseYear, const String& input);
    String formatDate(const String& format, int baseYear, int year, int month, int day);
    static String dateFormatText(const String& format, const String& yearText, const String& monthText, const String& dayText);

private:
    explicit LocaleWin(LCID);
    String getLocaleInfoString(LCTYPE);
    void ensureShortMonthLabels();
    void ensureMonthLabels();
    void ensureShortDateTokens();
    int parseNumberOrMonth(const String&, unsigned& index);
    double parseDate(const Vector<DateFormatToken>&, int baseYear, const String&);
    String formatDate(const Vector<DateFormatToken>&, int baseYear, int year, int month, int day);
#if ENABLE(CALENDAR_PICKER)
    void ensureWeekDayShortLabels();
#endif

    LCID m_lcid;
    int m_baseYear;
    Vector<DateFormatToken> m_shortDateTokens;
    Vector<String> m_shortMonthLabels;
    Vector<String> m_monthLabels;
#if ENABLE(CALENDAR_PICKER)
    Vector<String> m_weekDayShortLabels;
    unsigned m_firstDayOfWeek;
#endif

};

}
#endif
