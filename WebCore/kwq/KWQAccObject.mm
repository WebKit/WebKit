/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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

// need this until accesstool supports arrays of markers
#define MARKERARRAY_SELF_TEST 0

// for AXTextMarker support
#import <ApplicationServices/ApplicationServicesPriv.h>

#include <mach-o/dyld.h>
#if OMIT_TIGER_FEATURES
// no parameterized attributes in Panther... they were introduced in Tiger
#else
typedef AXTextMarkerRef (*TextMarkerFromTextMarkerRangeProc) (AXTextMarkerRangeRef theTextMarkerRange);
#endif

#import "KWQAccObject.h"
#import "KWQAccObjectCache.h"
#import "KWQAssertions.h"
#import "KWQFoundationExtras.h"
#import "KWQWidget.h"

#import "dom_docimpl.h"
#import "dom_elementimpl.h"
#import "html_inlineimpl.h"
#import "html_imageimpl.h"
#import "dom_string.h"
#import "dom2_range.h"
#import "htmlattrs.h"
#import "htmltags.h"
#import "khtmlview.h"
#import "khtml_part.h"
#import "render_canvas.h"
#import "render_object.h"
#import "render_image.h"
#import "render_list.h"
#import "render_style.h"
#import "render_text.h"
#import "selection.h"
#import "kjs_html.h"
#import "text_granularity.h"
#import "visible_position.h"
#import "visible_text.h"
#import "visible_units.h"
#import "html_miscimpl.h"
#import "qptrstack.h"

using DOM::DocumentImpl;
using DOM::ElementImpl;
using DOM::HTMLAnchorElementImpl;
using DOM::HTMLMapElementImpl;
using DOM::HTMLAreaElementImpl;
using DOM::HTMLCollection;
using DOM::HTMLCollectionImpl;
using DOM::Node;
using DOM::NodeImpl;
using DOM::Position;
using DOM::Range;
using DOM::DOMString;

using khtml::plainText;
using khtml::RenderObject;
using khtml::RenderWidget;
using khtml::RenderCanvas;
using khtml::RenderText;
using khtml::RenderBlock;
using khtml::RenderListMarker;
using khtml::RenderImage;
using khtml::Selection;
using khtml::VisiblePosition;

/* NSAccessibilityDescriptionAttribute is only defined on 10.4 and newer */
#if BUILDING_ON_PANTHER
#define ACCESSIBILITY_DESCRIPTION_ATTRIBUTE @"AXDescription"
#else
#define ACCESSIBILITY_DESCRIPTION_ATTRIBUTE NSAccessibilityDescriptionAttribute
#endif

// FIXME: This will eventually need to really localize.
#define UI_STRING(string, comment) ((NSString *)[NSString stringWithUTF8String:(string)])

@implementation KWQAccObject

-(id)initWithRenderer:(RenderObject*)renderer
{
    [super init];
    m_renderer = renderer;
    return self;
}

-(BOOL)detached
{
    return !m_renderer;
}

- (BOOL)accessibilityShouldUseUniqueId {
    return m_renderer && m_renderer->isCanvas();
}

extern "C" void NSAccessibilityUnregisterUniqueIdForUIElement(id element);
-(void)detach
{
    if ([self accessibilityShouldUseUniqueId])
        NSAccessibilityUnregisterUniqueIdForUIElement(self);
    [m_data release];
    m_data = 0;
    [self removeAccObjectID];
    m_renderer = 0;
    [self clearChildren];
}

-(id)data
{
    return m_data;
}

-(void)setData:(id)data
{
    if (!m_renderer)
        return;

    [data retain];
    [m_data release];
    m_data = data;
}

-(HTMLAnchorElementImpl*)anchorElement
{
    // return already-known anchor for image areas
    if (m_areaElement)
        return m_areaElement;

    // search up the render tree for a RenderObject with a DOM node.  Defer to an earlier continuation, though.
    RenderObject* currRenderer;
    for (currRenderer = m_renderer; currRenderer && !currRenderer->element(); currRenderer = currRenderer->parent()) {
        if (currRenderer->continuation())
            return [currRenderer->document()->getAccObjectCache()->accObject(currRenderer->continuation()) anchorElement];
    }
    
    // bail of none found
    if (!currRenderer)
        return 0;
    
    // search up the DOM tree for an anchor element
    // NOTE: this assumes that any non-image with an anchor is an HTMLAnchorElementImpl
    NodeImpl* elt = currRenderer->element();
    for ( ; elt; elt = elt->parentNode()) {
        if (elt->hasAnchor() && elt->renderer() && !elt->renderer()->isImage())
            return static_cast<HTMLAnchorElementImpl*>(elt);
    }
  
    return 0;
}

-(KWQAccObject*)firstChild
{
    if (!m_renderer || !m_renderer->firstChild())
        return nil;
    return m_renderer->document()->getAccObjectCache()->accObject(m_renderer->firstChild());
}

-(KWQAccObject*)lastChild
{
    if (!m_renderer || !m_renderer->lastChild())
        return nil;
    return m_renderer->document()->getAccObjectCache()->accObject(m_renderer->lastChild());
}

-(KWQAccObject*)previousSibling
{
    if (!m_renderer || !m_renderer->previousSibling())
        return nil;
    return m_renderer->document()->getAccObjectCache()->accObject(m_renderer->previousSibling());
}

-(KWQAccObject*)nextSibling
{
    if (!m_renderer || !m_renderer->nextSibling())
        return nil;
    return m_renderer->document()->getAccObjectCache()->accObject(m_renderer->nextSibling());
}

-(KWQAccObject*)parentObject
{
    if (m_areaElement)
        return m_renderer->document()->getAccObjectCache()->accObject(m_renderer);

    if (!m_renderer || !m_renderer->parent())
        return nil;
    return m_renderer->document()->getAccObjectCache()->accObject(m_renderer->parent());
}

-(KWQAccObject*)parentObjectUnignored
{
    KWQAccObject* obj = [self parentObject];
    if ([obj accessibilityIsIgnored])
        return [obj parentObjectUnignored];
    else
        return obj;
}

-(void)addChildrenToArray:(NSMutableArray*)array
{
    // nothing to add if there is no RenderObject
    if (!m_renderer)
        return;
    
    // try to add RenderWidget's children, but fall thru if there are none
    if (m_renderer->isWidget()) {
        RenderWidget* renderWidget = static_cast<RenderWidget*>(m_renderer);
        QWidget* widget = renderWidget->widget();
        if (widget) {
            NSArray* childArr = [(widget->getView()) accessibilityAttributeValue: NSAccessibilityChildrenAttribute];
            [array addObjectsFromArray: childArr];
            return;
        }
    }
    
    // add all unignored acc children
    for (KWQAccObject* obj = [self firstChild]; obj; obj = [obj nextSibling]) {
        if ([obj accessibilityIsIgnored])
            [obj addChildrenToArray: array];
        else
            [array addObject: obj];
    }
    
    // for a RenderImage, add the <area> elements as individual accessibility objects
    if (m_renderer->isImage() && !m_areaElement) {
        HTMLMapElementImpl* map = static_cast<RenderImage*>(m_renderer)->imageMap();
        if (map) {
            QPtrStack<NodeImpl> nodeStack;
            NodeImpl *current = map->firstChild();
            while (1) {
                // make sure we have a node to process
                if (!current) {
                    // done if there is no node and no remembered node
                    if(nodeStack.isEmpty())
                        break;
                    
                    // pop the previously processed parent
                    current = nodeStack.pop();
                    
                    // move on
                    current = current->nextSibling();
                    continue;
                }
                
                // add an <area> element for this child if it has a link
                // NOTE: can't cache these because they all have the same renderer, which is the cache key, right?
                // plus there may be little reason to since they are being added to the handy array
                if (current->hasAnchor()) {
                    KWQAccObject* obj = [[[KWQAccObject alloc] initWithRenderer: m_renderer] autorelease];
                    obj->m_areaElement = static_cast<HTMLAreaElementImpl*>(current);
                    [array addObject: obj];
                }
                
                // move on to children (if any) or next sibling
                NodeImpl *child = current->firstChild();
                if (child) {
                    // switch to doing all the children, remember the parent to pop later
                    nodeStack.push(current);
                    current = child;
                }
                else
                    current = current->nextSibling();
            }
        }
    }
}

