/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "IntlSegmenter.h"

#include "IntlObjectInlines.h"
#include "IntlSegments.h"
#include "JSCInlines.h"
#include "ObjectConstructor.h"

namespace JSC {

const ClassInfo IntlSegmenter::s_info = { "Object", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(IntlSegmenter) };

IntlSegmenter* IntlSegmenter::create(VM& vm, Structure* structure)
{
    auto* object = new (NotNull, allocateCell<IntlSegmenter>(vm.heap)) IntlSegmenter(vm, structure);
    object->finishCreation(vm);
    return object;
}

Structure* IntlSegmenter::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

IntlSegmenter::IntlSegmenter(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void IntlSegmenter::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
}

// https://tc39.es/proposal-intl-segmenter/#sec-intl.segmenter
void IntlSegmenter::initializeSegmenter(JSGlobalObject* globalObject, JSValue locales, JSValue optionsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto requestedLocales = canonicalizeLocaleList(globalObject, locales);
    RETURN_IF_EXCEPTION(scope, void());

    JSObject* options;
    if (optionsValue.isUndefined())
        options = constructEmptyObject(vm, globalObject->nullPrototypeObjectStructure());
    else {
        options = optionsValue.toObject(globalObject);
        RETURN_IF_EXCEPTION(scope, void());
    }

    ResolveLocaleOptions localeOptions;

    LocaleMatcher localeMatcher = intlOption<LocaleMatcher>(globalObject, options, vm.propertyNames->localeMatcher, { { "lookup"_s, LocaleMatcher::Lookup }, { "best fit"_s, LocaleMatcher::BestFit } }, "localeMatcher must be either \"lookup\" or \"best fit\""_s, LocaleMatcher::BestFit);
    RETURN_IF_EXCEPTION(scope, void());

    auto localeData = [](const String&, RelevantExtensionKey) -> Vector<String> {
        return { };
    };

    auto& availableLocales = intlSegmenterAvailableLocales();
    auto resolved = resolveLocale(globalObject, availableLocales, requestedLocales, localeMatcher, localeOptions, { }, localeData);

    m_locale = resolved.locale;
    if (m_locale.isEmpty()) {
        throwTypeError(globalObject, scope, "failed to initialize Segmenter due to invalid locale"_s);
        return;
    }

    m_granularity = intlOption<Granularity>(globalObject, options, vm.propertyNames->granularity, { { "grapheme"_s, Granularity::Grapheme }, { "word"_s, Granularity::Word }, { "sentence"_s, Granularity::Sentence } }, "granularity must be either \"grapheme\", \"word\", or \"sentence\""_s, Granularity::Grapheme);
    RETURN_IF_EXCEPTION(scope, void());

    UBreakIteratorType type = UBRK_CHARACTER;
    switch (m_granularity) {
    case Granularity::Grapheme:
        type = UBRK_CHARACTER;
        break;
    case Granularity::Word:
        type = UBRK_WORD;
        break;
    case Granularity::Sentence:
        type = UBRK_SENTENCE;
        break;
    }

    UErrorCode status = U_ZERO_ERROR;
    m_segmenter = std::unique_ptr<UBreakIterator, UBreakIteratorDeleter>(ubrk_open(type, m_locale.utf8().data(), nullptr, 0, &status));
    if (U_FAILURE(status)) {
        throwTypeError(globalObject, scope, "failed to initialize Segmenter"_s);
        return;
    }
}

// https://tc39.es/proposal-intl-segmenter/#sec-intl.segmenter.prototype.segment
JSValue IntlSegmenter::segment(JSGlobalObject* globalObject, JSValue stringValue) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSString* jsString = stringValue.toString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    String string = jsString->value(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    auto upconvertedCharacters = Box<Vector<UChar>>::create(string.charactersWithoutNullTermination());

    UErrorCode status = U_ZERO_ERROR;
    auto segmenter = std::unique_ptr<UBreakIterator, UBreakIteratorDeleter>(ubrk_safeClone(m_segmenter.get(), nullptr, nullptr, &status));
    if (U_FAILURE(status)) {
        throwTypeError(globalObject, scope, "failed to initialize Segments"_s);
        return { };
    }
    ubrk_setText(segmenter.get(), upconvertedCharacters->data(), upconvertedCharacters->size(), &status);
    if (U_FAILURE(status)) {
        throwTypeError(globalObject, scope, "failed to initialize Segments"_s);
        return { };
    }

    return IntlSegments::create(vm, globalObject->segmentsStructure(), WTFMove(segmenter), WTFMove(upconvertedCharacters), jsString, m_granularity);
}

// https://tc39.es/proposal-intl-segmenter/#sec-intl.segmenter.prototype.resolvedoptions
JSObject* IntlSegmenter::resolvedOptions(JSGlobalObject* globalObject) const
{
    VM& vm = globalObject->vm();
    JSObject* options = constructEmptyObject(globalObject);
    options->putDirect(vm, vm.propertyNames->locale, jsString(vm, m_locale));
    options->putDirect(vm, vm.propertyNames->granularity, jsNontrivialString(vm, granularityString(m_granularity)));
    return options;
}

ASCIILiteral IntlSegmenter::granularityString(Granularity granularity)
{
    switch (granularity) {
    case Granularity::Grapheme:
        return "grapheme"_s;
    case Granularity::Word:
        return "word"_s;
    case Granularity::Sentence:
        return "sentence"_s;
    }
    ASSERT_NOT_REACHED();
    return ASCIILiteral::null();
}

JSObject* IntlSegmenter::createSegmentDataObject(JSGlobalObject* globalObject, JSString* string, int32_t startIndex, int32_t endIndex, UBreakIterator& segmenter, Granularity granularity)
{
    VM& vm = globalObject->vm();
    JSObject* result = constructEmptyObject(globalObject);
    result->putDirect(vm, vm.propertyNames->segment, jsSubstring(globalObject, string, startIndex, endIndex - startIndex));
    result->putDirect(vm, vm.propertyNames->index, jsNumber(startIndex));
    result->putDirect(vm, vm.propertyNames->input, string);
    if (granularity == IntlSegmenter::Granularity::Word) {
        int32_t ruleStatus = ubrk_getRuleStatus(&segmenter);
        result->putDirect(vm, vm.propertyNames->isWordLike, jsBoolean(!(ruleStatus >= UBRK_WORD_NONE && ruleStatus <= UBRK_WORD_NONE_LIMIT)));
    }
    return result;
}

} // namespace JSC
