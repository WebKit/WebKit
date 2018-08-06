/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2010 Rob Buis <buis@kde.org>
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "SVGAttribute.h"
#include "SVGStringListValues.h"

namespace WebCore {

class SVGElement;
class SVGStringList;

template<typename OwnerType, typename... BaseTypes>
class SVGAttributeRegistry;

template<typename OwnerType, typename... BaseTypes>
class SVGAttributeOwnerProxyImpl;

class SVGTests {
    WTF_MAKE_NONCOPYABLE(SVGTests);
public:
    static bool hasExtension(const String&);
    bool isValid() const;

    using AttributeRegistry = SVGAttributeRegistry<SVGTests>;
    static AttributeRegistry& attributeRegistry();
    static bool isKnownAttribute(const QualifiedName& attributeName);

    void parseAttribute(const QualifiedName&, const AtomicString&);
    void svgAttributeChanged(const QualifiedName&);

    static void addSupportedAttributes(HashSet<QualifiedName>&);

    WEBCORE_EXPORT static bool hasFeatureForLegacyBindings(const String& feature, const String& version);

    // These methods are called from DOM through the super classes.
    Ref<SVGStringList> requiredFeatures();
    Ref<SVGStringList> requiredExtensions();
    Ref<SVGStringList> systemLanguage();

protected:
    SVGTests(SVGElement* contextElement);

private:
    using AttributeOwnerProxy = SVGAttributeOwnerProxyImpl<SVGTests>;
    static void registerAttributes();

    SVGElement& m_contextElement;
    SVGStringListValuesAttribute m_requiredFeatures { SVGNames::requiredFeaturesAttr };
    SVGStringListValuesAttribute m_requiredExtensions { SVGNames::requiredExtensionsAttr };
    SVGStringListValuesAttribute m_systemLanguage { SVGNames::systemLanguageAttr };
};

} // namespace WebCore
