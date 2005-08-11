/*
	Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
				  2004, 2005 Rob Buis <buis@kde.org>

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

#include <kdebug.h>
#include <kglobal.h>

#include <qregion.h>
#include <qintdict.h>

#include "KCanvas.h"
#include "KCanvasItem.h"
#include "KCanvasView.h"
#include "KCanvasView.moc"
#include "KCanvasMatrix.h"
#include "KCanvasRegistry.h"
#include "KCanvasContainer.h"
#include "KRenderingDevice.h"
#include "KCanvasImage.h"

#ifdef ENABLE_X11_SUPPORT
// We should really use this only on X11.
#include <kcanvas/x11support/X11Platform.h>
#endif

class KCanvasView::Private
{
public:
	Private()
	{
		canvas = 0;
#ifdef ENABLE_X11_SUPPORT
		targetWidget = 0;
#endif

		backgroundColor = QColor(qRgba(255, 255, 255, 255));

		pan = QPoint(0, 0);
		zoom = 1;
		zoomAndPan = true;

#ifdef ENABLE_X11_SUPPORT
		platform = 0;
		buffer = 0;
#endif
		context = 0;
	}

	~Private()
	{
#ifdef ENABLE_X11_SUPPORT
		delete buffer;
		delete platform;
#endif
		//delete context;
	}

	KCanvas *canvas;
#ifdef ENABLE_X11_SUPPORT
	QWidget *targetWidget;
#endif

	QColor backgroundColor;
	
	QPoint pan;
	float zoom;
	bool zoomAndPan : 1;

#ifdef ENABLE_X11_SUPPORT
	// Special cases for buffered devices
	X11Platform *platform;
	KCanvasImageBuffer *buffer;
#endif
	KRenderingDeviceContext *context;
};

KCanvasView::KCanvasView() : QObject(), d(new Private())
{
}

KCanvasView::~KCanvasView()
{
	if(d->canvas) // Unlink with canvas...
		d->canvas->removeView(this);

	delete d;
}

KRenderingDeviceContext *KCanvasView::context() const
{
	return d->context;
}

void KCanvasView::setContext(KRenderingDeviceContext *context)
{
	d->context = context;
}

void KCanvasView::init(KCanvas *canvas, QWidget *targetWidget)
{
	Q_ASSERT(canvas);
	d->canvas = canvas;
	
#ifdef ENABLE_X11_SUPPORT
	Q_ASSERT(targetWidget);
	d->targetWidget = targetWidget;
#endif

	// Link with canvas...
	canvas->addView(this);

#ifdef ENABLE_X11_SUPPORT
	// Special case for buffered devices
	if(d->canvas->renderingDevice()->isBuffered())
	{
		if(!d->platform) // color conversions etc..
			d->platform = new X11Platform();

		if(!d->buffer)
			d->buffer = new KCanvasImageBuffer(true /* RGBA buffer */);
	
		// Resize main rendering buffer size (will be used by the buffered device)
		d->buffer->resize(targetWidget->width(), targetWidget->height());
		
		// Fill whole image, using user-specified background color
		QColor color = d->backgroundColor;
		d->buffer->fill(qRgba(color.red(), color.green(), color.blue(), 0));

		my_pix_format_e format;
		if(d->buffer->depth() == 24 && d->buffer->channels() == 3)
			format = my_pix_format_bgr24;
		else
			format = my_pix_format_bgra32;

		// Initialze X11Platform code (handles moving our buffer to the screen)
		d->platform->setFormat(format);
		d->platform->setBuffer(d->buffer->bits());

		d->platform->init(targetWidget->x11Display(), targetWidget->x11Depth(),
						  static_cast<Visual *>(targetWidget->x11Visual()), targetWidget->handle());
		d->platform->setupImage(targetWidget->width(), targetWidget->height());
	}
#endif
}

