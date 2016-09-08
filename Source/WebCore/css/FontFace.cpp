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

#include "CSSFontFaceSource.h"
#include "CSSFontFeatureValue.h"
#include "CSSParser.h"
#include "CSSUnicodeRangeValue.h"
#include "CSSValuePool.h"
#include "Document.h"
#include "FontVariantBuilder.h"
#include "JSFontFace.h"
#include "StyleProperties.h"

namespace WebCore {

static bool populateFontFaceWithArrayBuffer(CSSFontFace& fontFace, Ref<JSC::ArrayBufferView>&& arrayBufferView)
{
    auto source = std::make_unique<CSSFontFaceSource>(fontFace, String(), nullptr, nullptr, WTFMove(arrayBufferView));
    fontFace.adoptSource(WTFMove(source));
    return false;
}

RefPtr<FontFace> FontFace::create(JSC::ExecState& state, Document& document, const String& family, JSC::JSValue source, const Descriptors& descriptors, ExceptionCode& ec)
{
    auto result = adoptRef(*new FontFace(document.fontSelector()));

    bool dataRequiresAsynchronousLoading = true;

    ec = 0;
    result->setFamily(family, ec);
    if (ec)
        return nullptr;

    if (source.isString()) {
        auto value = FontFace::parseString(source.getString(&state), CSSPropertySrc);
        if (!is<CSSValueList>(value.get())) {
            ec = SYNTAX_ERR;
            return nullptr;
        }
        CSSFontFace::appendSources(result->backing(), downcast<CSSValueList>(*value), &document, false);
    } else if (auto arrayBufferView = toArrayBufferView(source))
        dataRequiresAsynchronousLoading = populateFontFaceWithArrayBuffer(result->backing(), arrayBufferView.releaseNonNull());
    else if (auto arrayBuffer = toArrayBuffer(source)) {
        auto arrayBufferView = JSC::Uint8Array::create(arrayBuffer, 0, arrayBuffer->byteLength());
        dataRequiresAsynchronousLoading = populateFontFaceWithArrayBuffer(result->backing(), arrayBufferView.releaseNonNull());
    }

    // These ternaries match the default strings inside the FontFaceDescriptors dictionary inside FontFace.idl.
    result->setStyle(descriptors.style.isEmpty() ? ASCIILiteral("normal") : descriptors.style, ec);
    if (ec)
        return nullptr;
    result->setWeight(descriptors.weight.isEmpty() ? ASCIILiteral("normal") : descriptors.weight, ec);
    if (ec)
        return nullptr;
    result->setStretch(descriptors.stretch.isEmpty() ? ASCIILiteral("normal") : descriptors.stretch, ec);
    if (ec)
        return nullptr;
    result->setUnicodeRange(descriptors.unicodeRange.isEmpty() ? ASCIILiteral("U+0-10FFFF") : descriptors.unicodeRange, ec);
    if (ec)
        return nullptr;
    result->setVariant(descriptors.variant.isEmpty() ? ASCIILiteral("normal") : descriptors.variant, ec);
    if (ec)
        return nullptr;
    result->setFeatureSettings(descriptors.featureSettings.isEmpty() ? ASCIILiteral("normal") : descriptors.featureSettings, ec);
    if (ec)
        return nullptr;

    if (!dataRequiresAsynchronousLoading) {
        result->backing().load();
        ASSERT(result->backing().status() == CSSFontFace::Status::Success);
    }

    return WTFMove(result);
}

Ref<FontFace> FontFace::create(CSSFontFace& face)
{
    return adoptRef(*new FontFace(face));
}

FontFace::FontFace(CSSFontSelector& fontSelector)
    : m_weakPtrFactory(this)
    , m_backing(CSSFontFace::create(&fontSelector, nullptr, this))
{
    m_backing->addClient(*this);
}

FontFace::FontFace(CSSFontFace& face)
    : m_weakPtrFactory(this)
    , m_backing(face)
{
    m_backing->addClient(*this);
}

FontFace::~FontFace()
{
    m_backing->removeClient(*this);
}

WeakPtr<FontFace> FontFace::createWeakPtr() const
{
    return m_weakPtrFactory.createWeakPtr();
}

RefPtr<CSSValue> FontFace::parseString(const String& string, CSSPropertyID propertyID)
{
    auto style = MutableStyleProperties::create();
    if (CSSParser::parseValue(style, propertyID, string, true, CSSStrictMode, nullptr) == CSSParser::ParseResult::Error)
        return nullptr;
    return style->getPropertyCSSValue(propertyID);
}

void FontFace::setFamily(const String& family, ExceptionCode& ec)
{
    if (family.isEmpty()) {
        ec = SYNTAX_ERR;
        return;
    }

    bool success = false;
    if (auto value = parseString(family, CSSPropertyFontFamily))
        success = m_backing->setFamilies(*value);
    if (!success)
        ec = SYNTAX_ERR;
}

void FontFace::setStyle(const String& style, ExceptionCode& ec)
{
    if (style.isEmpty()) {
        ec = SYNTAX_ERR;
        return;
    }

    bool success = false;
    if (auto value = parseString(style, CSSPropertyFontStyle))
        success = m_backing->setStyle(*value);
    if (!success)
        ec = SYNTAX_ERR;
}

void FontFace::setWeight(const String& weight, ExceptionCode& ec)
{
    if (weight.isEmpty()) {
        ec = SYNTAX_ERR;
        return;
    }

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
    if (unicodeRange.isEmpty()) {
        ec = SYNTAX_ERR;
        return;
    }

    bool success = false;
    if (auto value = parseString(unicodeRange, CSSPropertyUnicodeRange))
        success = m_backing->setUnicodeRange(*value);
    if (!success)
        ec = SYNTAX_ERR;
}

void FontFace::setVariant(const String& variant, ExceptionCode& ec)
{
    if (variant.isEmpty()) {
        ec = SYNTAX_ERR;
        return;
    }

    auto style = MutableStyleProperties::create();
    auto result = CSSParser::parseValue(style, CSSPropertyFontVariant, variant, true, CSSStrictMode, nullptr);
    if (result == CSSParser::ParseResult::Error) {
        ec = SYNTAX_ERR;
        return;
    }

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
        ec = SYNTAX_ERR;
    }
}

