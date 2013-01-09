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

#ifndef WebDateTimeChooserParams_h
#define WebDateTimeChooserParams_h

#include "WebDateTimeInputType.h"

#include <public/WebRect.h>
#include <public/WebString.h>
#include <public/WebVector.h>

namespace WebKit {

// This class conveys various information to make date/time chooser UI.
// See WebViewClient::openDateTimeChooser.
struct WebDateTimeChooserParams {
    // The type of chooser to show.
    WebDateTimeInputType type;
    // Bounding rectangle of the requester element.
    WebRect anchorRectInScreen;
    // The current value of the requester element.
    WebString currentValue;
    // <datalist> option values associated to the requester element. These
    // values should not be shown to users. The vector size might be 0.
    WebVector<WebString> suggestionValues;
    // Localized values of <datalist> options associated to the requester
    // element. These values should be shown to users. The vector size must be
    // same as suggestionValues size.
    WebVector<WebString> localizedSuggestionValues;
    // <datalist> option labels associated to the requester element. These
    // values should be shown to users. The vector size must be same as
    // suggestionValues size.
    WebVector<WebString> suggestionLabels;
    // HTMLInputElement::min attribute value parsed in the valusAsNumber rule,
    // that is to say, milliseconds from the epoch for non-month types and
    // months from the epoch for month type. If the min attribute is missing,
    // this field has the hard minimum limit.
    double minimum;
    // Similar to minimum.
    double maximum;
    // Step value represented in milliseconds for non-month types, and
    // represetnted in months for month type.
    double step;
    // Step-base value represeted in milliseconds, or months.
    double stepBase;
    // True if the requester element has required attribute.
    bool isRequired;
    // True if the requester element is rendered in rtl direction.
    bool isAnchorElementRTL;

    WebDateTimeChooserParams()
        : minimum(0)
        , maximum(0)
        , step(0)
        , stepBase(0)
        , isRequired(false)
        , isAnchorElementRTL(false)
    {
    }
};

} // namespace WebKit

#endif
