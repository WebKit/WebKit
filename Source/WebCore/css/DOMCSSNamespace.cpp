/*
 * Copyright (C) 2012 Motorola Mobility Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Motorola Mobility Inc. nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
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
#include "DOMCSSNamespace.h"

#include "CSSMarkup.h"
#include "CSSParser.h"
#include "CSSPropertyParser.h"
#include "Document.h"
#include "HighlightMap.h"
#include "StyleProperties.h"
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

static String valueWithoutImportant(const String& value)
{
    if (!value.endsWithIgnoringASCIICase("important"))
        return value;

    String newValue = value;
    int bangIndex = newValue.length() - 9 - 1;
    if (newValue[bangIndex] == ' ')
        bangIndex--;
    newValue = newValue.left(bangIndex);

    return newValue;
}

bool DOMCSSNamespace::supports(Document& document, const String& property, const String& value)
{
    CSSPropertyID propertyID = cssPropertyID(property.stripWhiteSpace());

    if (propertyID == CSSPropertyInvalid)
        return false;

    // CSSParser::parseValue() won't work correctly if !important is present,
    // so just get rid of it. It doesn't matter to supports() if it's actually
    // there or not, provided how it's specified in the value is correct.
    String normalizedValue = value.stripWhiteSpace().simplifyWhiteSpace();
    normalizedValue = valueWithoutImportant(normalizedValue);

    if (normalizedValue.isEmpty())
        return false;

    auto dummyStyle = MutableStyleProperties::create();
    return CSSParser::parseValue(dummyStyle, propertyID, normalizedValue, false, document) != CSSParser::ParseResult::Error;
}

bool DOMCSSNamespace::supports(Document& document, const String& conditionText)
{
    CSSParserContext context(document);
    CSSParser parser(context);
    return parser.parseSupportsCondition(conditionText);
}

String DOMCSSNamespace::escape(const String& ident)
{
    StringBuilder builder;
    serializeIdentifier(ident, builder);
    return builder.toString();
}

HighlightMap& DOMCSSNamespace::highlights(Document& document)
{
    return document.highlightMap();
}

}
