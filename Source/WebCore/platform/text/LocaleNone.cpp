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

class LocaleNone final : public Locale {
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(LocaleNone);
public:
    virtual ~LocaleNone();

private:
    void initializeLocaleData() final;
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

    Vector<RefPtr<StringImpl>> m_timeAMPMLabels;
    Vector<RefPtr<StringImpl>> m_shortMonthLabels;
    Vector<RefPtr<StringImpl>> m_monthLabels;
};

std::unique_ptr<Locale> Locale::create(AtomStringImpl*)
{
    return makeUnique<LocaleNone>();
}

LocaleNone::~LocaleNone() = default;

void LocaleNone::initializeLocaleData()
{
}

const Vector<RefPtr<StringImpl>>& LocaleNone::monthLabels()
{
    if (!m_monthLabels.isEmpty())
        return m_monthLabels;
    m_monthLabels = { WTF::monthFullName, std::size(WTF::monthFullName) };
    return m_monthLabels;
}

RefPtr<StringImpl> LocaleNone::dateFormat()
{
    return "yyyy-MM-dd"_si;
}

RefPtr<StringImpl> LocaleNone::monthFormat()
{
    return "yyyy-MM"_si;
}

RefPtr<StringImpl> LocaleNone::shortMonthFormat()
{
    return "yyyy-MM"_si;
}

RefPtr<StringImpl> LocaleNone::timeFormat()
{
    return "HH:mm:ss"_si;
}

RefPtr<StringImpl> LocaleNone::shortTimeFormat()
{
    return "HH:mm"_si;
}

RefPtr<StringImpl> LocaleNone::dateTimeFormatWithSeconds()
{
    return "yyyy-MM-dd'T'HH:mm:ss"_si;
}

RefPtr<StringImpl> LocaleNone::dateTimeFormatWithoutSeconds()
{
    return "yyyy-MM-dd'T'HH:mm"_si;
}

const Vector<RefPtr<StringImpl>>& LocaleNone::shortMonthLabels()
{
    if (!m_shortMonthLabels.isEmpty())
        return m_shortMonthLabels;
    m_shortMonthLabels = { WTF::monthName, std::size(WTF::monthName) };
    return m_shortMonthLabels;
}

const Vector<RefPtr<StringImpl>>& LocaleNone::shortStandAloneMonthLabels()
{
    return shortMonthLabels();
}

const Vector<RefPtr<StringImpl>>& LocaleNone::standAloneMonthLabels()
{
    return monthLabels();
}

const Vector<RefPtr<StringImpl>>& LocaleNone::timeAMPMLabels()
{
    if (!m_timeAMPMLabels.isEmpty())
        return m_timeAMPMLabels;
    m_timeAMPMLabels.appendList({ "AM"_s, "PM"_s });
    return m_timeAMPMLabels;
}

} // namespace WebCore
