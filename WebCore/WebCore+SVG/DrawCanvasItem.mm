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

#include "config.h"
#import "DrawCanvasItemPrivate.h"

#import <kcanvas/KCanvas.h>
#import <kcanvas/KCanvasItem.h>

#import <qapplication.h>

#define id ID_HACK

#import <kdom/Namespace.h>
#import <kdom/Helper.h>
#import <kdom/DOMString.h>

#import <ksvg2/svg/SVGStyledElementImpl.h>
#import <ksvg2/svg/SVGEllipseElementImpl.h>
#import <ksvg2/svg/SVGRectElementImpl.h>
#import <ksvg2/svg/SVGPathElementImpl.h>

#undef id

#import "KWQTextStream.h"

using namespace KDOM;

@interface DrawCanvasItemPrivate : NSObject {
    @public
    KCanvasItem *item; // this is likely dangerous...
}
@end 

@implementation DrawCanvasItemPrivate
@end

@implementation DrawCanvasItem

+ (id)canvasItemForItem:(KCanvasItem *)canvasItem
{
    return [[[self alloc] initWithItem:canvasItem] autorelease];
}

- (id)initWithItem:(KCanvasItem *)canvasItem
{
    if ((self = [super init])) {
        _private = [[DrawCanvasItemPrivate alloc] init];
        _private->item = canvasItem;
    }
    return self;
}

- (KCanvasItem *)item
{
    return _private->item;
}

- (NSRect)boundingBox
{
    return NSRect(_private->item->bbox());
}

- (unsigned int)zIndex
{
    return _private->item->zIndex();
}

- (void)raise
{
    _private->item->raise();
}

- (void)lower
{
    _private->item->lower();
}

// see note in header.
+ (NSPoint)dragAnchorPointForControlPointIndex:(int)controlPointIndex fromRectControlPoints:(NSArray *)controlPoints{
    int anchorPointIndex = (controlPointIndex + 2) % 4; // the anchor point is the opposite corner.
    NSPoint anchorPoint = [[controlPoints objectAtIndex:anchorPointIndex] pointValue];
    return anchorPoint;
}

// see note in header.
- (NSPoint)dragAnchorPointForControlPointIndex:(int)controlPointIndex
{
    KDOM::NodeImpl *node = (KDOM::NodeImpl *)_private->item->userData();
    int localId = node->localId();
    NSPoint dragAnchorPoint;
    
    switch (localId) {
	case ID_PATH:
            //KSVG::SVGPathElementImpl *path = (KSVG::SVGPathElementImpl *)node;
	default:
            // if we have no special knob list, grab the default
            dragAnchorPoint = [DrawCanvasItem dragAnchorPointForControlPointIndex:controlPointIndex
                                                            fromRectControlPoints:[self controlPoints]];
            
    }
    return dragAnchorPoint;
}

// see note in header.
+ (NSArray *)controlPointsForRect:(NSRect)bbox
{
    return [NSArray arrayWithObjects:
        [NSValue valueWithPoint:NSMakePoint(NSMinX(bbox), NSMinY(bbox))],
        [NSValue valueWithPoint:NSMakePoint(NSMinX(bbox), NSMaxY(bbox))],
        [NSValue valueWithPoint:NSMakePoint(NSMaxX(bbox), NSMaxY(bbox))],
        [NSValue valueWithPoint:NSMakePoint(NSMaxX(bbox), NSMinY(bbox))],
        nil];
}

// see note in header.
- (NSArray *)controlPoints
{	
    KDOM::NodeImpl *node = (KDOM::NodeImpl *)_private->item->userData();
    int localId = node->localId();
    NSArray *controlPoints = nil;
    
    switch (localId) {
        case ID_PATH:
            //KSVG::SVGPathElementImpl *path = (KSVG::SVGPathElementImpl *)node;
        default:
            // if we have no special knob list, grab the default
            controlPoints = [DrawCanvasItem controlPointsForRect:[self boundingBox]];
    }
    return controlPoints;
}