-(NSString*)role
{
    if (!m_renderer)
        return NSAccessibilityUnknownRole;

    if (m_areaElement)
        return @"AXLink";
    if (m_renderer->element() && m_renderer->element()->hasAnchor()) {
        if (m_renderer->isImage())
            return @"AXImageMap";
        return @"AXLink";
    }
    if (m_renderer->isListMarker())
        return @"AXListMarker";
    if (m_renderer->element() && m_renderer->element()->isHTMLElement() &&
        Node(m_renderer->element()).elementId() == ID_BUTTON)
        return NSAccessibilityButtonRole;
    if (m_renderer->isText())
        return NSAccessibilityStaticTextRole;
    if (m_renderer->isImage())
        return NSAccessibilityImageRole;
    if (m_renderer->isCanvas())
        return @"AXWebArea";
    if (m_renderer->isBlockFlow())
        return NSAccessibilityGroupRole;

    return NSAccessibilityUnknownRole;
}

-(NSString*)roleDescription
{
#if OMIT_TIGER_FEATURES
    // We don't need role descriptions on Panther and we don't have the call
    // to get at localized ones anyway. At some point we may want to conditionally
    // compile this entire file instead, but this is OK too.
    return nil;
#else
    if (!m_renderer)
        return nil;
    
    // FIXME 3517227: These need to be localized (UI_STRING here is a dummy macro)
    // FIXME 3447564: It would be better to call some AppKit API to get these strings
    // (which would be the best way to localize them)
    
    NSString *role = [self role];
    if ([role isEqualToString:NSAccessibilityButtonRole])
        return NSAccessibilityRoleDescription(NSAccessibilityButtonRole, nil);
    
    if ([role isEqualToString:NSAccessibilityStaticTextRole])
        return NSAccessibilityRoleDescription(NSAccessibilityStaticTextRole, nil);

    if ([role isEqualToString:NSAccessibilityImageRole])
        return NSAccessibilityRoleDescription(NSAccessibilityImageRole, nil);
    
    if ([role isEqualToString:NSAccessibilityGroupRole])
        return NSAccessibilityRoleDescription(NSAccessibilityGroupRole, nil);
    
    if ([role isEqualToString:@"AXWebArea"])
        return UI_STRING("web area", "accessibility role description for web area");
    
    if ([role isEqualToString:@"AXLink"])
        return UI_STRING("link", "accessibility role description for link");
    
    if ([role isEqualToString:@"AXListMarker"])
        return UI_STRING("list marker", "accessibility role description for list marker");
    
    if ([role isEqualToString:@"AXImageMap"])
        return UI_STRING("image map", "accessibility role description for image map");
    
    return NSAccessibilityRoleDescription(NSAccessibilityUnknownRole, nil);
#endif
}

-(NSString*)helpText
{
    if (!m_renderer)
        return nil;

    if (m_areaElement) {
        QString summary = static_cast<ElementImpl*>(m_areaElement)->getAttribute(ATTR_SUMMARY).string();
        if (!summary.isEmpty())
            return summary.getNSString();
        QString title = static_cast<ElementImpl*>(m_areaElement)->getAttribute(ATTR_TITLE).string();
        if (!title.isEmpty())
            return title.getNSString();
    }

    for (RenderObject* curr = m_renderer; curr; curr = curr->parent()) {
        if (curr->element() && curr->element()->isHTMLElement()) {
            QString summary = static_cast<ElementImpl*>(curr->element())->getAttribute(ATTR_SUMMARY).string();
            if (!summary.isEmpty())
                return summary.getNSString();
            QString title = static_cast<ElementImpl*>(curr->element())->getAttribute(ATTR_TITLE).string();
            if (!title.isEmpty())
                return title.getNSString();
        }
    }

    return nil;
}

-(NSString*)textUnderElement
{
    if (!m_renderer)
        return nil;

    NodeImpl* e = m_renderer->element();
    DocumentImpl* d = m_renderer->document();
    if (e && d) {
	KHTMLPart* p = d->part();
	if (p) {
	    Range r(p->document());
	    if (m_renderer->isText()) {
		r.setStartBefore(e);
		r.setEndAfter(e);
		return p->text(r).getNSString();
	    }
	    if (e->firstChild()) {
		r.setStartBefore(e->firstChild());
		r.setEndAfter(e->lastChild());
		return p->text(r).getNSString();
	    }
        }
    }

    return nil;
}

-(NSString*)value
{
    if (!m_renderer || m_areaElement)
        return nil;

    if (m_renderer->isText())
        return [self textUnderElement];
    
    if (m_renderer->isListMarker())
        return static_cast<RenderListMarker*>(m_renderer)->text().getNSString();

    if (m_renderer->isCanvas()) {
        KWQKHTMLPart *docPart = KWQ(m_renderer->document()->part());
        if (!docPart)
            return nil;
        
        Position startPos = VisiblePosition(m_renderer->positionForCoordinates (0, 0, nil)).deepEquivalent();
        Position endPos   = VisiblePosition(m_renderer->positionForCoordinates (LONG_MAX, LONG_MAX, nil)).deepEquivalent();
        NSAttributedString * attrString = docPart->attributedString(startPos.node(), startPos.offset(), endPos.node(), endPos.offset());
        return [attrString string];
    }
        
    // FIXME: We might need to implement a value here for more types
    // FIXME: It would be better not to advertise a value at all for the types for which we don't implement one;
    // this would require subclassing or making accessibilityAttributeNames do something other than return a
    // single static array.
    return nil;
}

-(NSString*)title
{
    if (!m_renderer || m_areaElement)
        return nil;
    
    if (m_renderer->element() && m_renderer->element()->isHTMLElement() &&
             Node(m_renderer->element()).elementId() == ID_BUTTON)
        return [self textUnderElement];
    else if (m_renderer->element() && m_renderer->element()->hasAnchor())
        return [self textUnderElement];
    
    return nil;
}

-(NSString*)accessibilityDescription
{
    if (!m_renderer || m_areaElement)
        return nil;
    
    if (m_renderer->isImage()) {
        if (m_renderer->element() && m_renderer->element()->isHTMLElement()) {
            QString alt = static_cast<ElementImpl*>(m_renderer->element())->getAttribute(ATTR_ALT).string();
            return !alt.isEmpty() ? alt.getNSString() : nil;
        }
    }
    
    return nil;
}

static QRect boundingBoxRect(RenderObject* obj)
{
    QRect rect(0,0,0,0);
    if (obj) {
        if (obj->isInlineContinuation())
            obj = obj->element()->renderer();
        QValueList<QRect> rects;
        int x = 0, y = 0;
        obj->absolutePosition(x, y);
        obj->absoluteRects(rects, x, y);
        for (QValueList<QRect>::ConstIterator it = rects.begin(); it != rects.end(); ++it) {
            QRect r = *it;
            if (r.isValid()) {
                if (rect.isEmpty())
                    rect = r;
                else
                    rect.unite(r);
            }
        }
    }
    return rect;
}

-(NSValue*)position
{
    QRect rect = m_areaElement ? m_areaElement->getRect(m_renderer) : boundingBoxRect(m_renderer);
    
    // The Cocoa accessibility API wants the lower-left corner, not the upper-left, so we add in our height.
    NSPoint point = NSMakePoint(rect.x(), rect.y() + rect.height());
    if (m_renderer && m_renderer->canvas() && m_renderer->canvas()->view()) {
        NSView* view = m_renderer->canvas()->view()->getDocumentView();
        point = [[view window] convertBaseToScreen: [view convertPoint: point toView:nil]];
    }
    return [NSValue valueWithPoint: point];
}

