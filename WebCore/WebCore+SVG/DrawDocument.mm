/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "DrawDocumentPrivate.h"
#import "DrawCanvasItemPrivate.h"

#import "DrawViewPrivate.h"
#import "QuartzSupport.h" // for CGAffineTransformMakeMapBetweenRects()

#import <kcursor.h>
#import <qevent.h>

#import <kcanvas/KCanvas.h>
#import <kcanvas/KCanvasItem.h>
#import <kcanvas/KCanvasContainer.h>
#import <kcanvas/KCanvasTreeDebug.h>
#import <kcanvas/device/quartz/KCanvasViewQuartz.h>
#import <kcanvas/device/quartz/KRenderingDeviceQuartz.h>

#define id ID_HACK

#import <kdom/Namespace.h>
#import <kdom/Helper.h>
#import <kdom/parser/KDOMParser.h>
#import <kdom/backends/libxml/LibXMLParser.h>
#import <kdom/impl/NodeImpl.h>
#import <kdom/impl/NodeListImpl.h>
#import <kdom/impl/DOMConfigurationImpl.h>
#import <kdom/impl/DOMImplementationImpl.h>
#import <kdom/impl/ElementImpl.h>
#import <kdom/events/kdomevents.h>
#import <kdom/events/impl/EventImpl.h>
#import <kdom/events/impl/MouseEventImpl.h>

#import <ksvg2/KSVGPart.h>
#import <ksvg2/KSVGView.h>
#import <ksvg2/core/KSVGDocumentBuilder.h>
#import <ksvg2/impl/SVGDocumentImpl.h>
#import <ksvg2/impl/SVGSVGElementImpl.h>
#import <ksvg2/impl/SVGDescElementImpl.h>
#import <ksvg2/impl/SVGStyledElementImpl.h>
#import <ksvg2/impl/SVGEllipseElementImpl.h>
#import <ksvg2/impl/SVGRectElementImpl.h>

#undef id

using namespace KDOM;
using namespace KSVG;

@interface DrawDocumentPrivate : NSObject {
    @public
    SVGDocumentImpl *svgDocument;
    KCanvas *canvas;
    KCanvasViewQuartz *primaryView;
    
    KCanvasViewQuartz *dummyCanvasView; // used to hold the canvas, until we get a real view.
	KSVGPart *part;
	KSVGView *svgView;
}

+ (KRenderingDeviceQuartz *)sharedRenderingDevice;
- (void)setSVGDocument:(SVGDocumentImpl *)svgDocumentImpl;
- (void)setPrimaryView:(KCanvasViewQuartz *)view;
@end

@implementation DrawDocumentPrivate

+ (KRenderingDeviceQuartz *)sharedRenderingDevice
{
    static KRenderingDeviceQuartz *__quartzRenderingDevice = NULL;
    if (!__quartzRenderingDevice)
        __quartzRenderingDevice = new KRenderingDeviceQuartz();
    
    return __quartzRenderingDevice;
}

- (id)init
{
	if ((self = [super init])) {
		// FIXME: HACK: this is needed until post-parse attach works.
		part = new KSVGPart();
		svgView = static_cast<KSVGView *>(part->view());
		dummyCanvasView =  static_cast<KCanvasViewQuartz *>(svgView->canvasView());
		[self setPrimaryView:dummyCanvasView];
	}
	return self;
}

- (void)dealloc
{
    if (canvas)
        delete canvas;
    if (svgDocument)
        svgDocument->deref();
	//delete svgView;
	delete part;
    [super dealloc];
}

- (void)setSVGDocument:(SVGDocumentImpl *)svgDocumentImpl
{
    KDOM_SAFE_SET(svgDocument, svgDocumentImpl);
}

- (void)setPrimaryView:(KCanvasViewQuartz *)view
{
	if (view == primaryView) return;
    if (!primaryView || (primaryView == dummyCanvasView)) {
        if (!canvas)
            canvas = new KCanvas([DrawDocumentPrivate sharedRenderingDevice]);
        
        primaryView = view;
        primaryView->init(canvas, NULL);
        if (svgDocument) {
            svgDocument->setCanvasView(primaryView);
            canvas->addView(primaryView);
            //svgDocument->attach();
            
//          // if the SVG doc doesn't contain any size information
//          // size it appropriately...
//          if (NSEqualSizes([self canvasSize], NSMakeSize(-1,-1))) {
//              [self sizeCanvasToFitContent];
//          }
            delete dummyCanvasView;
            dummyCanvasView = NULL;
        }
    }
}

