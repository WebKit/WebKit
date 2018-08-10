/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(INTERSECTION_OBSERVER)
#include "IntersectionObserver.h"

#include "CSSParserTokenRange.h"
#include "CSSPropertyParserHelpers.h"
#include "CSSTokenizer.h"
#include "Element.h"
#include "IntersectionObserverCallback.h"
#include "IntersectionObserverEntry.h"
#include <wtf/Vector.h>

namespace WebCore {

static ExceptionOr<LengthBox> parseRootMargin(String& rootMargin)
{
    CSSTokenizer tokenizer(rootMargin);
    auto tokenRange = tokenizer.tokenRange();
    Vector<Length, 4> margins;
    while (!tokenRange.atEnd()) {
        if (margins.size() == 4)
            return Exception { SyntaxError, "Failed to construct 'IntersectionObserver': Extra text found at the end of rootMargin." };
        RefPtr<CSSPrimitiveValue> parsedValue = CSSPropertyParserHelpers::consumeLengthOrPercent(tokenRange, HTMLStandardMode, ValueRangeAll);
        if (!parsedValue || parsedValue->isCalculated())
            return Exception { SyntaxError, "Failed to construct 'IntersectionObserver': rootMargin must be specified in pixels or percent." };
        if (parsedValue->isPercentage())
            margins.append(Length(parsedValue->doubleValue(), Percent));
        else if (parsedValue->isPx())
            margins.append(Length(parsedValue->intValue(), Fixed));
        else
            return Exception { SyntaxError, "Failed to construct 'IntersectionObserver': rootMargin must be specified in pixels or percent." };
    }
    switch (margins.size()) {
    case 0:
        for (unsigned i = 0; i < 4; ++i)
            margins.append(Length());
        break;
    case 1:
        for (unsigned i = 0; i < 3; ++i)
            margins.append(margins[0]);
        break;
    case 2:
        margins.append(margins[0]);
        margins.append(margins[1]);
        break;
    case 3:
        margins.append(margins[1]);
        break;
    case 4:
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    return LengthBox(WTFMove(margins[0]), WTFMove(margins[1]), WTFMove(margins[2]), WTFMove(margins[3]));
}

ExceptionOr<Ref<IntersectionObserver>> IntersectionObserver::create(Ref<IntersectionObserverCallback>&& callback, IntersectionObserver::Init&& init)
{
    auto rootMarginOrException = parseRootMargin(init.rootMargin);
    if (rootMarginOrException.hasException())
        return rootMarginOrException.releaseException();

    return adoptRef(*new IntersectionObserver(WTFMove(callback), WTFMove(init), rootMarginOrException.releaseReturnValue()));
}

IntersectionObserver::IntersectionObserver(Ref<IntersectionObserverCallback>&& callback, Init&& init, LengthBox&& parsedRootMargin)
    : m_root(init.root)
    , m_rootMargin(WTFMove(parsedRootMargin))
    , m_callback(WTFMove(callback))
{
    if (WTF::holds_alternative<double>(init.threshold))
        m_thresholds.append(WTF::get<double>(init.threshold));
    else
        m_thresholds = WTF::get<Vector<double>>(WTFMove(init.threshold));
}

String IntersectionObserver::rootMargin() const
{
    StringBuilder stringBuilder;
    PhysicalBoxSide sides[4] = { PhysicalBoxSide::Top, PhysicalBoxSide::Right, PhysicalBoxSide::Bottom, PhysicalBoxSide::Left };
    for (auto side : sides) {
        auto& length = m_rootMargin.at(side);
        stringBuilder.appendNumber(length.intValue());
        if (length.type() == Percent)
            stringBuilder.append('%');
        else
            stringBuilder.append("px", 2);
        if (side != PhysicalBoxSide::Left)
            stringBuilder.append(' ');
    }
    return stringBuilder.toString();
}

void IntersectionObserver::observe(Element&)
{
}

void IntersectionObserver::unobserve(Element&)
{
}

void IntersectionObserver::disconnect()
{
}

Vector<RefPtr<IntersectionObserverEntry>> IntersectionObserver::takeRecords()
{
    return { };
}


} // namespace WebCore

#endif // ENABLE(INTERSECTION_OBSERVER)
