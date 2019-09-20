/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2015 Apple Inc. All right reserved.
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
#include "SVGDocument.h"

#include "SVGSVGElement.h"
#include "SVGViewSpec.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGDocument);

SVGDocument::SVGDocument(Frame* frame, const URL& url)
    : XMLDocument(frame, url, SVGDocumentClass)
{
}

RefPtr<SVGSVGElement> SVGDocument::rootElement(const Document& document)
{
    auto* element = document.documentElement();
    if (!is<SVGSVGElement>(element))
        return nullptr;
    return downcast<SVGSVGElement>(element);
}

bool SVGDocument::zoomAndPanEnabled() const
{
    auto element = rootElement(*this);
    if (!element)
        return false;
    return (element->useCurrentView() ? element->currentView().zoomAndPan() : element->zoomAndPan()) == SVGZoomAndPanMagnify;
}

void SVGDocument::startPan(const FloatPoint& start)
{
    auto element = rootElement(*this);
    if (!element)
        return;
    m_panningOffset = start - element->currentTranslateValue();
}

void SVGDocument::updatePan(const FloatPoint& position) const
{
    auto element = rootElement(*this);
    if (!element)
        return;
    element->setCurrentTranslate(position - m_panningOffset);
}

Ref<Document> SVGDocument::cloneDocumentWithoutChildren() const
{
    return create(nullptr, url());
}

}