@end


@interface DrawView (InternalCanvasMethods)
- (KCanvasViewQuartz *)canvasView;
@end

@implementation DrawDocument

+ (id)documentWithSVGData:(NSData *)data
{
    return [[[DrawDocument alloc] initWithSVGData:data] autorelease];
}

// makes new empty document
//- (id)init
//{
//	if (self = [super init]) {
//		// need an alternative call than "self"
//		SVGDocumentImpl *svgDoc = new SVGDocumentImpl(SVGDOMImplementation::self());
//		[_private setSVGDocument:svgDoc];
//	}
//	return self;
//}


- (id)initWithSVGData:(NSData *)data
{
    self = [super init];
    if (!self)
        return nil;
    
    _private = [[DrawDocumentPrivate alloc] init];
    
    // Builder is owned (deleted) by parser...
    KSVG::DocumentBuilder *builder = new KSVG::DocumentBuilder(_private->svgView);
    KDOM::LibXMLParser *parser = new KDOM::LibXMLParser(KURL());
    parser->setDocumentBuilder(builder);
    
    // no entity refs
    parser->domConfig()->setParameter(KDOM::ENTITIES.handle(), false);
    parser->domConfig()->setParameter(KDOM::ELEMENT_CONTENT_WHITESPACE.handle(), false);
    
    // Feed the parser the whole document (a total hack)
    parser->doOneShotParse((const char *)[data bytes], [data length]);
    
    SVGDocumentImpl *svgDoc = static_cast<SVGDocumentImpl *>(parser->document());
    [_private setSVGDocument:svgDoc];
    delete parser; // we're done parsing.
    if(!_private->svgDocument) {
        NSLog(@"Failed to get document!");
        [self release];
        return nil;
    }
    
    // FIXME: temporary
    // this needs to go somewhere in ksvg2
    if ((_private->canvas->canvasSize() == QSize(-1,-1)) && _private->canvas->rootContainer()) {
        QRect canvasBounds = _private->canvas->rootContainer()->bbox();
        _private->canvas->setCanvasSize(QSize(abs(canvasBounds.x()) + canvasBounds.width(),
                                              abs(canvasBounds.y()) + canvasBounds.height()));
    }
    
    return self;
}

- (id)initWithContentsOfFile:(NSString *)path
{
    NSData *data = [NSData dataWithContentsOfFile:path];
    if ([[path pathExtension] isEqualToString:@"svg"]) {
        //NSLog(@"Loading SVG Data from file: %@", path);
        self = [self initWithSVGData:data];
    } else {
        NSLog(@"Error: Asked to create DrawDocument from unsupported file type!  File: %@", path);
        [self release];
        self = nil;
    }
    return self;
}

- (void)dealloc
{
	[[self primaryView] _clearDocument];
    [_private release];
    [super dealloc];
}

- (NSString *)title
{
    // Detect title if possible...
    KDOM::DOMStringImpl *title = _private->svgDocument->title();
    return title->string().getNSString();
}

- (NSString *)description
{
    // Detect description if possible...
//  KDOM::NodeListImpl *descList = _private->svgDocument->getElementsByTagName("desc");
//  if(descList && (descList->length() > 0)) {
//  SVGDescElementImpl *descImpl = dynamic_cast<SVGDescElementImpl *>(descList->item(0));
//  return descImpl->description().string().getNSString();
//  }
    return nil;
}

// internal SPI
- (DrawView *)primaryView
{
    if (_private->primaryView)
        return _private->primaryView->view();
    return nil;
}

- (void)setPrimaryView:(DrawView *)view
{
    [_private setPrimaryView:(KCanvasViewQuartz *)[view canvasView]];
}

- (void)registerView:(DrawView *)view
{
    if (!_private->primaryView || (_private->primaryView == _private->dummyCanvasView))
        [self setPrimaryView:view];
    else
        _private->canvas->addView([view canvasView]);
}

- (void)unregisterView:(DrawView *)view
{
	// canvas views clean up themselves on dealloc.
}

