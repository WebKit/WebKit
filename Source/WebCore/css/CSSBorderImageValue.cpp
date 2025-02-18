/*
 * Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "CSSBorderImageValue.h"

#include "CSSBorderImageOutsetValue.h"
#include "CSSBorderImageRepeatValue.h"
#include "CSSBorderImageSliceValue.h"
#include "CSSBorderImageSourceValue.h"
#include "CSSBorderImageWidthValue.h"
#include "CSSPrimitiveNumericTypes+CSSValueVisitation.h"
#include "CSSPrimitiveNumericTypes+Serialization.h"
#include "DeprecatedCSSOMPrimitiveValue.h"
#include "DeprecatedCSSOMValueList.h"

namespace WebCore {

CSSBorderImageValue::CSSBorderImageValue(CSS::BorderImage&& borderImage)
    : CSSValue(ClassType::BorderImage)
    , m_borderImage(WTFMove(borderImage))
{
}

CSSBorderImageValue::~CSSBorderImageValue() = default;

Ref<CSSBorderImageValue> CSSBorderImageValue::create(CSS::BorderImage&& borderImage)
{
    return adoptRef(*new CSSBorderImageValue(WTFMove(borderImage)));
}

String CSSBorderImageValue::customCSSText(const CSS::SerializationContext& context) const
{
    return CSS::serializationForCSS(context, m_borderImage);
}

bool CSSBorderImageValue::equals(const CSSBorderImageValue& other) const
{
    return m_borderImage == other.m_borderImage;
}

IterationStatus CSSBorderImageValue::customVisitChildren(NOESCAPE const Function<IterationStatus(CSSValue&)>& func) const
{
    return CSS::visitCSSValueChildren(func, m_borderImage);
}

Ref<DeprecatedCSSOMValue> CSSBorderImageValue::createDeprecatedCSSOMWrapper(CSSStyleDeclaration& owner) const
{
    if (!m_borderImage.source || WTF::holdsAlternative<CSS::Keyword::None>(*m_borderImage.source))
        return DeprecatedCSSOMPrimitiveValue::create(CSSPrimitiveValue::create(CSSValueNone), owner);

    Vector<Ref<DeprecatedCSSOMValue>, 4> list;
    list.append(CSSBorderImageSourceValue::create(*m_borderImage.source)->createDeprecatedCSSOMWrapper(owner));

    if (m_borderImage.width || m_borderImage.outset) {
        Vector<Ref<DeprecatedCSSOMValue>, 4> listSlash;
        if (m_borderImage.slice)
            listSlash.append(CSSBorderImageSliceValue::create(*m_borderImage.slice)->createDeprecatedCSSOMWrapper(owner));
        if (m_borderImage.width)
            listSlash.append(CSSBorderImageWidthValue::create(*m_borderImage.width)->createDeprecatedCSSOMWrapper(owner));
        if (m_borderImage.outset)
            listSlash.append(CSSBorderImageOutsetValue::create(*m_borderImage.outset)->createDeprecatedCSSOMWrapper(owner));
        list.append(DeprecatedCSSOMValueList::create(WTFMove(listSlash), CSSValue::SlashSeparator, owner));
    } else if (m_borderImage.slice)
        list.append(CSSBorderImageSliceValue::create(*m_borderImage.slice)->createDeprecatedCSSOMWrapper(owner));
    if (m_borderImage.repeat)
        list.append(CSSBorderImageRepeatValue::create(*m_borderImage.repeat)->createDeprecatedCSSOMWrapper(owner));

    return DeprecatedCSSOMValueList::create(WTFMove(list), CSSValue::SpaceSeparator, owner);
}

} // namespace WebCore
