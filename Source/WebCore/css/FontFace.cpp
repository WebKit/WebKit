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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FontFace.h"

#include "CSSComputedStyleDeclaration.h"
#include "CSSFontFaceSource.h"
#include "CSSFontFeatureValue.h"
#include "CSSFontSelector.h"
#include "CSSFontStyleValue.h"
#include "CSSParser.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSPropertyParserWorkerSafe.h"
#include "CSSUnicodeRangeValue.h"
#include "CSSValueList.h"
#include "CSSValuePool.h"
#include "DOMPromiseProxy.h"
#include "Document.h"
#include "JSFontFace.h"
#include "StyleProperties.h"
#include <JavaScriptCore/ArrayBuffer.h>
#include <JavaScriptCore/ArrayBufferView.h>
#include <JavaScriptCore/JSCInlines.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

static bool populateFontFaceWithArrayBuffer(CSSFontFace& fontFace, Ref<JSC::ArrayBufferView>&& arrayBufferView)
{
    auto source = makeUnique<CSSFontFaceSource>(fontFace, String(), WTFMove(arrayBufferView));
    fontFace.adoptSource(WTFMove(source));
    return false;
}

void FontFace::setErrorState()
{
    m_loadedPromise->reject(Exception { SyntaxError });
    m_backing->setErrorState();
}

Ref<FontFace> FontFace::create(ScriptExecutionContext& context, const String& family, Source&& source, const Descriptors& descriptors)
{
    ASSERT(context.cssFontSelector());
    auto result = adoptRef(*new FontFace(*context.cssFontSelector()));
    result->suspendIfNeeded();

    bool dataRequiresAsynchronousLoading = true;

    auto setFamilyResult = result->setFamily(context, family);
    if (setFamilyResult.hasException()) {
        result->setErrorState();
        return result;
    }

    bool allowDownloading = context.settingsValues().downloadableBinaryFontsEnabled;
    auto sourceConversionResult = WTF::switchOn(source,
        [&] (String& string) -> ExceptionOr<void> {
            auto value = CSSPropertyParserWorkerSafe::parseFontFaceSrc(string, is<Document>(context) ? CSSParserContext(downcast<Document>(context)) : HTMLStandardMode);
            if (!value)
                return Exception { SyntaxError };
            CSSFontFace::appendSources(result->backing(), *value, &context, false);
            return { };
        },
        [&, allowDownloading] (RefPtr<ArrayBufferView>& arrayBufferView) -> ExceptionOr<void> {
            if (!allowDownloading)
                return { };

            dataRequiresAsynchronousLoading = populateFontFaceWithArrayBuffer(result->backing(), arrayBufferView.releaseNonNull());
            return { };
        },
        [&, allowDownloading] (RefPtr<ArrayBuffer>& arrayBuffer) -> ExceptionOr<void> {
            if (!allowDownloading)
                return { };

            unsigned byteLength = arrayBuffer->byteLength();
            auto arrayBufferView = JSC::Uint8Array::create(WTFMove(arrayBuffer), 0, byteLength);
            dataRequiresAsynchronousLoading = populateFontFaceWithArrayBuffer(result->backing(), WTFMove(arrayBufferView));
            return { };
        }
    );

    if (sourceConversionResult.hasException()) {
        result->setErrorState();
        return result;
    }

    // These ternaries match the default strings inside the FontFaceDescriptors dictionary inside FontFace.idl.
    auto setStyleResult = result->setStyle(context, descriptors.style.isEmpty() ? "normal"_s : descriptors.style);
    if (setStyleResult.hasException()) {
        result->setErrorState();
        return result;
    }
    auto setWeightResult = result->setWeight(context, descriptors.weight.isEmpty() ? "normal"_s : descriptors.weight);
    if (setWeightResult.hasException()) {
        result->setErrorState();
        return result;
    }
    auto setStretchResult = result->setStretch(context, descriptors.stretch.isEmpty() ? "normal"_s : descriptors.stretch);
    if (setStretchResult.hasException()) {
        result->setErrorState();
        return result;
    }
    auto setUnicodeRangeResult = result->setUnicodeRange(context, descriptors.unicodeRange.isEmpty() ? "U+0-10FFFF"_s : descriptors.unicodeRange);
    if (setUnicodeRangeResult.hasException()) {
        result->setErrorState();
        return result;
    }
    auto setFeatureSettingsResult = result->setFeatureSettings(context, descriptors.featureSettings.isEmpty() ? "normal"_s : descriptors.featureSettings);
    if (setFeatureSettingsResult.hasException()) {
        result->setErrorState();
        return result;
    }
    auto setDisplayResult = result->setDisplay(context, descriptors.display.isEmpty() ? "auto"_s : descriptors.display);
    if (setDisplayResult.hasException()) {
        result->setErrorState();
        return result;
    }

    if (!dataRequiresAsynchronousLoading) {
        result->backing().load();
        auto status = result->backing().status();
        ASSERT_UNUSED(status, status == CSSFontFace::Status::Success || status == CSSFontFace::Status::Failure);
    }

    return result;
}