- (NSString *)svgText
{
    QString *dumpText = new QString();
    QTextStream dumpStream(dumpText, IO_WriteOnly);
    KDOM::Helper::PrintNode(dumpStream, _private->svgDocument);
    NSString *svgText = dumpText->getNSString();
    delete dumpText;
    
    return svgText;
}

@end

@implementation DrawDocument (DrawMouseEvents)

- (BOOL)documentListensForMouseMovedEvents
{
    SVGDocumentImpl *document = _private->svgDocument;
    
    return (document &&	
            (document->hasListenerType(KDOM::DOMFOCUSOUT_EVENT) ||
             document->hasListenerType(KDOM::MOUSEOVER_EVENT) ||
             document->hasListenerType(KDOM::MOUSEMOVE_EVENT) ||
             document->hasListenerType(KDOM::MOUSEOUT_EVENT)));
}

- (BOOL)documentListensForMouseDownEvents
{
    return (_private->svgDocument && _private->svgDocument->hasListenerType(KDOM::MOUSEDOWN_EVENT));
}

- (BOOL)documentListensForMouseUpEvents
{
    SVGDocumentImpl *document = _private->svgDocument;
    
    return (document &&
            (document->hasListenerType(KDOM::DOMFOCUSIN_EVENT) ||
             document->hasListenerType(KDOM::DOMACTIVATE_EVENT) ||
             document->hasListenerType(KDOM::CLICK_EVENT) ||
             document->hasListenerType(KDOM::MOUSEUP_EVENT)));
}

- (KDOM::MouseEventImpl *)newMouseEventWithEventId:(KDOM::EventId)eventId qMouseEvent:(QMouseEvent *)qevent
{
    if (!_private->svgDocument)
        return NULL;
    
    KDOM::DOMStringImpl *eventString = KDOM::DOMImplementationImpl::self()->idToType(eventId);
    
    SVGSVGElementImpl *root = _private->svgDocument->rootElement();
    float scale = (root ? root->currentScale() : 1.0);
    
    // Setup kdom 'MouseEvent'...
    KDOM::MouseEventImpl *event = static_cast<KDOM::MouseEventImpl *>(_private->svgDocument->createEvent(DOMString("MouseEvents").handle()));
    event->ref();
    
    event->initMouseEvent(eventString, qevent, scale);
    return event;
}

NSCursor *cursorForStyle(KDOM::RenderStyle *style)
{
    if(!style) return nil;
    NSCursor *newCursor = nil;
    switch(style->cursor())
    {
        case KDOM::CS_AUTO:
        case KDOM::CS_DEFAULT:
            newCursor = KCursor::arrowCursor().handle();
            break;
        case KDOM::CS_CROSS:
        case KDOM::CS_PROGRESS:
            newCursor = KCursor::crossCursor().handle();
            break;
        case KDOM::CS_POINTER:
            newCursor = KCursor::handCursor().handle();
            break;
        case KDOM::CS_MOVE:
            newCursor = KCursor::sizeAllCursor().handle();
            break;
        case KDOM::CS_E_RESIZE:
        case KDOM::CS_W_RESIZE:
            newCursor = KCursor::sizeHorCursor().handle();
            break;
        case KDOM::CS_NE_RESIZE:
        case KDOM::CS_SW_RESIZE:
            newCursor = KCursor::sizeBDiagCursor().handle();
            break;
        case KDOM::CS_NW_RESIZE:
        case KDOM::CS_SE_RESIZE:
            newCursor = KCursor::sizeFDiagCursor().handle();
            break;
        case KDOM::CS_N_RESIZE:
        case KDOM::CS_S_RESIZE:
            newCursor = KCursor::sizeVerCursor().handle();
            break;
        case KDOM::CS_TEXT:
            newCursor = KCursor::ibeamCursor().handle();
            break;
        case KDOM::CS_WAIT:
            newCursor = KCursor::waitCursor().handle();
            break;
        case KDOM::CS_HELP:
            newCursor = KCursor::whatsThisCursor().handle();
            break;
        default:
        NSLog(@"setting default mouse cursor");
            newCursor = KCursor::arrowCursor().handle();
    }
    return newCursor;
}

