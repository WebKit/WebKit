/*
 * Copyright (C) 2007, 2008, 2009, 2013 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE, INC. ``AS IS'' AND ANY
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
 *
 */

#if PLATFORM(IOS)

#import "config.h"
#import "DOMUIKitExtensions.h"

#import "CachedImage.h"
#import "DOM.h"
#import "DOMCore.h"
#import "DOMExtensions.h"
#import "DOMHTML.h"
#import "DOMHTMLAreaElementInternal.h"
#import "DOMHTMLElementInternal.h"
#import "DOMHTMLImageElementInternal.h"
#import "DOMHTMLSelectElementInternal.h"
#import "DOMInternal.h"
#import "DOMNodeInternal.h"
#import "DOMRangeInternal.h"
#import "FloatPoint.h"
#import "Font.h"
#import "FrameSelection.h"
#import "HTMLAreaElement.h"
#import "htmlediting.h"
#import "HTMLImageElement.h"
#import "HTMLInputElement.h"
#import "HTMLSelectElement.h"
#import "HTMLTextAreaElement.h"
#import "Image.h"
#import "InlineBox.h"
#import "Node.h"
#import "Range.h"
#import "RenderBlock.h"
#import "RenderBlockFlow.h"
#import "RenderBox.h"
#import "RoundedRect.h"
#import "RenderObject.h"
#import "RenderStyleConstants.h"
#import "RenderText.h"
#import "ResourceBuffer.h"
#import "SharedBuffer.h"
#import "TextIterator.h"
#import "VisiblePosition.h"
#import "VisibleUnits.h"

#import "WAKAppKitStubs.h"

using namespace WebCore;

using WebCore::FloatPoint;
using WebCore::Font;
using WebCore::HTMLAreaElement;
using WebCore::HTMLImageElement;
using WebCore::HTMLSelectElement;
using WebCore::InlineBox;
using WebCore::IntRect;
using WebCore::Node;
using WebCore::Position;
using WebCore::Range;
using WebCore::RenderBlock;
using WebCore::RenderBox;
using WebCore::RenderObject;
using WebCore::RenderStyle;
using WebCore::RenderText;
using WebCore::RootInlineBox;
using WebCore::TextIterator;
using WebCore::VisiblePosition;

@implementation DOMRange (UIKitExtensions)

- (void)move:(UInt32)amount inDirection:(WebTextAdjustmentDirection)direction
{
    Range *range = core(self);
    FrameSelection frameSelection;
    frameSelection.moveTo(range, DOWNSTREAM);
    
    TextGranularity granularity = CharacterGranularity;
    // Until WebKit supports vertical layout, "down" is equivalent to "forward by a line" and
    // "up" is equivalent to "backward by a line".
    if (direction == WebTextAdjustmentDown) {
        direction = WebTextAdjustmentForward;
        granularity = LineGranularity;
    } else if (direction == WebTextAdjustmentUp) {
        direction = WebTextAdjustmentBackward;
        granularity = LineGranularity;
    }
    
    for (UInt32 i = 0; i < amount; i++)
        frameSelection.modify(FrameSelection::AlterationMove, (SelectionDirection)direction, granularity);
    
    ExceptionCode ignored;
    range->setStart(frameSelection.start().anchorNode(), frameSelection.start().deprecatedEditingOffset(), ignored);
    range->setEnd(frameSelection.end().anchorNode(), frameSelection.end().deprecatedEditingOffset(), ignored);
}

- (void)extend:(UInt32)amount inDirection:(WebTextAdjustmentDirection)direction
{
    Range *range = core(self);
    FrameSelection frameSelection;
    frameSelection.moveTo(range, DOWNSTREAM);
    
    for (UInt32 i = 0; i < amount; i++)
        frameSelection.modify(FrameSelection::AlterationExtend, (SelectionDirection)direction, CharacterGranularity);    
    
    ExceptionCode ignored;
    range->setStart(frameSelection.start().anchorNode(), frameSelection.start().deprecatedEditingOffset(), ignored);
    range->setEnd(frameSelection.end().anchorNode(), frameSelection.end().deprecatedEditingOffset(), ignored);    
}

- (DOMNode *)firstNode
{
    return kit(core(self)->firstNode());
}

@end

//-------------------

@implementation DOMNode (UIKitExtensions)

