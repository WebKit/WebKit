/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "GenericMediaQueryParser.h"
#include "MediaQuery.h"
#include "MediaQueryParserContext.h"

namespace WebCore {
namespace MQ {

class MediaQueryParser : public GenericMediaQueryParser<MediaQueryParser>  {
public:
    MediaQueryParser(const MediaQueryParserContext&);

    static MediaQueryList parse(const String&, const MediaQueryParserContext&);
    static MediaQueryList parse(CSSParserTokenRange, const MediaQueryParserContext&);
    static std::optional<MediaQuery> parseCondition(CSSParserTokenRange, const MediaQueryParserContext&);

    MediaQueryList consumeMediaQueryList(CSSParserTokenRange&);
    std::optional<MediaQuery> consumeMediaQuery(CSSParserTokenRange&);
    const FeatureSchema* schemaForFeatureName(const AtomString&) const;

    static Vector<const FeatureSchema*> featureSchemas();
};

void serialize(StringBuilder&, const MediaQueryList&);
void serialize(StringBuilder&, const MediaQuery&);

}
}