- (NSCursor *)cursorAfterPropagatingMouseMovedEvent:(NSEvent *)theEvent fromView:(DrawView *)view
{
    NSCursor *newCursor = [NSCursor arrowCursor];
    
    QMouseEvent *event = new QMouseEvent(QEvent::MouseMove, theEvent);
    KDOM::MouseEventImpl *mev = [self newMouseEventWithEventId:KDOM::MOUSEMOVE_EVENT qMouseEvent:event];
    if(mev)
    {
        // FIXME this mapping should be done elsewhere...
        NSPoint viewPoint = [view convertPoint:[theEvent locationInWindow] fromView:nil];
        QPoint canvasPoint = QPoint([view mapViewPointToCanvas:viewPoint]);
        _private->svgDocument->prepareMouseEvent(false, canvasPoint.x(), canvasPoint.y(), mev);
        
        KDOM::ElementImpl *target = static_cast<KDOM::ElementImpl *>(mev->relatedTarget());
        if(target)
        {
            KDOM::RenderStyle *style = target->renderStyle();
            newCursor = cursorForStyle(style);
        }
        mev->deref();
    }
    return newCursor;
}

- (void)propagateMouseUpEvent:(NSEvent *)theEvent fromView:(DrawView *)view
{
    QMouseEvent *event = new QMouseEvent(QEvent::MouseButtonRelease, theEvent);
    KDOM::MouseEventImpl *mev = [self newMouseEventWithEventId:KDOM::MOUSEUP_EVENT qMouseEvent:event];
    if(mev)
    {
        // FIXME this mapping should be done elsewhere...
        NSPoint viewPoint = [view  convertPoint:[theEvent locationInWindow] fromView:nil];
        QPoint canvasPoint = QPoint([view mapViewPointToCanvas:viewPoint]);
        _private->svgDocument->prepareMouseEvent(false, canvasPoint.x(), canvasPoint.y(), mev);
        mev->deref();
    }
}

- (void)propagateMouseDownEvent:(NSEvent *)theEvent fromView:(DrawView *)view
{
    QMouseEvent *event = new QMouseEvent(QEvent::MouseButtonPress, theEvent);
    KDOM::MouseEventImpl *mev = [self newMouseEventWithEventId:KDOM::MOUSEDOWN_EVENT qMouseEvent:event];
    if(mev)
    {
        // FIXME this mapping should be done elsewhere...
        NSPoint viewPoint = [view convertPoint:[theEvent locationInWindow] fromView:nil];
        QPoint canvasPoint = QPoint([view mapViewPointToCanvas:viewPoint]);
        _private->svgDocument->prepareMouseEvent(false, canvasPoint.x(), canvasPoint.y(), mev);
        mev->deref();
    }
}


@end

@implementation DrawDocument (KCanvasManipulation)

// SPI
- (KCanvas *)canvas
{
    return _private->canvas;
}

- (NSSize)canvasSize
{
    KCanvas *canvas = [self canvas];
    if (!canvas)
        return NSZeroSize;
    return NSSize(canvas->canvasSize());
}

- (NSString *)renderTreeAsExternalRepresentation
{
    KCanvas *canvas = [self canvas];
    if (canvas)
        return externalRepresentation(canvas->rootContainer()).getNSString();
    return nil;
}

- (void)sizeCanvasToFitContent
{
    NSRect canvasBounds = NSRect(_private->canvas->rootContainer()->bbox());
    if (!NSEqualRects(canvasBounds, NSZeroRect)) {
        NSLog(@"zooming to rect: %@", NSStringFromRect(canvasBounds));
        _private->canvas->setCanvasSize(QSize(int(abs((int)canvasBounds.origin.x) + canvasBounds.size.width),
                                              int(abs((int)canvasBounds.origin.y) + canvasBounds.size.height)));
        // this pan should be moved out of the doc...
        [[self primaryView] setCanvasVisibleOrigin:NSMakePoint(canvasBounds.origin.x * -1, canvasBounds.origin.y * -1)];
    }
}


