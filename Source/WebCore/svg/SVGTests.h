/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2010 Rob Buis <buis@kde.org>
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

#pragma once

#include "SVGAnimatedPropertyMacros.h"
#include "SVGStringListValues.h"

namespace WebCore {

class SVGElement;
class SVGStringList;

class SVGTests {
public:
    static bool hasExtension(const String&);
    bool isValid() const;

    void parseAttribute(const QualifiedName&, const AtomicString&);

    static bool isKnownAttribute(const QualifiedName&);
    static void addSupportedAttributes(HashSet<QualifiedName>&);

    static bool handleAttributeChange(SVGElement*, const QualifiedName&);

    static const SVGAttributeToPropertyMap& attributeToPropertyMap();

    WEBCORE_EXPORT static bool hasFeatureForLegacyBindings(const String& feature, const String& version);

protected:
    SVGTests();

    Ref<SVGStringList> requiredFeatures(SVGElement&);
    Ref<SVGStringList> requiredExtensions(SVGElement&);
    Ref<SVGStringList> systemLanguage(SVGElement&);

    void synchronizeRequiredFeatures(SVGElement&);
    void synchronizeRequiredExtensions(SVGElement&);
    void synchronizeSystemLanguage(SVGElement&);

private:
    void synchronizeAttribute(SVGElement& contextElement, SVGSynchronizableAnimatedProperty<SVGStringListValues>&, const QualifiedName& attributeName);

    SVGSynchronizableAnimatedProperty<SVGStringListValues> m_requiredFeatures;
    SVGSynchronizableAnimatedProperty<SVGStringListValues> m_requiredExtensions;
    SVGSynchronizableAnimatedProperty<SVGStringListValues> m_systemLanguage;
};

} // namespace WebCore