-(NSValue*)size
{
    QRect rect = m_areaElement ? m_areaElement->getRect(m_renderer) : boundingBoxRect(m_renderer);
    return [NSValue valueWithSize: NSMakeSize(rect.width(), rect.height())];
}

-(BOOL)accessibilityIsIgnored
{
    if (!m_renderer || m_renderer->style()->visibility() != khtml::VISIBLE)
        return YES;

    if (m_renderer->isText())
        return m_renderer->isBR() || !static_cast<RenderText*>(m_renderer)->firstTextBox();
    
    if (m_areaElement || (m_renderer->element() && m_renderer->element()->hasAnchor()))
        return NO;

    if (m_renderer->isBlockFlow() && m_renderer->childrenInline())
        return !static_cast<RenderBlock*>(m_renderer)->firstLineBox();

    return (!m_renderer->isListMarker() && !m_renderer->isCanvas() && 
            !m_renderer->isImage() &&
            !(m_renderer->element() && m_renderer->element()->isHTMLElement() &&
              Node(m_renderer->element()).elementId() == ID_BUTTON));
}

- (NSArray *)accessibilityAttributeNames
{
    static NSArray* attributes = nil;
    static NSArray* anchorAttrs = nil;
    static NSArray* webAreaAttrs = nil;
    if (attributes == nil) {
        attributes = [[NSArray alloc] initWithObjects: NSAccessibilityRoleAttribute,
            NSAccessibilityRoleDescriptionAttribute,
            NSAccessibilityChildrenAttribute,
            NSAccessibilityHelpAttribute,
            NSAccessibilityParentAttribute,
            NSAccessibilityPositionAttribute,
            NSAccessibilitySizeAttribute,
            NSAccessibilityTitleAttribute,
            ACCESSIBILITY_DESCRIPTION_ATTRIBUTE,
            NSAccessibilityValueAttribute,
            NSAccessibilityFocusedAttribute,
            NSAccessibilityEnabledAttribute,
            NSAccessibilityWindowAttribute,
#if OMIT_TIGER_FEATURES
// no parameterized attributes in Panther... they were introduced in Tiger
#else
            (NSString *) kAXSelectedTextMarkerRangeAttribute,
//          (NSString *) kAXVisibleCharacterTextMarkerRangeAttribute,    // NOTE: BUG FO 4
            (NSString *) kAXStartTextMarkerAttribute,
            (NSString *) kAXEndTextMarkerAttribute,
#endif
            nil];
    }
    if (anchorAttrs == nil) {
        anchorAttrs = [[NSArray alloc] initWithObjects: NSAccessibilityRoleAttribute,
            NSAccessibilityRoleDescriptionAttribute,
            NSAccessibilityChildrenAttribute,
            NSAccessibilityHelpAttribute,
            NSAccessibilityParentAttribute,
            NSAccessibilityPositionAttribute,
            NSAccessibilitySizeAttribute,
            NSAccessibilityTitleAttribute,
            NSAccessibilityValueAttribute,
            NSAccessibilityFocusedAttribute,
            NSAccessibilityEnabledAttribute,
            NSAccessibilityWindowAttribute,
            @"AXURL",
#if OMIT_TIGER_FEATURES
// no parameterized attributes in Panther... they were introduced in Tiger
#else
            (NSString *) kAXSelectedTextMarkerRangeAttribute,
//          (NSString *) kAXVisibleCharacterTextMarkerRangeAttribute,     // NOTE: BUG FO 4
            (NSString *) kAXStartTextMarkerAttribute,
            (NSString *) kAXEndTextMarkerAttribute,
#endif
            nil];
    }
    if (webAreaAttrs == nil) {
        webAreaAttrs = [[NSArray alloc] initWithObjects: NSAccessibilityRoleAttribute,
            NSAccessibilityRoleDescriptionAttribute,
            NSAccessibilityChildrenAttribute,
            NSAccessibilityHelpAttribute,
            NSAccessibilityParentAttribute,
            NSAccessibilityPositionAttribute,
            NSAccessibilitySizeAttribute,
            NSAccessibilityTitleAttribute,
            NSAccessibilityValueAttribute,
            NSAccessibilityFocusedAttribute,
            NSAccessibilityEnabledAttribute,
            NSAccessibilityWindowAttribute,
            @"AXLinkUIElements",
            @"AXLoaded",
            @"AXLayoutCount",
#if OMIT_TIGER_FEATURES
// no parameterized attributes in Panther... they were introduced in Tiger
#else
            (NSString *) kAXSelectedTextMarkerRangeAttribute,
//          (NSString *) kAXVisibleCharacterTextMarkerRangeAttribute,     // NOTE: BUG FO 4
            (NSString *) kAXStartTextMarkerAttribute,
            (NSString *) kAXEndTextMarkerAttribute,
#endif
            nil];
    }
    
    if (m_renderer && m_renderer->isCanvas())
        return webAreaAttrs;
    if (m_areaElement || (m_renderer && !m_renderer->isImage() && m_renderer->element() && m_renderer->element()->hasAnchor()))
        return anchorAttrs;
    return attributes;
}

- (NSArray*)accessibilityActionNames
{
    static NSArray* actions = nil;
    if ([self anchorElement]) {
        if (actions == nil)
            actions = [[NSArray alloc] initWithObjects: NSAccessibilityPressAction, nil];
        return actions;
    }
    return nil;
}

- (NSString *)accessibilityActionDescription:(NSString *)action
{
#if OMIT_TIGER_FEATURES
    // We don't need action descriptions on Panther and we don't have the call
    // to get at localized ones anyway. At some point we may want to conditionally
    // compile this entire file instead, but this is OK too.
    return nil;
#else
    // we have no custom actions
    return NSAccessibilityActionDescription(action);
#endif
}

- (void)accessibilityPerformAction:(NSString *)action
{
    // We only have the one action (press).
    if ([action isEqualToString:NSAccessibilityPressAction]) {
        // Locate the anchor element. If it doesn't exist, just bail.
        HTMLAnchorElementImpl* anchorElt = [self anchorElement];
        if (!anchorElt)
            return;

        DocumentImpl* d = anchorElt->getDocument();
        if (d) {
            KHTMLPart* p = d->part();
            if (p) {
                KWQ(p)->prepareForUserAction();
            }
        }

        anchorElt->click();
    }
}

#if OMIT_TIGER_FEATURES
// no parameterized attributes in Panther... they were introduced in Tiger
#else
- (AXTextMarkerRangeRef) textMarkerRangeFromMarkers: (AXTextMarkerRef) textMarker1 andEndMarker:(AXTextMarkerRef) textMarker2
{
    AXTextMarkerRangeRef textMarkerRange;
    
    // create the range
    textMarkerRange = AXTextMarkerRangeCreate (nil, textMarker1, textMarker2);

    // autorelease it because we will never see it again
    KWQCFAutorelease(textMarkerRange);
    return textMarkerRange;
}

- (AXTextMarkerRef) textMarkerForVisiblePosition: (VisiblePosition)visiblePos
{
    if (visiblePos.isNull())
        return nil;
        
    return m_renderer->document()->getAccObjectCache()->textMarkerForVisiblePosition(visiblePos);
}

- (VisiblePosition) visiblePositionForTextMarker: (AXTextMarkerRef)textMarker
{
    return m_renderer->document()->getAccObjectCache()->visiblePositionForTextMarker(textMarker);
}

- (AXTextMarkerRef) AXTextMarkerRangeCopyStartMarkerWrapper: (AXTextMarkerRangeRef)textMarkerRange
{
    NSSymbol        axSymbol;
    static TextMarkerFromTextMarkerRangeProc   axStartImpl = nil;
    
    if (axStartImpl == nil) {
        axSymbol = NSLookupAndBindSymbol("_AXTextMarkerRangeCopyStartMarker");
        if (axSymbol == nil)
            axSymbol = NSLookupAndBindSymbol("_AXTextMarkerCopyStartMarker");
        ASSERT(axSymbol);
        axStartImpl = (TextMarkerFromTextMarkerRangeProc) NSAddressOfSymbol(axSymbol);
        ASSERT(axStartImpl);
    }
    
    return axStartImpl(textMarkerRange);
}

