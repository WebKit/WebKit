/*
 * Copyright (C) 2004, 2005, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
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

#include "QualifiedName.h"
#include "SVGAttributeOwnerProxyImpl.h"
#include <wtf/HashSet.h>

namespace WebCore {

class SVGElement;

class SVGLangSpace {
public:
    const String& xmllang() const { return m_lang.value(); }
    void setXmllang(const AtomicString& xmlLang) { m_lang.setValue(xmlLang); }

    const String& xmlspace() const;
    void setXmlspace(const AtomicString& xmlSpace) { m_space.setValue(xmlSpace); }

    void parseAttribute(const QualifiedName&, const AtomicString&);

    void svgAttributeChanged(const QualifiedName&);

    using AttributeOwnerProxy = SVGAttributeOwnerProxyImpl<SVGLangSpace>;
    using AttributeRegistry = SVGAttributeRegistry<SVGLangSpace>;
    static auto& attributeRegistry() { return AttributeOwnerProxy::attributeRegistry(); }
    static bool isKnownAttribute(const QualifiedName& attributeName) { return attributeRegistry().isKnownAttribute(attributeName); }

protected:
    SVGLangSpace(SVGElement* contextElement);

private:
    using SVGStringAttribute = SVGPropertyAttribute<String>;
    using SVGStringAttributeAccessor = SVGPropertyAttributeAccessor<SVGLangSpace, SVGStringAttribute>;
    static void registerAttributes();

    SVGElement& m_contextElement;
    SVGStringAttribute m_lang;
    SVGStringAttribute m_space;
};

} // namespace WebCore
