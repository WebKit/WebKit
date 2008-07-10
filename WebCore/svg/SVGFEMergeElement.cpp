/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"

#if ENABLE(SVG) && ENABLE(SVG_FILTERS)
#include "SVGFEMergeElement.h"

#include "SVGFEMergeNodeElement.h"
#include "SVGResourceFilter.h"

namespace WebCore {

SVGFEMergeElement::SVGFEMergeElement(const QualifiedName& tagName, Document* doc)
    : SVGFilterPrimitiveStandardAttributes(tagName, doc)
    , m_filterEffect(0)
{
}

SVGFEMergeElement::~SVGFEMergeElement()
{
}

SVGFilterEffect* SVGFEMergeElement::filterEffect(SVGResourceFilter* filter) const
{
    ASSERT_NOT_REACHED();
    return 0;
}

bool SVGFEMergeElement::build(FilterBuilder* builder)
{
    Vector<FilterEffect*> mergeInputs;
    for (Node* n = firstChild(); n != 0; n = n->nextSibling()) {
        if (n->hasTagName(SVGNames::feMergeNodeTag)) {
            FilterEffect* mergeEffect = builder->getEffectById(static_cast<SVGFEMergeNodeElement*>(n)->in1());
            mergeInputs.append(mergeEffect);
        }
    }

    if(mergeInputs.isEmpty())
        return false;

    builder->add(result(), FEMerge::create(mergeInputs));

    return true;
}

}

#endif // ENABLE(SVG)

// vim:ts=4:noet
