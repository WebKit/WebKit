/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005-2019 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2012, 2013 Google Inc. All rights reserved.
 * Copyright (C) 2014 Igalia S.L.
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
#include "MatchedDeclarationsCache.h"

#include "CSSFontSelector.h"
#include "Document.h"
#include "DocumentInlines.h"
#include "FontCascade.h"
#include "RenderStyleInlines.h"
#include "StyleResolver.h"
#include <wtf/text/StringHash.h>

namespace WebCore {
namespace Style {

MatchedDeclarationsCache::MatchedDeclarationsCache(const Resolver& owner)
    : m_owner(owner)
    , m_sweepTimer(*this, &MatchedDeclarationsCache::sweep)
{
}

MatchedDeclarationsCache::~MatchedDeclarationsCache() = default;

void MatchedDeclarationsCache::ref() const
{
    m_owner->ref();
}

void MatchedDeclarationsCache::deref() const
{
    m_owner->deref();
}

bool MatchedDeclarationsCache::isCacheable(const Element& element, const RenderStyle& style, const RenderStyle& parentStyle)
{
    // FIXME: Writing mode and direction properties modify state when applying to document element by calling
    // Document::setWritingMode/DirectionSetOnDocumentElement. We can't skip the applying by caching.
    if (&element == element.document().documentElement())
        return false;
    // content:attr() value depends on the element it is being applied to.
    if (style.hasAttrContent() || (style.pseudoElementType() != PseudoId::None && parentStyle.hasAttrContent()))
        return false;
    if (style.zoom() != RenderStyle::initialZoom())
        return false;
    if (style.writingMode() != RenderStyle::initialWritingMode() || style.direction() != RenderStyle::initialDirection())
        return false;
    if (style.usesContainerUnits())
        return false;

    // Getting computed style after a font environment change but before full style resolution may involve styles with non-current fonts.
    // Avoid caching them.
    auto& fontSelector = element.document().fontSelector();
    if (!style.fontCascade().isCurrent(fontSelector))
        return false;
    if (!parentStyle.fontCascade().isCurrent(fontSelector))
        return false;

    // FIXME: counter-style: we might need to resolve cache like for fontSelector here (rdar://103018993).

    return true;
}

bool MatchedDeclarationsCache::Entry::isUsableAfterHighPriorityProperties(const RenderStyle& style) const
{
    if (style.effectiveZoom() != renderStyle->effectiveZoom())
        return false;

#if ENABLE(DARK_MODE_CSS)
    if (style.colorScheme() != renderStyle->colorScheme())
        return false;
#endif

    return CSSPrimitiveValue::equalForLengthResolution(style, *renderStyle);
}

unsigned MatchedDeclarationsCache::computeHash(const MatchResult& matchResult, const StyleCustomPropertyData& inheritedCustomProperties)
{
    if (!matchResult.isCacheable)
        return 0;

    return WTF::computeHash(matchResult, &inheritedCustomProperties);
}

const MatchedDeclarationsCache::Entry* MatchedDeclarationsCache::find(unsigned hash, const MatchResult& matchResult, const StyleCustomPropertyData& inheritedCustomProperties)
{
    if (!hash)
        return nullptr;

    auto it = m_entries.find(hash);
    if (it == m_entries.end())
        return nullptr;

    auto& entry = it->value;
    if (matchResult != entry.matchResult)
        return nullptr;

    if (&entry.parentRenderStyle->inheritedCustomProperties() != &inheritedCustomProperties)
        return nullptr;

    return &entry;
}

void MatchedDeclarationsCache::add(const RenderStyle& style, const RenderStyle& parentStyle, const RenderStyle* userAgentAppearanceStyle, unsigned hash, const MatchResult& matchResult)
{
    constexpr unsigned additionsBetweenSweeps = 100;
    if (++m_additionsSinceLastSweep >= additionsBetweenSweeps && !m_sweepTimer.isActive()) {
        constexpr auto sweepDelay = 1_min;
        m_sweepTimer.startOneShot(sweepDelay);
    }

    auto userAgentAppearanceStyleCopy = [&]() -> std::unique_ptr<RenderStyle> {
        if (userAgentAppearanceStyle)
            return RenderStyle::clonePtr(*userAgentAppearanceStyle);
        return { };
    };

    ASSERT(hash);
    // Note that we don't cache the original RenderStyle instance. It may be further modified.
    // The RenderStyle in the cache is really just a holder for the substructures and never used as-is.
    m_entries.add(hash, Entry { matchResult, RenderStyle::clonePtr(style), RenderStyle::clonePtr(parentStyle), userAgentAppearanceStyleCopy() });
}

void MatchedDeclarationsCache::remove(unsigned hash)
{
    m_entries.remove(hash);
}

void MatchedDeclarationsCache::invalidate()
{
    m_entries.clear();
}

void MatchedDeclarationsCache::clearEntriesAffectedByViewportUnits()
{
    Ref protectedThis { *this };

    m_entries.removeIf([](auto& keyValue) {
        return keyValue.value.renderStyle->usesViewportUnits();
    });
}

void MatchedDeclarationsCache::sweep()
{
    Ref protectedThis { *this };

    // Look for cache entries containing a style declaration with a single ref and remove them.
    // This may happen when an element attribute mutation causes it to generate a new inlineStyle()
    // or presentationalHintStyle(), potentially leaving this cache with the last ref on the old one.
    auto hasOneRef = [](auto& declarations) {
        for (auto& matchedProperties : declarations) {
            if (matchedProperties.properties->hasOneRef())
                return true;
        }
        return false;
    };

    m_entries.removeIf([&](auto& keyValue) {
        auto& matchResult = keyValue.value.matchResult;
        return hasOneRef(matchResult.userAgentDeclarations) || hasOneRef(matchResult.userDeclarations) || hasOneRef(matchResult.authorDeclarations);
    });

    m_additionsSinceLastSweep = 0;
}

}
}