// NOTE: Code blatantly copied from [WebInspector _hightNode:] in WebKit/WebInspector/WebInspector.m@19861
- (NSArray *)boundingBoxes
{
    NSArray *rects = nil;
    NSRect bounds = [self boundingBox];
    if (!NSIsEmptyRect(bounds)) {
        if ([self isKindOfClass:[DOMElement class]]) {
            DOMDocument *document = [self ownerDocument];
            DOMCSSStyleDeclaration *style = [document getComputedStyle:(DOMElement *)self pseudoElement:@""];
            if ([[style getPropertyValue:@"display"] isEqualToString:@"inline"])
                rects = [self lineBoxRects];
        } else if ([self isKindOfClass:[DOMText class]]
#if ENABLE(SVG_DOM_OBJC_BINDINGS)
                   && ![[self parentNode] isKindOfClass:NSClassFromString(@"DOMSVGElement")]
#endif
                  )
            rects = [self lineBoxRects];
    }

    if (![rects count])
        rects = [NSArray arrayWithObject:[NSValue valueWithRect:bounds]];

    return rects;
}

- (NSArray *)absoluteQuads
{
    NSArray *quads = nil;
    NSRect bounds = [self boundingBox];
    if (!NSIsEmptyRect(bounds)) {
        if ([self isKindOfClass:[DOMElement class]]) {
            DOMDocument *document = [self ownerDocument];
            DOMCSSStyleDeclaration *style = [document getComputedStyle:(DOMElement *)self pseudoElement:@""];
            if ([[style getPropertyValue:@"display"] isEqualToString:@"inline"])
                quads = [self lineBoxQuads];
        } else if ([self isKindOfClass:[DOMText class]]
#if ENABLE(SVG_DOM_OBJC_BINDINGS)
                   && ![[self parentNode] isKindOfClass:NSClassFromString(@"DOMSVGElement")]
#endif
                   )
            quads = [self lineBoxQuads];
    }

    if (![quads count]) {
        WKQuadObject* quadObject = [[WKQuadObject alloc] initWithQuad:[self absoluteQuad]];
        quads = [NSArray arrayWithObject:quadObject];
        [quadObject release];
    }

    return quads;
}

- (NSArray *)borderRadii
{
    RenderObject* renderer = core(self)->renderer();
    

    if (renderer && renderer->isBox()) {
        RoundedRect::Radii radii = toRenderBox(renderer)->borderRadii();
        return @[[NSValue valueWithSize:(FloatSize)radii.topLeft()],
                 [NSValue valueWithSize:(FloatSize)radii.topRight()],
                 [NSValue valueWithSize:(FloatSize)radii.bottomLeft()],
                 [NSValue valueWithSize:(FloatSize)radii.bottomRight()]];
    }
    NSValue *emptyValue = [NSValue valueWithSize:CGSizeZero];
    return @[emptyValue, emptyValue, emptyValue, emptyValue];
}

- (BOOL)containsOnlyInlineObjects
{
    RenderObject * renderer = core(self)->renderer();
    return  (renderer &&
             renderer->childrenInline() &&
             (renderer->isRenderBlock() && toRenderBlock(renderer)->inlineElementContinuation() == nil) &&
             !renderer->isTable());
}

- (BOOL)isSelectableBlock
{
    RenderObject * renderer = core(self)->renderer();
    return (renderer && (renderer->isRenderBlockFlow() || (renderer->isRenderBlock() && toRenderBlock(renderer)->inlineElementContinuation() != nil)));
}

- (DOMRange *)rangeOfContainingParagraph
{
    DOMRange *result = nil;
    
    Node *node = core(self);    
    VisiblePosition visiblePosition(createLegacyEditingPosition(node, 0), WebCore::DOWNSTREAM);
    VisiblePosition visibleParagraphStart = startOfParagraph(visiblePosition);
    VisiblePosition visibleParagraphEnd = endOfParagraph(visiblePosition);
    
    Position paragraphStart = visibleParagraphStart.deepEquivalent().parentAnchoredEquivalent();
    Position paragraphEnd = visibleParagraphEnd.deepEquivalent().parentAnchoredEquivalent();    
    
    if (paragraphStart.isNotNull() && paragraphEnd.isNotNull()) {
        PassRefPtr<Range> range = Range::create(*node->ownerDocument(), paragraphStart, paragraphEnd);
        result = kit(range.get());
    }
    
    return result;
}