- (AXTextMarkerRef) AXTextMarkerRangeCopyEndMarkerWrapper: (AXTextMarkerRangeRef)textMarkerRange
{
    NSSymbol        axSymbol;
    static TextMarkerFromTextMarkerRangeProc   axEndImpl = nil;
    
    if (axEndImpl == nil) {
        axSymbol = NSLookupAndBindSymbol("_AXTextMarkerRangeCopyEndMarker");
        if (axSymbol == nil)
            axSymbol = NSLookupAndBindSymbol("_AXTextMarkerCopyEndMarker");
        ASSERT(axSymbol);
        axEndImpl = (TextMarkerFromTextMarkerRangeProc) NSAddressOfSymbol(axSymbol);
        ASSERT(axEndImpl);
    }
    
    return axEndImpl(textMarkerRange);
}

- (VisiblePosition) visiblePositionForStartOfTextMarkerRange: (AXTextMarkerRangeRef)textMarkerRange
{
    AXTextMarkerRef textMarker;
    VisiblePosition visiblePos;

    textMarker = [self AXTextMarkerRangeCopyStartMarkerWrapper:textMarkerRange];
    visiblePos = [self visiblePositionForTextMarker:textMarker];
    if (textMarker)
        CFRelease(textMarker);
    return visiblePos;
}

- (VisiblePosition) visiblePositionForEndOfTextMarkerRange: (AXTextMarkerRangeRef) textMarkerRange
{
    AXTextMarkerRef textMarker;
    VisiblePosition visiblePos;
    
    textMarker = [self AXTextMarkerRangeCopyEndMarkerWrapper:textMarkerRange];
    visiblePos = [self visiblePositionForTextMarker:textMarker];
    if (textMarker)
        CFRelease(textMarker);
    return visiblePos;
}
#endif

- (AXTextMarkerRangeRef) textMarkerRangeFromVisiblePositions: (VisiblePosition) startPosition andEndPos: (VisiblePosition) endPosition
{
    AXTextMarkerRef startTextMarker = [self textMarkerForVisiblePosition: startPosition];
    AXTextMarkerRef endTextMarker   = [self textMarkerForVisiblePosition: endPosition];
    return [self textMarkerRangeFromMarkers: startTextMarker andEndMarker:endTextMarker];
}

- (DocumentImpl *)topDocument
{
    return m_renderer->document()->topDocument();
}

- (RenderObject *)topRenderer
{
    return m_renderer->document()->topDocument()->renderer();
}

- (KHTMLView *)topView
{
    return m_renderer->document()->topDocument()->renderer()->canvas()->view();
}

- (id)accessibilityAttributeValue:(NSString *)attributeName
{
    if (!m_renderer)
        return nil;

    if ([attributeName isEqualToString: NSAccessibilityRoleAttribute])
        return [self role];

    if ([attributeName isEqualToString: NSAccessibilityRoleDescriptionAttribute])
        return [self roleDescription];
    
    if ([attributeName isEqualToString: NSAccessibilityParentAttribute]) {
        if (m_renderer->isCanvas())
            return m_renderer->canvas()->view()->getView();
        return [self parentObjectUnignored];
    }

    if ([attributeName isEqualToString: NSAccessibilityChildrenAttribute]) {
        if (!m_children) {
            m_children = [NSMutableArray arrayWithCapacity: 8];
            [m_children retain];
            [self addChildrenToArray: m_children];
        }
        return m_children;
    }

    if (m_renderer->isCanvas()) {
        if ([attributeName isEqualToString: @"AXLinkUIElements"]) {
            NSMutableArray* links = [NSMutableArray arrayWithCapacity: 32];
            HTMLCollection coll(m_renderer->document(), HTMLCollectionImpl::DOC_LINKS);
            if (coll.isNull())
                return links;
            Node curr = coll.firstItem();
            while (!curr.isNull()) {
                RenderObject* obj = curr.handle()->renderer();
                if (obj)
                    [links addObject: obj->document()->getAccObjectCache()->accObject(obj)];
                curr = coll.nextItem();
            }
            return links;
        }
        else if ([attributeName isEqualToString: @"AXLoaded"])
            return [NSNumber numberWithBool: (!m_renderer->document()->tokenizer())];
        else if ([attributeName isEqualToString: @"AXLayoutCount"])
            return [NSNumber numberWithInt: (static_cast<RenderCanvas*>(m_renderer)->view()->layoutCount())];
    }
    
    if ([attributeName isEqualToString: @"AXURL"] && 
        (m_areaElement || (!m_renderer->isImage() && m_renderer->element() && m_renderer->element()->hasAnchor()))) {
        HTMLAnchorElementImpl* anchor = [self anchorElement];
        if (anchor) {
            QString s = anchor->getAttribute(ATTR_HREF).string();
            if (!s.isNull()) {
                s = anchor->getDocument()->completeURL(s);
                return s.getNSString();
            }
        }
    }
    
    if ([attributeName isEqualToString: NSAccessibilityTitleAttribute])
        return [self title];
    
    if ([attributeName isEqualToString: ACCESSIBILITY_DESCRIPTION_ATTRIBUTE])
        return [self accessibilityDescription];
    
    if ([attributeName isEqualToString: NSAccessibilityValueAttribute])
        return [self value];

    if ([attributeName isEqualToString: NSAccessibilityHelpAttribute])
        return [self helpText];
    
    if ([attributeName isEqualToString: NSAccessibilityFocusedAttribute])
        return [NSNumber numberWithBool: (m_renderer->element() && m_renderer->document()->focusNode() == m_renderer->element())];

    if ([attributeName isEqualToString: NSAccessibilityEnabledAttribute])
        return [NSNumber numberWithBool: YES];
    
    if ([attributeName isEqualToString: NSAccessibilitySizeAttribute])
        return [self size];

    if ([attributeName isEqualToString: NSAccessibilityPositionAttribute])
        return [self position];

    if ([attributeName isEqualToString: NSAccessibilityWindowAttribute])
        return [m_renderer->canvas()->view()->getView() window];

#if OMIT_TIGER_FEATURES
// no parameterized attributes in Panther... they were introduced in Tiger
#else
    if ([attributeName isEqualToString: (NSString *) kAXSelectedTextMarkerRangeAttribute]) {
        // get the selection from the document part
        // NOTE: BUG support nested WebAreas, like in <http://webcourses.niu.edu/>
        // (there is a web archive of this page attached to <rdar://problem/3888973>)
        Selection   sel = [self topView]->part()->selection();
        if (sel.isNone()) {
            sel = m_renderer->document()->renderer()->canvas()->view()->part()->selection();
            if (sel.isNone())
                return nil;
        }
            
        // return a marker range for the selection start to end
        VisiblePosition startPosition = VisiblePosition(sel.start());
        VisiblePosition endPosition = VisiblePosition(sel.end());
        return (id) [self textMarkerRangeFromVisiblePositions:startPosition andEndPos:endPosition];
    }
    
    if ([attributeName isEqualToString: (NSString *) kAXVisibleCharacterTextMarkerRangeAttribute]) {
        return nil;     // NOTE: BUG FO 4
    }
    
    if ([attributeName isEqualToString: (NSString *) kAXStartTextMarkerAttribute]) {
        return (id) [self textMarkerForVisiblePosition: VisiblePosition([self topRenderer]->positionForCoordinates (0, 0, nil))];
    }

    if ([attributeName isEqualToString: (NSString *) kAXEndTextMarkerAttribute]) {
        return (id) [self textMarkerForVisiblePosition: VisiblePosition([self topRenderer]->positionForCoordinates (LONG_MAX, LONG_MAX, nil))];
    }
#endif

    return nil;
}

