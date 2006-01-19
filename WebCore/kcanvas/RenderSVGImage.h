/*
    Copyright (C) 2006 Alexander Kellett <lypanov@kde.org>
    Copyright (C) 2006 Apple Computer, Inc.

    This file is part of the WebKit project.

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

#ifndef KCanvas_RenderSVGImage_H
#define KCanvas_RenderSVGImage_H

#include "render_image.h"
#include <qmatrix.h>

namespace KSVG
{
    class SVGImageElementImpl;
    class RenderSVGImage : public khtml::RenderImage {
    public:
        RenderSVGImage(SVGImageElementImpl *impl);
        virtual ~RenderSVGImage();
        
        virtual QMatrix localTransform() const { return m_transform; }
        virtual void setLocalTransform(const QMatrix& transform) { m_transform = transform; }
        
        virtual void paint(PaintInfo& paintInfo, int parentX, int parentY);
    private:
        void translateForAttributes();
        QMatrix m_transform;
    };
}

#endif

// vim:ts=4:noet