void FontFace::setFeatureSettings(const String& featureSettings, ExceptionCode& ec)
{
    if (featureSettings.isEmpty()) {
        ec = SYNTAX_ERR;
        return;
    }

    auto value = parseString(featureSettings, CSSPropertyFontFeatureSettings);
    if (!value) {
        ec = SYNTAX_ERR;
        return;
    }
    m_backing->setFeatureSettings(*value);
}

String FontFace::family() const
{
    const_cast<CSSFontFace&>(m_backing.get()).updateStyleIfNeeded();
    return m_backing->families()->cssText();
}

String FontFace::style() const
{
    const_cast<CSSFontFace&>(m_backing.get()).updateStyleIfNeeded();
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
    const_cast<CSSFontFace&>(m_backing.get()).updateStyleIfNeeded();
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
    return ASCIILiteral("normal");
}

String FontFace::unicodeRange() const
{
    const_cast<CSSFontFace&>(m_backing.get()).updateStyleIfNeeded();
    if (!m_backing->ranges().size())
        return ASCIILiteral("U+0-10FFFF");
    RefPtr<CSSValueList> values = CSSValueList::createCommaSeparated();
    for (auto& range : m_backing->ranges())
        values->append(CSSUnicodeRangeValue::create(range.from, range.to));
    return values->cssText();
}

String FontFace::variant() const
{
    const_cast<CSSFontFace&>(m_backing.get()).updateStyleIfNeeded();
    return computeFontVariant(m_backing->variantSettings())->cssText();
}

String FontFace::featureSettings() const
{
    const_cast<CSSFontFace&>(m_backing.get()).updateStyleIfNeeded();
    if (!m_backing->featureSettings().size())
        return ASCIILiteral("normal");
    RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
    for (auto& feature : m_backing->featureSettings())
        list->append(CSSFontFeatureValue::create(FontFeatureTag(feature.tag()), feature.value()));
    return list->cssText();
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
    m_promise = Nullopt;
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
        if (m_promise)
            std::exchange(m_promise, Nullopt)->resolve(*this);
        deref();
        return;
    case CSSFontFace::Status::Failure:
        if (m_promise)
            std::exchange(m_promise, Nullopt)->reject(NETWORK_ERR);
        deref();
        return;
    case CSSFontFace::Status::Pending:
        ASSERT_NOT_REACHED();
        return;
    }
}

void FontFace::registerLoaded(Promise&& promise)
{
    ASSERT(!m_promise);
    switch (m_backing->status()) {
    case CSSFontFace::Status::Loading:
    case CSSFontFace::Status::Pending:
        m_promise = WTFMove(promise);
        return;
    case CSSFontFace::Status::Success:
        promise.resolve(*this);
        return;
    case CSSFontFace::Status::TimedOut:
    case CSSFontFace::Status::Failure:
        promise.reject(NETWORK_ERR);
        return;
    }
}

void FontFace::load()
{
    m_backing->load();
}

}