#if OMIT_TIGER_FEATURES
// no parameterized attributes in Panther... they were introduced in Tiger
#else
- (NSArray *)accessibilityParameterizedAttributeNames
{
    static NSArray* paramAttributes = nil;
    if (paramAttributes == nil) {
        paramAttributes = [[NSArray alloc] initWithObjects: 
            (id) kAXLineForTextMarkerParameterizedAttribute,
            kAXTextMarkerRangeForLineParameterizedAttribute,
            kAXStringForTextMarkerRangeParameterizedAttribute,
            kAXTextMarkerForPositionParameterizedAttribute,
            kAXBoundsForTextMarkerRangeParameterizedAttribute,
//          kAXStyleTextMarkerRangeForTextMarkerParameterizedAttribute,                 // NOTE: BUG FO 3
//          kAXAttributedStringForTextMarkerRangeParameterizedAttribute,                // NOTE: BUG FO 2
            kAXTextMarkerRangeForUnorderedTextMarkersParameterizedAttribute,
            kAXNextTextMarkerForTextMarkerParameterizedAttribute,
            kAXPreviousTextMarkerForTextMarkerParameterizedAttribute,
            kAXLeftWordTextMarkerRangeForTextMarkerParameterizedAttribute,
            kAXRightWordTextMarkerRangeForTextMarkerParameterizedAttribute,
            kAXLeftLineTextMarkerRangeForTextMarkerParameterizedAttribute,
            kAXRightLineTextMarkerRangeForTextMarkerParameterizedAttribute,
            kAXSentenceTextMarkerRangeForTextMarkerParameterizedAttribute,
            kAXParagraphTextMarkerRangeForTextMarkerParameterizedAttribute,
            kAXNextWordEndTextMarkerForTextMarkerParameterizedAttribute,
            kAXPreviousWordStartTextMarkerForTextMarkerParameterizedAttribute,
            kAXNextLineEndTextMarkerForTextMarkerParameterizedAttribute,
            kAXPreviousLineStartTextMarkerForTextMarkerParameterizedAttribute,
            kAXNextSentenceEndTextMarkerForTextMarkerParameterizedAttribute,
            kAXPreviousSentenceStartTextMarkerForTextMarkerParameterizedAttribute,
            kAXNextParagraphEndTextMarkerForTextMarkerParameterizedAttribute,
            kAXPreviousParagraphStartTextMarkerForTextMarkerParameterizedAttribute,
            kAXLengthForTextMarkerRangeParameterizedAttribute,
            nil];
    }

    return paramAttributes;
}

- (id)doAXLineForTextMarker: (AXTextMarkerRef) textMarker
{
    unsigned int    lineCount = 0;
    VisiblePosition savedVisiblePos;
    VisiblePosition visiblePos = [self visiblePositionForTextMarker:textMarker];
    if (visiblePos.isNull())
        return nil;

    // move up until we get to the top
    // NOTE: BUG this is not correct in non-editable elements when the position is on the
    // first line, but not at the first offset, because previousLinePosition takes you to the
    // first offset, the same as if you had started on the second line.  In editable elements,
    // previousLinePosition returns nil, so the count is accurate.
    // NOTE: BUG This only takes us to the top of the rootEditableElement, not the top of the
    // top document.
    // NOTE: Can we use Selection::modify(EAlter alter, int verticalDistance)?
    while (visiblePos.isNotNull() && visiblePos != savedVisiblePos) {
        lineCount += 1;
        visiblePos.debugPosition("doAXLineForTextMarker");
        savedVisiblePos = visiblePos;
        visiblePos = previousLinePosition(visiblePos, khtml::DOWNSTREAM, 0);
    }
    
    return [NSNumber numberWithUnsignedInt:lineCount];
}

- (id)doAXTextMarkerRangeForLine: (NSNumber *) lineNumber
{
    unsigned lineCount = [lineNumber unsignedIntValue];
    if (lineCount == 0 || !m_renderer) return nil;
    
    // iterate over the lines
    // NOTE: BUG this is wrong when lineNumber is lineCount+1,  because nextLinePosition takes you to the
    // last offset of the last line
    VisiblePosition visiblePos = VisiblePosition([self topRenderer]->positionForCoordinates (0, 0, nil));
    VisiblePosition savedVisiblePos;
    while (--lineCount != 0) {
        savedVisiblePos = visiblePos;
        visiblePos = nextLinePosition(visiblePos, khtml::DOWNSTREAM, 0);
        if (visiblePos.isNull() || visiblePos == savedVisiblePos)
            return nil;
    }
    
    // make a caret selection for the marker position, then extend it to the line
    Selection sel = Selection(visiblePos, visiblePos);
    bool worked = sel.modify(Selection::EXTEND, Selection::RIGHT, khtml::LINE_BOUNDARY);
    if (!worked)
        return nil;

    // return a marker range for the selection start to end
    VisiblePosition startPosition = VisiblePosition(sel.start());
    VisiblePosition endPosition = VisiblePosition(sel.end());
    return (id) [self textMarkerRangeFromVisiblePositions:startPosition andEndPos:endPosition];
}

- (id)doAXStringForTextMarkerRange: (AXTextMarkerRangeRef) textMarkerRange
{
    VisiblePosition startVisiblePosition, endVisiblePosition;
    Position startDeepPos, endDeepPos;
    QString qString;
    
    // extract the start and end VisiblePosition
    startVisiblePosition = [self visiblePositionForStartOfTextMarkerRange: textMarkerRange];
    if (startVisiblePosition.isNull())
        return nil;
    
    endVisiblePosition = [self visiblePositionForEndOfTextMarkerRange: textMarkerRange];
    if (endVisiblePosition.isNull())
        return nil;
    
    // use deepEquivalent because it is what the user sees
    startDeepPos = startVisiblePosition.deepEquivalent();
    endDeepPos = endVisiblePosition.deepEquivalent();
    
    // get the visible text in the range
    qString = plainText(Range(startDeepPos.node(), startDeepPos.offset(), endDeepPos.node(), endDeepPos.offset()));
    if (qString == nil) return nil;
    
    // transform it to a CFString and return that
    return (id)qString.getCFString();
}

- (id)doAXTextMarkerForPosition: (CGPoint) point
{
    NSPoint screenpoint = NSMakePoint(point.x, point.y);
    NSView * view = [self topView]->getView();
    NSPoint windowpoint = [[view window] convertScreenToBase: screenpoint];
    NSPoint ourpoint = [view convertPoint:windowpoint fromView:nil];

    VisiblePosition visiblePos = VisiblePosition([self topRenderer]->positionForCoordinates ((int)ourpoint.x, (int)ourpoint.y, nil));
    return (id) [self textMarkerForVisiblePosition:visiblePos];
}

- (id)doAXBoundsForTextMarkerRange: (AXTextMarkerRangeRef) textMarkerRange
{

    // extract the start and end VisiblePosition
    VisiblePosition startVisiblePosition = [self visiblePositionForStartOfTextMarkerRange: textMarkerRange];
    if (startVisiblePosition.isNull())
        return nil;
    
    VisiblePosition endVisiblePosition = [self visiblePositionForEndOfTextMarkerRange: textMarkerRange];
    if (endVisiblePosition.isNull())
        return nil;
    
    // use the Selection class to help calculate the corresponding rectangle
    // NOTE: If the selection spans lines, the rectangle is to extend across
    // the width of the view
    NSView * view = [self topView]->getView();
    QRect rect1 = Selection(startVisiblePosition, startVisiblePosition).caretRect();
    QRect rect2 = Selection(endVisiblePosition, endVisiblePosition).caretRect();
    QRect ourrect = rect1.unite(rect2);
    if (rect1.bottom() != rect2.bottom()) {
        ourrect.setX((int)[view frame].origin.x);
        ourrect.setWidth((int)[view frame].size.width);
    }
 
    // convert our rectangle to screen coordinates
    NSRect rect = NSMakeRect(ourrect.left(), ourrect.top(), ourrect.width(), ourrect.height());
    rect = [view convertRect:rect toView:nil];
    rect.origin = [[view window] convertBaseToScreen:rect.origin];
    
    // return the converted rect
    return [NSValue valueWithRect:rect];
}

