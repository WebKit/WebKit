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

// need this until accesstool supports markers and marker ranges
#define MARKER_SELF_TEST 0

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
#import "kjs_html.h"
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

-(void)detach
{
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
            return [currRenderer->document()->getOrCreateAccObjectCache()->accObject(currRenderer->continuation()) anchorElement];
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
    return m_renderer->document()->getOrCreateAccObjectCache()->accObject(m_renderer->firstChild());
}

-(KWQAccObject*)lastChild
{
    if (!m_renderer || !m_renderer->lastChild())
        return nil;
    return m_renderer->document()->getOrCreateAccObjectCache()->accObject(m_renderer->lastChild());
}

-(KWQAccObject*)previousSibling
{
    if (!m_renderer || !m_renderer->previousSibling())
        return nil;
    return m_renderer->document()->getOrCreateAccObjectCache()->accObject(m_renderer->previousSibling());
}

-(KWQAccObject*)nextSibling
{
    if (!m_renderer || !m_renderer->nextSibling())
        return nil;
    return m_renderer->document()->getOrCreateAccObjectCache()->accObject(m_renderer->nextSibling());
}

-(KWQAccObject*)parentObject
{
    if (m_areaElement)
        return m_renderer->document()->getOrCreateAccObjectCache()->accObject(m_renderer);

    if (!m_renderer || !m_renderer->parent())
        return nil;
    return m_renderer->document()->getOrCreateAccObjectCache()->accObject(m_renderer->parent());
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
//          (NSString *) kAXVisibleCharacterTextMarkerRangeAttribute,     FO 4
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
//          (NSString *) kAXVisibleCharacterTextMarkerRangeAttribute,     FO 4
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
//          (NSString *) kAXVisibleCharacterTextMarkerRangeAttribute,     FO 4
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

- (BOOL)accessibilityIsAttributeSettable:(NSString*)attributeName
{
    return NO;
}

#if 0
// what we'd need to implement if accessibilityIsAttributeSettable ever returned YES
- (void)accessibilitySetValue:(id)value forAttribute:(NSString *)attribute;
{
}
#endif

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
        
    KWQAccObjectCache* cache = m_renderer->document()->getExistingAccObjectCache();
    return cache->textMarkerForVisiblePosition(visiblePos);
}

