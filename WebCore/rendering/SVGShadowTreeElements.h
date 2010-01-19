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

class SVGShadowTreeContainerElement : public SVGGElement {
public:
    SVGShadowTreeContainerElement(Document*);
    virtual ~SVGShadowTreeContainerElement();

    virtual bool isShadowTreeContainerElement() const { return true; }

    FloatSize containerTranslation() const;
    void setContainerOffset(const SVGLength& x, const SVGLength& y)
    {
        m_xOffset = x;
        m_yOffset = y;
    }

private:
    SVGLength m_xOffset;
    SVGLength m_yOffset;
};

class SVGShadowTreeRootElement : public SVGShadowTreeContainerElement {
public:
    SVGShadowTreeRootElement(Document*, Node* shadowParent);
    virtual ~SVGShadowTreeRootElement();

    virtual bool isShadowNode() const { return m_shadowParent; }
    virtual Node* shadowParentNode() { return m_shadowParent; }

    void attachElement(PassRefPtr<RenderStyle>, RenderArena*);

private:
    Node* m_shadowParent;
};

}

#endif
#endif