- (id)doAXAttributedStringForTextMarkerRange: (AXTextMarkerRangeRef) textMarkerRange
{
    // NOTE: BUG FO 2 Needs to make AX attributed string 
    // Patti: Get the basic done first.  Add various text attribute support incrementally,
    // starting with attachment and hyperlink.  The rest of the attributes can be FO2/3.

    // extract the start and end VisiblePosition
    VisiblePosition startVisiblePosition = [self visiblePositionForStartOfTextMarkerRange: textMarkerRange];
    if (startVisiblePosition.isNull())
        return nil;
    
    VisiblePosition endVisiblePosition = [self visiblePositionForEndOfTextMarkerRange: textMarkerRange];
    if (endVisiblePosition.isNull())
        return nil;
    
    // get the attributed string by asking the document part
    KWQKHTMLPart *docPart = KWQ([self topDocument]->part());
    if (!docPart)
        return nil;
    
    Position    startPos = startVisiblePosition.deepEquivalent();
    Position    endPos   = endVisiblePosition.deepEquivalent();
    NSAttributedString * attrString = docPart->attributedString(startPos.node(), startPos.offset(), endPos.node(), endPos.offset());
    return attrString;
}

- (id)doAXTextMarkerRangeForUnorderedTextMarkers: (NSArray *) markers
{
#if MARKERARRAY_SELF_TEST
    AXTextMarkerRangeRef tmr = [self getSelectedTextMarkerRange];
    AXTextMarkerRef tm1 = [self AXTextMarkerRangeCopyEndMarkerWrapper:tmr];
    AXTextMarkerRef tm2 = [self AXTextMarkerRangeCopyStartMarkerWrapper:tmr];
    markers = [NSArray arrayWithObjects: (id) tm1, (id) tm2, nil];
#endif
    // get and validate the markers
    if ([markers count] < 2)
        return nil;
    
    AXTextMarkerRef textMarker1 = (AXTextMarkerRef) [markers objectAtIndex:0];
    AXTextMarkerRef textMarker2 = (AXTextMarkerRef) [markers objectAtIndex:1];
    if (CFGetTypeID(textMarker1) != AXTextMarkerGetTypeID() || CFGetTypeID(textMarker2) != AXTextMarkerGetTypeID())
        return nil;
    
    // convert to VisiblePosition
    VisiblePosition visiblePos1 = [self visiblePositionForTextMarker:textMarker1];
    VisiblePosition visiblePos2 = [self visiblePositionForTextMarker:textMarker2];
    if (visiblePos1.isNull() || visiblePos2.isNull())
        return nil;
    
    // use the Selection class to do the ordering
    // NOTE: Perhaps we could add a Selection method to indicate direction, based on m_baseIsStart
    AXTextMarkerRef startTextMarker;
    AXTextMarkerRef endTextMarker;
    Selection   sel(visiblePos1, visiblePos2);
    if (sel.base() == sel.start()) {
        startTextMarker = textMarker1;
        endTextMarker = textMarker2;
    } else {
        startTextMarker = textMarker2;
        endTextMarker = textMarker1;
    }
    
    // return a range based on the Selection verdict
    return (id) [self textMarkerRangeFromMarkers: startTextMarker andEndMarker:endTextMarker];
}

- (id)doAXNextTextMarkerForTextMarker: (AXTextMarkerRef) textMarker
{
    VisiblePosition visiblePos = [self visiblePositionForTextMarker:textMarker];
    VisiblePosition nextVisiblePos = visiblePos.next();
    if (nextVisiblePos.isNull())
        return nil;
    
    return (id) [self textMarkerForVisiblePosition:nextVisiblePos];
}

- (id)doAXPreviousTextMarkerForTextMarker: (AXTextMarkerRef) textMarker
{
    VisiblePosition visiblePos = [self visiblePositionForTextMarker:textMarker];
    VisiblePosition previousVisiblePos = visiblePos.previous();
    if (previousVisiblePos.isNull())
        return nil;
    
    return (id) [self textMarkerForVisiblePosition:previousVisiblePos];
}

- (id)doAXLeftWordTextMarkerRangeForTextMarker: (AXTextMarkerRef) textMarker
{
    VisiblePosition visiblePos = [self visiblePositionForTextMarker:textMarker];
    VisiblePosition startPosition = startOfWord(visiblePos, khtml::LeftWordIfOnBoundary);
    VisiblePosition endPosition = endOfWord(startPosition);

    return (id) [self textMarkerRangeFromVisiblePositions:startPosition andEndPos:endPosition];
}

- (id)doAXRightWordTextMarkerRangeForTextMarker: (AXTextMarkerRef) textMarker
{
    VisiblePosition visiblePos = [self visiblePositionForTextMarker:textMarker];
    VisiblePosition startPosition = startOfWord(visiblePos, khtml::RightWordIfOnBoundary);
    VisiblePosition endPosition = endOfWord(startPosition);

    return (id) [self textMarkerRangeFromVisiblePositions:startPosition andEndPos:endPosition];
}

- (id)doAXLeftLineTextMarkerRangeForTextMarker: (AXTextMarkerRef) textMarker
{
    // use Selection class instead of visible_units because startOfLine and endOfLine
    // are declared but not defined
    VisiblePosition visiblePos = [self visiblePositionForTextMarker:textMarker];
    if (visiblePos.isNull())
        return nil;
    
    // make a caret selection for the position before marker position (to make sure
    // we move off of a line start)
    VisiblePosition prevVisiblePos = visiblePos.previous();
    if (prevVisiblePos.isNull())
        return nil;
    
    // extend selection to the line
    // NOTE: ignores results of sel.modify because it returns false when
    // starting at an empty line.  The resulting selection in that case
    // will be a caret at prevVisiblePos. 
    Selection sel = Selection(prevVisiblePos, prevVisiblePos);
    (void)sel.modify(Selection::MOVE, Selection::LEFT, khtml::LINE_BOUNDARY);
    (void)sel.modify(Selection::EXTEND, Selection::RIGHT, khtml::LINE_BOUNDARY);

    // return a marker range for the selection start to end
    VisiblePosition startPosition = VisiblePosition(sel.start());
    VisiblePosition endPosition = VisiblePosition(sel.end());
    return (id) [self textMarkerRangeFromVisiblePositions:startPosition andEndPos:endPosition];
}

- (id)doAXRightLineTextMarkerRangeForTextMarker: (AXTextMarkerRef) textMarker
{
    // use Selection class instead of visible_units because startOfLine and endOfLine
    // are declared but not defined
    VisiblePosition visiblePos = [self visiblePositionForTextMarker:textMarker];
    if (visiblePos.isNull())
        return nil;
    
    // make a caret selection for the marker position, then extend it to the line
    VisiblePosition nextVisiblePos = visiblePos.next();
    if (nextVisiblePos.isNull())
        return nil;
        
    // NOTE: ignores results of sel.modify because it returns false when
    // starting at an empty line.  The resulting selection in that case
    // will be a caret at nextVisiblePos. 
    Selection sel = Selection(nextVisiblePos, nextVisiblePos);
    (void)sel.modify(Selection::MOVE, Selection::RIGHT, khtml::LINE_BOUNDARY);
    (void)sel.modify(Selection::EXTEND, Selection::LEFT, khtml::LINE_BOUNDARY);

    // return a marker range for the selection start to end
    VisiblePosition startPosition = VisiblePosition(sel.start());
    VisiblePosition endPosition = VisiblePosition(sel.end());
    return (id) [self textMarkerRangeFromVisiblePositions:startPosition andEndPos:endPosition];
}

