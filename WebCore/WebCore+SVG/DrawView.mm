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


#import "DrawViewPrivate.h"
#import "DrawCanvasItem.h"
#import "DrawDocumentPrivate.h"

#import <kcanvas/KCanvas.h>
#import <kcanvas/KCanvasContainer.h>
#import <kcanvas/device/quartz/KCanvasViewQuartz.h>
#import <kcanvas/device/quartz/KRenderingDeviceQuartz.h>

// This should go in the Prefix header eventually.
#define foreacharray(__variable, __container) \
for (int __variable##__i=0, __variable##__n=[__container count];  \
     __variable##__i < __variable##__n && (__variable = [__container objectAtIndex:__variable##__i]);  \
     ++__variable##__i)

#define NSDifferencePoint(a,b) (NSMakePoint(a.x-b.x, a.y-b.y))
#define NSSumPoint(a,b) (NSMakePoint(a.x+b.x, a.y+b.y))

// editing knobs.
#define KNOB_SIZE 6
#define KNOB_HALF_SIZE 3

typedef enum {
    DragActionNone = 0,
    DragActionPan,
    DragActionResize,
    DragActionMove,
    DragActionSelection
} DrawDragAction;

// this is sorta a hack
@interface DrawDocument (InternalCanvasMethod)
- (KCanvas *)canvas;
@end


@interface DrawViewPrivate : NSObject {
    @public
    NSArray *selectedItems;
    
    DrawView *drawView; // pointer to "owner"
    DrawDocument *document;
    
    NSColor *backgroundColor;
    
    int dragControlPointIndex;
    NSPoint dragStartPoint;
    NSPoint lastDragPoint;
    NSPoint canvasDragAnchorPoint; // used mostly for group resize.
    NSPoint canvasDragOriginOffset;
    
    int trackingRectTag;
    DrawDragAction currentDragAction;
    NSRect selectionRect;
    
    NSSize _maxSize;
    
    // temporary hack, will eventually be stored in scrollview/view
    float canvasZoom;
    NSPoint canvasvisibleOrigin;
    
    KCanvasViewQuartz *canvasView;
    KRenderingDeviceContextQuartz *quartzContext;
}
@end 

@implementation DrawViewPrivate

+ (void)setFilterSupportEnabled:(BOOL)enabled
{
    KRenderingDeviceQuartz::setFiltersEnabled(enabled);
}

+ (BOOL)isFilterSupportEnabled
{
    return KRenderingDeviceQuartz::filtersEnabled();
}

- (id)initWithDrawView:(DrawView *)view
{
    if ((self = [super init])) {
        drawView = view;
        quartzContext = new KRenderingDeviceContextQuartz();
        canvasView = new KCanvasViewQuartz();
        canvasView->setView(drawView);
        canvasView->setContext(quartzContext);
    }
    return self;
}

- (DrawDocument *)document
{
    return document;
}

- (void)setDocument:(DrawDocument *)doc
{
    if (doc != document) {
        [document unregisterView:drawView];
        [document release];
        document = [doc retain];
        [document registerView:drawView];
        
        delete canvasView;
        canvasView = NULL;
        if (document) {
            canvasView = new KCanvasViewQuartz();
            canvasView->init([document canvas], NULL);
            canvasView->setView(drawView);
            canvasView->setContext(quartzContext);
        }
    }
}

// c++ enabled drawing code.
- (void)drawRect:(NSRect)dirtyViewRect
{
    if (![document canvas])
        return;
    if (!canvasView)
        return;
    
    // push the drawing context
    KRenderingDevice *renderingDevice = [document canvas]->renderingDevice();
    
    // Apply the top-level "world transform" for zoom/pan
    CGContextRef context = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
    quartzContext->setCGContext(context); // should probably just be set once...
    renderingDevice->pushContext(quartzContext);
    CGContextSaveGState(context);
    CGContextConcatCTM(context, CGAffineTransformInvert([drawView transformFromViewToCanvas]));
    
    // do the draw
    //[document drawRect:[drawView mapViewRectToCanvas:dirtyViewRect] inCGContext:context];
    [document canvas]->rootContainer()->draw(QRect([drawView mapViewRectToCanvas:dirtyViewRect]));
    
    // restore drawing state
    CGContextRestoreGState(context);
    renderingDevice->popContext();
    quartzContext->setCGContext(NULL);
}

- (void)dealloc
{
    // If we go away, make sure we clear the view pointer.
    [document unregisterView:drawView];
    delete canvasView;
    delete quartzContext;
    [super dealloc];
}
@end

NSArray *DrawViewDragTypes;

@interface DrawView (InternalMethods)
- (void)drawKnobsForRect:(NSRect)rect;
- (void)updateCanvasScale;
- (void)enableMouseMovedEventsIfNeeded;
@end

@implementation DrawView

+ (void)initialize
{
    //DrawViewDragTypes = [[NSArray arrayWithObject:NSFilenamesPboardType] retain];
    [self setKeys:[NSArray arrayWithObject:@"toolMode"] triggerChangeNotificationsForDependentKey:@"selectedCanvasItems"];
}

- (id)initWithFrame:(NSRect)frameRect
{
    if ((self = [super initWithFrame:frameRect]) != nil) {
        _private = [[DrawViewPrivate alloc] initWithDrawView:self];
    }
    return self;
}

- (void)dealloc
{
    [_private release];
    [super dealloc];
}

- (NSRect)selectionCanvasBoundingBox
{
    NSRect selectionBoundingBox = NSZeroRect;
    DrawCanvasItem *item = nil;
    NSArray *selectedItems = [self selectedCanvasItems];
    foreacharray(item, selectedItems) {
        selectionBoundingBox = NSUnionRect(selectionBoundingBox,[item boundingBox]);
    }
    return selectionBoundingBox;
}

- (void)_clearAndDrawBackground:(NSRect)dirtyRect
{
    [[NSColor lightGrayColor] set];
    NSRectFill(dirtyRect);
    
    [[self backgroundColor] set];
    NSSize canvasSize = [[self document] canvasSize];
    NSRect canvasRect = NSMakeRect(0,0, canvasSize.width, canvasSize.height);
    NSRectFill([self mapCanvasRectToView:canvasRect]);
}

- (void)_drawSelectionKnobs
{
    if ((_toolMode == DrawViewToolArrow) && [[self selectedCanvasItems] count]) {
        NSRect viewSelectionBoundingBox = [self mapCanvasRectToView:[self selectionCanvasBoundingBox]];
        [self drawKnobsForRect:viewSelectionBoundingBox];
    }
    
    if (_private->currentDragAction == DragActionSelection) {
        [[NSColor lightGrayColor] set];
        NSFrameRect(_private->selectionRect);
    }
}

- (void)drawRect:(NSRect)dirtyRect
{
    [self _clearAndDrawBackground:dirtyRect];
    [_private drawRect:dirtyRect];
    [self _drawSelectionKnobs];
}

- (void)setBackgroundColor:(NSColor *)color
{
    id oldColor = _private->backgroundColor ;
    _private->backgroundColor = [color retain];
    [oldColor release];
}

- (NSColor *)backgroundColor
{
    if (!_private->backgroundColor)
        _private->backgroundColor = [[NSColor whiteColor] retain];
    return _private->backgroundColor;
}

- (BOOL)isOpaque
{
    return ![_private->backgroundColor isEqual:[NSColor clearColor]];
}

- (BOOL)isFlipped
{
    return YES;
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (BOOL)acceptsFirstMouse:(NSEvent *)event
{
    return ((_toolMode == DrawViewToolBrowse) || (_toolMode == DrawViewToolArrow));
}

- (NSImageScaling)imageScaling
{
    return _scaleRule;
}

- (void)setImageScaling:(NSImageScaling)scaling
{
    _scaleRule = scaling;
    [self updateCanvasScale];
}

- (void)setFrame:(NSRect)newFrame
{
    [self removeTrackingRect:_private->trackingRectTag];
    [self setFrameOrigin:newFrame.origin];
    [self setFrameSize:newFrame.size];
    _private->trackingRectTag =
        [self addTrackingRect:[self frame]
                        owner:self userData:NULL
                 assumeInside:[self mouse:[NSEvent mouseLocation] inRect:[self bounds]]];
}

- (void)setFrameSize:(NSSize)newSize
{
    [super setFrameSize:newSize];
    [self updateCanvasScale];
}

- (void)setDocument:(DrawDocument *)document
{
    if ([_private document] != document) {
        [_private setDocument:document];
        [self updateCanvasScale];
        [self setNeedsDisplay:YES];
    }
}

- (DrawDocument *)document
{
    return [_private document];
}

- (KCanvasViewQuartz *)canvasView
{
    return _private->canvasView;
}

#pragma mark -
#pragma mark Editing support

- (void)setEditable:(BOOL)editable
{
    [self unregisterDraggedTypes];
    _isEditable = editable;
    if (_isEditable)
        [self registerForDraggedTypes:DrawViewDragTypes];
}

- (BOOL)isEditable
{
    return _isEditable;
}

- (NSRect)boundsForCanvasItem:(DrawCanvasItem *)canvasItem
{
    NSRect itemBBoxInCanvas = [canvasItem boundingBox];
    NSRect itemBBoxInView = [self mapCanvasRectToView:itemBBoxInCanvas];
    return NSInsetRect(itemBBoxInView, -KNOB_HALF_SIZE, -KNOB_HALF_SIZE);
}

- (NSArray *)selectedCanvasItems
{
    // selection only "exists" when using the arrow tool.
    // but we sill remember selection between uses.
    return _private->selectedItems;
}

- (void)setSelectedCanvasItems:(NSArray *)selectedItems
{
    DrawCanvasItem *newSelectedItem = nil;
    NSMutableArray *newSelectedItems = [[NSMutableArray alloc] init];
    foreacharray(newSelectedItem, selectedItems) {
        [newSelectedItems addObject:newSelectedItem];
    }
    [_private->selectedItems release];
    if ([newSelectedItems count])
        _private->selectedItems = newSelectedItems;
    else
        _private->selectedItems = nil;
    [self setNeedsDisplay:YES]; // FIXME: just invalidate the whole view for now.
}

- (IBAction)moveSelectionForward:(id)sender
{
    DrawCanvasItem *item = nil;
    NSArray *selectedItems = [self selectedCanvasItems];
    foreacharray(item, selectedItems) {
        [item raise];
    }
}

- (IBAction)moveSelectionBackward:(id)sender
{
    DrawCanvasItem *item = nil;
    NSArray *selectedItems = [self selectedCanvasItems];
    foreacharray(item, selectedItems) {
        [item lower];
    }
}

- (NSArray *)canvasControlPointsForSelection
{
    NSArray *selectedItems = [self selectedCanvasItems];
    if ([selectedItems count] == 1)
        return [[selectedItems objectAtIndex:0] controlPoints];
    else if ([selectedItems count] > 1)
        return [DrawCanvasItem controlPointsForRect:[self selectionCanvasBoundingBox]];
    
    return nil;
}

- (NSPoint)canvasDragAnchorPointForControlPointIndex:(int)controlPointIndex
{
    NSArray *selectedItems = [self selectedCanvasItems];
    if ([selectedItems count] == 1) {
        return [[selectedItems objectAtIndex:0] dragAnchorPointForControlPointIndex:controlPointIndex];
    } else if ([selectedItems count] > 1) {
        NSArray *controlPoints = [DrawCanvasItem controlPointsForRect:[self selectionCanvasBoundingBox]];
        return [DrawCanvasItem dragAnchorPointForControlPointIndex:controlPointIndex fromRectControlPoints:controlPoints];
    }
    NSLog(@"Error, no selection, you shouldn't ask for drag anchor point!");
    return NSZeroPoint;
}

- (int)controlPointIndexForCanvasHitPoint:(NSPoint)canvasPoint
{
    NSValue *controlPointValue = nil;
    NSArray *controlPoints = [self canvasControlPointsForSelection];
    
    NSSize canvasKnobSize = [self mapViewSizeToCanvas:NSMakeSize(KNOB_SIZE, KNOB_SIZE)];
    
    NSRect controlRect = NSZeroRect;
    controlRect.size = canvasKnobSize;
    NSSize halfCanvasKnobSize = NSMakeSize(canvasKnobSize.width/2.f, canvasKnobSize.height/2.f);
    int index = 0;
    
    foreacharray (controlPointValue, controlPoints) {
        NSPoint controlPoint = [controlPointValue pointValue];
        controlRect.origin.x = controlPoint.x - halfCanvasKnobSize.width;
        controlRect.origin.y = controlPoint.y - halfCanvasKnobSize.height;
        
        if (NSPointInRect(canvasPoint, controlRect))
            return index;
        index++;
    }
    return NSNotFound;
}

- (BOOL)createCanvasItemAtPoint:(NSPoint)mousePoint
{
    // first map to canvas coords.
    mousePoint = [self mapViewPointToCanvas:mousePoint];
    
    DrawCanvasItem *newItem = [[_private document] createItemForTool:_toolMode atPoint:mousePoint];
    if (newItem) {
        [self setSelectedCanvasItems:[NSArray arrayWithObject:newItem]];
        return YES;
    }
    return NO;
}

- (IBAction)deleteSelection:(id)sender
{
    NSArray *oldItems = [[self selectedCanvasItems] retain];
    [self setSelectedCanvasItems:nil];
    DrawCanvasItem *oldItem = nil;
    foreacharray(oldItem, oldItems) {
        [[_private document] removeItemFromDOM:oldItem];
    }
    [oldItems release];
}

- (int)toolMode
{
    return _toolMode;
}

- (void)setToolMode:(int)newToolMode
{
    _toolMode = (DrawViewTool)newToolMode;
    //[[self window] invalidateCursorRectsForView:self];
    // Not getting called until the view becomes key...
    [[self window] resetCursorRects]; // HACK?
    if ([self mouse:[NSEvent mouseLocation] inRect:[self bounds]])
        [self enableMouseMovedEventsIfNeeded];
}

- (void)resetCursorRects
{
    [super resetCursorRects];
    
    NSCursor *toolCursor = nil;
    switch (_toolMode) {
	case DrawViewToolPan:
            toolCursor = [NSCursor openHandCursor];
            break;
	case DrawViewToolZoom:
            toolCursor = [NSCursor closedHandCursor]; // for now...
            break;
	case DrawViewToolLine:
	case DrawViewToolElipse:
	case DrawViewToolRectangle:
	case DrawViewToolPolyLine:
	case DrawViewToolArc:
            toolCursor = [NSCursor crosshairCursor];
            break;
	default:
            break;
    }
    if (toolCursor)
        [self addCursorRect:[self frame] cursor:toolCursor];
}

- (void)drawKnobsForRect:(NSRect)rect
{
    [[NSColor blackColor] set];
    [NSBezierPath fillRect:NSMakeRect(rect.origin.x - KNOB_HALF_SIZE, rect.origin.y - KNOB_HALF_SIZE, KNOB_SIZE, KNOB_SIZE)];
    [NSBezierPath fillRect:NSMakeRect(NSMaxX(rect) - KNOB_HALF_SIZE, rect.origin.y - KNOB_HALF_SIZE, KNOB_SIZE, KNOB_SIZE)];
    [NSBezierPath fillRect:NSMakeRect(rect.origin.x - KNOB_HALF_SIZE, NSMaxY(rect) - KNOB_HALF_SIZE, KNOB_SIZE, KNOB_SIZE)];
    [NSBezierPath fillRect:NSMakeRect(NSMaxX(rect) - KNOB_HALF_SIZE, NSMaxY(rect) - KNOB_HALF_SIZE, KNOB_SIZE, KNOB_SIZE)];
}


#pragma mark -
#pragma mark Event support

- (void)mouseDown:(NSEvent *)theEvent
{
    NSPoint mousePoint = [self convertPoint:[theEvent locationInWindow] fromView:nil];
    NSPoint canvasPoint = [self mapViewPointToCanvas:mousePoint];
    _private->dragStartPoint = mousePoint;
    _private->lastDragPoint = mousePoint;
    
    switch (_toolMode) {
	case DrawViewToolBrowse:
	{
            // send mouse down events to DOM
            if (![[_private document] documentListensForMouseDownEvents])
                return;
            [[_private document] propagateMouseDownEvent:theEvent fromView:self];
            break;
	}
	case DrawViewToolArrow:
	{
            if ( (_private->dragControlPointIndex = [self controlPointIndexForCanvasHitPoint:canvasPoint]) != NSNotFound) {
                _private->canvasDragAnchorPoint = [self canvasDragAnchorPointForControlPointIndex:_private->dragControlPointIndex];
                NSLog(@"resize -- dragStartPoint: %@ canvasPoint: %@ controlIndex: %i canvasDragAnchorPoint: %@",
                      NSStringFromPoint(mousePoint), NSStringFromPoint(canvasPoint),
                      _private->dragControlPointIndex, NSStringFromPoint(_private->canvasDragAnchorPoint));
                _private->currentDragAction = DragActionResize;
            } else {
                // FIXME: we really should select SVGElements, not canvas items...
                DrawCanvasItem *hitItem = [[_private document] canvasItemAtPoint:canvasPoint];			
                //NSLog(@"move -- dragStartPoint: %@ canvasPoint: %@", NSStringFromPoint(mousePoint), NSStringFromPoint(canvasPoint));
                if (hitItem) {
                    NSArray *hitItems = [NSArray arrayWithObject:hitItem];
                    [self setSelectedCanvasItems:hitItems];
                    _private->currentDragAction = DragActionMove;
                    NSPoint hitOrigin = [hitItem boundingBox].origin;
                    _private->canvasDragOriginOffset = NSDifferencePoint(hitOrigin, canvasPoint);
                } else {
                    [self setSelectedCanvasItems:nil];
                    _private->currentDragAction = DragActionSelection;
                }
            }
            break;
	}
	case DrawViewToolPan:
            [[NSCursor closedHandCursor] push];
            _private->currentDragAction = DragActionPan;
            //NSLog(@"mouseDown: %@", NSStringFromPoint(_private->lastDragPoint));
            break;
            // creation tools:
	case DrawViewToolLine:
	case DrawViewToolElipse:
	case DrawViewToolTriangle:
	case DrawViewToolRectangle:
	case DrawViewToolPolyLine:
	case DrawViewToolArc:
	{
            if ([self createCanvasItemAtPoint:mousePoint]) {
                _private->currentDragAction = DragActionResize;
                _private->canvasDragAnchorPoint = canvasPoint;
                _private->dragControlPointIndex = 0;
            }
            break;
	}
	default:
            break;
    }
}

- (void)mouseDragged:(NSEvent *)theEvent
{
    NSPoint mousePoint = [self convertPoint:[theEvent locationInWindow] fromView:nil];
    
    switch (_toolMode) {
	case DrawViewToolBrowse:
	{
            // send mouse dragged events to DOM.
            break;
	}
	case DrawViewToolArrow:
	{
            // move shapes here.
            if (_private->currentDragAction == DragActionMove) {
                NSSize dragOffset = NSMakeSize(mousePoint.x - _private->lastDragPoint.x, mousePoint.y - _private->lastDragPoint.y);
                NSSize canvasDragOffset = [self mapViewSizeToCanvas:dragOffset];
                NSLog(@"move: translating selection by: %@", NSStringFromSize(canvasDragOffset));
                DrawCanvasItem *item = nil;
                NSArray *selectedItems = [self selectedCanvasItems];
                foreacharray(item, selectedItems) {
                    [self setNeedsDisplayInRect:[self boundsForCanvasItem:item]];
                    [item translateByOffset:canvasDragOffset];
                    [self setNeedsDisplayInRect:[self boundsForCanvasItem:item]];
                }
            } else if (_private->currentDragAction == DragActionSelection) {
                [self setNeedsDisplayInRect:_private->selectionRect];
                _private->selectionRect = NSMakeRect(_private->dragStartPoint.x,
                                                     _private->dragStartPoint.y,
                                                     mousePoint.x - _private->dragStartPoint.x,
                                                     mousePoint.y - _private->dragStartPoint.y);
                [self setNeedsDisplayInRect:_private->selectionRect];
            }
            // fall through for resize code.
	}
	case DrawViewToolLine:
	case DrawViewToolElipse:
	case DrawViewToolTriangle:
	case DrawViewToolRectangle:
	case DrawViewToolPolyLine:
	case DrawViewToolArc:
	{
            if (_private->currentDragAction == DragActionResize) {
                NSPoint canvasPoint = [self mapViewPointToCanvas:mousePoint];
                DrawCanvasItem *item = nil;
                NSArray *selectedItems = [self selectedCanvasItems];
                foreacharray(item, selectedItems) {
                    [self setNeedsDisplayInRect:[self boundsForCanvasItem:item]];
                    [item resizeWithPoint:canvasPoint usingControlPoint:_private->dragControlPointIndex dragAnchorPoint:_private->canvasDragAnchorPoint];
                    [self setNeedsDisplayInRect:[self boundsForCanvasItem:item]];
                }
            }
            break;
	}
            
	case DrawViewToolPan:
	{
            NSPoint diff = NSDifferencePoint(mousePoint, _private->lastDragPoint);
            NSPoint canvasOrigin = [self canvasVisibleOrigin];
            NSPoint newOrigin = NSDifferencePoint(canvasOrigin, diff);
            [self setCanvasVisibleOrigin:newOrigin];
            //		NSLog(@"mouseDragged: %@  diff: %@ old origin: %@ newOrigin: %@",
            //			NSStringFromPoint(_private->lastDragPoint), 
            //			NSStringFromPoint(diff), NSStringFromPoint(canvasOrigin), NSStringFromPoint(newOrigin));
            break;
	}
            
	default:
            break;
    }
    
    // record the last drag point.
    _private->lastDragPoint = mousePoint;
}


- (void)mouseUp:(NSEvent *)theEvent
{
    //NSPoint mousePoint = [self convertPoint:[theEvent locationInWindow] fromView:nil];
    
    switch (_toolMode) {
	case DrawViewToolBrowse:
	{
            if (![[_private document] documentListensForMouseUpEvents])
                return;
            [[_private document] propagateMouseUpEvent:theEvent fromView:self];
	}
	case DrawViewToolPan:
            [NSCursor pop];
            break;
	case DrawViewToolArrow:
            if (_private->currentDragAction == DragActionSelection) {
                [self setNeedsDisplayInRect:_private->selectionRect];
                // figure out what all was in that rect...
                _private->selectionRect = NSZeroRect;
            }
            break;
	default:
            break;
    }
    _private->currentDragAction = DragActionNone;
}

- (void)disableMouseMovedEvents
{
    if ([[self window] acceptsMouseMovedEvents])
        NSLog(@"Mouse move events now OFF");
    [[self window] setAcceptsMouseMovedEvents:NO];
}

- (void)enableMouseMovedEventsIfNeeded
{
    // FIXME:  This could still be more efficient
    BOOL needsMouseMoved = (_toolMode == DrawViewToolBrowse)
    && [[_private document] documentListensForMouseMovedEvents];
    if (needsMouseMoved != [[self window] acceptsMouseMovedEvents])
        NSLog(@"Mouse move events now %@", needsMouseMoved ? @"ON" : @"OFF");
    if (needsMouseMoved) {
        [[self window] setAcceptsMouseMovedEvents:needsMouseMoved];
    }
}

- (void)mouseEntered:(NSEvent *)theEvent
{
    [self enableMouseMovedEventsIfNeeded];
}

- (void)mouseExited:(NSEvent *)theEvent
{
    [self disableMouseMovedEvents];
}

- (void)mouseMoved:(NSEvent *)theEvent
{
    //NSPoint mousePoint = [self convertPoint:[theEvent locationInWindow] fromView:nil];
    
    switch (_toolMode) {
	case DrawViewToolBrowse:
	{
            if (![[_private document] documentListensForMouseMovedEvents])
                return;
            
            NSCursor *cursor = [[_private document] cursorAfterPropagatingMouseMovedEvent:theEvent fromView:self];
            if (cursor && (cursor != [NSCursor currentCursor])) NSLog(@"Changing cursor: %@ name: %@", cursor, [[cursor image] name]);
            [cursor set];
	}
	default:
            break;
    }
}

- (void)keyDown:(NSEvent *)theEvent
{
    if (_toolMode == DrawViewToolArrow) {
        unichar key = [[theEvent charactersIgnoringModifiers] characterAtIndex:0];
        int flags = [theEvent modifierFlags] & NSDeviceIndependentModifierFlagsMask;
        if (([theEvent keyCode] == NSDeleteFunctionKey) || ((key == NSDeleteCharacter) && (flags == 0)) ) {
            [self deleteSelection:nil];
        }
    } else if (_toolMode == DrawViewToolBrowse) {
        // Propagate the key event up through the DOM
    }
}

#pragma mark -
#pragma mark Zoom/Pan support

- (IBAction)zoomToFit:(id)sender
{
    NSSize canvasSize = [[_private document] canvasSize];
    float widthScale = canvasSize.width / [self bounds].size.width;
    float heightScale = canvasSize.height / [self bounds].size.height;
    
    float minScale = MAX(widthScale, heightScale);
    [self setCanvasZoom:minScale];
}

- (void)updateCanvasScale
{
    if (_scaleRule == NSScaleProportionally) {
        [self zoomToFit:nil];
    } else if (_scaleRule == NSScaleToFit) {
        // Not yet supported!
    } else if (_scaleRule == NSScaleNone) {
        [self setCanvasZoom:1.0];
    }
}

- (void)sizeToFitCanvas
{
    NSSize canvasSize = [[_private document] canvasSize];
    if ((canvasSize.width == 0) || (canvasSize.height == 0))
        return; // avoid disappearing.
    [self setFrameSize:canvasSize];
}

- (void)sizeToFitViewBox
{
    // FIXME: this is broken!
    [self sizeToFitCanvas];
}

- (IBAction)zoomIn:(id)sender
{
    NSPoint center = [self canvasvisibleCenterPoint];
    [self setCanvasZoom:([self canvasZoom] * 0.9)];
    [self panToCanvasCenterPoint:center];
}

- (IBAction)zoomOut:(id)sender
{
    NSPoint center = [self canvasvisibleCenterPoint];
    [self setCanvasZoom:([self canvasZoom] * 1.1)];
    [self panToCanvasCenterPoint:center];
}

- (IBAction)zoomOriginal:(id)sender
{
    // do these need to call into the canvas or document instead?
    [self setCanvasZoom:1.0];
    [self setCanvasVisibleOrigin:NSMakePoint(0,0)];
}

- (CGAffineTransform)transformFromViewToCanvas
{
    float zoom = [self canvasZoom];
    CGAffineTransform transform = CGAffineTransformMakeScale(zoom, zoom);
    NSPoint origin = [self canvasVisibleOrigin];
    return CGAffineTransformTranslate(transform,origin.x,origin.y);
}

- (NSPoint)mapViewPointToCanvas:(NSPoint)viewPoint
{
    CGPoint canvasPoint = CGPointApplyAffineTransform(*(CGPoint *)&viewPoint, [self transformFromViewToCanvas]);
    return (*(NSPoint *)&canvasPoint);
}
- (NSRect)mapViewRectToCanvas:(NSRect)viewRect
{
    CGRect canvasRect = CGRectApplyAffineTransform(*(CGRect *)&viewRect, [self transformFromViewToCanvas]);
    return (*(NSRect *)&canvasRect);
}
- (NSSize)mapViewSizeToCanvas:(NSSize)viewSize
{
    CGSize canvasSize = CGSizeApplyAffineTransform(*(CGSize *)&viewSize, [self transformFromViewToCanvas]);
    return (*(NSSize *)&canvasSize);
}

- (NSPoint)mapCanvasPointToView:(NSPoint)canvasPoint
{
    CGPoint viewPoint = CGPointApplyAffineTransform(*(CGPoint *)&canvasPoint, CGAffineTransformInvert([self transformFromViewToCanvas]));
    return (*(NSPoint *)&viewPoint);
}
- (NSRect)mapCanvasRectToView:(NSRect)canvasRect
{
    CGRect viewRect = CGRectApplyAffineTransform(*(CGRect *)&canvasRect, CGAffineTransformInvert([self transformFromViewToCanvas]));
    return (*(NSRect *)&viewRect);
}
- (NSSize)mapCanvasSizeToView:(NSSize)canavsSize
{
    CGSize viewSize = CGSizeApplyAffineTransform(*(CGSize *)&canavsSize, CGAffineTransformInvert([self transformFromViewToCanvas]));
    return (*(NSSize *)&viewSize);
}
#pragma mark -
#pragma mark Canvas Zoom/Pan (Hack!)

- (NSPoint)canvasVisibleOrigin
{
    return _private->canvasvisibleOrigin;
}

- (void)setCanvasVisibleOrigin:(NSPoint)newOrigin
{
    // will eventually move into NSScrollView I figure.
    _private->canvasvisibleOrigin = newOrigin;
    [self setNeedsDisplay:YES];
}

- (NSPoint)canvasvisibleCenterPoint
{
    NSPoint centerPoint = [self canvasVisibleOrigin];
    NSSize canvasVisibleSize = [self mapViewSizeToCanvas:[self bounds].size];
    centerPoint.x += canvasVisibleSize.width/2.f;
    centerPoint.y += canvasVisibleSize.height/2.f;
    //	NSPoint cvo = [self canvasVisibleOrigin];
    //	NSLog(@"Visible center: %@  visible origin: %@  visible size: %@  farPoint: %@",
    //		NSStringFromPoint(centerPoint), NSStringFromPoint(cvo),
    //		NSStringFromSize(canvasVisibleSize), NSStringFromPoint(NSMakePoint(cvo.x + canvasVisibleSize.width, cvo.y + canvasVisibleSize.height)));
    return centerPoint;
}

- (void)panToCanvasCenterPoint:(NSPoint)newCenter
{
    NSPoint newVisibleOrigin = newCenter;
    NSSize canvasVisibleSize = [self mapViewSizeToCanvas:[self bounds].size];
    newVisibleOrigin.x -= canvasVisibleSize.width/2.f;
    newVisibleOrigin.y -= canvasVisibleSize.height/2.f;
    //	NSLog(@"setting new origin: %@  visible size: %@", NSStringFromPoint(newVisibleOrigin), NSStringFromSize(canvasVisibleSize));
    [self setCanvasVisibleOrigin:newVisibleOrigin];
} 

- (float)canvasZoom
{
    return _private->canvasZoom;
    //return [self scale].width;
}

- (void)setCanvasZoom:(float)newZoom
{
    if (_private->canvasZoom != newZoom) {
        _private->canvasZoom = newZoom;
        //[self setScale:NSMakeSize(newZoom, newZoom)];
        [self setNeedsDisplay:YES];
    }
}

@end

