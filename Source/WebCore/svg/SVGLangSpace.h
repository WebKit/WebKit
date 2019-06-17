/*
 * Copyright (C) 2004, 2005, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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
#include <wtf/HashSet.h>

namespace WebCore {

class SVGElement;

class SVGLangSpace {
public:
    const AtomString& xmllang() const { return m_lang; }
    void setXmllang(const AtomString& xmlLang) { m_lang = xmlLang; }

    const AtomString& xmlspace() const;
    void setXmlspace(const AtomString& xmlSpace) { m_space = xmlSpace; }

    void parseAttribute(const QualifiedName&, const AtomString&);

    void svgAttributeChanged(const QualifiedName&);

    static bool isKnownAttribute(const QualifiedName&);

protected:
    SVGLangSpace(SVGElement* contextElement);

private:
    SVGElement& m_contextElement;
    AtomString m_lang;
    AtomString m_space;
};

} // namespace WebCore
