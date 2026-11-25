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

#pragma once

#include "PlatformLocale.h"
#include <wtf/Forward.h>
#include <wtf/RetainPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(MAC)
#define PlatformNSParagraphStyle NSParagraphStyle.class
#else
#define PlatformNSParagraphStyle PAL::getNSParagraphStyleClassSingleton()
#endif

OBJC_CLASS NSCalendar;
OBJC_CLASS NSDateFormatter;
OBJC_CLASS NSParagraphStyle;
OBJC_CLASS NSLocale;

namespace WebCore {

class DateComponents;

class LocaleCocoa final : public Locale {
    WTF_MAKE_TZONE_ALLOCATED(LocaleCocoa);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(LocaleCocoa);
public:
    explicit LocaleCocoa(AtomStringImpl*);
    ~LocaleCocoa();

    Locale::WritingDirection defaultWritingDirection() const final;

    RefPtr<StringImpl> formatDateTime(const DateComponents&, FormatType = FormatTypeUnspecified) final;

    RefPtr<StringImpl> dateFormat() final;
    RefPtr<StringImpl> monthFormat() final;
    RefPtr<StringImpl> shortMonthFormat() final;
    RefPtr<StringImpl> timeFormat() final;
    RefPtr<StringImpl> shortTimeFormat() final;
    RefPtr<StringImpl> dateTimeFormatWithSeconds() final;
    RefPtr<StringImpl> dateTimeFormatWithoutSeconds() final;
    const Vector<RefPtr<StringImpl>>& monthLabels() final;
    const Vector<RefPtr<StringImpl>>& shortMonthLabels() final;
    const Vector<RefPtr<StringImpl>>& standAloneMonthLabels() final;
    const Vector<RefPtr<StringImpl>>& shortStandAloneMonthLabels() final;
    const Vector<RefPtr<StringImpl>>& timeAMPMLabels() final;

    static RetainPtr<CFStringRef> canonicalLanguageIdentifierFromString(AtomStringImpl*);
    static void releaseMemory();

private:
    RetainPtr<NSDateFormatter> shortDateFormatter();
    void initializeLocaleData() final;

    RetainPtr<NSLocale> m_locale;
    const RetainPtr<NSCalendar> m_gregorianCalendar;
    Vector<RefPtr<StringImpl>> m_monthLabels;
    RetainPtr<NSDateFormatter> timeFormatter();
    RetainPtr<NSDateFormatter> shortTimeFormatter();
    RetainPtr<NSDateFormatter> dateTimeFormatterWithSeconds();
    RetainPtr<NSDateFormatter> dateTimeFormatterWithoutSeconds();

    RefPtr<StringImpl> m_dateFormat;
    RefPtr<StringImpl> m_monthFormat;
    RefPtr<StringImpl> m_shortMonthFormat;
    RefPtr<StringImpl> m_timeFormatWithSeconds;
    RefPtr<StringImpl> m_timeFormatWithoutSeconds;
    RefPtr<StringImpl> m_dateTimeFormatWithSeconds;
    RefPtr<StringImpl> m_dateTimeFormatWithoutSeconds;
    Vector<RefPtr<StringImpl>> m_shortMonthLabels;
    Vector<RefPtr<StringImpl>> m_standAloneMonthLabels;
    Vector<RefPtr<StringImpl>> m_shortStandAloneMonthLabels;
    Vector<RefPtr<StringImpl>> m_timeAMPMLabels;
    bool m_didInitializeNumberData;
};

} // namespace WebCore
