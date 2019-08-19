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
#include "CSSFontStyleValue.h"
#include "CSSParser.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSUnicodeRangeValue.h"
#include "CSSValueList.h"
#include "CSSValuePool.h"
#include "Document.h"
#include "FontVariantBuilder.h"
#include "JSFontFace.h"
#include "Quirks.h"
#include "StyleProperties.h"
#include <JavaScriptCore/ArrayBuffer.h>
#include <JavaScriptCore/ArrayBufferView.h>
#include <JavaScriptCore/JSCInlines.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

static bool populateFontFaceWithArrayBuffer(CSSFontFace& fontFace, Ref<JSC::ArrayBufferView>&& arrayBufferView)
{
    auto source = makeUnique<CSSFontFaceSource>(fontFace, String(), nullptr, nullptr, WTFMove(arrayBufferView));
    fontFace.adoptSource(WTFMove(source));
    return false;
}

ExceptionOr<Ref<FontFace>> FontFace::create(Document& document, const String& family, Source&& source, const Descriptors& descriptors)
{
    auto result = adoptRef(*new FontFace(document.fontSelector()));

    bool dataRequiresAsynchronousLoading = true;

    auto setFamilyResult = result->setFamily(document, family);
    if (setFamilyResult.hasException())
        return setFamilyResult.releaseException();

    auto sourceConversionResult = WTF::switchOn(source,
        [&] (String& string) -> ExceptionOr<void> {
            auto value = FontFace::parseString(string, CSSPropertySrc);
            if (!is<CSSValueList>(value))
                return Exception { SyntaxError };
            CSSFontFace::appendSources(result->backing(), downcast<CSSValueList>(*value), &document, false);
            return { };
        },
        [&] (RefPtr<ArrayBufferView>& arrayBufferView) -> ExceptionOr<void> {
            dataRequiresAsynchronousLoading = populateFontFaceWithArrayBuffer(result->backing(), arrayBufferView.releaseNonNull());
            return { };
        },
        [&] (RefPtr<ArrayBuffer>& arrayBuffer) -> ExceptionOr<void> {
            unsigned byteLength = arrayBuffer->byteLength();
            auto arrayBufferView = JSC::Uint8Array::create(WTFMove(arrayBuffer), 0, byteLength);
            dataRequiresAsynchronousLoading = populateFontFaceWithArrayBuffer(result->backing(), WTFMove(arrayBufferView));
            return { };
        }
    );

    if (sourceConversionResult.hasException())
        return sourceConversionResult.releaseException();

    // These ternaries match the default strings inside the FontFaceDescriptors dictionary inside FontFace.idl.
    auto setStyleResult = result->setStyle(descriptors.style.isEmpty() ? "normal"_s : descriptors.style);
    if (setStyleResult.hasException())
        return setStyleResult.releaseException();
    auto setWeightResult = result->setWeight(descriptors.weight.isEmpty() ? "normal"_s : descriptors.weight);
    if (setWeightResult.hasException())
        return setWeightResult.releaseException();
    auto setStretchResult = result->setStretch(descriptors.stretch.isEmpty() ? "normal"_s : descriptors.stretch);
    if (setStretchResult.hasException())
        return setStretchResult.releaseException();
    auto setUnicodeRangeResult = result->setUnicodeRange(descriptors.unicodeRange.isEmpty() ? "U+0-10FFFF"_s : descriptors.unicodeRange);
    if (setUnicodeRangeResult.hasException())
        return setUnicodeRangeResult.releaseException();
    auto setVariantResult = result->setVariant(descriptors.variant.isEmpty() ? "normal"_s : descriptors.variant);
    if (setVariantResult.hasException())
        return setVariantResult.releaseException();
    auto setFeatureSettingsResult = result->setFeatureSettings(descriptors.featureSettings.isEmpty() ? "normal"_s : descriptors.featureSettings);
    if (setFeatureSettingsResult.hasException())
        return setFeatureSettingsResult.releaseException();
    auto setDisplayResult = result->setDisplay(descriptors.display.isEmpty() ? "auto"_s : descriptors.display);
    if (setDisplayResult.hasException())
        return setDisplayResult.releaseException();

    if (!dataRequiresAsynchronousLoading) {
        result->backing().load();
        auto status = result->backing().status();
        ASSERT_UNUSED(status, status == CSSFontFace::Status::Success || status == CSSFontFace::Status::Failure);
    }

    return result;
}

