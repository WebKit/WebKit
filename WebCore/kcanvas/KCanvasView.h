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

#ifndef KCanvasView_H
#define KCanvasView_H

#include <qsize.h>
#include <qpoint.h>
#include <qwidget.h>
#include <qobject.h>

class KCanvas;
class KCanvasItem;
class KCanvasImage;
class KCanvasMatrix;
class KRenderingDeviceContext;
class KCanvasView : public QObject
{
Q_OBJECT
public:
    KCanvasView();
    virtual ~KCanvasView();

    // Initialize the newly created view
    void init(KCanvas *canvas, QWidget *targetWidget);

    // Ask to view to update a specific target region.
    void draw(const QRect &dirtyRect);

    // Gets/Sets a background color (default: white)
    const QColor &backgroundColor() const;
    void setBackgroundColor(const QColor &color);

    // Returns active canvas
    KCanvas *canvas() const;

#ifdef ENABLE_X11_SUPPORT
    // Returns active canvas target
    QWidget *canvasTarget() const;
#endif

    // Returns the view size
    QSize viewSize() const;

    // Resize the view
    virtual void resizeView(int width, int height);

    // Draw the whole view
    void updateView();

    // Zooming/Panning
    float zoom() const;
    void setZoom(float zoom);

    const QPoint &pan() const;
    void setPan(const QPoint &pan);

    bool zoomAndPanEnabled() const;
    void enableZoomAndPan(bool enable = true);

    // Invalidates canvas regions either by
    // directly specifiying a rect in 'targetWidget'
    // coordinates, or using a KCanvasItem pointer.
    virtual void invalidateCanvasRect(const QRect &rect) const = 0;
    void invalidateCanvasItem(const KCanvasItem *item) const;

#ifdef ENABLE_X11_SUPPORT
    // Makes a pixel-wise 'screenshot' of the KCanvas.
    KCanvasImageBuffer *grabBuffer();
#endif
    // Makes a 'screenshot' of the KCanvas.
    KCanvasImage *grabImage();
    
    KRenderingDeviceContext *context() const;
    void setContext(KRenderingDeviceContext *context);

protected:
    virtual KCanvasMatrix viewToCanvasMatrix() const = 0;
    virtual int viewHeight() const = 0;
    virtual int viewWidth() const = 0;
signals:
    void newCanvasSize(const QSize &size);

private:
    friend class KCanvas;
    
    // Internal helper.
    virtual void canvasSizeChanged(int width, int height) = 0;

private:
    void clampAgainstTarget(const QRect &rect, int &x0, int &y0, int &x1, int &y1);

    class Private;
    Private *d;
};

#endif

// vim:ts=4:noet
