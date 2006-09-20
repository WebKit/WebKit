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

#ifndef KCanvasResource_H
#define KCanvasResource_H
#ifdef SVG_SUPPORT

#include "DeprecatedValueList.h"
#include "Path.h"
#include "RenderPath.h"
#include "KCanvasResourceListener.h"

namespace WebCore {

typedef DeprecatedValueList<const RenderPath *> RenderPathList;

class TextStream;

typedef enum
{
    // Painting mode
    RS_CLIPPER = 0,
    RS_MARKER = 1,
    RS_IMAGE = 2,
    RS_FILTER = 3,
    RS_MASKER = 4
} KCResourceType;

class KRenderingPaintServer;

class KCanvasResource
{
public:
    KCanvasResource();
    virtual ~KCanvasResource();

    virtual void invalidate();
    void addClient(const RenderPath*);

    const RenderPathList &clients() const;
    
    String idInRegistry() const;
    void setIdInRegistry(const String&);
    
    virtual bool isPaintServer() const { return false; }
    virtual bool isFilter() const { return false; }
    virtual bool isClipper() const { return false; }
    virtual bool isMarker() const { return false; }
    virtual bool isMasker() const { return false; }
    
    virtual TextStream& externalRepresentation(TextStream &) const; 
private:
    RenderPathList m_clients;
    String m_registryId;
};

KCanvasResource* getResourceById(Document*, const AtomicString&);
KRenderingPaintServer* getPaintServerById(Document*, const AtomicString&);

TextStream &operator<<(TextStream&, const KCanvasResource&);

}

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
