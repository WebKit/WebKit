/*
    Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
                  2005 Eric Seidel <eric@webkit.org>

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

#ifndef SVGResourceFilter_h
#define SVGResourceFilter_h

#if ENABLE(SVG) && ENABLE(SVG_FILTERS)
#include "SVGResource.h"
#include "SVGFilterEffect.h"

#include "FloatRect.h"

#include <wtf/OwnPtr.h>

namespace WebCore {

class GraphicsContext;
class SVGFilterEffect;
    
class SVGResourceFilterPlatformData {
public:
    virtual ~SVGResourceFilterPlatformData() {}
};

class SVGResourceFilter : public SVGResource {
public:
    SVGResourceFilter();
    
    virtual SVGResourceType resourceType() const { return FilterResourceType; }

    bool filterBoundingBoxMode() const { return m_filterBBoxMode; }
    void setFilterBoundingBoxMode(bool bboxMode) { m_filterBBoxMode = bboxMode; }

    bool effectBoundingBoxMode() const { return m_effectBBoxMode; }
    void setEffectBoundingBoxMode(bool bboxMode) { m_effectBBoxMode = bboxMode; }

    bool xBoundingBoxMode() const { return m_xBBoxMode; }
    void setXBoundingBoxMode(bool bboxMode) { m_xBBoxMode = bboxMode; }

    bool yBoundingBoxMode() const { return m_yBBoxMode; }
    void setYBoundingBoxMode(bool bboxMode) { m_yBBoxMode = bboxMode; }

    FloatRect filterRect() const { return m_filterRect; }
    void setFilterRect(const FloatRect& rect) { m_filterRect = rect; }

    FloatRect filterBBoxForItemBBox(const FloatRect& itemBBox) const;

    void clearEffects();
    void addFilterEffect(SVGFilterEffect*);

    virtual TextStream& externalRepresentation(TextStream&) const;

    // To be implemented in platform specific code.
    void prepareFilter(GraphicsContext*&, const FloatRect& bbox);
    void applyFilter(GraphicsContext*&, const FloatRect& bbox);
    
    SVGResourceFilterPlatformData* platformData() { return m_platformData.get(); }
    const Vector<SVGFilterEffect*>& effects() { return m_effects; }
    
private:
    SVGResourceFilterPlatformData* createPlatformData();
    
    OwnPtr<SVGResourceFilterPlatformData> m_platformData;

    bool m_filterBBoxMode : 1;
    bool m_effectBBoxMode : 1;

    bool m_xBBoxMode : 1;
    bool m_yBBoxMode : 1;

    FloatRect m_filterRect;
    Vector<SVGFilterEffect*> m_effects;
};

SVGResourceFilter* getFilterById(Document*, const AtomicString&);

} // namespace WebCore

#endif // ENABLE(SVG)

#endif // SVGResourceFilter_h
