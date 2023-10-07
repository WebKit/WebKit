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
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "PlatformLocale.h"
#include <wtf/DateMath.h>

namespace WebCore {

class LocaleNone : public Locale {
public:
    virtual ~LocaleNone();

private:
    void initializeLocaleData() final;
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

    Vector<String> m_timeAMPMLabels;
    Vector<String> m_shortMonthLabels;
    Vector<String> m_monthLabels;
#endif
};

std::unique_ptr<Locale> Locale::create(const AtomString&)
{
    return makeUnique<LocaleNone>();
}

LocaleNone::~LocaleNone() = default;

void LocaleNone::initializeLocaleData()
{
}

#if ENABLE(DATE_AND_TIME_INPUT_TYPES)
const Vector<String>& LocaleNone::monthLabels()
{
    if (!m_monthLabels.isEmpty())
        return m_monthLabels;
    m_monthLabels = { WTF::monthFullName, std::size(WTF::monthFullName) };
    return m_monthLabels;
}

String LocaleNone::dateFormat()
{
    return "yyyy-MM-dd"_s;
}

String LocaleNone::monthFormat()
{
    return "yyyy-MM"_s;
}

String LocaleNone::shortMonthFormat()
{
    return "yyyy-MM"_s;
}

String LocaleNone::timeFormat()
{
    return "HH:mm:ss"_s;
}

String LocaleNone::shortTimeFormat()
{
    return "HH:mm"_s;
}

String LocaleNone::dateTimeFormatWithSeconds()
{
    return "yyyy-MM-dd'T'HH:mm:ss"_s;
}

String LocaleNone::dateTimeFormatWithoutSeconds()
{
    return "yyyy-MM-dd'T'HH:mm"_s;
}

const Vector<String>& LocaleNone::shortMonthLabels()
{
    if (!m_shortMonthLabels.isEmpty())
        return m_shortMonthLabels;
    m_shortMonthLabels = { WTF::monthName, std::size(WTF::monthName) };
    return m_shortMonthLabels;
}

const Vector<String>& LocaleNone::shortStandAloneMonthLabels()
{
    return shortMonthLabels();
}

const Vector<String>& LocaleNone::standAloneMonthLabels()
{
    return monthLabels();
}

const Vector<String>& LocaleNone::timeAMPMLabels()
{
    if (!m_timeAMPMLabels.isEmpty())
        return m_timeAMPMLabels;
    m_timeAMPMLabels.appendList({ "AM"_s, "PM"_s });
    return m_timeAMPMLabels;
}

#endif

} // namespace WebCore
