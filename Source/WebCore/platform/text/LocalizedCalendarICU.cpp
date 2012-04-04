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

#include "config.h"
#include "LocalizedCalendar.h"

#if ENABLE(CALENDAR_PICKER)
#include "Language.h"
#include <unicode/ucal.h>
#include <unicode/udat.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/text/CString.h>

namespace WebCore {

class ScopedDateFormat {
public:
    ScopedDateFormat()
    {
        UErrorCode status = U_ZERO_ERROR;
        m_dateFormat = udat_open(UDAT_DEFAULT, UDAT_DEFAULT, defaultLanguage().utf8().data(), 0, -1, 0, -1, &status);
    }

    ~ScopedDateFormat()
    {
        if (m_dateFormat)
            udat_close(m_dateFormat);
    }

    UDateFormat* get() const { return m_dateFormat; }

private:
    UDateFormat* m_dateFormat;
};

static PassOwnPtr<Vector<String> > createFallbackMonthLabels()
{
    OwnPtr<Vector<String> > labels = adoptPtr(new Vector<String>());
    labels->reserveCapacity(12);
    labels->append("January");
    labels->append("February");
    labels->append("March");
    labels->append("April");
    labels->append("May");
    labels->append("June");
    labels->append("July");
    labels->append("August");
    labels->append("September");
    labels->append("October");
    labels->append("November");
    labels->append("December");
    return labels.release();
}

static PassOwnPtr<Vector<String> > createLabelVector(UDateFormatSymbolType type, int32_t size)
{
    ScopedDateFormat dateFormat;
    if (!dateFormat.get())
        return PassOwnPtr<Vector<String> >();
    if (udat_countSymbols(dateFormat.get(), type) != size)
        return PassOwnPtr<Vector<String> >();

    OwnPtr<Vector<String> > labels = adoptPtr(new Vector<String>());
    labels->reserveCapacity(size);
    for (int32_t i = 0; i < size; ++i) {
        UErrorCode status = U_ZERO_ERROR;
        int32_t length = udat_getSymbols(dateFormat.get(), type, i, 0, 0, &status);
        if (status != U_BUFFER_OVERFLOW_ERROR)
            return PassOwnPtr<Vector<String> >();
        Vector<UChar> buffer(length);
        status = U_ZERO_ERROR;
        udat_getSymbols(dateFormat.get(), type, i, buffer.data(), length, &status);
        if (U_FAILURE(status))
            return PassOwnPtr<Vector<String> >();
        labels->append(String::adopt(buffer));
    }
    return labels.release();
}

static PassOwnPtr<Vector<String> > createMonthLabels()
{
    OwnPtr<Vector<String> > labels = createLabelVector(UDAT_MONTHS, 12);
    return labels ? labels.release() : createFallbackMonthLabels();
}

const Vector<String>& monthLabels()
{
    static const Vector<String>* labels = createMonthLabels().leakPtr();
    return *labels;
}

static PassOwnPtr<Vector<String> > createFallbackWeekDayShortLabels()
{
    OwnPtr<Vector<String> > labels = adoptPtr(new Vector<String>());
    labels->reserveCapacity(7);
    labels->append("Sun");
    labels->append("Mon");
    labels->append("Tue");
    labels->append("Wed");
    labels->append("Thu");
    labels->append("Fri");
    labels->append("Sat");
    return labels.release();
}

static PassOwnPtr<Vector<String> > createWeekDayShortLabels()
{
    OwnPtr<Vector<String> > labels = createLabelVector(UDAT_SHORT_WEEKDAYS, 7);
    return labels ? labels.release() : createFallbackWeekDayShortLabels();
}

const Vector<String>& weekDayShortLabels()
{
    static const Vector<String>* labels = createWeekDayShortLabels().leakPtr();
    return *labels;
}

static unsigned getFirstDayOfWeek()
{
    ScopedDateFormat dateFormat;
    if (!dateFormat.get())
        return 0;
    unsigned firstDay = ucal_getAttribute(udat_getCalendar(dateFormat.get()), UCAL_FIRST_DAY_OF_WEEK);
    return firstDay;
}

unsigned firstDayOfWeek()
{
    static unsigned firstDay = getFirstDayOfWeek();
    return firstDay;
}

}
#endif
