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
#import "dom_string.h"
#import "htmlattrs.h"
#import "khtmlview.h"
#import "render_canvas.h"
#import "render_object.h"
#import "render_replaced.h"
#import "render_style.h"
#import "render_text.h"

using DOM::DOMString;
using DOM::ElementImpl;
using khtml::InlineTextBoxArray;
using khtml::RenderCanvas;
using khtml::RenderObject;
using khtml::RenderText;
using khtml::RenderWidget;

// FIXME: This will eventually need to really localize.
#define UI_STRING(string, comment) ((NSString *)[NSString stringWithUTF8String:(string)])

@implementation KWQAccObject
-(id)initWithRenderer:(RenderObject*)renderer
{
    [super init];
    m_renderer = renderer;
    return self;
}

-(long)x
{
    if (!m_renderer)
        return 0;
    int x, y;
    m_renderer->absolutePosition(x,y);
    return x;
}

-(long)y
{
    if (!m_renderer)
        return 0;
    int x, y;
    m_renderer->absolutePosition(x,y);
    return y;
}

-(long)width
{
    if (!m_renderer)
        return 0;
    return m_renderer->width();
}

-(long)height
{
    if (!m_renderer)
        return 0;
    return m_renderer->height();
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
    if (m_renderer->isText())
        return NSAccessibilityStaticTextRole;
    if (m_renderer->isImage())
        return NSAccessibilityImageRole;
    if (m_renderer->isListMarker()) // FIXME: Can be an image/bullet or can be text.
        return NSAccessibilityStaticTextRole;
    if (m_renderer->isCanvas())
        return NSAccessibilityGroupRole;
    if (m_renderer->isTable() || m_renderer->isTableCell())
        return NSAccessibilityTableRole;
    
    return NSAccessibilityUnknownRole;
}

-(NSString*)roleDescription
{
    if (!m_renderer)
        return nil;

    if (m_renderer->element() && m_renderer->element()->isHTMLElement()) {
        QString title = static_cast<ElementImpl*>(m_renderer->element())->getAttribute(ATTR_TITLE).string();
        if (!title.isEmpty())
            return title.getNSString();
    }

    return UI_STRING("Testing role description", "not real yet");
}

-(NSString*)value
{
    if (!m_renderer)
        return nil;
    if (m_renderer->isText()) {
        RenderText* textObj = static_cast<RenderText*>(m_renderer);
        QString text;
        QString str = textObj->data().string();
        InlineTextBoxArray runs = textObj->inlineTextBoxes();
        bool addedSpace = true;
        for (unsigned i = 0; i < runs.count(); i++) {
            int runStart = runs[i]->m_start;
            int runEnd =  runs[i]->m_start + runs[i]->m_len;
            bool spaceBetweenRuns = false;
            text += str.mid(runStart, runEnd - runStart);
            spaceBetweenRuns = i+1 < runs.count() && runs[i+1]->m_start > runEnd;
            addedSpace = str[runEnd-1].direction() == QChar::DirWS;
            if (spaceBetweenRuns && !addedSpace) {
                text += " ";
                addedSpace = true;
            }
        }
        return text.getNSString();
    }
    
    return UI_STRING("Value not implemented yet.", "not real yet");
}

-(NSString*)title
{
    return UI_STRING("Title not implemented yet.", "not real yet");
}

-(NSValue*)position
{
    return [NSValue valueWithPoint: NSMakePoint([self x],[self y])];
}

-(NSValue*)size
{
    return [NSValue valueWithSize: NSMakeSize([self width],[self height])];
}

-(BOOL)accessibilityIsIgnored
{
    if (!m_renderer || m_renderer->style()->visibility() != khtml::VISIBLE)
        return YES;
    
    if (m_renderer->element() && m_renderer->element()->hasAnchor())
        return NO;
    
    return (!m_renderer->isCanvas() && !m_renderer->isTable() && !m_renderer->isTableCell() &&
            !m_renderer->isImage() && !m_renderer->isText() && !m_renderer->isListMarker());
}

- (NSArray *)accessibilityAttributeNames
{
    return [NSArray arrayWithObjects: NSAccessibilityRoleAttribute,
        NSAccessibilityRoleDescriptionAttribute,
        NSAccessibilityChildrenAttribute,
        NSAccessibilityHelpAttribute,
        NSAccessibilityParentAttribute,
        NSAccessibilityPositionAttribute,
        NSAccessibilitySizeAttribute,
        //NSAccessibilitySubroleAttribute,
        NSAccessibilityTitleAttribute,
        NSAccessibilityValueAttribute,
        NSAccessibilityFocusedAttribute,
        NSAccessibilityEnabledAttribute,
        NSAccessibilityWindowAttribute,
        nil];
}

- (NSArray*)accessibilityActionNames
{
    return nil;
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
        return @"Help";

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