- (VisiblePosition) visiblePositionForTextMarker: (AXTextMarkerRef)textMarker
{
    KWQAccObjectCache* cache = m_renderer->document()->getExistingAccObjectCache();
    return cache->visiblePositionForTextMarker(textMarker);
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
                    [links addObject: obj->document()->getOrCreateAccObjectCache()->accObject(obj)];
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
        int startPos, endPos;

        RenderObject*   startRenderer = m_renderer->canvas()->selectionStart();
        RenderObject*   endRenderer = m_renderer->canvas()->selectionEnd();
        if (!startRenderer || !endRenderer)
            return nil;
        m_renderer->selectionStartEnd(startPos, endPos);
            
        AXTextMarkerRef startTextMarker = [self textMarkerForVisiblePosition: VisiblePosition(Position(startRenderer->node(), (long) startPos))];
        AXTextMarkerRef endTextMarker   = [self textMarkerForVisiblePosition: VisiblePosition(Position(endRenderer->node(), (long) endPos))];
        return (id) [self textMarkerRangeFromMarkers: startTextMarker andEndMarker:endTextMarker];
    }
    
    if ([attributeName isEqualToString: (NSString *) kAXVisibleCharacterTextMarkerRangeAttribute]) {
        return nil;     // IMPLEMENT FO4
    }
    
    if ([attributeName isEqualToString: (NSString *) kAXStartTextMarkerAttribute]) {
        return (id) [self textMarkerForVisiblePosition: VisiblePosition(m_renderer->positionForCoordinates (0, 0, nil))];
    }

    if ([attributeName isEqualToString: (NSString *) kAXEndTextMarkerAttribute]) {
        return (id) [self textMarkerForVisiblePosition: VisiblePosition(m_renderer->positionForCoordinates (LONG_MAX, LONG_MAX, nil))];
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
    static NSArray* anchorParamAttrs = nil;
    static NSArray* webAreaParamAttrs = nil;
    if (paramAttributes == nil) {
        paramAttributes = [[NSArray alloc] initWithObjects: NSAccessibilityRoleAttribute,
            kAXLineForTextMarkerParameterizedAttribute,
//          kAXTextMarkerRangeForLineParameterizedAttribute,                // FO 1
            kAXStringForTextMarkerRangeParameterizedAttribute,
//          kAXTextMarkerForPositionParameterizedAttribute,
//          kAXBoundsForTextMarkerRangeParameterizedAttribute,
//          kAXStyleTextMarkerRangeForTextMarkerParameterizedAttribute,
//          kAXAttributedStringForTextMarkerRangeParameterizedAttribute,
//          kAXTextMarkerRangeForUnorderedTextMarkersParameterizedAttribute,
            kAXNextTextMarkerForTextMarkerParameterizedAttribute,
            kAXPreviousTextMarkerForTextMarkerParameterizedAttribute,
            kAXLeftWordTextMarkerRangeForTextMarkerParameterizedAttribute,
            kAXRightWordTextMarkerRangeForTextMarkerParameterizedAttribute,
//          kAXLeftLineTextMarkerRangeForTextMarkerParameterizedAttribute,                // FO 1
//          kAXRightLineTextMarkerRangeForTextMarkerParameterizedAttribute,                // FO 1
//          kAXSentenceTextMarkerRangeForTextMarkerParameterizedAttribute,
//          kAXParagraphTextMarkerRangeForTextMarkerParameterizedAttribute,
//          kAXNextWordEndTextMarkerForTextMarkerParameterizedAttribute,
//          kAXPreviousWordStartTextMarkerForTextMarkerParameterizedAttribute,
//          kAXNextLineEndTextMarkerForTextMarkerParameterizedAttribute,
//          kAXPreviousLineStartTextMarkerForTextMarkerParameterizedAttribute,
//          kAXNextSentenceEndTextMarkerForTextMarkerParameterizedAttribute,
//          kAXPreviousSentenceStartTextMarkerForTextMarkerParameterizedAttribute,
//          kAXNextParagraphEndTextMarkerForTextMarkerParameterizedAttribute,
//          kAXPreviousParagraphStartTextMarkerForTextMarkerParameterizedAttribute,
//          kAXLengthForTextMarkerRangeParameterizedAttribute,
            nil];
    }
    if (anchorParamAttrs == nil) {
        anchorParamAttrs = [[NSArray alloc] initWithObjects: NSAccessibilityRoleAttribute,
            kAXLineForTextMarkerParameterizedAttribute,
//          kAXTextMarkerRangeForLineParameterizedAttribute,                // FO 1
            kAXStringForTextMarkerRangeParameterizedAttribute,
//          kAXTextMarkerForPositionParameterizedAttribute,
//          kAXBoundsForTextMarkerRangeParameterizedAttribute,
//          kAXStyleTextMarkerRangeForTextMarkerParameterizedAttribute,
//          kAXAttributedStringForTextMarkerRangeParameterizedAttribute,
//          kAXTextMarkerRangeForUnorderedTextMarkersParameterizedAttribute,
            kAXNextTextMarkerForTextMarkerParameterizedAttribute,
            kAXPreviousTextMarkerForTextMarkerParameterizedAttribute,
            kAXLeftWordTextMarkerRangeForTextMarkerParameterizedAttribute,
            kAXRightWordTextMarkerRangeForTextMarkerParameterizedAttribute,
//          kAXLeftLineTextMarkerRangeForTextMarkerParameterizedAttribute,                // FO 1
//          kAXRightLineTextMarkerRangeForTextMarkerParameterizedAttribute,                // FO 1
//          kAXSentenceTextMarkerRangeForTextMarkerParameterizedAttribute,
//          kAXParagraphTextMarkerRangeForTextMarkerParameterizedAttribute,
//          kAXNextWordEndTextMarkerForTextMarkerParameterizedAttribute,
//          kAXPreviousWordStartTextMarkerForTextMarkerParameterizedAttribute,
//          kAXNextLineEndTextMarkerForTextMarkerParameterizedAttribute,
//          kAXPreviousLineStartTextMarkerForTextMarkerParameterizedAttribute,
//          kAXNextSentenceEndTextMarkerForTextMarkerParameterizedAttribute,
//          kAXPreviousSentenceStartTextMarkerForTextMarkerParameterizedAttribute,
//          kAXNextParagraphEndTextMarkerForTextMarkerParameterizedAttribute,
//          kAXPreviousParagraphStartTextMarkerForTextMarkerParameterizedAttribute,
//          kAXLengthForTextMarkerRangeParameterizedAttribute,
            nil];
    }
    if (webAreaParamAttrs == nil) {
        webAreaParamAttrs = [[NSArray alloc] initWithObjects: NSAccessibilityRoleAttribute,
            kAXLineForTextMarkerParameterizedAttribute,
//          kAXTextMarkerRangeForLineParameterizedAttribute,                // FO 1
            kAXStringForTextMarkerRangeParameterizedAttribute,
//          kAXTextMarkerForPositionParameterizedAttribute,
//          kAXBoundsForTextMarkerRangeParameterizedAttribute,
//          kAXStyleTextMarkerRangeForTextMarkerParameterizedAttribute,
//          kAXAttributedStringForTextMarkerRangeParameterizedAttribute,
//          kAXTextMarkerRangeForUnorderedTextMarkersParameterizedAttribute,
            kAXNextTextMarkerForTextMarkerParameterizedAttribute,
            kAXPreviousTextMarkerForTextMarkerParameterizedAttribute,
            kAXLeftWordTextMarkerRangeForTextMarkerParameterizedAttribute,
            kAXRightWordTextMarkerRangeForTextMarkerParameterizedAttribute,
//          kAXLeftLineTextMarkerRangeForTextMarkerParameterizedAttribute,                // FO 1
//          kAXRightLineTextMarkerRangeForTextMarkerParameterizedAttribute,                // FO 1
//          kAXSentenceTextMarkerRangeForTextMarkerParameterizedAttribute,
//          kAXParagraphTextMarkerRangeForTextMarkerParameterizedAttribute,
//          kAXNextWordEndTextMarkerForTextMarkerParameterizedAttribute,
//          kAXPreviousWordStartTextMarkerForTextMarkerParameterizedAttribute,
//          kAXNextLineEndTextMarkerForTextMarkerParameterizedAttribute,
//          kAXPreviousLineStartTextMarkerForTextMarkerParameterizedAttribute,
//          kAXNextSentenceEndTextMarkerForTextMarkerParameterizedAttribute,
//          kAXPreviousSentenceStartTextMarkerForTextMarkerParameterizedAttribute,
//          kAXNextParagraphEndTextMarkerForTextMarkerParameterizedAttribute,
//          kAXPreviousParagraphStartTextMarkerForTextMarkerParameterizedAttribute,
//          kAXLengthForTextMarkerRangeParameterizedAttribute,
            nil];
    }
    
    if (m_renderer && m_renderer->isCanvas())
        return webAreaParamAttrs;
    if (m_areaElement || (m_renderer && !m_renderer->isImage() && m_renderer->element() && m_renderer->element()->hasAnchor()))
        return anchorParamAttrs;
    return paramAttributes;
}

