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
#include "LocalizedDate.h"

#include "Language.h"
#include <limits>
#include <unicode/udat.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/text/CString.h>

using namespace icu;
using namespace std;

namespace WebCore {

double parseLocalizedDate(const String& input, DateComponents::Type type)
{
    switch (type) {
    case DateComponents::Date: {
        if (input.length() > static_cast<unsigned>(numeric_limits<int32_t>::max()))
            break;
        int32_t inputLength = static_cast<int32_t>(input.length());
        UErrorCode status = U_ZERO_ERROR;
        UDateFormat* dateFormat = udat_open(UDAT_NONE, UDAT_SHORT, defaultLanguage().utf8().data(), 0, -1, 0, -1, &status);
        if (!dateFormat)
            break;
        status = U_ZERO_ERROR;
        int32_t parsePosition = 0;
        UDate date = udat_parse(dateFormat, input.characters(), inputLength, &parsePosition, &status);
        udat_close(dateFormat);
        if (parsePosition != inputLength || U_FAILURE(status))
            break;
        // UDate, which is an alias of double, is compatible with our expectation.
        return date;
    }
    case DateComponents::DateTime:
    case DateComponents::DateTimeLocal:
    case DateComponents::Month:
    case DateComponents::Time:
    case DateComponents::Week:
    case DateComponents::Invalid:
        break;
    }
    return numeric_limits<double>::quiet_NaN();
}

String formatLocalizedDate(const DateComponents& dateComponents)
{
    switch (dateComponents.type()) {
    case DateComponents::Date: {
        UErrorCode status = U_ZERO_ERROR;
        UDateFormat* dateFormat = udat_open(UDAT_NONE, UDAT_SHORT, defaultLanguage().utf8().data(), 0, -1, 0, -1, &status);
        if (!dateFormat)
            break;
        double input = dateComponents.millisecondsSinceEpoch();
        int32_t length = udat_format(dateFormat, input, 0, 0, 0, &status);
        if (status != U_BUFFER_OVERFLOW_ERROR) {
            udat_close(dateFormat);
            break;
        }
        Vector<UChar> buffer(length);
        status = U_ZERO_ERROR;
        udat_format(dateFormat, input, buffer.data(), length, 0, &status);
        udat_close(dateFormat);
        if (U_FAILURE(status))
            break;
        return String::adopt(buffer);
    }
    case DateComponents::DateTime:
    case DateComponents::DateTimeLocal:
    case DateComponents::Month:
    case DateComponents::Time:
    case DateComponents::Week:
    case DateComponents::Invalid:
        break;
    }
    return String();
}

}
