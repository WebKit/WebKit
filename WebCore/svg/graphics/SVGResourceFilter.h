/*
    Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
                  2005 Eric Seidel <eric@webkit.org>
                  2009 Dirk Schulze <krit@webkit.org>

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

#if ENABLE(SVG) && ENABLE(FILTERS)
#include "SVGResource.h"

#include "Image.h"
#include "ImageBuffer.h"
#include "FloatRect.h"
#include "RenderObject.h"
#include "SVGFilterPrimitiveStandardAttributes.h"

#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class Filter;
class FilterEffect;
class GraphicsContext;
class SVGFilterBuilder;
class SVGFilterPrimitiveStandardAttributes;

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

    FloatRect filterBoundingBox() { return m_filterBBox; }
    void setFilterBoundingBox(const FloatRect& rect) { m_filterBBox = rect; }

    FloatRect itemBoundingBox() { return m_itemBBox; }
    void setItemBoundingBox(const FloatRect& rect) { m_itemBBox = rect; }

    FloatRect filterBBoxForItemBBox(const FloatRect& itemBBox) const;

    virtual TextStream& externalRepresentation(TextStream&) const;

    void prepareFilter(GraphicsContext*&, const RenderObject*);
    void applyFilter(GraphicsContext*&, const RenderObject*);

    void addFilterEffect(SVGFilterPrimitiveStandardAttributes*, PassRefPtr<FilterEffect>);

    SVGFilterBuilder* builder() { return m_filterBuilder.get(); }
    
private:

    bool m_filterBBoxMode : 1;
    bool m_effectBBoxMode : 1;

    bool m_xBBoxMode : 1;
    bool m_yBBoxMode : 1;

    FloatRect m_filterRect;

    FloatRect m_filterBBox;
    FloatRect m_itemBBox;

    OwnPtr<SVGFilterBuilder> m_filterBuilder;
    GraphicsContext* m_savedContext;
    OwnPtr<ImageBuffer> m_sourceGraphicBuffer;
    RefPtr<Filter> m_filter;
};

SVGResourceFilter* getFilterById(Document*, const AtomicString&);

} // namespace WebCore

#endif // ENABLE(SVG)

#endif // SVGResourceFilter_h
