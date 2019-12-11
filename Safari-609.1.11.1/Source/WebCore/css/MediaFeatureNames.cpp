/*
 * Copyright (C) 2005 Apple Inc.
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
#include "MediaFeatureNames.h"

namespace WebCore {
namespace MediaFeatureNames {

#define DEFINE_MEDIAFEATURE_GLOBAL(name, string) LazyNeverDestroyed<const AtomString> name;
CSS_MEDIAQUERY_NAMES_FOR_EACH_MEDIAFEATURE(DEFINE_MEDIAFEATURE_GLOBAL)
#undef DEFINE_MEDIAFEATURE_GLOBAL

void init()
{
    static bool initialized;
    if (!initialized) {
        AtomString::init();
#define INITIALIZE_GLOBAL(name, string) name.construct(string, AtomString::ConstructFromLiteral);
        CSS_MEDIAQUERY_NAMES_FOR_EACH_MEDIAFEATURE(INITIALIZE_GLOBAL)
#undef INITIALIZE_GLOBAL
        initialized = true;
    }
}

} // namespace MediaFeatureNames
} // namespace WebCore
