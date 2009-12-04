/*
    Copyright (C) 2006 Nikolas Zimmermann <wildfox@kde.org>
                  2006 Apple Computer Inc.

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

#ifndef SVGListTraits_h
#define SVGListTraits_h

#if ENABLE(SVG)

#include <wtf/RefPtr.h>

namespace WebCore {

    template<typename Item> struct UsesDefaultInitializer { static const bool value = true; };
    template<> struct UsesDefaultInitializer<double>      { static const bool value = false; };
    template<> struct UsesDefaultInitializer<float>       { static const bool value = false; };

    template<bool usesDefaultInitializer, typename Item>
    struct SVGListTraits { };

    template<typename ItemPtr>
    struct SVGListTraits<true, ItemPtr*> {
        static ItemPtr* nullItem() { return 0; }
        static bool isNull(ItemPtr* it) { return !it; }
    };
    
    template<typename ItemPtr>
    struct SVGListTraits<true, RefPtr<ItemPtr> > {
        static RefPtr<ItemPtr> nullItem() { return 0; }
        static bool isNull(const RefPtr<ItemPtr>& it) { return !it; }
    };
    
    template<typename Item>
    struct SVGListTraits<true, Item> {
        static Item nullItem() { return Item(); }
        static bool isNull(Item it) { return !it; }
    };

    template<>
    struct SVGListTraits<false, double> {
        static double nullItem() { return 0.0; }
        static bool isNull(double) { return false; }
    };

    template<>
    struct SVGListTraits<false, float> {
        static float nullItem() { return 0; }
        static bool isNull(float) { return false; }
    };


} // namespace WebCore

#endif // SVG_SUPPORT
#endif // SVGListTraits_h

// vim:ts=4:noet