void KCanvasView::draw(const QRect &dirtyRect)
{
	Q_ASSERT(d->canvas);
	Q_ASSERT(context());

#ifdef ENABLE_X11_SUPPORT
	Q_ASSERT(d->targetWidget);
#endif
	
	// Canvas not ready yet.
	if(!d->canvas->rootContainer())
		return;

	// Put our main rendering buffer onto the context stack.
	d->canvas->renderingDevice()->pushContext(d->context);

	// Normalized & converted to 'physical' coordinates (aka. buffer size)
	QRect normalized = dirtyRect.normalize();

#ifdef ENABLE_X11_SUPPORT
	if(d->canvas->renderingDevice()->isBuffered())
	{	
		QColor color = backgroundColor();
		d->buffer->fill(qRgba(color.red(), color.green(), color.blue(), 0), normalized);
	}
#endif

	// Ask our root container to draw all items contained in 'rect'.
	d->canvas->rootContainer()->draw(d->context->mapFromVisual(normalized));

	// Remove our main rendering buffer again from the context stack.
	d->canvas->renderingDevice()->popContext();

#ifdef ENABLE_X11_SUPPORT
	// Special case for buffered devices: blit the buffer
	if(d->canvas->renderingDevice()->isBuffered())
	{
		int x0, y0, x1, y1;
		clampAgainstTarget(normalized, x0, y0, x1, y1);

		if(d->buffer && d->buffer->bits())
		{
			d->platform->setBuffer(d->buffer->bits());
			d->platform->putImage(x0, y0, x1, y1);			
		}
	}
#endif
}

const QColor &KCanvasView::backgroundColor() const
{
	return d->backgroundColor;
}

void KCanvasView::setBackgroundColor(const QColor &color)
{
	Q_ASSERT(d->canvas);

#ifdef ENABLE_X11_SUPPORT
	Q_ASSERT(d->targetWidget);

	d->targetWidget->setPaletteBackgroundColor(color);

	// Special case for buffered devices
	if(d->canvas->renderingDevice()->isBuffered())
		d->buffer->fill(qRgba(color.red(), color.green(), color.blue(), 0));
#endif

	d->backgroundColor = color;
}

KCanvas *KCanvasView::canvas() const
{
	Q_ASSERT(d->canvas);
	return d->canvas;
}

#ifdef ENABLE_X11_SUPPORT
QWidget *KCanvasView::canvasTarget() const
{
	Q_ASSERT(d->targetWidget);
	return d->targetWidget;
}		
#endif


#ifdef ENABLE_X11_SUPPORT

KCanvasMatrix KCanvasView::viewToCanvasMatrix() const
{
	float _zoom = 1.0 / zoom();
	KCanvasMatrix matrix;
	matrix.translate(-d->pan.x(), -d->pan.y());
	matrix.multiply(KCanvasMatrix().scale(_zoom, _zoom));
}

int KCanvasView::viewHeight() const
{
	Q_ASSERT(d->targetWidget);
	return d->targetWidget->height()
}

int KCanvasView::viewWidth() const
{
	Q_ASSERT(d->targetWidget);
	return d->targetWidget->width();
}
#endif

QSize KCanvasView::viewSize() const
{
	Q_ASSERT(d->targetWidget);
	return QSize(viewWidth(), viewHeight());
}

void KCanvasView::resizeView(int _width, int _height)
{
	Q_ASSERT(d->canvas);
#ifdef ENABLE_X11_SUPPORT
	Q_ASSERT(d->targetWidget);
#endif

	// Clamp newly requested view size
	// against available target widget size.
	QRect rect(0, 0, _width, _height);
	rect = rect.normalize();

#ifdef ENABLE_X11_SUPPORT
	int x0, y0, x1, y1;
	clampAgainstTarget(rect, x0, y0, x1, y1);

	// Special case for buffered devices
	if(d->canvas->renderingDevice()->isBuffered())
	{
		d->buffer->resize(x1, y1);

		QColor color = backgroundColor();
		d->buffer->fill(qRgba(color.red(), color.green(), color.blue(), 0));

		d->platform->setBuffer(d->buffer->bits());
		d->platform->setupImage(d->buffer->width(), d->buffer->height());
	}

	// Allocate a rendering context for our main rendering buffer
	// TODO: Offer a way to not reallocating it everytime.
	delete context();
	setContext(d->canvas->renderingDevice()->contextForBuffer(d->buffer));
#endif

	// Update rendering contexts world matrix
	context()->setWorldMatrix(viewToCanvasMatrix());

	// Invalidate the entire screen region
	QRect full(0, 0, viewWidth(), viewHeight());
	invalidateCanvasRect(full);
}

float KCanvasView::zoom() const
{
	return d->zoom;
}

void KCanvasView::setZoom(float zoom)
{
	if(zoomAndPanEnabled())
	{
		d->zoom = zoom;
		updateView();
	}
}

const QPoint &KCanvasView::pan() const
{
	return d->pan;
}

void KCanvasView::setPan(const QPoint &pan)
{
	if(zoomAndPanEnabled())
	{
		d->pan = pan;
		updateView();
	}
}