- (AXTextMarkerRangeRef) textMarkerRangeFromVisiblePositions: (VisiblePosition) startPos andEndPos: (VisiblePosition) endPos
{
    AXTextMarkerRef startTextMarker = [self textMarkerForVisiblePosition: startPos];
    AXTextMarkerRef endTextMarker   = [self textMarkerForVisiblePosition: endPos];
    return [self textMarkerRangeFromMarkers: startTextMarker andEndMarker:endTextMarker];
}

- (AXTextMarkerRangeRef) getSelectedTextMarkerRange
{
        int startOffset, endOffset;

        RenderObject*   startRenderer = m_renderer->canvas()->selectionStart();
        RenderObject*   endRenderer = m_renderer->canvas()->selectionEnd();
        if (!startRenderer || !endRenderer)
            return nil;
        m_renderer->selectionStartEnd(startOffset, endOffset);
            
        VisiblePosition startPosition = VisiblePosition(Position(startRenderer->node(), (long) startOffset));
        VisiblePosition endPosition   = VisiblePosition(Position(endRenderer->node(), (long) endOffset));
        return [self textMarkerRangeFromVisiblePositions:startPosition andEndPos:endPosition];
}

- (id)doAXLineForTextMarker: (AXTextMarkerRef) textMarker
{
#if MARKER_SELF_TEST
    AXTextMarkerRangeRef textMarkerRange = [self getSelectedTextMarkerRange];
    textMarker = [self AXTextMarkerRangeCopyStartMarkerWrapper:textMarkerRange];
#endif
    VisiblePosition visiblePos = [self visiblePositionForTextMarker:textMarker];
    unsigned int    lineCount = 0;
    
    // return nil if the marker was out of date
    if (visiblePos.isNull())
        return nil;
    
    // move up until we get to the top
    // NOTE: This only takes us to the top of the element, not the top of the document
    while (visiblePos.isNotNull()) {
        lineCount += 1;
        visiblePos = previousLinePosition(visiblePos, khtml::UPSTREAM, 0);
    }
    
    return [NSNumber numberWithUnsignedInt:lineCount];
}

