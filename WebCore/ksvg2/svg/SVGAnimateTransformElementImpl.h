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

#ifndef KSVG_SVGAnimateTransformElementImpl_H
#define KSVG_SVGAnimateTransformElementImpl_H
#if SVG_SUPPORT

#include <kcanvas/KCanvasMatrix.h>

#include "ksvg.h"
#include "SVGAnimationElementImpl.h"

namespace WebCore
{
    class SVGTransformImpl;
    class SVGAnimateTransformElementImpl : public SVGAnimationElementImpl
    {
    public:
        SVGAnimateTransformElementImpl(const QualifiedName& tagName, DocumentImpl *doc);
        virtual ~SVGAnimateTransformElementImpl();

        virtual void parseMappedAttribute(MappedAttributeImpl *attr);

        virtual void handleTimerEvent(double timePercentage);

        // Helpers
        RefPtr<SVGTransformImpl> parseTransformValue(const QString &data) const;
        void calculateRotationFromMatrix(const QWMatrix &matrix, double &angle, double &cx, double &cy) const;

        SVGMatrixImpl *initialMatrix() const;
        SVGMatrixImpl *transformMatrix() const;

    private:
        int m_currentItem;
        SVGTransformType m_type;

        RefPtr<SVGTransformImpl> m_toTransform;
        RefPtr<SVGTransformImpl> m_fromTransform;
        RefPtr<SVGTransformImpl> m_initialTransform;

        RefPtr<SVGMatrixImpl> m_lastMatrix;
        RefPtr<SVGMatrixImpl> m_transformMatrix;

        mutable bool m_rotateSpecialCase : 1;
        bool m_toRotateSpecialCase : 1;
        bool m_fromRotateSpecialCase : 1;
    };
};

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
