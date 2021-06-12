/*
 * Copyright (C) 2008 Alex Mathews <possessedpenguinbob@gmail.com>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
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

#ifndef SourceGraphic_h
#define SourceGraphic_h

#include "FilterEffect.h"
#include "Filter.h"

namespace WebCore {

class SourceGraphic : public FilterEffect {
public:        
    static Ref<SourceGraphic> create(Filter&);

    static const AtomString& effectName();

private:
    SourceGraphic(Filter& filter)
        : FilterEffect(filter, Type::SourceGraphic)
    {
        setOperatingColorSpace(DestinationColorSpace::SRGB());
    }

    const char* filterName() const final { return "SourceGraphic"; }

    void determineAbsolutePaintRect() override;
    void platformApplySoftware() override;
    WTF::TextStream& externalRepresentation(WTF::TextStream&, RepresentationType) const override;

    FilterEffectType filterEffectType() const override { return FilterEffectTypeSourceInput; }
};

} //namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::SourceGraphic)
    static bool isType(const WebCore::FilterEffect& effect) { return effect.filterEffectClassType() == WebCore::FilterEffect::Type::SourceGraphic; }
SPECIALIZE_TYPE_TRAITS_END()


#endif // SourceGraphic_h
