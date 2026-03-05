/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "StyleTextDecorationLine.h"

#include "AnimationUtilities.h"
#include "CSSPrimitiveValue.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSValueList.h"
#include "CSSValuePool.h"
#include "StyleBuilderChecking.h"
#include "StyleValueTypes.h"
#include <wtf/Assertions.h>

namespace WebCore {

TextStream& operator<<(TextStream& ts, Style::TextDecorationLine::Flag flag)
{
    switch (flag) {
    case Style::TextDecorationLine::Flag::Underline: ts << "underline"_s; break;
    case Style::TextDecorationLine::Flag::Overline: ts << "overline"_s; break;
    case Style::TextDecorationLine::Flag::LineThrough: ts << "line-through"_s; break;
    case Style::TextDecorationLine::Flag::Blink: ts << "blink"_s; break;
    }
    return ts;
}
namespace Style {

void TextDecorationLine::addOrReplaceIfNotNone(TextDecorationLine value)
{
    value.switchOn(
        [&](CSS::Keyword::None) {
        },
        [&](CSS::Keyword::SpellingError) {
            setSpellingError();
        },
        [&](CSS::Keyword::GrammarError) {
            setGrammarError();
        },
        [&](const OptionSet<TextDecorationLine::Flag>& newValue) {
            setFlags(newValue);
        }
    );
}

// MARK: - Conversion

auto CSSValueConversion<TextDecorationLine>::operator()(BuilderState& state, const CSSValue& value) -> TextDecorationLine
{
    auto invalidValue = [&state]() -> TextDecorationLine {
        state.setCurrentPropertyInvalidAtComputedValueTime();
        return CSS::Keyword::None { };
    };

    if (RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        if (primitiveValue->isValueID()) {
            switch (primitiveValue->valueID()) {
            case CSSValueNone:
                return CSS::Keyword::None { };
            case CSSValueSpellingError:
                return CSS::Keyword::SpellingError { };
            case CSSValueGrammarError:
                return CSS::Keyword::GrammarError { };
            default:
                break;
            }
        }
        return invalidValue();
    }

    if (RefPtr valueList = dynamicDowncast<CSSValueList>(value)) {
        OptionSet<TextDecorationLine::Flag> flags;

        for (Ref item : *valueList) {
            RefPtr primitiveValue = requiredDowncast<CSSPrimitiveValue>(state, item);
            if (!primitiveValue)
                return invalidValue();

            switch (primitiveValue->valueID()) {
            case CSSValueUnderline:
                flags.add(TextDecorationLine::Flag::Underline);
                break;
            case CSSValueOverline:
                flags.add(TextDecorationLine::Flag::Overline);
                break;
            case CSSValueLineThrough:
                flags.add(TextDecorationLine::Flag::LineThrough);
                break;
            case CSSValueBlink:
                flags.add(TextDecorationLine::Flag::Blink);
                break;
            default:
                return invalidValue();
            }
        }

        if (flags.isEmpty())
            return invalidValue();

        return flags;
    }

    return invalidValue();
}

Ref<CSSValue> CSSValueCreation<OptionSet<TextDecorationLine::Flag>>::operator()(CSSValuePool&, const RenderStyle&, const OptionSet<TextDecorationLine::Flag>& value)
{
    ASSERT(!value.isEmpty());

    CSSValueListBuilder list;
    if (value.contains(TextDecorationLine::Flag::Underline))
        list.append(CSSPrimitiveValue::create(CSSValueUnderline));
    if (value.contains(TextDecorationLine::Flag::Overline))
        list.append(CSSPrimitiveValue::create(CSSValueOverline));
    if (value.contains(TextDecorationLine::Flag::LineThrough))
        list.append(CSSPrimitiveValue::create(CSSValueLineThrough));
    if (value.contains(TextDecorationLine::Flag::Blink))
        list.append(CSSPrimitiveValue::create(CSSValueBlink));
    return CSSValueList::createSpaceSeparated(WTF::move(list));
}

// MARK: - Serialization

void Serialize<OptionSet<TextDecorationLine::Flag>>::operator()(StringBuilder& builder, const CSS::SerializationContext&, const RenderStyle&, const OptionSet<TextDecorationLine::Flag>& value)
{
    ASSERT(!value.isEmpty());

    bool needsSpace = false;
    auto appendOption = [&](TextDecorationLine::Flag option, CSSValueID valueID) {
        if (value.contains(option)) {
            if (needsSpace)
                builder.append(' ');
            builder.append(nameLiteralForSerialization(valueID));
            needsSpace = true;
        }
    };
    appendOption(TextDecorationLine::Flag::Underline, CSSValueUnderline);
    appendOption(TextDecorationLine::Flag::Overline, CSSValueOverline);
    appendOption(TextDecorationLine::Flag::LineThrough, CSSValueLineThrough);
    // Blink value is ignored for rendering but not for the computed value.
    appendOption(TextDecorationLine::Flag::Blink, CSSValueBlink);
}

// MARK: - Logging

WTF::TextStream& operator<<(WTF::TextStream& ts, const TextDecorationLine& decoration)
{
    decoration.switchOn(
        [&](CSS::Keyword::None) {
            ts << "none";
        },
        [&](CSS::Keyword::SpellingError) {
            ts << "spelling-error";
        },
        [&](CSS::Keyword::GrammarError) {
            ts << "grammar-error";
        },
        [&](const OptionSet<TextDecorationLine::Flag>& flags) {
            bool needsSpace = false;
            auto streamFlag = [&](TextDecorationLine::Flag flag, CSSValueID valueID) {
                if (flags.contains(flag)) {
                    if (needsSpace)
                        ts << ' ';
                    ts << nameLiteralForSerialization(valueID);
                    needsSpace = true;
                }
            };
            streamFlag(TextDecorationLine::Flag::Underline, CSSValueUnderline);
            streamFlag(TextDecorationLine::Flag::Overline, CSSValueOverline);
            streamFlag(TextDecorationLine::Flag::LineThrough, CSSValueLineThrough);
            streamFlag(TextDecorationLine::Flag::Blink, CSSValueBlink);
        }
    );
    return ts;
}

} // namespace Style

} // namespace WebCore