- (CGFloat)textHeight
{  
    RenderObject *o = core(self)->renderer();
    if (o && o->isText()) {
        RenderText *t = toRenderText(o);
        return t->style().computedLineHeight();;
    }
    
    return CGFLOAT_MAX;
}

- (DOMNode *)findExplodedTextNodeAtPoint:(CGPoint)point
{
    // A light, non-recursive version of RenderBlock::positionForCoordinates that looks at
    // whether a point lies within the gaps between its root line boxes, to be called against
    // a node returned from elementAtPoint.  We make the assumption that either the node or one
    // of its immediate children contains the root line boxes in question.
    // See <rdar://problem/6824650> for context.
    RenderObject *renderer = core(self)->renderer();
    if (!renderer || !renderer->isRenderBlockFlow())
        return nil;

    RenderBlock *block = static_cast<RenderBlock *>(renderer);
    
    FloatPoint absPoint(point);
    FloatPoint localPoint = block->absoluteToLocal(absPoint);

    if (!block->childrenInline()) {
        // Look among our immediate children for an alternate box that contains the point.
        for (RenderBox* child = block->firstChildBox(); child; child = child->nextSiblingBox()) {
            if (child->height() == 0 || child->style().visibility() != WebCore::VISIBLE || child->isFloatingOrOutOfFlowPositioned())
                continue;
            float top = child->y();
            
            RenderBox* nextChild = child->nextSiblingBox();
            while (nextChild && nextChild->isFloatingOrOutOfFlowPositioned())
                nextChild = nextChild->nextSiblingBox();
            if (!nextChild) {
                if (localPoint.y() >= top) {
                    block = static_cast<RenderBlock *>(child);
                    break;
                }
                continue;
            }
            
            float bottom = nextChild->y();
            
            if (localPoint.y() >= top && localPoint.y() < bottom && child->isRenderBlock()) {
                block = static_cast<RenderBlock *>(child);
                break;
            }                
        }
        
        if (!block->childrenInline())
            return nil;
        
        localPoint = block->absoluteToLocal(absPoint);
    }

    RenderBlockFlow *blockFlow = toRenderBlockFlow(block);
    
    // Only check the gaps between the root line boxes.  We deliberately ignore overflow because
    // experience has shown that hit tests on an exploded text node can fail when within the
    // overflow region.
    for (RootInlineBox *cur = blockFlow->firstRootBox(); cur && cur != blockFlow->lastRootBox(); cur = cur->nextRootBox()) {
        float currentBottom = cur->y() + cur->logicalHeight();        
        if (localPoint.y() < currentBottom)
            return nil;

        RootInlineBox *next = cur->nextRootBox();
        float nextTop = next->y();
        if (localPoint.y() < nextTop) {
            InlineBox *inlineBox = cur->closestLeafChildForLogicalLeftPosition(localPoint.x());
            if (inlineBox && inlineBox->behavesLikeText() && inlineBox->renderer().isText()) {
                RenderText *t = toRenderText(&inlineBox->renderer());
                if (t->textNode()) {
                    return kit(t->textNode());
                }
            }
        }

    }
    return nil;    
}

@end

//-----------------

@implementation DOMElement (DOMUIKitComplexityExtensions) 

- (int)structuralComplexityContribution { return 0; }

@end

@implementation DOMHTMLElement (DOMUIKitComplexityExtensions) 

- (int)structuralComplexityContribution
{
    int result = 0;
    RenderObject * renderer = core(self)->renderer();
    if (renderer) {
        if (renderer->isFloatingOrOutOfFlowPositioned() ||
            renderer->isWidget()) {
            result = INT_MAX;
        } else if (renderer->isEmpty()) {
            result = 0;
        } else if (renderer->isRenderBlockFlow() || (renderer->isRenderBlock() && toRenderBlock(renderer)->inlineElementContinuation() != 0)) {
            BOOL noCost = NO;
            if (renderer->isBox()) {
                RenderBox *asBox = renderer->enclosingBox();
                RenderObject *parent = asBox->parent();
                RenderBox *parentRenderBox = (parent && parent->isBox()) ? toRenderBox(parent) : 0;
                if (parentRenderBox && asBox && asBox->width() == parentRenderBox->width()) {
                    noCost = YES;
                }
            }
            result = (noCost ? 0 : 1);
        } else if (renderer->hasTransform()) {
            result = INT_MAX;
        }
    }
    return result;
}

@end

@implementation DOMHTMLBodyElement (DOMUIKitComplexityExtensions)
- (int)structuralComplexityContribution { return 0; }
@end