void KCanvasView::updateView()
{
	Q_ASSERT(d->context);
#ifdef ENABLE_X11_SUPPORT
	Q_ASSERT(d->targetWidget);

	float invertedZoom = 1.0 / zoom();

	// Special case for buffered devices
	if(d->canvas->renderingDevice()->isBuffered())
	{
		int prefWidth = qRound(invertedZoom * (d->canvas->canvasSize().width()) - d->pan.x());
		int prefHeight = qRound(invertedZoom * (d->canvas->canvasSize().height()) - d->pan.y());

		QRect rect(0, 0, prefWidth, prefHeight);
		rect = rect.normalize();

		int x0, y0, x1, y1;
		clampAgainstTarget(rect, x0, y0, x1, y1);
			
		d->buffer->resize(x1, y1);

		QColor color = backgroundColor();
		d->buffer->fill(qRgba(color.red(), color.green(), color.blue(), 0));

		d->platform->setBuffer(d->buffer->bits());
		d->platform->setupImage(d->buffer->width(), d->buffer->height());
	}
#endif

	context()->setWorldMatrix(viewToCanvasMatrix());

#ifdef ENABLE_X11_SUPPORT
	// Invalidate the entire screen region
	if(d->canvas->renderingDevice()->isBuffered())
	{
		// Avoid drawing glitches when the rendering buffer size
		// is smaller then the target widget size (erase in that case :-)
		if(d->buffer->width() != d->targetWidget->width() ||
		   d->buffer->height() != d->targetWidget->height())
		{
			QRegion bufferRegion(QRect(0, 0, d->buffer->width(), d->buffer->height()));
			QRegion targetRegion(QRect(0, 0, d->targetWidget->width(), d->targetWidget->height()));

			d->targetWidget->erase(targetRegion.subtract(bufferRegion));
		}
	}
#endif
	
	QRect full(0, 0, viewWidth(), viewHeight());
	invalidateCanvasRect(full);
}

bool KCanvasView::zoomAndPanEnabled() const
{
	return d->zoomAndPan;
}

void KCanvasView::enableZoomAndPan(bool enable)
{
	d->zoomAndPan = enable;
}

#ifdef ENABLE_X11_SUPPORT
void KCanvasView::invalidateCanvasRect(const QRect &rect) const
{
	// no default implementation for unbuffered targets.
	if(!d->canvas->renderingDevice()->isBuffered())
		return;

	Q_ASSERT(d->targetWidget);
	d->targetWidget->update(rect);
}
#endif

void KCanvasView::invalidateCanvasItem(const KCanvasItem *item) const
{
	invalidateCanvasRect(item->bbox());
}

#ifdef ENABLE_X11
KCanvasImageBuffer *KCanvasView::grabBuffer()
{
	KCanvasImageBuffer *ret = new KCanvasImageBuffer(true);
	ret->resize(d->targetWidget->width(), d->targetWidget->height());

	QColor color = backgroundColor();
	ret->fill(qRgba(color.red(), color.green(), color.blue(), 0));
	
	KRenderingDeviceContext *savedContext = context();

	setContext(d->canvas->renderingDevice()->contextForImage(ret));
	context()->setCanvas(d->canvas);

	// Draw whole image.
	draw(QRect(0, 0, width, height));
	
	delete context();
	setContext(savedContext);

	return ret;
}
#else
KCanvasImage *KCanvasView::grabImage()
{
	QRect full(0, 0, viewWidth(), viewHeight());
	KRenderingDevice *renderingDevice = d->canvas->renderingDevice();
	KCanvasImage *newImage = static_cast<KCanvasImage *>(renderingDevice->createResource(RS_IMAGE));
	newImage->init(full.size());
	
	KRenderingDeviceContext *imageContext = renderingDevice->contextForImage(newImage);
	renderingDevice->pushContext(imageContext);
	
	draw(full);
	
	renderingDevice->popContext();
	delete imageContext;
	
	return newImage;
}
#endif

void KCanvasView::clampAgainstTarget(const QRect &rect, int &x0, int &y0, int &x1, int &y1)
{
	int curWidth = viewWidth();
	int curHeight = viewHeight();

	// clamp to viewport
	x0 = rect.x();
	x0 = kMax(x0, 0);
	x0 = kMin(x0, int(curWidth - 1));

	y0 = rect.y();
	y0 = kMax(y0, 0);
	y0 = kMin(y0, int(curHeight - 1));

	x1 = rect.x() + rect.width() + 1;
	x1 = kMax(x1, 0);
	x1 = kMin(x1, int(curWidth));

	y1 = rect.y() + rect.height() + 1;
	y1 = kMax(y1, 0);
	y1 = kMin(y1, int(curHeight));
}


// vim:ts=4:noet
