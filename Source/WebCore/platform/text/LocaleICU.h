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

#include "DateComponents.h"
#include "Localizer.h"
#include <unicode/udat.h>
#include <unicode/unum.h>
#include <wtf/Forward.h>
#include <wtf/OwnPtr.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

// We should use this class only for LocalizedNumberICU.cpp, LocalizedDateICU.cpp,
// and LocalizedNumberICUTest.cpp.
class LocaleICU : public Localizer {
public:
    static PassOwnPtr<LocaleICU> create(const char* localeString);
    static LocaleICU* currentLocale();
    virtual ~LocaleICU();

    // For LocalizedDate
    virtual double parseDateTime(const String&, DateComponents::Type) OVERRIDE;
    virtual String formatDateTime(const DateComponents&, FormatType = FormatTypeUnspecified) OVERRIDE;
#if ENABLE(CALENDAR_PICKER)
    virtual String dateFormatText() OVERRIDE;

    virtual const Vector<String>& monthLabels() OVERRIDE;
    virtual const Vector<String>& weekDayShortLabels() OVERRIDE;
    virtual unsigned firstDayOfWeek() OVERRIDE;
    virtual bool isRTL() OVERRIDE;
#endif

#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
    virtual String dateFormat() OVERRIDE;
    virtual String timeFormat() OVERRIDE;
    virtual String shortTimeFormat() OVERRIDE;
    virtual const Vector<String>& timeAMPMLabels() OVERRIDE;
#endif

private:
    static PassOwnPtr<LocaleICU> createForCurrentLocale();
    explicit LocaleICU(const char*);
    String decimalSymbol(UNumberFormatSymbol);
    String decimalTextAttribute(UNumberFormatTextAttribute);
    virtual void initializeLocalizerData() OVERRIDE;

    bool detectSignAndGetDigitRange(const String& input, bool& isNegative, unsigned& startIndex, unsigned& endIndex);
    unsigned matchedDecimalSymbolIndex(const String& input, unsigned& position);

    bool initializeShortDateFormat();
    UDateFormat* openDateFormat(UDateFormatStyle timeStyle, UDateFormatStyle dateStyle) const;

#if ENABLE(CALENDAR_PICKER)
    void initializeLocalizedDateFormatText();
    void initializeCalendar();
#endif

#if ENABLE(CALENDAR_PICKER) || ENABLE(INPUT_MULTIPLE_FIELDS_UI)
    PassOwnPtr<Vector<String> > createLabelVector(const UDateFormat*, UDateFormatSymbolType, int32_t startIndex, int32_t size);
#endif

#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
    void initializeDateTimeFormat();
#endif

    CString m_locale;
    UNumberFormat* m_numberFormat;
    UDateFormat* m_shortDateFormat;
    bool m_didCreateDecimalFormat;
    bool m_didCreateShortDateFormat;

#if ENABLE(CALENDAR_PICKER)
    String m_localizedDateFormatText;
    OwnPtr<Vector<String> > m_monthLabels;
    OwnPtr<Vector<String> > m_weekDayShortLabels;
    unsigned m_firstDayOfWeek;
#endif

#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
    String m_dateFormat;
    UDateFormat* m_mediumTimeFormat;
    UDateFormat* m_shortTimeFormat;
    Vector<String> m_timeAMPMLabels;
    bool m_didCreateTimeFormat;
#endif
};

} // namespace WebCore
#endif