Ref<FontFace> FontFace::create(ScriptExecutionContext* context, CSSFontFace& face)
{
    auto fontFace = adoptRef(*new FontFace(context, face));
    fontFace->suspendIfNeeded();
    return fontFace;
}

FontFace::FontFace(CSSFontSelector& fontSelector)
    : ActiveDOMObject(fontSelector.scriptExecutionContext())
    , m_backing(CSSFontFace::create(fontSelector, nullptr, this))
    , m_loadedPromise(makeUniqueRef<LoadedPromise>(*this, &FontFace::loadedPromiseResolve))
{
    m_backing->addClient(*this);
}

FontFace::FontFace(ScriptExecutionContext* context, CSSFontFace& face)
    : ActiveDOMObject(context)
    , m_backing(face)
    , m_loadedPromise(makeUniqueRef<LoadedPromise>(*this, &FontFace::loadedPromiseResolve))
{
    m_backing->addClient(*this);
}

FontFace::~FontFace()
{
    m_backing->removeClient(*this);
}

ExceptionOr<void> FontFace::setFamily(ScriptExecutionContext& context, const String& family)
{
    if (family.isEmpty())
        return Exception { SyntaxError };

    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=196381 Don't use a list here.
    // See consumeFontFamilyDescriptor() in CSSPropertyParser.cpp for why we're using it.
    auto list = CSSValueList::createCommaSeparated();
    list->append(context.cssValuePool().createFontFamilyValue(family));
    bool success = m_backing->setFamilies(list);
    if (!success)
        return Exception { SyntaxError };
    return { };
}

ExceptionOr<void> FontFace::setStyle(ScriptExecutionContext& context, const String& style)
{
    if (style.isEmpty())
        return Exception { SyntaxError };

    if (auto value = CSSPropertyParserWorkerSafe::parseFontFaceStyle(style, context)) {
        m_backing->setStyle(*value);
        return { };
    }
    return Exception { SyntaxError };
}

ExceptionOr<void> FontFace::setWeight(ScriptExecutionContext& context, const String& weight)
{
    if (weight.isEmpty())
        return Exception { SyntaxError };

    if (auto value = CSSPropertyParserWorkerSafe::parseFontFaceWeight(weight, context)) {
        m_backing->setWeight(*value);
        return { };
    }
    return Exception { SyntaxError };
}

ExceptionOr<void> FontFace::setStretch(ScriptExecutionContext& context, const String& stretch)
{
    if (stretch.isEmpty())
        return Exception { SyntaxError };

    if (auto value = CSSPropertyParserWorkerSafe::parseFontFaceStretch(stretch, context)) {
        m_backing->setStretch(*value);
        return { };
    }
    return Exception { SyntaxError };
}

ExceptionOr<void> FontFace::setUnicodeRange(ScriptExecutionContext& context, const String& unicodeRange)
{
    if (unicodeRange.isEmpty())
        return Exception { SyntaxError };

    bool success = false;
    if (auto value = CSSPropertyParserWorkerSafe::parseFontFaceUnicodeRange(unicodeRange, context))
        success = m_backing->setUnicodeRange(*value);
    if (!success)
        return Exception { SyntaxError };
    return { };
}

ExceptionOr<void> FontFace::setFeatureSettings(ScriptExecutionContext& context, const String& featureSettings)
{
    if (featureSettings.isEmpty())
        return Exception { SyntaxError };

    auto value = CSSPropertyParserWorkerSafe::parseFontFaceFeatureSettings(featureSettings, context);
    if (!value)
        return Exception { SyntaxError };
    m_backing->setFeatureSettings(*value);
    return { };
}