- (id)doAXTextMarkerRangeForLine: (NSNumber *) lineNumber
{
    if (!lineNumber) return nil;
    
    return nil;     // IMPLEMENT
}

- (id)doAXStringForTextMarkerRange: (AXTextMarkerRangeRef) textMarkerRange
{
    VisiblePosition startVisiblePosition, endVisiblePosition;
    Position startDeepPos, endDeepPos;
    QString qString;
    
#if MARKER_SELF_TEST
    textMarkerRange = [self getSelectedTextMarkerRange];
#endif
    
    // extract the start and end VisiblePosition
    startVisiblePosition = [self visiblePositionForStartOfTextMarkerRange: textMarkerRange];
    if (startVisiblePosition.isNull()) return nil;
    
    endVisiblePosition = [self visiblePositionForEndOfTextMarkerRange: textMarkerRange];
    if (endVisiblePosition.isNull()) return nil;
    
    // use deepEquivalent because it is what the user sees
    startDeepPos = startVisiblePosition.deepEquivalent();
    endDeepPos = endVisiblePosition.deepEquivalent();
    
    // get the visible text in the range
    qString = plainText(Range(startDeepPos.node(), startDeepPos.offset(), endDeepPos.node(), endDeepPos.offset()));
    if (qString == nil) return nil;
    
    // transform it to a CFString
    return (id)qString.getCFString();
}

- (id)doAXNextTextMarkerForTextMarker: (AXTextMarkerRef) textMarker
{
#if MARKER_SELF_TEST
    AXTextMarkerRangeRef textMarkerRange = [self getSelectedTextMarkerRange];
    textMarker = [self AXTextMarkerRangeCopyEndMarkerWrapper:textMarkerRange];
#endif
    VisiblePosition visiblePos = [self visiblePositionForTextMarker:textMarker];
    VisiblePosition nextVisiblePos = visiblePos.next();
    if (nextVisiblePos.isNull()) return nil;
    
    return (id) [self textMarkerForVisiblePosition:nextVisiblePos];
}

- (id)doAXPreviousTextMarkerForTextMarker: (AXTextMarkerRef) textMarker
{
#if MARKER_SELF_TEST
    AXTextMarkerRangeRef textMarkerRange = [self getSelectedTextMarkerRange];
    textMarker = [self AXTextMarkerRangeCopyStartMarkerWrapper:textMarkerRange];
#endif
    VisiblePosition visiblePos = [self visiblePositionForTextMarker:textMarker];
    VisiblePosition previousVisiblePos = visiblePos.previous();
    if (previousVisiblePos.isNull()) return nil;
    
    return (id) [self textMarkerForVisiblePosition:previousVisiblePos];
}

- (id)doAXLeftWordTextMarkerRangeForTextMarker: (AXTextMarkerRef) textMarker
{
#if MARKER_SELF_TEST
    AXTextMarkerRangeRef textMarkerRange = [self getSelectedTextMarkerRange];
    textMarker = [self AXTextMarkerRangeCopyStartMarkerWrapper:textMarkerRange];
#endif
    VisiblePosition visiblePos = [self visiblePositionForTextMarker:textMarker];
    VisiblePosition startPosition = startOfWord(visiblePos, khtml::LeftWordIfOnBoundary);
    VisiblePosition endPosition = endOfWord(startPosition);

    return (id) [self textMarkerRangeFromVisiblePositions:startPosition andEndPos:endPosition];
}