// Maximally complex elements

@implementation DOMHTMLFormElement (DOMUIKitComplexityExtensions)
- (int)structuralComplexityContribution { return INT_MAX; }
@end

@implementation DOMHTMLTableElement (DOMUIKitComplexityExtensions)
- (int)structuralComplexityContribution { return INT_MAX; }
@end

@implementation DOMHTMLFrameElement (DOMUIKitComplexityExtensions)
- (int)structuralComplexityContribution { return INT_MAX; }
@end

@implementation DOMHTMLIFrameElement (DOMUIKitComplexityExtensions)
- (int)structuralComplexityContribution { return INT_MAX; }
@end

@implementation DOMHTMLButtonElement (DOMUIKitComplexityExtensions)
- (int)structuralComplexityContribution { return INT_MAX; }
@end

@implementation DOMHTMLTextAreaElement (DOMUIKitComplexityExtensions)
- (int)structuralComplexityContribution { return INT_MAX; }
@end

@implementation DOMHTMLInputElement (DOMUIKitComplexityExtensions)
- (int)structuralComplexityContribution { return INT_MAX; }
@end

@implementation DOMHTMLSelectElement (DOMUIKitComplexityExtensions)
- (int)structuralComplexityContribution { return INT_MAX; }
@end


//-----------------
        
@implementation DOMHTMLAreaElement (DOMUIKitExtensions)

- (CGRect)boundingBoxWithOwner:(DOMNode *)anOwner
{
    // ignores transforms
    return anOwner ? pixelSnappedIntRect(core(self)->computeRect(core(anOwner)->renderer())) : CGRectZero;
}

- (WKQuad)absoluteQuadWithOwner:(DOMNode *)anOwner
{
    if (anOwner) {
        // FIXME: ECLAIR
        //WebCore::FloatQuad theQuad = core(self)->getAbsoluteQuad(core(anOwner)->renderer());
        WebCore::IntRect rect = pixelSnappedIntRect(core(self)->computeRect(core(anOwner)->renderer()));
        WKQuad quad;
        quad.p1 = CGPointMake(rect.x(), rect.y());
        quad.p2 = CGPointMake(rect.maxX(), rect.y());
        quad.p3 = CGPointMake(rect.maxX(), rect.maxY());
        quad.p4 = CGPointMake(rect.x(), rect.maxY());
        return quad;
    }

    WKQuad zeroQuad = { CGPointZero, CGPointZero, CGPointZero, CGPointZero };
    return zeroQuad;
}

- (NSArray *)boundingBoxesWithOwner:(DOMNode *)anOwner
{
    return [NSArray arrayWithObject:[NSValue valueWithRect:[self boundingBoxWithOwner:anOwner]]];
}

- (NSArray *)absoluteQuadsWithOwner:(DOMNode *)anOwner
{
    WKQuadObject* quadObject = [[WKQuadObject alloc] initWithQuad:[self absoluteQuadWithOwner:anOwner]];
    NSArray*    quadArray = [NSArray arrayWithObject:quadObject];
    [quadObject release];
    return quadArray;
}

@end

@implementation DOMHTMLSelectElement (DOMUIKitExtensions)

- (unsigned)completeLength
{
    return core(self)->listItems().size();
}

- (DOMNode *)listItemAtIndex:(int)anIndex
{
    return kit(core(self)->listItems()[anIndex]);
}

@end

@implementation DOMHTMLImageElement (WebDOMHTMLImageElementOperationsPrivate)

- (NSData *)dataRepresentation:(BOOL)rawImageData
{
    WebCore::CachedImage *cachedImage = core(self)->cachedImage();
    if (!cachedImage)
        return nil;
    WebCore::Image *image = cachedImage->image();
    if (!image)
        return nil;
    WebCore::SharedBuffer *data = nil;
    if (rawImageData) {
        ResourceBuffer *resourceBuffer = cachedImage->resourceBuffer();
        if (resourceBuffer)
            data = resourceBuffer->sharedBuffer();
    } else {
        data = image->data();
    }
    if (!data)
        return nil;
    
    return [data->createNSData().leakRef() autorelease];
}

- (NSString *)mimeType
{
    WebCore::CachedImage *cachedImage = core(self)->cachedImage();
    if (!cachedImage || !cachedImage->image())
        return nil;
    
    return cachedImage->response().mimeType();
}

@end

#endif
