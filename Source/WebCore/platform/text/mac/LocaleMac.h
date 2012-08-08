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

#ifndef LocaleMac_h
#define LocaleMac_h

#include "NumberLocalizer.h"
#include <wtf/Forward.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

OBJC_CLASS NSDateFormatter;
OBJC_CLASS NSLocale;

namespace WebCore {

class DateComponents;

class LocaleMac : public NumberLocalizer {
public:
    static PassOwnPtr<LocaleMac> create(const String&);
    static LocaleMac* currentLocale();
    ~LocaleMac();
    double parseDate(const String&);
    String formatDate(const DateComponents&);

#if ENABLE(CALENDAR_PICKER)
    String dateFormatText();
    const Vector<String>& monthLabels();
    const Vector<String>& weekDayShortLabels();
    unsigned firstDayOfWeek();
#endif

#if ENABLE(INPUT_TYPE_TIME_MULTIPLE_FIELDS)
    String timeFormatText();
    String shortTimeFormatText();
    const Vector<String>& timeAMPMLabels();
#endif

private:
    explicit LocaleMac(NSLocale*);
    explicit LocaleMac(const String&);
    NSDateFormatter *createShortDateFormatter();
    virtual void initializeNumberLocalizerData() OVERRIDE;

    RetainPtr<NSLocale> m_locale;
#if ENABLE(CALENDAR_PICKER)
    String m_localizedDateFormatText;
    Vector<String> m_monthLabels;
    Vector<String> m_weekDayShortLabels;
#endif
#if ENABLE(INPUT_TYPE_TIME_MULTIPLE_FIELDS)
    NSDateFormatter *createTimeFormatter();
    NSDateFormatter *createShortTimeFormatter();

    String m_localizedTimeFormatText;
    String m_localizedShortTimeFormatText;
    Vector<String> m_timeAMPMLabels;
#endif
    bool m_didInitializeNumberData;
};

} // namespace WebCore
#endif