- (id)doAXSentenceTextMarkerRangeForTextMarker: (AXTextMarkerRef) textMarker
{
    // NOTE: BUG FO 2 IMPLEMENT (currently returns incorrect answer)
    // Related? <rdar://problem/3927736> Text selection broken in 8A336
    VisiblePosition visiblePos = [self visiblePositionForTextMarker:textMarker];
    VisiblePosition startPosition = startOfSentence(visiblePos);
    VisiblePosition endPosition = endOfSentence(startPosition);

    return (id) [self textMarkerRangeFromVisiblePositions:startPosition andEndPos:endPosition];
}

- (id)doAXParagraphTextMarkerRangeForTextMarker: (AXTextMarkerRef) textMarker
{
    VisiblePosition visiblePos = [self visiblePositionForTextMarker:textMarker];
    VisiblePosition startPosition = startOfParagraph(visiblePos);
    VisiblePosition endPosition = endOfParagraph(startPosition);

    return (id) [self textMarkerRangeFromVisiblePositions:startPosition andEndPos:endPosition];
}

- (id)doAXNextWordEndTextMarkerForTextMarker: (AXTextMarkerRef) textMarker
{
    VisiblePosition visiblePos = [self visiblePositionForTextMarker:textMarker];
    if (visiblePos.isNull())
        return nil;

    VisiblePosition endPosition = endOfWord(visiblePos, khtml::RightWordIfOnBoundary);
    return (id) [self textMarkerForVisiblePosition: endPosition];
}

- (id)doAXPreviousWordStartTextMarkerForTextMarker: (AXTextMarkerRef) textMarker
{
    VisiblePosition visiblePos = [self visiblePositionForTextMarker:textMarker];
    if (visiblePos.isNull())
        return nil;

    VisiblePosition startPosition = startOfWord(visiblePos, khtml::LeftWordIfOnBoundary);
    return (id) [self textMarkerForVisiblePosition: startPosition];
}

- (id)doAXNextLineEndTextMarkerForTextMarker: (AXTextMarkerRef) textMarker
{
    // use Selection class instead of visible_units because startOfLine and endOfLine
    // are declared but not defined
    VisiblePosition visiblePos = [self visiblePositionForTextMarker:textMarker];
    if (visiblePos.isNull())
        return nil;
    
    // make a caret selection for the marker position, then move it to the line end
    VisiblePosition nextVisiblePos = visiblePos.next();
    if (nextVisiblePos.isNull())
        return nil;
        
    Selection sel = Selection(nextVisiblePos, nextVisiblePos);
    bool worked = sel.modify(Selection::MOVE, Selection::RIGHT, khtml::LINE_BOUNDARY);
    if (!worked)
        return nil;

    // return a marker for the selection end
    VisiblePosition endPosition = VisiblePosition(sel.end());
    return (id) [self textMarkerForVisiblePosition: endPosition];
}

- (id)doAXPreviousLineStartTextMarkerForTextMarker: (AXTextMarkerRef) textMarker
{
    // use Selection class instead of visible_units because startOfLine and endOfLine
    // are declared but not defined
    VisiblePosition visiblePos = [self visiblePositionForTextMarker:textMarker];
    if (visiblePos.isNull())
        return nil;
    
    // make a caret selection for the marker position, then move it to the line start
    VisiblePosition prevVisiblePos = visiblePos.previous();
    if (prevVisiblePos.isNull())
        return nil;
        
    Selection sel = Selection(prevVisiblePos, prevVisiblePos);
    bool worked = sel.modify(Selection::MOVE, Selection::LEFT, khtml::LINE_BOUNDARY);
    if (!worked)
        return nil;

    // return a marker for the selection start
    VisiblePosition startPosition = VisiblePosition(sel.start());
    return (id) [self textMarkerForVisiblePosition: startPosition];
}

- (id)doAXNextSentenceEndTextMarkerForTextMarker: (AXTextMarkerRef) textMarker
{
    // NOTE: BUG FO 2 IMPLEMENT (currently returns incorrect answer)
    // Related? <rdar://problem/3927736> Text selection broken in 8A336
    VisiblePosition visiblePos = [self visiblePositionForTextMarker:textMarker];
    if (visiblePos.isNull())
        return nil;

    VisiblePosition endPosition = endOfSentence(visiblePos);
    return (id) [self textMarkerForVisiblePosition: endPosition];
}

- (id)doAXPreviousSentenceStartTextMarkerForTextMarker: (AXTextMarkerRef) textMarker
{
    // NOTE: BUG FO 2 IMPLEMENT (currently returns incorrect answer)
    // Related? <rdar://problem/3927736> Text selection broken in 8A336
    VisiblePosition visiblePos = [self visiblePositionForTextMarker:textMarker];
    if (visiblePos.isNull())
        return nil;

    VisiblePosition startPosition = startOfSentence(visiblePos);
    return (id) [self textMarkerForVisiblePosition: startPosition];
}

- (id)doAXNextParagraphEndTextMarkerForTextMarker: (AXTextMarkerRef) textMarker
{
    VisiblePosition visiblePos = [self visiblePositionForTextMarker:textMarker];
    if (visiblePos.isNull())
        return nil;

    VisiblePosition endPosition = endOfParagraph(visiblePos);
    return (id) [self textMarkerForVisiblePosition: endPosition];
}

- (id)doAXPreviousParagraphStartTextMarkerForTextMarker: (AXTextMarkerRef) textMarker
{
    VisiblePosition visiblePos = [self visiblePositionForTextMarker:textMarker];
    if (visiblePos.isNull())
        return nil;

    VisiblePosition startPosition = startOfParagraph(visiblePos);
    return (id) [self textMarkerForVisiblePosition: startPosition];
}

- (id)doAXLengthForTextMarkerRange: (AXTextMarkerRangeRef) textMarkerRange
{
    // NOTE: BUG Multi-byte support
    CFStringRef string = (CFStringRef) [self doAXStringForTextMarkerRange: textMarkerRange];
    if (!string)
        return nil;

    return [NSNumber numberWithInt:CFStringGetLength(string)];
}

