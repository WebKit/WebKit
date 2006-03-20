/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
                  2006 Alexander Kellett <lypanov@kde.org>

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

#ifndef KRenderingPaintServer_H
#define KRenderingPaintServer_H
#if SVG_SUPPORT

#include <kcanvas/KCanvasResources.h>

class QTextStream;

namespace WebCore {

// Enumerations
enum KCPaintServerType {
    // Painting mode
    PS_SOLID = 0,
    PS_PATTERN = 1,
    PS_LINEAR_GRADIENT = 2,
    PS_RADIAL_GRADIENT = 3
};

enum KCPaintTargetType {
    // Target mode
    APPLY_TO_FILL = 1,
    APPLY_TO_STROKE = 2
};

class RenderStyle;
class RenderPath;
class KRenderingDeviceContext;
class KRenderingPaintServer : public KCanvasResource
{
public:
    KRenderingPaintServer() : KCanvasResource(), m_activeClient(0), m_paintingText(false) { }
    virtual ~KRenderingPaintServer() { }
    
    virtual bool isPaintServer() const { return true; }

    const RenderPath *activeClient() const { return m_activeClient;}
    void setActiveClient(const RenderPath *client) { m_activeClient = client; }

    DeprecatedString idInRegistry() const {  return m_registryId; }
    void setIdInRegistry(const DeprecatedString& newId) { m_registryId = newId; } 
    
    virtual KCPaintServerType type() const = 0;
    
    // Actual rendering function
    virtual void draw(KRenderingDeviceContext*, const RenderPath* renderPath, KCPaintTargetType) const = 0;

    virtual bool setup(KRenderingDeviceContext*, const RenderObject*, KCPaintTargetType) const = 0;
    virtual void teardown(KRenderingDeviceContext*, const RenderObject*, KCPaintTargetType) const = 0;
    
    bool isPaintingText() const { return m_paintingText; }
    void setPaintingText(bool paintingText) { m_paintingText = paintingText; }

    virtual QTextStream &externalRepresentation(QTextStream &) const = 0;

    virtual void renderPath(KRenderingDeviceContext*, const RenderPath*, KCPaintTargetType) const = 0;
private:
    const RenderPath *m_activeClient;
    DeprecatedString m_registryId;
    bool m_paintingText;
};

}

QTextStream &operator<<(QTextStream &, const WebCore::KRenderingPaintServer &);

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
