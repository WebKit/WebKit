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
    aint with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#ifndef KRenderingPaintServerPattern_H
#define KRenderingPaintServerPattern_H
#ifdef SVG_SUPPORT

#include "KRenderingPaintServer.h"

namespace WebCore {

class KCanvasImage;

class KRenderingPaintServerPattern : public KRenderingPaintServer
{
public:
    KRenderingPaintServerPattern();
    virtual ~KRenderingPaintServerPattern();

    virtual KCPaintServerType type() const;

    // Pattern bbox
    void setBbox(const FloatRect&);
    FloatRect bbox() const;
    
    // Pattern x,y phase points are relative when in boundingBoxMode
    // BoundingBox mode is true by default.
    bool boundingBoxMode() const;
    void setBoundingBoxMode(bool mode = true);
    
    // 'Pattern' interface
    KCanvasImage* tile() const;
    void setTile(KCanvasImage*);

    AffineTransform patternTransform() const;
    void setPatternTransform(const AffineTransform&);

    KCanvasResourceListener* listener() const;
    void setListener(KCanvasResourceListener*);
    TextStream& externalRepresentation(TextStream&) const;

private:
    KCanvasImage* m_tile;
    AffineTransform m_patternTransform;
    FloatRect m_bbox;
    bool m_useBoundingBoxMode;
    KCanvasResourceListener* m_listener;

};

}

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