- (id)accessibilityAttributeValue:(NSString *)attribute forParameter:(id)parameter
{
    AXTextMarkerRef         textMarker = nil;
    AXTextMarkerRangeRef    textMarkerRange = nil;
    AXValueRef              value = nil;
    NSNumber *              number = nil;
    NSArray *               array = nil;
    CGPoint                 point;
    CGSize                  size;
    CGRect                  rect;
    CFRange                 range;
    AXError                 error;
    
    // basic parameter validation
    if (!m_renderer || !attribute || !parameter)
        return nil;

    // common parameter type check/casting.  Nil checks in handlers catch wrong type case.
    // NOTE: This assumes nil is not a valid parameter, because it is indistinguishable from
    // a parameter of the wrong type.
    if (CFGetTypeID(parameter) == AXTextMarkerGetTypeID())
        textMarker = (AXTextMarkerRef) parameter;

    else if (CFGetTypeID(parameter) == AXTextMarkerRangeGetTypeID())
        textMarkerRange = (AXTextMarkerRangeRef) parameter;

    else if ([parameter isKindOfClass:[NSNumber self]])
        number = parameter;

    else if ([parameter isKindOfClass:[NSArray self]])
        array = parameter;

    else if (CFGetTypeID(parameter) == AXValueGetTypeID()) {
        value = (AXValueRef) parameter;
        switch (AXValueGetType(value)) {
            case kAXValueCGPointType:
                AXValueGetValue(value, kAXValueCGPointType, &point);
                break;
            case kAXValueCGSizeType:
                AXValueGetValue(value, kAXValueCGSizeType, &size);
                break;
            case kAXValueCGRectType:
                AXValueGetValue(value, kAXValueCGRectType, &rect);
                break;
            case kAXValueCFRangeType:
                AXValueGetValue(value, kAXValueCFRangeType, &range);
                break;
            case kAXValueAXErrorType:
                AXValueGetValue(value, kAXValueAXErrorType, &error);
                break;
            default:
                break;
        }
    }
  
    // dispatch
    if ([attribute isEqualToString: (NSString *) kAXLineForTextMarkerParameterizedAttribute])
        return [self doAXLineForTextMarker: textMarker];

    if ([attribute isEqualToString: (NSString *) kAXTextMarkerRangeForLineParameterizedAttribute])
        return [self doAXTextMarkerRangeForLine: number];

    if ([attribute isEqualToString: (NSString *) kAXStringForTextMarkerRangeParameterizedAttribute])
        return [self doAXStringForTextMarkerRange: textMarkerRange];

    if ([attribute isEqualToString: (NSString *) kAXTextMarkerForPositionParameterizedAttribute])
        return [self doAXTextMarkerForPosition: point];

    if ([attribute isEqualToString: (NSString *) kAXBoundsForTextMarkerRangeParameterizedAttribute])
        return [self doAXBoundsForTextMarkerRange: textMarkerRange];

    if ([attribute isEqualToString: (NSString *) kAXAttributedStringForTextMarkerRangeParameterizedAttribute])
        return [self doAXAttributedStringForTextMarkerRange: textMarkerRange];

    if ([attribute isEqualToString: (NSString *) kAXTextMarkerRangeForUnorderedTextMarkersParameterizedAttribute])
        return [self doAXTextMarkerRangeForUnorderedTextMarkers: array];

    if ([attribute isEqualToString: (NSString *) kAXNextTextMarkerForTextMarkerParameterizedAttribute])
        return [self doAXNextTextMarkerForTextMarker: textMarker];

    if ([attribute isEqualToString: (NSString *) kAXPreviousTextMarkerForTextMarkerParameterizedAttribute])
        return [self doAXPreviousTextMarkerForTextMarker: textMarker];

    if ([attribute isEqualToString: (NSString *) kAXLeftWordTextMarkerRangeForTextMarkerParameterizedAttribute])
        return [self doAXLeftWordTextMarkerRangeForTextMarker: textMarker];

    if ([attribute isEqualToString: (NSString *) kAXRightWordTextMarkerRangeForTextMarkerParameterizedAttribute])
        return [self doAXRightWordTextMarkerRangeForTextMarker: textMarker];

    if ([attribute isEqualToString: (NSString *) kAXLeftLineTextMarkerRangeForTextMarkerParameterizedAttribute])
        return [self doAXLeftLineTextMarkerRangeForTextMarker: textMarker];

    if ([attribute isEqualToString: (NSString *) kAXRightLineTextMarkerRangeForTextMarkerParameterizedAttribute])
        return [self doAXRightLineTextMarkerRangeForTextMarker: textMarker];

    if ([attribute isEqualToString: (NSString *) kAXSentenceTextMarkerRangeForTextMarkerParameterizedAttribute])
        return [self doAXSentenceTextMarkerRangeForTextMarker: textMarker];

    if ([attribute isEqualToString: (NSString *) kAXParagraphTextMarkerRangeForTextMarkerParameterizedAttribute])
        return [self doAXParagraphTextMarkerRangeForTextMarker: textMarker];

    if ([attribute isEqualToString: (NSString *) kAXNextWordEndTextMarkerForTextMarkerParameterizedAttribute])
        return [self doAXNextWordEndTextMarkerForTextMarker: textMarker];

    if ([attribute isEqualToString: (NSString *) kAXPreviousWordStartTextMarkerForTextMarkerParameterizedAttribute])
        return [self doAXPreviousWordStartTextMarkerForTextMarker: textMarker];
        
    if ([attribute isEqualToString: (NSString *) kAXNextLineEndTextMarkerForTextMarkerParameterizedAttribute])
        return [self doAXNextLineEndTextMarkerForTextMarker: textMarker];
        
    if ([attribute isEqualToString: (NSString *) kAXPreviousLineStartTextMarkerForTextMarkerParameterizedAttribute])
        return [self doAXPreviousLineStartTextMarkerForTextMarker: textMarker];
        
    if ([attribute isEqualToString: (NSString *) kAXNextSentenceEndTextMarkerForTextMarkerParameterizedAttribute])
        return [self doAXNextSentenceEndTextMarkerForTextMarker: textMarker];
        
    if ([attribute isEqualToString: (NSString *) kAXPreviousSentenceStartTextMarkerForTextMarkerParameterizedAttribute])
        return [self doAXPreviousSentenceStartTextMarkerForTextMarker: textMarker];
        
    if ([attribute isEqualToString: (NSString *) kAXNextParagraphEndTextMarkerForTextMarkerParameterizedAttribute])
        return [self doAXNextParagraphEndTextMarkerForTextMarker: textMarker];

    if ([attribute isEqualToString: (NSString *) kAXPreviousParagraphStartTextMarkerForTextMarkerParameterizedAttribute])
        return [self doAXPreviousParagraphStartTextMarkerForTextMarker: textMarker];
        
    if ([attribute isEqualToString: (NSString *) kAXLengthForTextMarkerRangeParameterizedAttribute])
        return [self doAXLengthForTextMarkerRange: textMarkerRange];

    return nil;
}

#endif

- (id)accessibilityHitTest:(NSPoint)point
{
    if (!m_renderer)
        return self;
    
    RenderObject::NodeInfo nodeInfo(true, true);
    m_renderer->layer()->hitTest(nodeInfo, (int)point.x, (int)point.y);
    if (!nodeInfo.innerNode())
        return self;
    RenderObject* obj = nodeInfo.innerNode()->renderer();
    if (!obj)
        return self;
    return obj->document()->getAccObjectCache()->accObject(obj);
}

- (id)accessibilityFocusedUIElement
{
    // NOTE: BUG support nested WebAreas
    NodeImpl *focusNode = m_renderer->document()->focusNode();
    if (!focusNode || !focusNode->renderer())
        return nil;

    return focusNode->renderer()->document()->getAccObjectCache()->accObject(focusNode->renderer());
}

- (BOOL)accessibilityIsAttributeSettable:(NSString*)attributeName
{
    if ([attributeName isEqualToString: (NSString *) kAXSelectedTextMarkerRangeAttribute])
        return YES;
        
    return NO;
}

- (void)doSetAXSelectedTextMarkerRange: (AXTextMarkerRangeRef)textMarkerRange
{
    VisiblePosition startVisiblePosition, endVisiblePosition;
    
    // extract the start and end VisiblePosition
    startVisiblePosition = [self visiblePositionForStartOfTextMarkerRange: textMarkerRange];
    if (startVisiblePosition.isNull())
        return;
    
    endVisiblePosition = [self visiblePositionForEndOfTextMarkerRange: textMarkerRange];
    if (endVisiblePosition.isNull())
        return;
    
    // make selection and tell the document to use it
    // NOTE: BUG support nested WebAreas
    Selection sel = Selection(startVisiblePosition, endVisiblePosition);
    [self topDocument]->part()->setSelection(sel);
}

- (void)accessibilitySetValue:(id)value forAttribute:(NSString *)attributeName;
{
    AXTextMarkerRangeRef    textMarkerRange = nil;

    if (CFGetTypeID(value) == AXTextMarkerRangeGetTypeID())
        textMarkerRange = (AXTextMarkerRangeRef) value;
        
    if ([attributeName isEqualToString: (NSString *) kAXSelectedTextMarkerRangeAttribute]) {
        [self doSetAXSelectedTextMarkerRange:textMarkerRange];
    }
}

- (void)childrenChanged
{
    [self clearChildren];
    
    if ([self accessibilityIsIgnored])
        [[self parentObject] childrenChanged];
}

- (void)clearChildren
{
    [m_children release];
    m_children = nil;
}

-(KWQAccObjectID)accObjectID
{
    return m_accObjectID;
}

-(void)setAccObjectID:(KWQAccObjectID) accObjectID
{
    m_accObjectID = accObjectID;
}

- (void)removeAccObjectID
{
    m_renderer->document()->getAccObjectCache()->removeAccObjectID(self);
}

@end