Ref<FontFace> FontFace::create(CSSFontFace& face)
{
    return adoptRef(*new FontFace(face));
}

FontFace::FontFace(CSSFontSelector& fontSelector)
    : m_backing(CSSFontFace::create(&fontSelector, nullptr, this))
    , m_loadedPromise(*this, &FontFace::loadedPromiseResolve)
{
    m_backing->addClient(*this);
}

FontFace::FontFace(CSSFontFace& face)
    : m_backing(face)
    , m_loadedPromise(*this, &FontFace::loadedPromiseResolve)
{
    m_backing->addClient(*this);
}

FontFace::~FontFace()
{
    m_backing->removeClient(*this);
}

RefPtr<CSSValue> FontFace::parseString(const String& string, CSSPropertyID propertyID)
{
    // FIXME: Should use the Document to get the right parsing mode.
    return CSSParser::parseFontFaceDescriptor(propertyID, string, HTMLStandardMode);
}

ExceptionOr<void> FontFace::setFamily(Document& document, const String& family)
{
    if (family.isEmpty())
        return Exception { SyntaxError };

    String familyNameToUse = family;
    if (familyNameToUse.contains('\'') && document.quirks().shouldStripQuotationMarkInFontFaceSetFamily())
        familyNameToUse = family.removeCharacters([](auto character) { return character == '\''; });

    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=196381 Don't use a list here.
    // See consumeFontFamilyDescriptor() in CSSPropertyParser.cpp for why we're using it.
    auto list = CSSValueList::createCommaSeparated();
    list->append(CSSValuePool::singleton().createFontFamilyValue(familyNameToUse));
    bool success = m_backing->setFamilies(list);
    if (!success)
        return Exception { SyntaxError };
    return { };
}

ExceptionOr<void> FontFace::setStyle(const String& style)
{
    if (style.isEmpty())
        return Exception { SyntaxError };

    if (auto value = parseString(style, CSSPropertyFontStyle)) {
        m_backing->setStyle(*value);
        return { };
    }
    return Exception { SyntaxError };
}

ExceptionOr<void> FontFace::setWeight(const String& weight)
{
    if (weight.isEmpty())
        return Exception { SyntaxError };

    if (auto value = parseString(weight, CSSPropertyFontWeight)) {
        m_backing->setWeight(*value);
        return { };
    }
    return Exception { SyntaxError };
}

ExceptionOr<void> FontFace::setStretch(const String& stretch)
{
    if (stretch.isEmpty())
        return Exception { SyntaxError };

    if (auto value = parseString(stretch, CSSPropertyFontStretch)) {
        m_backing->setStretch(*value);
        return { };
    }
    return Exception { SyntaxError };
}

ExceptionOr<void> FontFace::setUnicodeRange(const String& unicodeRange)
{
    if (unicodeRange.isEmpty())
        return Exception { SyntaxError };

    bool success = false;
    if (auto value = parseString(unicodeRange, CSSPropertyUnicodeRange))
        success = m_backing->setUnicodeRange(*value);
    if (!success)
        return Exception { SyntaxError };
    return { };
}

ExceptionOr<void> FontFace::setVariant(const String& variant)
{
    if (variant.isEmpty())
        return Exception { SyntaxError };

    auto style = MutableStyleProperties::create();
    auto result = CSSParser::parseValue(style, CSSPropertyFontVariant, variant, true, HTMLStandardMode);
    if (result == CSSParser::ParseResult::Error)
        return Exception { SyntaxError };

    // FIXME: Would be much better to stage the new settings and set them all at once
    // instead of this dance where we make a backup and revert to it if something fails.
    FontVariantSettings backup = m_backing->variantSettings();

    auto normal = CSSValuePool::singleton().createIdentifierValue(CSSValueNormal);
    bool success = true;

    if (auto value = style->getPropertyCSSValue(CSSPropertyFontVariantLigatures))
        success &= m_backing->setVariantLigatures(*value);
    else
        m_backing->setVariantLigatures(normal);

    if (auto value = style->getPropertyCSSValue(CSSPropertyFontVariantPosition))
        success &= m_backing->setVariantPosition(*value);
    else
        m_backing->setVariantPosition(normal);

    if (auto value = style->getPropertyCSSValue(CSSPropertyFontVariantCaps))
        success &= m_backing->setVariantCaps(*value);
    else
        m_backing->setVariantCaps(normal);

    if (auto value = style->getPropertyCSSValue(CSSPropertyFontVariantNumeric))
        success &= m_backing->setVariantNumeric(*value);
    else
        m_backing->setVariantNumeric(normal);

    if (auto value = style->getPropertyCSSValue(CSSPropertyFontVariantAlternates))
        success &= m_backing->setVariantAlternates(*value);
    else
        m_backing->setVariantAlternates(normal);

    if (auto value = style->getPropertyCSSValue(CSSPropertyFontVariantEastAsian))
        success &= m_backing->setVariantEastAsian(*value);
    else
        m_backing->setVariantEastAsian(normal);

    if (!success) {
        m_backing->setVariantSettings(backup);
        return Exception { SyntaxError };
    }

    return { };
}