- (id)doAXRightWordTextMarkerRangeForTextMarker: (AXTextMarkerRef) textMarker
{
#if MARKER_SELF_TEST
    AXTextMarkerRangeRef textMarkerRange = [self getSelectedTextMarkerRange];
    textMarker = [self AXTextMarkerRangeCopyEndMarkerWrapper:textMarkerRange];
#endif
    VisiblePosition visiblePos = [self visiblePositionForTextMarker:textMarker];
    VisiblePosition startPosition = startOfWord(visiblePos, khtml::RightWordIfOnBoundary);
    VisiblePosition endPosition = endOfWord(startPosition);

    return (id) [self textMarkerRangeFromVisiblePositions:startPosition andEndPos:endPosition];
}

- (id)doAXLeftLineTextMarkerRangeForTextMarker: (AXTextMarkerRef) textMarker
{
#if 0
## startOfLine and endOfLine are declared but not defined
    VisiblePosition visiblePos = [self visiblePositionForTextMarker:textMarker];
    VisiblePosition startPosition = startOfLine(visiblePos, khtml::UPSTREAM);
    VisiblePosition endPosition = endOfLine(startPosition, khtml::DOWNSTREAM);

    return (id) [self textMarkerRangeFromVisiblePositions:startPosition andEndPos:endPosition];
#else
    // use Selection class instead
    return nil;
#endif
}

- (id)doAXRightLineTextMarkerRangeForTextMarker: (AXTextMarkerRef) textMarker
{
#if 0
## startOfLine and endOfLine are declared but not defined
    VisiblePosition visiblePos = [self visiblePositionForTextMarker:textMarker];
    VisiblePosition startPosition = startOfLine(visiblePos, khtml::DOWNSTREAM);
    VisiblePosition endPosition = endOfLine(startPosition, khtml::DOWNSTREAM);

    return (id) [self textMarkerRangeFromVisiblePositions:startPosition andEndPos:endPosition];
#else
    // use Selection class instead
    return nil;
#endif
}

- (id)accessibilityAttributeValue:(NSString *)attribute forParameter:(id)parameter
{
    AXTextMarkerRef         textMarker = nil;
    AXTextMarkerRangeRef    textMarkerRange = nil;
    NSNumber *              number = nil;

    // basic parameter validation
    if (!m_renderer || !attribute || !parameter)
        return nil;

    // common parameter type check/casting.  Nil checks in handlers catch wrong type case.
    // NOTE: This assumes nil is not a valid parameter, because itis indistinguishable from
    // a parameter of the wrong type.
    if (CFGetTypeID(parameter) == AXTextMarkerGetTypeID())
        textMarker = (AXTextMarkerRef) parameter;

    else if (CFGetTypeID(parameter) == AXTextMarkerRangeGetTypeID())
        textMarkerRange = (AXTextMarkerRangeRef) parameter;

    else if ([parameter isKindOfClass:[NSNumber self]]) {
        number = parameter;
    }
  
    // dispatch
    if ([attribute isEqualToString: (NSString *) kAXLineForTextMarkerParameterizedAttribute])
        return [self doAXLineForTextMarker: textMarker];

    if ([attribute isEqualToString: (NSString *) kAXTextMarkerRangeForLineParameterizedAttribute])
        return [self doAXTextMarkerRangeForLine: number];

    if ([attribute isEqualToString: (NSString *) kAXStringForTextMarkerRangeParameterizedAttribute])
        return [self doAXStringForTextMarkerRange: textMarkerRange];

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

#if 0
    if ([attribute isEqualToString: (NSString *) XXXX])
        return [self doXXXX: textMarker];
#endif
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
    return obj->document()->getOrCreateAccObjectCache()->accObject(obj);
}

#if 0
// NOTE: why don't we override this?  Probablly because AppKit's done it for us.
- (id)accessibilityFocusedUIElement
{
}

#endif

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
//    ASSERT(accObjectID == 0 || m_accObjectID == 0);
    m_accObjectID = accObjectID;
}

- (void)removeAccObjectID
{
    KWQAccObjectCache* cache = m_renderer->document()->getExistingAccObjectCache();
    cache->removeAccObjectID(self);
}

@end
