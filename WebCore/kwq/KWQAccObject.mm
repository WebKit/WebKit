/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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
#import "dom_string.h"
#import "dom2_range.h"
#import "htmlattrs.h"
#import "htmltags.h"
#import "khtmlview.h"
#import "khtml_part.h"
#import "render_canvas.h"
#import "render_object.h"
#import "render_replaced.h"
#import "render_style.h"
#import "render_text.h"

using DOM::ElementImpl;
using DOM::HTMLAnchorElementImpl;
using khtml::RenderObject;
using khtml::RenderWidget;
using khtml::RenderCanvas;
using khtml::RenderText;
using khtml::RenderBlock;

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
    m_renderer = 0;
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
    RenderObject* currRenderer;
    for (currRenderer = m_renderer; currRenderer && !currRenderer->element(); currRenderer = currRenderer->parent())
        if (currRenderer->continuation())
            return [currRenderer->document()->getOrCreateAccObjectCache()->accObject(currRenderer->continuation()) anchorElement];
    
    if (!currRenderer || !currRenderer->element())
        return 0;
    
    NodeImpl* elt = currRenderer->element();
    for ( ; elt; elt = elt->parentNode()) {
        if (elt->hasAnchor())
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
    if (!m_renderer || !m_renderer->parent())
        return nil;
    return m_renderer->document()->getOrCreateAccObjectCache()->accObject(m_renderer->parent());
}

-(KWQAccObject*)parentObjectUnignored
{
    if (!m_renderer || !m_renderer->parent())
        return nil;
    KWQAccObject* obj = m_renderer->document()->getOrCreateAccObjectCache()->accObject(m_renderer->parent());
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
}

-(NSString*)role
{
    if (!m_renderer)
        return NSAccessibilityUnknownRole;

    if (m_renderer->element() && m_renderer->element()->hasAnchor())
        return NSAccessibilityButtonRole;
    if (m_renderer->element() && m_renderer->element()->isHTMLElement() &&
        Node(m_renderer->element()).elementId() == ID_BUTTON)
        return NSAccessibilityButtonRole;
    if (m_renderer->isText())
        return NSAccessibilityStaticTextRole;
    if (m_renderer->isImage())
       return NSAccessibilityImageRole;
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
    if ([role isEqualToString:NSAccessibilityButtonRole]) {
        return UI_STRING("button", "accessibility role description for button");
    }
    
    if ([role isEqualToString:NSAccessibilityStaticTextRole]) {
        return UI_STRING("text", "accessibility role description for static text");
    }
    
    if ([role isEqualToString:NSAccessibilityImageRole]) {
        return UI_STRING("image", "accessibility role description for image");
    }
    
    if ([role isEqualToString:NSAccessibilityGroupRole]) {
        return UI_STRING("group", "accessibility role description for group");
    }
    
    return UI_STRING("unknown", "accessibility role description for unknown role");
}

-(NSString*)helpText
{
    if (!m_renderer)
        return nil;

    for (RenderObject* curr = m_renderer; curr; curr = curr->parent()) {
        if (curr->element() && curr->element()->isHTMLElement()) {
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
    if (!m_renderer)
        return nil;

    if (m_renderer->isText())
        return [self textUnderElement];
    
    // FIXME: We might need to implement a value here for more types
    // FIXME: It would be better not to advertise a value at all for the types for which we don't implement one;
    // this would require subclassing or making accessibilityAttributeNames do something other than return a
    // single static array.
    return nil;
}

-(NSString*)title
{
    if (!m_renderer)
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
    QRect rect = boundingBoxRect(m_renderer);
    
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
    QRect rect = boundingBoxRect(m_renderer);
    return [NSValue valueWithSize: NSMakeSize(rect.width(), rect.height())];
}

-(BOOL)accessibilityIsIgnored
{
    if (!m_renderer || m_renderer->style()->visibility() != khtml::VISIBLE)
        return YES;

    if (m_renderer->isText())
        return m_renderer->isBR() || !static_cast<RenderText*>(m_renderer)->firstTextBox();
    
    if (m_renderer->element() && m_renderer->element()->hasAnchor())
        return NO;

    if (m_renderer->isBlockFlow() && m_renderer->childrenInline())
        return !static_cast<RenderBlock*>(m_renderer)->firstLineBox();

    return (!m_renderer->isCanvas() && 
            !m_renderer->isImage() &&
            !(m_renderer->element() && m_renderer->element()->isHTMLElement() &&
              Node(m_renderer->element()).elementId() == ID_BUTTON));
}

- (NSArray *)accessibilityAttributeNames
{
    static NSArray* attributes = nil;
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
        NSMutableArray* arr = [NSMutableArray arrayWithCapacity: 8];
        [self addChildrenToArray: arr];
        return arr;
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
@end
