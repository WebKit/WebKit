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
#include "Localizer.h"
#include <wtf/PassOwnPtr.h>

namespace WebCore {

class LocaleNone : public Localizer {
public:
    virtual ~LocaleNone();

private:
    virtual void initializeLocalizerData() OVERRIDE FINAL;
    virtual double parseDateTime(const String&, DateComponents::Type) OVERRIDE;
    virtual String formatDateTime(const DateComponents&, FormatType = FormatTypeUnspecified) OVERRIDE;
#if ENABLE(CALENDAR_PICKER)
    virtual String dateFormatText() OVERRIDE;
    virtual bool isRTL() OVERRIDE;
#endif
#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
    virtual String dateFormat() OVERRIDE;
#endif
};

PassOwnPtr<Localizer> Localizer::create(const AtomicString&)
{
    return adoptPtr(new LocaleNone());
}

LocaleNone::~LocaleNone()
{
}

void LocaleNone::initializeLocalizerData()
{
}

double LocaleNone::parseDateTime(const String&, DateComponents::Type)
{
    return std::numeric_limits<double>::quiet_NaN();
}

String LocaleNone::formatDateTime(const DateComponents&, FormatType)
{
    return String();
}

#if ENABLE(CALENDAR_PICKER)
String LocaleNone::dateFormatText()
{
    return ASCIILiteral("Year-Month-Day");
}

bool LocaleNone::isRTL()
{
    return false;
}
#endif

#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
String LocaleNone::dateFormat()
{
    return ASCIILiteral("dd/MM/yyyyy");
}
#endif

} // namespace WebCore
