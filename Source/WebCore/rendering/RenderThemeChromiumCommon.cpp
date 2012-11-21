/*
 * Copyright (C) 2012 Google Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "RenderThemeChromiumCommon.h"

#include "InputTypeNames.h"
#include "LayoutUnit.h"

namespace WebCore {

bool RenderThemeChromiumCommon::supportsDataListUI(const AtomicString& type)
{
    return type == InputTypeNames::text() || type == InputTypeNames::search() || type == InputTypeNames::url()
        || type == InputTypeNames::telephone() || type == InputTypeNames::email() || type == InputTypeNames::number()
#if ENABLE(INPUT_TYPE_COLOR)
        || type == InputTypeNames::color()
#endif
#if ENABLE(CALENDAR_PICKER)
        || type == InputTypeNames::date()
#endif
        || type == InputTypeNames::datetime()
        || type == InputTypeNames::datetimelocal()
        || type == InputTypeNames::month()
        || type == InputTypeNames::week()
        || type == InputTypeNames::time()
        || type == InputTypeNames::range();
}

#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
bool RenderThemeChromiumCommon::supportsCalendarPicker(const AtomicString& type)
{
    return type == InputTypeNames::date()
        || type == InputTypeNames::datetime()
        || type == InputTypeNames::datetimelocal()
        || type == InputTypeNames::month()
        || type == InputTypeNames::week();
}
#endif

LayoutUnit RenderThemeChromiumCommon::sliderTickSnappingThreshold()
{
    return 5;
}

}
