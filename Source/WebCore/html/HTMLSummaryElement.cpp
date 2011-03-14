/*
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)
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
#include "HTMLSummaryElement.h"

#include "HTMLDetailsElement.h"
#include "HTMLNames.h"
#include "RenderSummary.h"

namespace WebCore {

using namespace HTMLNames;

PassRefPtr<HTMLSummaryElement> HTMLSummaryElement::create(const QualifiedName& tagName, Document* document)
{
    return adoptRef(new HTMLSummaryElement(tagName, document));
}

HTMLSummaryElement::HTMLSummaryElement(const QualifiedName& tagName, Document* document)
    : HTMLElement(tagName, document)
{
    ASSERT(hasTagName(summaryTag));
}

RenderObject* HTMLSummaryElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    return new (arena) RenderSummary(this);
}

}
