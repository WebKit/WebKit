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

#ifndef KRenderingDevice_H
#define KRenderingDevice_H

#include <qcolor.h>
#include <qobject.h>
#include <q3ptrstack.h>

#include <kcanvas/KCanvasTypes.h>
#include <kcanvas/KCanvasFilters.h>
#include <kcanvas/device/KRenderingPaintServer.h>

// aka where to draw
class KCanvasMatrix;
class KRenderingDeviceContext
{
public:
    KRenderingDeviceContext() { }
    virtual ~KRenderingDeviceContext() { }

    virtual KCanvasMatrix concatCTM(const KCanvasMatrix &worldMatrix) = 0;
    virtual KCanvasMatrix ctm() const = 0;
    
    virtual QRect mapFromVisual(const QRect &rect) = 0;
    virtual QRect mapToVisual(const QRect &rect) = 0;
    
    virtual void clearPath() = 0;
    virtual void addPath(const KCanvasPath*) = 0;
};

// Must be a QObject to be able to be loaded by KLibLoader...
class KCanvasImage;
class KCanvasFilterEffect;
class KRenderingDevice : public QObject
{
Q_OBJECT
public:
    KRenderingDevice();
    virtual ~KRenderingDevice();

    // The rendering device will be directly inited
    // after the canvas target, it may be overwritten.
    virtual bool isBuffered() const = 0;

    // Global rendering device context
    KRenderingDeviceContext *currentContext() const;

    virtual KRenderingDeviceContext *popContext();
    virtual void pushContext(KRenderingDeviceContext *context);
    
    virtual KRenderingDeviceContext *contextForImage(KCanvasImage *image) const = 0;
    
    virtual QString stringForPath(const KCanvasPath* path) = 0;

    // Creation tools
    virtual KCanvasResource *createResource(const KCResourceType &type) const = 0;
    virtual KCanvasFilterEffect *createFilterEffect(const KCFilterEffectType &type) const = 0;
    virtual KRenderingPaintServer *createPaintServer(const KCPaintServerType &type) const = 0;

    virtual RenderPath *createItem(RenderArena *arena, khtml::RenderStyle *style, KSVG::SVGStyledElementImpl *node, KCanvasPath* path) const = 0;
    virtual KCanvasContainer *createContainer(RenderArena *arena, khtml::RenderStyle *style, KSVG::SVGStyledElementImpl *node) const = 0;
    virtual KCanvasPath* createPath() const = 0;

private:
    Q3PtrStack<KRenderingDeviceContext> m_contextStack;
};

#endif

// vim:ts=4:noet
