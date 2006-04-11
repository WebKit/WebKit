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

#ifndef KRenderingPaintServerSolid_H
#define KRenderingPaintServerSolid_H
#if SVG_SUPPORT

#include <kcanvas/device/KRenderingPaintServer.h>

namespace WebCore {

class KRenderingPaintServerSolid : public KRenderingPaintServer
{
public:
    KRenderingPaintServerSolid();
    virtual ~KRenderingPaintServerSolid();

    virtual KCPaintServerType type() const;

    // 'Solid' interface
    Color color() const;
    void setColor(const Color &color);

    QTextStream &externalRepresentation(QTextStream &) const;
private:
    class Private;
    Private *d;
};

}

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
