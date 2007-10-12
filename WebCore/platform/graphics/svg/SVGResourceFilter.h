/*
    Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
                  2005 Eric Seidel <eric.seidel@kdemail.net>

    This file is part of the KDE project

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

#if ENABLE(SVG) && ENABLE(SVG_EXPERIMENTAL_FEATURES)
#include "SVGResource.h"
#include "SVGFilterEffect.h"

#include "FloatRect.h"

#if PLATFORM(CI)
#include <ApplicationServices/ApplicationServices.h>

#ifdef __OBJC__
@class CIImage;
@class CIFilter;
@class CIContext;
@class NSArray;
@class NSMutableDictionary;
#else
class CIImage;
class CIFilter;
class CIContext;
class NSArray;
class NSMutableDictionary;
#endif
#endif

#include <wtf/RetainPtr.h>

namespace WebCore {

class GraphicsContext;
class SVGFilterEffect;

class SVGResourceFilter : public SVGResource {
public:
    // To be implemented in platform specific code.
    SVGResourceFilter();
    virtual ~SVGResourceFilter();

    static SVGFilterEffect* createFilterEffect(const SVGFilterEffectType&, SVGResourceFilter*);
    
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

#if PLATFORM(CI)
    CIImage* imageForName(const String&) const;
    void setImageForName(CIImage*, const String&);

    void setOutputImage(const SVGFilterEffect*, CIImage*);
    CIImage* inputImage(const SVGFilterEffect*);
#endif

private:
#if PLATFORM(CI)
    NSArray* getCIFilterStack(CIImage* inputImage, const FloatRect& bbox);

    CIContext* m_filterCIContext;
    CGLayerRef m_filterCGLayer;
    RetainPtr<NSMutableDictionary> m_imagesByName;
#endif

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
