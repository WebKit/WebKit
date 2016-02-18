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

#include "CSSFontFace.h"
#include "CSSFontFeatureValue.h"
#include "CSSFontSelector.h"
#include "CSSUnicodeRangeValue.h"
#include "CSSValue.h"
#include "CSSValuePool.h"
#include "Dictionary.h"
#include "Document.h"
#include "ExceptionCodeDescription.h"
#include "FontVariantBuilder.h"
#include "JSDOMCoreException.h"
#include "JSFontFace.h"
#include "ScriptExecutionContext.h"
#include "StyleProperties.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

static FontFace::Promise createPromise(JSC::ExecState& exec)
{
    JSDOMGlobalObject& globalObject = *JSC::jsCast<JSDOMGlobalObject*>(exec.lexicalGlobalObject());
    return FontFace::Promise(DeferredWrapper(&exec, &globalObject, JSC::JSPromiseDeferred::create(&exec, &globalObject)));
}

static inline Optional<String> valueFromDictionary(const Dictionary& dictionary, const char* key)
{
    String result;
    dictionary.get(key, result);
    return result.isNull() ? Nullopt : Optional<String>(result);
}

RefPtr<FontFace> FontFace::create(JSC::ExecState& execState, ScriptExecutionContext& context, const String& family, const Deprecated::ScriptValue& source, const Dictionary& descriptors, ExceptionCode& ec)
{
    if (!context.isDocument()) {
        ec = TypeError;
        return nullptr;
    }

    Ref<FontFace> result = adoptRef(*new FontFace(execState, downcast<Document>(context).fontSelector()));

    result->setFamily(family, ec);
    if (ec)
        return nullptr;

    if (source.jsValue().isString()) {
        String sourceString = source.jsValue().toString(&execState)->value(&execState);
        auto value = FontFace::parseString(sourceString, CSSPropertySrc);
        if (is<CSSValueList>(value.get())) {
            CSSValueList& srcList = downcast<CSSValueList>(*value);
            CSSFontSelector::appendSources(result->backing(), srcList, &downcast<Document>(context), false);
        } else {
            ec = SYNTAX_ERR;
            return nullptr;
        }
    }

    if (auto style = valueFromDictionary(descriptors, "style"))
        result->setStyle(style.value(), ec);
    if (ec)
        return nullptr;
    if (auto style = valueFromDictionary(descriptors, "weight"))
        result->setWeight(style.value(), ec);
    if (ec)
        return nullptr;
    if (auto style = valueFromDictionary(descriptors, "stretch"))
        result->setStretch(style.value(), ec);
    if (ec)
        return nullptr;
    if (auto style = valueFromDictionary(descriptors, "unicodeRange"))
        result->setUnicodeRange(style.value(), ec);
    if (ec)
        return nullptr;
    if (auto style = valueFromDictionary(descriptors, "variant"))
        result->setVariant(style.value(), ec);
    if (ec)
        return nullptr;
    if (auto style = valueFromDictionary(descriptors, "featureSettings"))
        result->setFeatureSettings(style.value(), ec);
    if (ec)
        return nullptr;

    return result.ptr();
}

FontFace::FontFace(JSC::ExecState& execState, CSSFontSelector& fontSelector)
    : m_backing(CSSFontFace::create(fontSelector, this))
    , m_promise(createPromise(execState))
{
    m_backing->addClient(*this);
}

FontFace::~FontFace()
{
    m_backing->removeClient(*this);
}

RefPtr<CSSValue> FontFace::parseString(const String& string, CSSPropertyID propertyID)
{
    Ref<MutableStyleProperties> style = MutableStyleProperties::create();
    auto result = CSSParser::parseValue(style.ptr(), propertyID, string, true, CSSStrictMode, nullptr);
    if (result == CSSParser::ParseResult::Error)
        return nullptr;
    return style->getPropertyCSSValue(propertyID);
}

void FontFace::setFamily(const String& family, ExceptionCode& ec)
{
    bool success = false;
    if (auto value = parseString(family, CSSPropertyFontFamily))
        success = m_backing->setFamilies(*value);
    if (!success)
        ec = SYNTAX_ERR;
}

void FontFace::setStyle(const String& style, ExceptionCode& ec)
{
    bool success = false;
    if (auto value = parseString(style, CSSPropertyFontStyle))
        success = m_backing->setStyle(*value);
    if (!success)
        ec = SYNTAX_ERR;
}

void FontFace::setWeight(const String& weight, ExceptionCode& ec)
{
    bool success = false;
    if (auto value = parseString(weight, CSSPropertyFontWeight))
        success = m_backing->setWeight(*value);
    if (!success)
        ec = SYNTAX_ERR;
}

void FontFace::setStretch(const String&, ExceptionCode&)
{
    // We don't support font-stretch. Swallow the call.
}

void FontFace::setUnicodeRange(const String& unicodeRange, ExceptionCode& ec)
{
    bool success = false;
    if (auto value = parseString(unicodeRange, CSSPropertyUnicodeRange))
        success = m_backing->setUnicodeRange(*value);
    if (!success)
        ec = SYNTAX_ERR;
}

