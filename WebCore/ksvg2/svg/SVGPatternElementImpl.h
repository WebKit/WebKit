/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

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
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#ifndef KSVG_SVGPatternElementImpl_H
#define KSVG_SVGPatternElementImpl_H

#include "SVGTestsImpl.h"
#include "SVGLangSpaceImpl.h"
#include "SVGURIReferenceImpl.h"
#include "SVGFitToViewBoxImpl.h"
#include "SVGStyledLocatableElementImpl.h"
#include "SVGExternalResourcesRequiredImpl.h"

#include <kcanvas/KCanvasResourceListener.h>
#include <kcanvas/device/KRenderingPaintServer.h>
#include <kcanvas/device/KRenderingPaintServerPattern.h>

class KCanvasImage;

namespace KSVG
{
    class SVGAnimatedLengthImpl;
    class SVGPatternElementImpl;
    class SVGAnimatedEnumerationImpl;
    class SVGAnimatedTransformListImpl;
    class SVGPatternElementImpl : public SVGStyledLocatableElementImpl,
                                  public SVGURIReferenceImpl,
                                  public SVGTestsImpl,
                                  public SVGLangSpaceImpl,
                                  public SVGExternalResourcesRequiredImpl,
                                  public SVGFitToViewBoxImpl,
                                  public KCanvasResourceListener
    {
    public:
        SVGPatternElementImpl(const KDOM::QualifiedName& tagName, KDOM::DocumentImpl *doc);
        virtual ~SVGPatternElementImpl();
        
        virtual bool isValid() const { return SVGTestsImpl::isValid(); }

        // 'SVGPatternElement' functions
        SVGAnimatedEnumerationImpl *patternUnits() const;
        SVGAnimatedEnumerationImpl *patternContentUnits() const;
        SVGAnimatedTransformListImpl *patternTransform() const;

        SVGAnimatedLengthImpl *x() const;
        SVGAnimatedLengthImpl *y() const;

        SVGAnimatedLengthImpl *width() const;
        SVGAnimatedLengthImpl *height() const;

        virtual void parseMappedAttribute(KDOM::MappedAttributeImpl *attr);

        const SVGStyledElementImpl *pushAttributeContext(const SVGStyledElementImpl *context);

        virtual void resourceNotification() const;
        virtual void notifyAttributeChange() const;

        virtual bool rendererIsNeeded(khtml::RenderStyle *) { return true; }
        virtual khtml::RenderObject *createRenderer(RenderArena *arena, khtml::RenderStyle *style);
        virtual KRenderingPaintServerPattern *canvasResource();

        // 'virtual SVGLocatable' functions
        virtual SVGMatrixImpl *getCTM() const;

    protected:
        mutable RefPtr<SVGAnimatedLengthImpl> m_x;
        mutable RefPtr<SVGAnimatedLengthImpl> m_y;
        mutable RefPtr<SVGAnimatedLengthImpl> m_width;
        mutable RefPtr<SVGAnimatedLengthImpl> m_height;
        
        mutable RefPtr<SVGAnimatedEnumerationImpl> m_patternUnits;
        mutable RefPtr<SVGAnimatedEnumerationImpl> m_patternContentUnits;

        mutable RefPtr<SVGAnimatedTransformListImpl> m_patternTransform;

        mutable KCanvasImage *m_tile;
        mutable bool m_ignoreAttributeChanges;
        mutable KRenderingPaintServerPattern *m_paintServer;
        
    private:
        // notifyAttributeChange helpers:
        void fillAttributesFromReferencePattern(const SVGPatternElementImpl *target, KCanvasMatrix &patternTransformMatrix) const;
        void drawPatternContentIntoTile(const SVGPatternElementImpl *target, const IntSize &newSize, KCanvasMatrix patternTransformMatrix) const;
        void notifyClientsToRepaint() const;
    };
};

#endif

// vim:ts=4:noet
