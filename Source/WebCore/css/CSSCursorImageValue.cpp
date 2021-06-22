/*
 * Copyright (C) 2006 Rob Buis <buis@kde.org>
 *           (C) 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2008-2021 Apple Inc. All rights reserved.
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
#include "CSSCursorImageValue.h"

#include "CSSImageSetValue.h"
#include "CSSImageValue.h"
#include "SVGCursorElement.h"
#include "SVGLengthContext.h"
#include "SVGURIReference.h"
#include "StyleBuilderState.h"
#include <wtf/MathExtras.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

CSSCursorImageValue::CSSCursorImageValue(Ref<CSSValue>&& imageValue, const std::optional<IntPoint>& hotSpot, LoadedFromOpaqueSource loadedFromOpaqueSource)
    : CSSValue(CursorImageClass)
    , m_imageValue(WTFMove(imageValue))
    , m_hotSpot(hotSpot)
    , m_loadedFromOpaqueSource(loadedFromOpaqueSource)
{
    if (is<CSSImageValue>(m_imageValue.get()))
        m_originalURL = downcast<CSSImageValue>(m_imageValue.get()).imageURL();
}

Ref<CSSCursorImageValue> CSSCursorImageValue::create(Ref<CSSValue>&& imageValue, const std::optional<IntPoint>& hotSpot, LoadedFromOpaqueSource loadedFromOpaqueSource)
{
    return adoptRef(*new CSSCursorImageValue(WTFMove(imageValue), hotSpot, loadedFromOpaqueSource));
}

CSSCursorImageValue::~CSSCursorImageValue()
{
    for (auto* element : m_cursorElements)
        element->removeClient(*this);
}

String CSSCursorImageValue::customCSSText() const
{
    String text = m_imageValue.get().cssText();
    if (!m_hotSpot)
        return text;
    return makeString(text, ' ', m_hotSpot->x(), ' ', m_hotSpot->y());
}

// FIXME: Should this function take a TreeScope instead?
SVGCursorElement* CSSCursorImageValue::updateCursorElement(const Document& document)
{
    auto element = SVGURIReference::targetElementFromIRIString(m_originalURL.string(), document).element;
    if (!is<SVGCursorElement>(element))
        return nullptr;

    // FIXME: Not right to keep old cursor elements as clients. The new one should replace the old, not join it in a set.
    auto& cursorElement = downcast<SVGCursorElement>(*element);
    if (m_cursorElements.add(&cursorElement).isNewEntry) {
        cursorElementChanged(cursorElement);
        cursorElement.addClient(*this);
    }
    return &cursorElement;
}

void CSSCursorImageValue::cursorElementRemoved(SVGCursorElement& cursorElement)
{
    // FIXME: Not right to stay a client of a cursor element until the element is destroyed. We'd want to stop being a client once it's no longer a valid target, like when it's disconnected.
    m_cursorElements.remove(&cursorElement);
}

void CSSCursorImageValue::cursorElementChanged(SVGCursorElement& cursorElement)
{
    // FIXME: Seems wrong that changing an old cursor element, one that that is no longer the target, changes the hot spot.
    // FIXME: This will override a hot spot that was specified in CSS, which is probably incorrect.
    // FIXME: Should we clamp from float to int instead of just casting here?
    SVGLengthContext lengthContext(nullptr);
    m_hotSpot = IntPoint {
        static_cast<int>(std::round(cursorElement.x().value(lengthContext))),
        static_cast<int>(std::round(cursorElement.y().value(lengthContext)))
    };

    // FIXME: Why doesn't this funtion check for a change to the href of the cursor element? Why would we dynamically track changes to x/y but not href?
}

ImageWithScale CSSCursorImageValue::selectBestFitImage(const Document& document)
{
    if (is<CSSImageSetValue>(m_imageValue.get()))
        return downcast<CSSImageSetValue>(m_imageValue.get()).selectBestFitImage(document);

    if (auto* cursorElement = updateCursorElement(document)) {
        auto location = document.completeURL(cursorElement->href());
        if (location != downcast<CSSImageValue>(m_imageValue.get()).imageURL())
            m_imageValue = CSSImageValue::create(WTFMove(location), m_loadedFromOpaqueSource);
    }

    return { m_imageValue.ptr() , 1 };
}

bool CSSCursorImageValue::equals(const CSSCursorImageValue& other) const
{
    return m_hotSpot == other.m_hotSpot && compareCSSValue(m_imageValue, other.m_imageValue);
}

Ref<CSSCursorImageValue> CSSCursorImageValue::valueWithStylesResolved(Style::BuilderState& state)
{
    auto imageValue = state.resolveImageStyles(m_imageValue.get());
    if (imageValue.ptr() == m_imageValue.ptr())
        return *this;
    return create(WTFMove(imageValue), m_hotSpot, m_loadedFromOpaqueSource);
}

} // namespace WebCore