ExceptionOr<void> FontFace::setDisplay(ScriptExecutionContext& context, const String& display)
{
    if (display.isEmpty())
        return Exception { SyntaxError };

    if (auto value = CSSPropertyParserWorkerSafe::parseFontFaceDisplay(display, context)) {
        m_backing->setLoadingBehavior(*value);
        return { };
    }

    return Exception { SyntaxError };
}

String FontFace::family() const
{
    m_backing->updateStyleIfNeeded();

    const auto& families = m_backing->families();
    if (!families)
        return "normal"_s;
    auto familiesUnrwapped = families.value();
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=196381 This is only here because CSSFontFace erroneously uses a list of values instead of a single value.
    // See consumeFontFamilyDescriptor() in CSSPropertyParser.cpp.
    if (familiesUnrwapped->length() == 1) {
        if (familiesUnrwapped->item(0)) {
            auto& item = *familiesUnrwapped->item(0);
            if (item.isPrimitiveValue()) {
                auto& primitiveValue = downcast<CSSPrimitiveValue>(item);
                if (primitiveValue.isFontFamily()) {
                    auto& fontFamily = primitiveValue.fontFamily();
                    return fontFamily.familyName;
                }
            }
        }
    }
    return familiesUnrwapped->cssText();
}

String FontFace::style() const
{
    m_backing->updateStyleIfNeeded();
    const auto& styleWrapped = m_backing->italic();
    
    if (!styleWrapped)
        return "normal"_s;
    auto style = styleWrapped.value();
    auto minimum = ComputedStyleExtractor::fontStyleFromStyleValue(style.minimum, FontStyleAxis::ital);
    auto maximum = ComputedStyleExtractor::fontStyleFromStyleValue(style.maximum, FontStyleAxis::ital);

    if (minimum.get().equals(maximum.get()))
        return minimum->cssText();

    auto minimumNonKeyword = ComputedStyleExtractor::fontNonKeywordStyleFromStyleValue(style.minimum);
    auto maximumNonKeyword = ComputedStyleExtractor::fontNonKeywordStyleFromStyleValue(style.maximum);

    ASSERT(minimumNonKeyword->fontStyleValue->valueID() == CSSValueOblique);
    ASSERT(maximumNonKeyword->fontStyleValue->valueID() == CSSValueOblique);

    StringBuilder builder;
    builder.append(minimumNonKeyword->fontStyleValue->cssText());
    builder.append(' ');
    if (minimum->obliqueValue.get() == maximum->obliqueValue.get())
        builder.append(minimumNonKeyword->obliqueValue->cssText());
    else {
        builder.append(minimumNonKeyword->obliqueValue->cssText());
        builder.append(' ');
        builder.append(maximumNonKeyword->obliqueValue->cssText());
    }
    return builder.toString();
    
}

String FontFace::weight() const
{
    m_backing->updateStyleIfNeeded();
    const auto& weightWrapped = m_backing->weight();
    if (!weightWrapped)
        return "normal"_s;
    auto weight = weightWrapped.value();
    auto minimum = ComputedStyleExtractor::fontWeightFromStyleValue(weight.minimum);
    auto maximum = ComputedStyleExtractor::fontWeightFromStyleValue(weight.maximum);

    if (minimum.get().equals(maximum.get()))
        return minimum->cssText();

    auto minimumNonKeyword = ComputedStyleExtractor::fontNonKeywordWeightFromStyleValue(weight.minimum);
    auto maximumNonKeyword = ComputedStyleExtractor::fontNonKeywordWeightFromStyleValue(weight.maximum);

    StringBuilder builder;
    builder.append(minimumNonKeyword->cssText());
    builder.append(' ');
    builder.append(maximumNonKeyword->cssText());
    return builder.toString();
}

String FontFace::stretch() const
{
    m_backing->updateStyleIfNeeded();
    const auto& stretchWrapped = m_backing->stretch();
    if (!stretchWrapped)
        return "normal"_s;
    auto stretch = stretchWrapped.value();
    auto minimum = ComputedStyleExtractor::fontStretchFromStyleValue(stretch.minimum);
    auto maximum = ComputedStyleExtractor::fontStretchFromStyleValue(stretch.maximum);

    if (minimum.get().equals(maximum.get()))
        return minimum->cssText();

    auto minimumNonKeyword = ComputedStyleExtractor::fontNonKeywordStretchFromStyleValue(stretch.minimum);
    auto maximumNonKeyword = ComputedStyleExtractor::fontNonKeywordStretchFromStyleValue(stretch.maximum);

    StringBuilder builder;
    builder.append(minimumNonKeyword->cssText());
    builder.append(' ');
    builder.append(maximumNonKeyword->cssText());
    return builder.toString();
}

