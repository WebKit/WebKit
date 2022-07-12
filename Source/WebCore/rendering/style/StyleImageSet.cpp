/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005-2008, 2016 Apple Inc. All rights reserved.
 * Copyright (C) 2020 Noam Rosenthal (noam@webkit.org)
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
 *
 */

#include "config.h"
#include "StyleImageSet.h"

#include "CSSImageSetValue.h"

namespace WebCore {

Ref<StyleImageSet> StyleImageSet::create(CSSImageSetValue& cssValue)
{ 
    return adoptRef(*new StyleImageSet(cssValue));
}

StyleImageSet::StyleImageSet(CSSImageSetValue& cssValue)
    : m_cssValue(cssValue)
{
    m_isImageSet = true;
}

bool StyleImageSet::operator==(const StyleImage& other) const
{
    return is<StyleImageSet>(other) && equals(downcast<StyleImageSet>(other));
}

StyleImageSet::~StyleImageSet() = default;

Ref<CSSValue> StyleImageSet::cssValue() const
{ 
    return m_cssValue.copyRef(); 
}

ImageWithScale StyleImageSet::selectBestFitImage(const Document& document) const
{
    return m_cssValue->selectBestFitImage(document);
}

}
