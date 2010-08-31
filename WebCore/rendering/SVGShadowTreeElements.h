/*
    Copyright (C) Research In Motion Limited 2010. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    aint with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef SVGShadowTreeElements_h
#define SVGShadowTreeElements_h

#if ENABLE(SVG)
#include "SVGGElement.h"
#include "SVGLength.h"

namespace WebCore {

class FloatSize;
class SVGUseElement;

class SVGShadowTreeContainerElement : public SVGGElement {
public:
    static PassRefPtr<SVGShadowTreeContainerElement> create(Document*);

    FloatSize containerTranslation() const;
    void setContainerOffset(const SVGLength& x, const SVGLength& y)
    {
        m_xOffset = x;
        m_yOffset = y;
    }

protected:
    SVGShadowTreeContainerElement(Document*);

private:
    virtual bool isShadowTreeContainerElement() const { return true; }

    SVGLength m_xOffset;
    SVGLength m_yOffset;
};

class SVGShadowTreeRootElement : public SVGShadowTreeContainerElement {
public:
    static PassRefPtr<SVGShadowTreeRootElement> create(Document*, SVGUseElement* shadowParent);

    void attachElement(PassRefPtr<RenderStyle>, RenderArena*);

    virtual ContainerNode* shadowParentNode() { return m_shadowParent; }

private:
    SVGShadowTreeRootElement(Document*, SVGUseElement* shadowParent);

    virtual bool isShadowNode() const { return m_shadowParent; }

    ContainerNode* m_shadowParent;
};

}

#endif
#endif
