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

#import "KWQAccObject.h"

#import "KWQAccObjectCache.h"
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
using DOM::Range;
using DOM::DOMString;

using khtml::RenderObject;
using khtml::RenderWidget;
using khtml::RenderCanvas;
using khtml::RenderText;
using khtml::RenderBlock;
using khtml::RenderListMarker;
using khtml::RenderImage;

// FIXME: This will eventually need to really localize.
#define UI_STRING(string, comment) ((NSString *)[NSString stringWithUTF8String:(string)])

@implementation KWQAccObject
-(id)initWithRenderer:(RenderObject*)renderer
{
    [super init];
    m_renderer = renderer;
    m_areaElement = 0;
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
    if (m_areaElement)
        return m_areaElement;

    RenderObject* currRenderer;
    for (currRenderer = m_renderer; currRenderer && !currRenderer->element(); currRenderer = currRenderer->parent())
        if (currRenderer->continuation())
            return [currRenderer->document()->getOrCreateAccObjectCache()->accObject(currRenderer->continuation()) anchorElement];
    
    if (!currRenderer || !currRenderer->element())
        return 0;
    
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
    if (!m_renderer)
        return;
    
    if (m_renderer->isWidget()) {
        RenderWidget* renderWidget = static_cast<RenderWidget*>(m_renderer);
        QWidget* widget = renderWidget->widget();
        if (widget) {
            NSArray* childArr = [(widget->getView()) accessibilityAttributeValue: NSAccessibilityChildrenAttribute];
            [array addObjectsFromArray: childArr];
            return;
        }
    }
    
    for (KWQAccObject* obj = [self firstChild]; obj; obj = [obj nextSibling]) {
        if ([obj accessibilityIsIgnored])
            [obj addChildrenToArray: array];
        else
            [array addObject: obj];
    }
    
    if (m_renderer->isImage() && !m_areaElement) {
        HTMLMapElementImpl* map = static_cast<RenderImage*>(m_renderer)->imageMap();
        if (map) {
            // Need to add the <area> elements as individual accessibility objects.
            QPtrStack<NodeImpl> nodeStack;
            NodeImpl *current = map->firstChild();
            while (1) {
                if (!current) {
                    if(nodeStack.isEmpty()) break;
                    current = nodeStack.pop();
                    current = current->nextSibling();
                    continue;
                }
                if (current->hasAnchor()) {
                    KWQAccObject* obj = [[[KWQAccObject alloc] initWithRenderer: m_renderer] autorelease];
                    obj->m_areaElement = static_cast<HTMLAreaElementImpl*>(current);
                    [array addObject: obj];
                }
                NodeImpl *child = current->firstChild();
                if (child) {
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
    if (!m_renderer)
        return nil;
    
    // FIXME 3517227: These need to be localized (UI_STRING here is a dummy macro)
    // FIXME 3447564: It would be better to call some AppKit API to get these strings
    // (which would be the best way to localize them)
    
    NSString *role = [self role];
    if ([role isEqualToString:NSAccessibilityButtonRole])
        return UI_STRING("button", "accessibility role description for button");
    
    if ([role isEqualToString:NSAccessibilityStaticTextRole])
        return UI_STRING("text", "accessibility role description for static text");

    if ([role isEqualToString:NSAccessibilityImageRole])
        return UI_STRING("image", "accessibility role description for image");
    
    if ([role isEqualToString:NSAccessibilityGroupRole])
        return UI_STRING("group", "accessibility role description for group");
    
    if ([role isEqualToString:@"AXWebArea"])
        return UI_STRING("web area", "accessibility role description for web area");
    
    if ([role isEqualToString:@"AXLink"])
        return UI_STRING("link", "accessibility role description for link");
    
    if ([role isEqualToString:@"AXListMarker"])
        return UI_STRING("list marker", "accessibility role description for list marker");
    
    if ([role isEqualToString:@"AXImageMap"])
        return UI_STRING("image map", "accessibility role description for image map");
        
    return UI_STRING("unknown", "accessibility role description for unknown role");
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

    if (m_renderer->isImage()) {
        if (m_renderer->element() && m_renderer->element()->isHTMLElement()) {
            QString alt = static_cast<ElementImpl*>(m_renderer->element())->getAttribute(ATTR_ALT).string();
            return !alt.isEmpty() ? alt.getNSString() : nil;
        }
    }
    else if (m_renderer->element() && m_renderer->element()->isHTMLElement() &&
             Node(m_renderer->element()).elementId() == ID_BUTTON)
        return [self textUnderElement];
    else if (m_renderer->element() && m_renderer->element()->hasAnchor())
        return [self textUnderElement];

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
            NSAccessibilityValueAttribute,
            NSAccessibilityFocusedAttribute,
            NSAccessibilityEnabledAttribute,
            NSAccessibilityWindowAttribute,
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
            nil];
    }
    
    if (m_renderer->isCanvas())
        return webAreaAttrs;
    if (m_areaElement || (!m_renderer->isImage() && m_renderer->element() && m_renderer->element()->hasAnchor()))
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
    // We only have the one action (press).
    return UI_STRING("press link", "accessibility action description");
}

- (void)accessibilityPerformAction:(NSString *)action
{
    // We only have the one action (press).
    // Locate the anchor element. If it doesn't exist, just bail.
    HTMLAnchorElementImpl* anchorElt = [self anchorElement];
    if (!anchorElt) return;
    anchorElt->click();
}

- (BOOL)accessibilityIsAttributeSettable:(NSString*)attributeName
{
    return NO;
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

    return nil;
}

- (id)accessibilityHitTest:(NSPoint)point
{
    if (!m_renderer)
        return self;
    
    RenderObject::NodeInfo nodeInfo(true, true);
    m_renderer->layer()->nodeAtPoint(nodeInfo, (int)point.x, (int)point.y);
    if (!nodeInfo.innerNode())
        return self;
    RenderObject* obj = nodeInfo.innerNode()->renderer();
    if (!obj)
        return self;
    return obj->document()->getOrCreateAccObjectCache()->accObject(obj);
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

@end
