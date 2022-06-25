/*
 * Copyright (C) 2005, 2005 Alexander Kellett <lypanov@kde.org>
 * Copyright (C) 2008 Rob Buis <buis@kde.org>
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
#include "SVGImageLoader.h"

#include "CachedImage.h"
#include "Event.h"
#include "EventNames.h"
#include "HTMLParserIdioms.h"
#include "SVGElementTypeHelpers.h"
#include "SVGImageElement.h"

namespace WebCore {

SVGImageLoader::SVGImageLoader(SVGImageElement& element)
    : ImageLoader(element)
{
}

SVGImageLoader::~SVGImageLoader() = default;

void SVGImageLoader::dispatchLoadEvent()
{
    if (image()->errorOccurred())
        element().dispatchEvent(Event::create(eventNames().errorEvent, Event::CanBubble::No, Event::IsCancelable::No));
    else
        downcast<SVGImageElement>(ImageLoader::element()).sendLoadEventIfPossible();
}

String SVGImageLoader::sourceURI(const AtomString& attribute) const
{
    URL base = element().baseURI();
    if (base != aboutBlankURL())
        return URL(base, stripLeadingAndTrailingHTMLSpaces(attribute)).string();
    return element().document().completeURL(stripLeadingAndTrailingHTMLSpaces(attribute)).string();
}

}