- (DrawCanvasItem *)canvasItemAtPoint:(NSPoint)canvasHitPoint
{
    KCanvasItemList hitItems;
    
    _private->canvas->collisions(QPoint(canvasHitPoint), hitItems);
    
    NSLog(@"canvasItemAtPoint:%@ hit %i items", NSStringFromPoint(canvasHitPoint), hitItems.count());
    KCanvasItemList::Iterator it = hitItems.begin();
    KCanvasItemList::Iterator end = hitItems.end();
    for(; it != end; ++it) {
        NSLog(@"canvasItemAtPoint: hit item with bbox: %@", NSStringFromRect(NSRect((*it)->bbox())));
    }
    
    if (!hitItems.isEmpty())
        return [DrawCanvasItem canvasItemForItem:(KCanvasItem *)hitItems.last()];
    
    return nil;
}

- (void)removeItemFromDOM:(DrawCanvasItem *)canvasItem
{
    if (canvasItem) {
        KCanvasItem *cItem = [canvasItem item];
        KDOM::NodeImpl *node = (KDOM::NodeImpl *)cItem->userData();
        KDOM::NodeImpl *parent = node->parentNode();
        parent->removeChild(node);
    }
}

- (DrawCanvasItem *)createItemForTool:(int)tool atPoint:(NSPoint)mousePoint{
    KDOM::ElementImpl *newElement = NULL;
    KCanvasItem *newCanvasItem = NULL;
    
    DOMString mouseX(QString::number(mousePoint.x));
    DOMString mouseY(QString::number(mousePoint.x));
    
    switch (tool) {
	case DrawViewToolElipse:
	{
            newElement = _private->svgDocument->createElement(DOMString("ellipse").handle());
            newElement->setAttributeNS(KDOM::NS_SVG.handle(), DOMString("cx").handle(), mouseX.handle());
            newElement->setAttributeNS(KDOM::NS_SVG.handle(), DOMString("cy").handle(), mouseY.handle());
            break;
	}
	case DrawViewToolTriangle:
            break;
	case DrawViewToolRectangle:
	{
            newElement = _private->svgDocument->createElement(DOMString("rect").handle());
            newElement->setAttributeNS(KDOM::NS_SVG.handle(), DOMString("x").handle(), mouseX.handle());
            newElement->setAttributeNS(KDOM::NS_SVG.handle(), DOMString("y").handle(), mouseY.handle());
            break;
	}
	case DrawViewToolLine:
	case DrawViewToolPolyLine:
	case DrawViewToolArc:
            // these just don't really work with this model...
	default:
            NSLog(@"Can't create item for unsupported tool.");
    }
    if (newElement) {
        newElement->setAttributeNS(KDOM::NS_SVG.handle(), DOMString("fill").handle(), DOMString("navy").handle());
        SVGSVGElementImpl *rootNode = _private->svgDocument->rootElement();
        rootNode->appendChild(newElement);
        newElement->ref(); // don't know why this is necessary...
        newElement->attach(); // attach it to the canvas.
        newCanvasItem = static_cast<SVGStyledElementImpl *>(newElement)->canvasItem();
        NSLog(@"Successfully created element: %p, now canvasItem: %p", newElement,  newCanvasItem);
        
        if (newCanvasItem) {
            // wrap it in a DrawCanvasItem, return.
            return [DrawCanvasItem canvasItemForItem:newCanvasItem];
        }
    }
    return nil;
}

@end

@implementation DrawDocument (SuperPrivateSPIForDavid)


// This is really "drawInRect: inCGContext:
- (void)drawRect:(NSRect)destRect inCGContext:(CGContextRef)context
{
    if (!_private->canvas)
        return;
    
    static KRenderingDeviceContextQuartz *__sharedQuartzContext = nil;
    if (!__sharedQuartzContext)
        __sharedQuartzContext = new KRenderingDeviceContextQuartz();
    
    KRenderingDevice *renderingDevice = _private->canvas->renderingDevice();
    
    // push the drawing context
    __sharedQuartzContext->setCGContext(context);
    renderingDevice->pushContext(__sharedQuartzContext);
    CGContextSaveGState(context);
    
    // transform to the dest rect.
    NSSize sourceSize = [self canvasSize];
    CGRect sourceRect = {{0,0},{sourceSize.width, sourceSize.height}};
    CGAffineTransform map = CGAffineTransformMakeMapBetweenRects(sourceRect,*(CGRect *)&destRect);
    CGContextConcatCTM(context,map);
    
    // do the draw
    _private->canvas->rootContainer()->draw(QRect(sourceRect));
    
    // restore drawing state
    CGContextRestoreGState(context);
    renderingDevice->popContext();
}

@end