void FontFace::setVariant(const String& variant, ExceptionCode& ec)
{
    Ref<MutableStyleProperties> style = MutableStyleProperties::create();
    auto result = CSSParser::parseValue(style.ptr(), CSSPropertyFontVariant, variant, true, CSSStrictMode, nullptr);
    if (result != CSSParser::ParseResult::Error) {
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

        if (success)
            return;
        m_backing->setVariantSettings(backup);
    }
    ec = SYNTAX_ERR;
}

void FontFace::setFeatureSettings(const String& featureSettings, ExceptionCode& ec)
{
    bool success = false;
    if (auto value = parseString(featureSettings, CSSPropertyFontFeatureSettings))
        success = m_backing->setFeatureSettings(*value);
    if (!success)
        ec = SYNTAX_ERR;
}

String FontFace::family() const
{
    return m_backing->families()->cssText();
}

String FontFace::style() const
{
    switch (m_backing->traitsMask() & FontStyleMask) {
    case FontStyleNormalMask:
        return String("normal", String::ConstructFromLiteral);
    case FontStyleItalicMask:
        return String("italic", String::ConstructFromLiteral);
    }
    ASSERT_NOT_REACHED();
    return String("normal", String::ConstructFromLiteral);
}

String FontFace::weight() const
{
    switch (m_backing->traitsMask() & FontWeightMask) {
    case FontWeight100Mask:
        return String("100", String::ConstructFromLiteral);
    case FontWeight200Mask:
        return String("200", String::ConstructFromLiteral);
    case FontWeight300Mask:
        return String("300", String::ConstructFromLiteral);
    case FontWeight400Mask:
        return String("normal", String::ConstructFromLiteral);
    case FontWeight500Mask:
        return String("500", String::ConstructFromLiteral);
    case FontWeight600Mask:
        return String("600", String::ConstructFromLiteral);
    case FontWeight700Mask:
        return String("bold", String::ConstructFromLiteral);
    case FontWeight800Mask:
        return String("800", String::ConstructFromLiteral);
    case FontWeight900Mask:
        return String("900", String::ConstructFromLiteral);
    }
    ASSERT_NOT_REACHED();
    return String("normal", String::ConstructFromLiteral);
}

String FontFace::stretch() const
{
    return "normal";
}

String FontFace::unicodeRange() const
{
    if (!m_backing->ranges().size())
        return "U+0-10FFFF";
    RefPtr<CSSValueList> values = CSSValueList::createCommaSeparated();
    for (auto& range : m_backing->ranges())
        values->append(CSSUnicodeRangeValue::create(range.from(), range.to()));
    return values->cssText();
}

String FontFace::variant() const
{
    return computeFontVariant(m_backing->variantSettings())->cssText();
}

String FontFace::featureSettings() const
{
    if (!m_backing->featureSettings().size())
        return "normal";
    RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
    for (auto& feature : m_backing->featureSettings())
        list->append(CSSFontFeatureValue::create(FontFeatureTag(feature.tag()), feature.value()));
    return list->cssText();
}

String FontFace::status() const
{
    switch (m_backing->status()) {
    case CSSFontFace::Status::Pending:
        return String("unloaded", String::ConstructFromLiteral);
    case CSSFontFace::Status::Loading:
        return String("loading", String::ConstructFromLiteral);
    case CSSFontFace::Status::TimedOut:
        return String("error", String::ConstructFromLiteral);
    case CSSFontFace::Status::Success:
        return String("loaded", String::ConstructFromLiteral);
    case CSSFontFace::Status::Failure:
        return String("error", String::ConstructFromLiteral);
    }
    ASSERT_NOT_REACHED();
    return String("error", String::ConstructFromLiteral);
}

void FontFace::stateChanged(CSSFontFace& face, CSSFontFace::Status, CSSFontFace::Status newState)
{
    ASSERT_UNUSED(face, &face == m_backing.ptr());
    switch (newState) {
    case CSSFontFace::Status::TimedOut:
        rejectPromise(NETWORK_ERR);
        return;
    case CSSFontFace::Status::Success:
        fulfillPromise();
        return;
    case CSSFontFace::Status::Failure:
        rejectPromise(NETWORK_ERR);
        return;
    default:
        return;
    }
}

void FontFace::load()
{
    m_backing->load();
}

void FontFace::fulfillPromise()
{
    // Normally, DeferredWrapper::callFunction resets the reference to the promise.
    // However, API semantics require our promise to live for the entire lifetime of the FontFace.
    // Let's make sure it stays alive.

    Promise guard(m_promise);
    m_promise.resolve(*this);
    m_promise = guard;
}

void FontFace::rejectPromise(ExceptionCode code)
{
    // Normally, DeferredWrapper::callFunction resets the reference to the promise.
    // However, API semantics require our promise to live for the entire lifetime of the FontFace.
    // Let's make sure it stays alive.

    Promise guard(m_promise);
    m_promise.reject(DOMCoreException::create(ExceptionCodeDescription(code)).get());
    m_promise = guard;
}

}
