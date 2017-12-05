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

#include "PlatformLocale.h"
#include <windows.h>
#include <wtf/Forward.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class DateComponents;
struct DateFormatToken;

class LocaleWin : public Locale {
public:
    explicit LocaleWin(LCID);
    ~LocaleWin();
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

    static String dateFormat(const String&);
#endif

private:
    String getLocaleInfoString(LCTYPE);
    void getLocaleInfo(LCTYPE, DWORD&);
    void ensureShortMonthLabels();
    void ensureMonthLabels();
    // Locale function:
    void initializeLocaleData() override;

    LCID m_lcid;
#if ENABLE(DATE_AND_TIME_INPUT_TYPES)
    Vector<String> m_shortMonthLabels;
    Vector<String> m_monthLabels;
    String m_dateFormat;
    String m_monthFormat;
    String m_shortMonthFormat;
    String m_timeFormatWithSeconds;
    String m_timeFormatWithoutSeconds;
    String m_dateTimeFormatWithSeconds;
    String m_dateTimeFormatWithoutSeconds;
    Vector<String> m_timeAMPMLabels;
#endif
    bool m_didInitializeNumberData;
};

} // namespace WebCore
#endif