- (void)fitToNewBBox:(NSRect)newRect
{
    [self willChangeValueForKey:@"boundingBox"];
    [self willChangeValueForKey:@"attributedXMLString"];
    //NSLog(@"Fitting canvasItem: %p from: %@ into new bbox: %@",
    //	self, NSStringFromRect([self boundingBox]), NSStringFromRect(newRect));
    KDOM::NodeImpl *node = (KDOM::NodeImpl *)_private->item->userData();
    int localId = node->localId();
    KDOM::DOMStringImpl *svgNamespace = KDOM::NS_SVG.handle();
    switch (localId) {
	case ID_ELLIPSE:
	{
            KSVG::SVGEllipseElementImpl *ellipse = (KSVG::SVGEllipseElementImpl *)node;
            ellipse->setAttributeNS(svgNamespace, DOMString("cx").handle(), DOMString(QString::number(newRect.origin.x + newRect.size.width/2.f)).handle());
            ellipse->setAttributeNS(svgNamespace, DOMString("cy").handle(), DOMString(QString::number(newRect.origin.y + newRect.size.height/2.f)).handle());
            ellipse->setAttributeNS(svgNamespace, DOMString("rx").handle(), DOMString(QString::number(newRect.size.width/2.f)).handle());
            ellipse->setAttributeNS(svgNamespace, DOMString("ry").handle(), DOMString(QString::number(newRect.size.height/2.f)).handle());
            break;
	}
	case ID_RECT:
	{
            KSVG::SVGRectElementImpl *rect = (KSVG::SVGRectElementImpl *)node;
            rect->setAttributeNS(svgNamespace, DOMString("x").handle(), DOMString(QString::number(newRect.origin.x)).handle());
            rect->setAttributeNS(svgNamespace, DOMString("y").handle(), DOMString(QString::number(newRect.origin.y)).handle());
            rect->setAttributeNS(svgNamespace, DOMString("width").handle(), DOMString(QString::number(newRect.size.width - 1)).handle());
            rect->setAttributeNS(svgNamespace, DOMString("height").handle(), DOMString(QString::number(newRect.size.height - 1)).handle());
            break;
	}
	default:
            NSLog(@"Id not handled: %i", localId);
    }
    [self didChangeValueForKey:@"boundingBox"];
    [self didChangeValueForKey:@"attributedXMLString"];
}

- (void)translateByOffset:(NSSize)offset
{
    NSRect newRect = [self boundingBox];
    newRect.origin.x += offset.width + 1; // HACK
    newRect.origin.y += offset.height + 1;
    [self fitToNewBBox:newRect];
}

- (void)resizeWithPoint:(NSPoint)canvasPoint usingControlPoint:(int)controlPointIndex dragAnchorPoint:(NSPoint)canvasDragAnchorPoint{
    // The controlPointIndex can be used for paths, etc.
    NSRect newRect;
    newRect.origin.x = MIN(canvasDragAnchorPoint.x, canvasPoint.x);
    newRect.origin.y = MIN(canvasDragAnchorPoint.y, canvasPoint.y);
    newRect.size.width = ABS(canvasDragAnchorPoint.x - canvasPoint.x + 1);
    newRect.size.height = ABS(canvasDragAnchorPoint.y - canvasPoint.y + 1);
    
    [self fitToNewBBox:newRect];
}

- (id)valueForKey:(NSString *)key
{
    KSVG::SVGStyledElementImpl *element = (KSVG::SVGStyledElementImpl *)_private->item->userData();
    id theValue = nil;
    
    KDOM::DOMStringImpl *svgNamespace = KDOM::NS_SVG.handle();
    if ([key isEqualToString:@"isFilled"]) {
        KDOM::DOMStringImpl *value = element->getAttributeNS(svgNamespace, DOMString("fill").handle());
        theValue = [NSNumber numberWithBool:(value->string().ascii() != "none")];
    } else if ([key isEqualToString:@"isStroked"]) {
        KDOM::DOMStringImpl *value = element->getAttributeNS(svgNamespace, DOMString("stroke").handle());
        theValue = [NSNumber numberWithBool:(value->string().ascii() != "none")];
    } else if ([key isEqualToString:@"fillColor"]) {
        KDOM::DOMStringImpl *value = element->getAttributeNS(svgNamespace, DOMString("fill").handle());
        theValue = nsColor(QColor(value->string()));
    } else if ([key isEqualToString:@"strokeColor"]) {
        KDOM::DOMStringImpl *value = element->getAttributeNS(svgNamespace, DOMString("stroke").handle());
        theValue = nsColor(QColor(value->string()));
    }
    
    if (theValue)
        return theValue;
    
    return [super valueForKey:key];
}

- (NSAttributedString *)attributedXMLString
{
    QString *nodeText = new QString();
    QTextStream nodeTextStream(nodeText, IO_WriteOnly);
    KDOM::Helper::PrintNode(nodeTextStream, (KDOM::NodeImpl *)_private->item->userData());
    NSString *nodeString = nodeText->getNSString();
    delete nodeText;
    
    if (nodeString)
        return [[NSAttributedString alloc] initWithString:nodeString];
    return nil;
}

@end