ExceptionOr<void> FontFace::setFeatureSettings(const String& featureSettings)
{
    if (featureSettings.isEmpty())
        return Exception { SyntaxError };

    auto value = parseString(featureSettings, CSSPropertyFontFeatureSettings);
    if (!value)
        return Exception { SyntaxError };
    m_backing->setFeatureSettings(*value);
    return { };
}

ExceptionOr<void> FontFace::setDisplay(const String& display)
{
    if (display.isEmpty())
        return Exception { SyntaxError };

    if (auto value = parseString(display, CSSPropertyFontDisplay)) {
        m_backing->setLoadingBehavior(*value);
        return { };
    }

    return Exception { SyntaxError };
}

String FontFace::family() const
{
    m_backing->updateStyleIfNeeded();

    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=196381 This is only here because CSSFontFace erroneously uses a list of values instead of a single value.
    // See consumeFontFamilyDescriptor() in CSSPropertyParser.cpp.
    if (m_backing->families()->length() == 1) {
        if (m_backing->families()->item(0)) {
            auto& item = *m_backing->families()->item(0);
            if (item.isPrimitiveValue()) {
                auto& primitiveValue = downcast<CSSPrimitiveValue>(item);
                if (primitiveValue.isFontFamily()) {
                    auto& fontFamily = primitiveValue.fontFamily();
                    return fontFamily.familyName;
                }
            }
        }
    }
    return m_backing->families()->cssText();
}

String FontFace::style() const
{
    m_backing->updateStyleIfNeeded();
    auto style = m_backing->italic();

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
    auto weight = m_backing->weight();

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
    auto stretch = m_backing->stretch();

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
    if (!m_backing->ranges().size())
        return "U+0-10FFFF"_s;
    auto values = CSSValueList::createCommaSeparated();
    for (auto& range : m_backing->ranges())
        values->append(CSSUnicodeRangeValue::create(range.from, range.to));
    return values->cssText();
}

String FontFace::variant() const
{
    m_backing->updateStyleIfNeeded();
    return computeFontVariant(m_backing->variantSettings())->cssText();
}

String FontFace::featureSettings() const
{
    m_backing->updateStyleIfNeeded();
    if (!m_backing->featureSettings().size())
        return "normal"_s;
    auto list = CSSValueList::createCommaSeparated();
    for (auto& feature : m_backing->featureSettings())
        list->append(CSSFontFeatureValue::create(FontTag(feature.tag()), feature.value()));
    return list->cssText();
}

String FontFace::display() const
{
    m_backing->updateStyleIfNeeded();
    return CSSValuePool::singleton().createValue(m_backing->loadingBehavior())->cssText();
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
        // We still need to resolve promises when loading completes, even if all references to use have fallen out of scope.
        ref();
        break;
    case CSSFontFace::Status::TimedOut:
        break;
    case CSSFontFace::Status::Success:
        // FIXME: This check should not be needed, but because FontFace's are sometimes adopted after they have already
        // gone through a load cycle, we can sometimes come back through here and try to resolve the promise again.  
        if (!m_loadedPromise.isFulfilled())
            m_loadedPromise.resolve(*this);
        deref();
        return;
    case CSSFontFace::Status::Failure:
        // FIXME: This check should not be needed, but because FontFace's are sometimes adopted after they have already
        // gone through a load cycle, we can sometimes come back through here and try to resolve the promise again.  
        if (!m_loadedPromise.isFulfilled())
            m_loadedPromise.reject(Exception { NetworkError });
        deref();
        return;
    case CSSFontFace::Status::Pending:
        ASSERT_NOT_REACHED();
        return;
    }
}

auto FontFace::load() -> LoadedPromise&
{
    m_backing->load();
    return m_loadedPromise;
}

FontFace& FontFace::loadedPromiseResolve()
{
    return *this;
}

}