String FontFace::unicodeRange() const
{
    m_backing->updateStyleIfNeeded();
    const auto& rangesWrapped = m_backing->ranges();
    if (!rangesWrapped)
        return "U+0-10FFFF"_s;
    auto ranges = rangesWrapped.value();
    if (!ranges.size())
        return "U+0-10FFFF"_s;
    auto values = CSSValueList::createCommaSeparated();
    for (auto& range : ranges)
        values->append(CSSUnicodeRangeValue::create(range.from, range.to));
    return values->cssText();
}

String FontFace::featureSettings() const
{
    m_backing->updateStyleIfNeeded();
    const auto& featureSettingsWrapped = m_backing->featureSettings();
    if (!featureSettingsWrapped)
        return "normal"_s;
    auto featureSettings = featureSettingsWrapped.value();
    if (!featureSettings.size())
        return "normal"_s;
    auto list = CSSValueList::createCommaSeparated();
    for (auto& feature : featureSettings)
        list->append(CSSFontFeatureValue::create(FontTag(feature.tag()), feature.value()));
    return list->cssText();
}

String FontFace::display(ScriptExecutionContext& context) const
{
    m_backing->updateStyleIfNeeded();
    const auto& loadingBehaviorWrapped = m_backing->loadingBehavior();
    if (!loadingBehaviorWrapped)
        return autoAtom();
    return context.cssValuePool().createValue(loadingBehaviorWrapped.value())->cssText();
}

auto FontFace::status() const -> LoadStatus
{
    switch (m_backing->status()) {
    case CSSFontFace::Status::Pending:
        return LoadStatus::Unloaded;
    case CSSFontFace::Status::Loading:
        return LoadStatus::Loading;
    case CSSFontFace::Status::TimedOut:
        return LoadStatus::Error;
    case CSSFontFace::Status::Success:
        return LoadStatus::Loaded;
    case CSSFontFace::Status::Failure:
        return LoadStatus::Error;
    }
    ASSERT_NOT_REACHED();
    return LoadStatus::Error;
}

void FontFace::adopt(CSSFontFace& newFace)
{
    m_backing->removeClient(*this);
    m_backing = newFace;
    m_backing->addClient(*this);
    newFace.setWrapper(*this);
}

void FontFace::fontStateChanged(CSSFontFace& face, CSSFontFace::Status, CSSFontFace::Status newState)
{
    ASSERT_UNUSED(face, &face == m_backing.ptr());
    switch (newState) {
    case CSSFontFace::Status::Loading:
        break;
    case CSSFontFace::Status::TimedOut:
        break;
    case CSSFontFace::Status::Success:
        // FIXME: This check should not be needed, but because FontFace's are sometimes adopted after they have already
        // gone through a load cycle, we can sometimes come back through here and try to resolve the promise again.
        if (!m_loadedPromise->isFulfilled())
            m_loadedPromise->resolve(*this);
        return;
    case CSSFontFace::Status::Failure:
        // FIXME: This check should not be needed, but because FontFace's are sometimes adopted after they have already
        // gone through a load cycle, we can sometimes come back through here and try to resolve the promise again.
        if (!m_loadedPromise->isFulfilled())
            m_loadedPromise->reject(Exception { NetworkError });
        return;
    case CSSFontFace::Status::Pending:
        ASSERT_NOT_REACHED();
        return;
    }
}

auto FontFace::loadForBindings() -> LoadedPromise&
{
    m_mayLoadedPromiseBeScriptObservable = true;
    m_backing->load();
    return m_loadedPromise.get();
}

auto FontFace::loadedForBindings() -> LoadedPromise&
{
    m_mayLoadedPromiseBeScriptObservable = true;
    return m_loadedPromise.get();
}

FontFace& FontFace::loadedPromiseResolve()
{
    return *this;
}

const char* FontFace::activeDOMObjectName() const
{
    return "FontFace";
}

bool FontFace::virtualHasPendingActivity() const
{
    return m_mayLoadedPromiseBeScriptObservable && !m_loadedPromise->isFulfilled();
}

}
