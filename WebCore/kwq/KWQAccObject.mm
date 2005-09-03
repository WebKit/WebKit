/*
 * Copyright (C) 2004, 2005 Apple Computer, Inc.  All rights reserved.
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

// need this until accesstool supports arrays of markers
#define MARKERARRAY_SELF_TEST 0

#include <mach-o/dyld.h>

#import "KWQAccObjectCache.h"
#import "KWQAssertions.h"
#import "KWQFoundationExtras.h"
#import "KWQWidget.h"
#import "WebCoreBridge.h"
#import "WebCoreFrameView.h"
#import "WebCoreViewFactory.h"

#import "dom_docimpl.h"
#import "dom_elementimpl.h"
#import "html_elementimpl.h"
#import "html_formimpl.h"
#import "html_inlineimpl.h"
#import "html_imageimpl.h"
#import "dom_string.h"
#import "dom2_eventsimpl.h"
#import "dom2_range.h"
#import "khtmlview.h"
#import "khtml_part.h"
#import "render_canvas.h"
#import "render_image.h"
#import "render_list.h"
#import "render_object.h"
#import "render_style.h"
#import "render_text.h"
#import "render_theme.h"
#import "selection.h"
#import "kjs_html.h"
#import "text_granularity.h"
#import "visible_position.h"
#import "visible_text.h"
#import "visible_units.h"
#import "html_miscimpl.h"
#import "qptrstack.h"
#import "DOMInternal.h"
#import "EventNames.h"

using namespace DOM::EventNames;
using namespace DOM::HTMLNames;

using DOM::DocumentImpl;
using DOM::DOMString;
using DOM::ElementImpl;
using DOM::HTMLAnchorElementImpl;
using DOM::HTMLAreaElementImpl;
using DOM::HTMLCollection;
using DOM::HTMLCollectionImpl;
using DOM::HTMLElementImpl;
using DOM::HTMLInputElementImpl;
using DOM::HTMLMapElementImpl;
using DOM::Node;
using DOM::NodeImpl;
using DOM::Position;

using khtml::theme;
using khtml::EAffinity;
using khtml::EVerticalAlign;
using khtml::plainText;
using khtml::RenderBlock;
using khtml::RenderCanvas;
using khtml::RenderImage;
using khtml::RenderListMarker;
using khtml::RenderObject;
using khtml::RenderStyle;
using khtml::RenderText;
using khtml::RenderWidget;
using khtml::Selection;
using khtml::TextIterator;
using khtml::VisiblePosition;

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

-(void)detach
{
    if ([self accessibilityShouldUseUniqueId])
        [[WebCoreViewFactory sharedFactory] unregisterUniqueIdForUIElement:self];
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
        if (elt->isLink() && elt->renderer() && !elt->renderer()->isImage())
            return static_cast<HTMLAnchorElementImpl*>(elt);
    }
  
    return 0;
}

-(BOOL)isImageButton
{
    return m_renderer->isImage() && static_cast<RenderImage*>(m_renderer)->isImageButton();
}

-(ElementImpl *)mouseButtonListener
{
    // FIXME: Do the continuation search like anchorElement does
    for (NodeImpl *elt = m_renderer->element(); elt; elt = elt->parentNode()) {
        if (elt->getHTMLEventListener(clickEvent) || elt->getHTMLEventListener(mousedownEvent) || elt->getHTMLEventListener(mouseupEvent))
            return static_cast<ElementImpl*>(elt);
    }
    return NULL;
}

-(ElementImpl *)actionElement
{
    if (m_renderer->element() && m_renderer->element()->hasTagName(inputTag)) {
        HTMLInputElementImpl* input = static_cast<HTMLInputElementImpl*>(m_renderer->element());
        if (!input->disabled() && (input->inputType() == HTMLInputElementImpl::CHECKBOX ||
                                   input->inputType() == HTMLInputElementImpl::RADIO))
            return input;
    }

    if ([self isImageButton])
        return static_cast<ElementImpl*>(m_renderer->element());

    ElementImpl * elt = [self anchorElement];
    if (!elt)
        elt = [self mouseButtonListener];

    return elt;
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
            NSArray* childArr = [(widget->getOuterView()) accessibilityAttributeValue: NSAccessibilityChildrenAttribute];
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
                if (current->isLink()) {
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

-(BOOL)isAttachment
{
    // widgets are the replaced elements that we represent to AX as attachments
    BOOL result = m_renderer->isWidget();
    
    // assert that a widget is a replaced element that is not an image
    ASSERT(!result || (m_renderer->isReplaced() && !m_renderer->isImage()));
    
    return result;
}

-(NSView*)attachmentView
{
    ASSERT(m_renderer->isReplaced() && m_renderer->isWidget() && !m_renderer->isImage());

    RenderWidget* renderWidget = static_cast<RenderWidget*>(m_renderer);
    QWidget* widget = renderWidget->widget();
    if (widget)
         return widget->getView();
    
    return nil;
}

-(NSString*)role
{
    if (!m_renderer)
        return NSAccessibilityUnknownRole;

    if (m_areaElement)
        return @"AXLink";
    if (m_renderer->element() && m_renderer->element()->isLink()) {
        if (m_renderer->isImage())
            return @"AXImageMap";
        return @"AXLink";
    }
    if (m_renderer->isListMarker())
        return @"AXListMarker";
    if (m_renderer->element() && m_renderer->element()->hasTagName(buttonTag))
        return NSAccessibilityButtonRole;
    if (m_renderer->isText())
        return NSAccessibilityStaticTextRole;
    if (m_renderer->isImage()) {
        if ([self isImageButton])
            return NSAccessibilityButtonRole;
        return NSAccessibilityImageRole;
    }
    if (m_renderer->isCanvas())
        return @"AXWebArea";
    
    if (m_renderer->element() && m_renderer->element()->hasTagName(inputTag)) {
        HTMLInputElementImpl* input = static_cast<HTMLInputElementImpl*>(m_renderer->element());
        if (input->inputType() == HTMLInputElementImpl::CHECKBOX)
            return NSAccessibilityCheckBoxRole;
        if (input->inputType() == HTMLInputElementImpl::RADIO)
            return NSAccessibilityRadioButtonRole;
    }
    
    if (m_renderer->isBlockFlow())
        return NSAccessibilityGroupRole;
    if ([self isAttachment])
        return [[self attachmentView] accessibilityAttributeValue:NSAccessibilityRoleAttribute];

    return NSAccessibilityUnknownRole;
}

-(NSString*)subrole
{
    if ([self isAttachment])
        return [[self attachmentView] accessibilityAttributeValue:NSAccessibilitySubroleAttribute];
    
    return nil;
}

-(NSString*)roleDescription
{
    if (!m_renderer)
        return nil;

    // attachments have the AXImage role, but a different subrole
    if ([self isAttachment])
        return [[self attachmentView] accessibilityAttributeValue:NSAccessibilityRoleDescriptionAttribute];
    
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
    
    if ([role isEqualToString:NSAccessibilityCheckBoxRole])
        return NSAccessibilityRoleDescription(NSAccessibilityCheckBoxRole, nil);
        
    if ([role isEqualToString:NSAccessibilityRadioButtonRole])
        return NSAccessibilityRoleDescription(NSAccessibilityRadioButtonRole, nil);

    if ([role isEqualToString:@"AXWebArea"])
        return UI_STRING("web area", "accessibility role description for web area");
    
    if ([role isEqualToString:@"AXLink"])
        return UI_STRING("link", "accessibility role description for link");
    
    if ([role isEqualToString:@"AXListMarker"])
        return UI_STRING("list marker", "accessibility role description for list marker");
    
    if ([role isEqualToString:@"AXImageMap"])
        return UI_STRING("image map", "accessibility role description for image map");
    
    return NSAccessibilityRoleDescription(NSAccessibilityUnknownRole, nil);
}

-(NSString*)helpText
{
    if (!m_renderer)
        return nil;

    if (m_areaElement) {
        QString summary = static_cast<ElementImpl*>(m_areaElement)->getAttribute(summaryAttr).qstring();
        if (!summary.isEmpty())
            return summary.getNSString();
        QString title = static_cast<ElementImpl*>(m_areaElement)->getAttribute(titleAttr).qstring();
        if (!title.isEmpty())
            return title.getNSString();
    }

    for (RenderObject* curr = m_renderer; curr; curr = curr->parent()) {
        if (curr->element() && curr->element()->isHTMLElement()) {
            QString summary = static_cast<ElementImpl*>(curr->element())->getAttribute(summaryAttr).qstring();
            if (!summary.isEmpty())
                return summary.getNSString();
            QString title = static_cast<ElementImpl*>(curr->element())->getAttribute(titleAttr).qstring();
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
            // catch stale KWQAccObject (see <rdar://problem/3960196>)
            if (p->xmlDocImpl() != d)
                return nil;
            return plainText(rangeOfContents(e).get()).getNSString();
        }
    }

    return nil;
}

-(id)value
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
        
        // FIXME: should use startOfDocument and endOfDocument (or rangeForDocument?) here
        VisiblePosition startVisiblePosition = m_renderer->positionForCoordinates (0, 0);
        VisiblePosition endVisiblePosition   = m_renderer->positionForCoordinates (LONG_MAX, LONG_MAX);
        if (startVisiblePosition.isNull() || endVisiblePosition.isNull())
            return nil;
            
        QString qString = plainText(makeRange(startVisiblePosition, endVisiblePosition).get());
        
        // transform it to a CFString and return that
        return (id)qString.getCFString();
    }
    
    if ([self isAttachment])
        return [[self attachmentView] accessibilityAttributeValue:NSAccessibilityValueAttribute];

    if (m_renderer->element() && m_renderer->element()->hasTagName(inputTag)) {
        HTMLInputElementImpl* input = static_cast<HTMLInputElementImpl*>(m_renderer->element());
        if (input->inputType() == HTMLInputElementImpl::CHECKBOX ||
            input->inputType() == HTMLInputElementImpl::RADIO)
            // Checkboxes return their state as an integer. 0 for off, 1 for on.
            return [NSNumber numberWithInt:input->checked()];
    }
    
    // FIXME: We might need to implement a value here for more types
    // FIXME: It would be better not to advertise a value at all for the types for which we don't implement one;
    // this would require subclassing or making accessibilityAttributeNames do something other than return a
    // single static array.
    return nil;
}

-(NSString*)title
{
    if (!m_renderer || m_areaElement || !m_renderer->element())
        return nil;
    
    if (m_renderer->element()->isHTMLElement() && m_renderer->element()->hasTagName(buttonTag))
        return [self textUnderElement];
    if (m_renderer->element()->isLink())
        return [self textUnderElement];
    if ([self isAttachment])
        return [[self attachmentView] accessibilityAttributeValue:NSAccessibilityTitleAttribute];
    
    return nil;
}

-(NSString*)accessibilityDescription
{
    if (!m_renderer || m_areaElement)
        return nil;
    
    if (m_renderer->isImage()) {
        if (m_renderer->element() && m_renderer->element()->isHTMLElement()) {
            QString alt = static_cast<ElementImpl*>(m_renderer->element())->getAttribute(altAttr).qstring();
            return !alt.isEmpty() ? alt.getNSString() : nil;
        }
    } else if ([self isAttachment])
        return [[self attachmentView] accessibilityAttributeValue:NSAccessibilityTitleAttribute];
    
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
                if (obj->style()->hasAppearance())
                    theme()->adjustRepaintRect(obj, r);
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

    // NOTE: BRs always have text boxes now, so the text box check here can be removed
    if (m_renderer->isText())
        return m_renderer->isBR() || !static_cast<RenderText*>(m_renderer)->firstTextBox();
    
    // delegate to the attachment
    if ([self isAttachment])
        return [[self attachmentView] accessibilityIsIgnored];
        
    if (m_areaElement || (m_renderer->element() && m_renderer->element()->isLink()))
        return NO;

    // All controls must be examined.
    if (m_renderer->element() && m_renderer->element()->isControl())
        return NO;

    if (m_renderer->isBlockFlow() && m_renderer->childrenInline())
        return !static_cast<RenderBlock*>(m_renderer)->firstLineBox() && ![self mouseButtonListener];

    return (!m_renderer->isListMarker() && !m_renderer->isCanvas() && 
            !m_renderer->isImage() &&
            !(m_renderer->element() && m_renderer->element()->isHTMLElement() &&
              m_renderer->element()->hasTagName(buttonTag)));
}

- (NSArray *)accessibilityAttributeNames
{
    static NSArray* attributes = nil;
    static NSArray* anchorAttrs = nil;
    static NSArray* webAreaAttrs = nil;
    if (attributes == nil) {
        attributes = [[NSArray alloc] initWithObjects: NSAccessibilityRoleAttribute,
            NSAccessibilitySubroleAttribute,
            NSAccessibilityRoleDescriptionAttribute,
            NSAccessibilityChildrenAttribute,
            NSAccessibilityHelpAttribute,
            NSAccessibilityParentAttribute,
            NSAccessibilityPositionAttribute,
            NSAccessibilitySizeAttribute,
            NSAccessibilityTitleAttribute,
            NSAccessibilityDescriptionAttribute,
            NSAccessibilityValueAttribute,
            NSAccessibilityFocusedAttribute,
            NSAccessibilityEnabledAttribute,
            NSAccessibilityWindowAttribute,
            @"AXSelectedTextMarkerRange",
            @"AXStartTextMarker",
            @"AXEndTextMarker",
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
            @"AXSelectedTextMarkerRange",
            @"AXStartTextMarker",
            @"AXEndTextMarker",
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
            @"AXSelectedTextMarkerRange",
            @"AXStartTextMarker",
            @"AXEndTextMarker",
            nil];
    }
    
    if (m_renderer && m_renderer->isCanvas())
        return webAreaAttrs;
    if (m_areaElement || (m_renderer && !m_renderer->isImage() && m_renderer->element() && m_renderer->element()->isLink()))
        return anchorAttrs;
    return attributes;
}

- (NSArray*)accessibilityActionNames
{
    static NSArray* actions = nil;
    
    if ([self actionElement]) {
        if (actions == nil)
            actions = [[NSArray alloc] initWithObjects: NSAccessibilityPressAction, nil];
        return actions;
    }
    return nil;
}

- (NSString *)accessibilityActionDescription:(NSString *)action
{
    // we have no custom actions
    return NSAccessibilityActionDescription(action);
}

- (void)accessibilityPerformAction:(NSString *)action
{
    if ([action isEqualToString:NSAccessibilityPressAction]) {
        ElementImpl *actionElement = [self actionElement];
        if (!actionElement)
            return;

        DocumentImpl* d = actionElement->getDocument();
        if (d) {
            KHTMLPart* p = d->part();
            if (p) {
                KWQ(p)->prepareForUserAction();
            }
        }

        actionElement->accessKeyAction(true);
    }
}

- (WebCoreTextMarkerRange *) textMarkerRangeFromMarkers: (WebCoreTextMarker *) textMarker1 andEndMarker:(WebCoreTextMarker *) textMarker2
{
    return [[WebCoreViewFactory sharedFactory] textMarkerRangeWithStart:textMarker1 end:textMarker2];
}

- (WebCoreTextMarker *) textMarkerForVisiblePosition: (VisiblePosition)visiblePos
{
    if (visiblePos.isNull())
        return nil;
        
    return m_renderer->document()->getAccObjectCache()->textMarkerForVisiblePosition(visiblePos);
}

- (VisiblePosition) visiblePositionForTextMarker: (WebCoreTextMarker *)textMarker
{
    return m_renderer->document()->getAccObjectCache()->visiblePositionForTextMarker(textMarker);
}

- (VisiblePosition) visiblePositionForStartOfTextMarkerRange: (WebCoreTextMarkerRange *)textMarkerRange
{
    return [self visiblePositionForTextMarker:[[WebCoreViewFactory sharedFactory] startOfTextMarkerRange:textMarkerRange]];
}

- (VisiblePosition) visiblePositionForEndOfTextMarkerRange: (WebCoreTextMarkerRange *) textMarkerRange
{
    return [self visiblePositionForTextMarker:[[WebCoreViewFactory sharedFactory] endOfTextMarkerRange:textMarkerRange]];
}

- (WebCoreTextMarkerRange *) textMarkerRangeFromVisiblePositions: (VisiblePosition) startPosition andEndPos: (VisiblePosition) endPosition
{
    WebCoreTextMarker *startTextMarker = [self textMarkerForVisiblePosition: startPosition];
    WebCoreTextMarker *endTextMarker   = [self textMarkerForVisiblePosition: endPosition];
    return [self textMarkerRangeFromMarkers: startTextMarker andEndMarker:endTextMarker];
}

- (WebCoreTextMarkerRange *)textMarkerRange
{
    if (!m_renderer)
        return nil;
        
    WebCoreTextMarker *startTextMarker = [self textMarkerForVisiblePosition: VisiblePosition(m_renderer->element(), m_renderer->caretMinOffset(), khtml::VP_DEFAULT_AFFINITY)];
    WebCoreTextMarker *endTextMarker   = [self textMarkerForVisiblePosition: VisiblePosition(m_renderer->element(), m_renderer->caretMaxRenderedOffset(), khtml::VP_DEFAULT_AFFINITY)];
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

    if ([attributeName isEqualToString: NSAccessibilitySubroleAttribute])
        return [self subrole];

    if ([attributeName isEqualToString: NSAccessibilityRoleDescriptionAttribute])
        return [self roleDescription];
    
    if ([attributeName isEqualToString: NSAccessibilityParentAttribute]) {
        if (m_renderer->isCanvas() && m_renderer->canvas() && m_renderer->canvas()->view())
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
            NSMutableArray *links = [NSMutableArray arrayWithCapacity: 32];
            HTMLCollectionImpl *coll = new HTMLCollectionImpl(m_renderer->document(), HTMLCollectionImpl::DOC_LINKS);
            coll->ref();
            NodeImpl *curr = coll->firstItem();
            while (curr) {
                RenderObject *obj = curr->renderer();
                if (obj) {
                    KWQAccObject *axobj = obj->document()->getAccObjectCache()->accObject(obj);
                    ASSERT([[axobj role] isEqualToString:@"AXLink"]);
                    if (![axobj accessibilityIsIgnored])
                        [links addObject: axobj];
                }
                curr = coll->nextItem();
            }
            coll->deref();
            return links;
        }
        if ([attributeName isEqualToString: @"AXLoaded"])
            return [NSNumber numberWithBool: (!m_renderer->document()->tokenizer())];
        if ([attributeName isEqualToString: @"AXLayoutCount"])
            return [NSNumber numberWithInt: (static_cast<RenderCanvas*>(m_renderer)->view()->layoutCount())];
    }
    
    if ([attributeName isEqualToString: @"AXURL"] && 
        (m_areaElement || (!m_renderer->isImage() && m_renderer->element() && m_renderer->element()->isLink()))) {
        HTMLAnchorElementImpl* anchor = [self anchorElement];
        if (anchor) {
            QString s = anchor->getAttribute(hrefAttr).qstring();
            if (!s.isNull()) {
                s = anchor->getDocument()->completeURL(s);
                return s.getNSString();
            }
        }
    }
    
    if ([attributeName isEqualToString: NSAccessibilityTitleAttribute])
        return [self title];
    
    if ([attributeName isEqualToString: NSAccessibilityDescriptionAttribute])
        return [self accessibilityDescription];
    
    if ([attributeName isEqualToString: NSAccessibilityValueAttribute])
        return [self value];

    if ([attributeName isEqualToString: NSAccessibilityHelpAttribute])
        return [self helpText];
    
    if ([attributeName isEqualToString: NSAccessibilityFocusedAttribute])
        return [NSNumber numberWithBool: (m_renderer->element() && m_renderer->document()->focusNode() == m_renderer->element())];

    if ([attributeName isEqualToString: NSAccessibilityEnabledAttribute])
        return [NSNumber numberWithBool: m_renderer->element() ? m_renderer->element()->isEnabled() : YES];
    
    if ([attributeName isEqualToString: NSAccessibilitySizeAttribute])
        return [self size];

    if ([attributeName isEqualToString: NSAccessibilityPositionAttribute])
        return [self position];

    if ([attributeName isEqualToString: NSAccessibilityWindowAttribute]) {
        if (m_renderer && m_renderer->canvas() && m_renderer->canvas()->view())
            return [m_renderer->canvas()->view()->getView() window];
        return nil;
    }
    
    if ([attributeName isEqualToString: @"AXSelectedTextMarkerRange"]) {
        // get the selection from the document part
        // NOTE: BUG support nested WebAreas, like in <http://webcourses.niu.edu/>
        // (there is a web archive of this page attached to <rdar://problem/3888973>)
        // Trouble is we need to know which document view to ask.
        Selection   sel = [self topView]->part()->selection();
        if (sel.isNone()) {
            sel = m_renderer->document()->renderer()->canvas()->view()->part()->selection();
            if (sel.isNone())
                return nil;
        }
            
        // return a marker range for the selection start to end
        VisiblePosition startPosition = VisiblePosition(sel.start(), sel.startAffinity());
        VisiblePosition endPosition = VisiblePosition(sel.end(), sel.endAffinity());
        return (id) [self textMarkerRangeFromVisiblePositions:startPosition andEndPos:endPosition];
    }
    
    if ([attributeName isEqualToString: @"AXStartTextMarker"]) {
        // FIXME: should use startOfDocument here
        VisiblePosition startPos = [self topRenderer]->positionForCoordinates (0, 0);
        return (id) [self textMarkerForVisiblePosition: startPos];
    }

    if ([attributeName isEqualToString: @"AXEndTextMarker"]) {
        // FIXME: should use endOfDocument here
        VisiblePosition endPos = [self topRenderer]->positionForCoordinates (LONG_MAX, LONG_MAX);
        return (id) [self textMarkerForVisiblePosition: endPos];
    }

    return nil;
}

- (NSArray *)accessibilityParameterizedAttributeNames
{
    static NSArray* paramAttributes = nil;
    if (paramAttributes == nil) {
        paramAttributes = [[NSArray alloc] initWithObjects:
            @"AXUIElementForTextMarker",
            @"AXTextMarkerRangeForUIElement",
            @"AXLineForTextMarker",
            @"AXTextMarkerRangeForLine",
            @"AXStringForTextMarkerRange",
            @"AXTextMarkerForPosition",
            @"AXBoundsForTextMarkerRange",
            @"AXAttributedStringForTextMarkerRange",
            @"AXTextMarkerRangeForUnorderedTextMarkers",
            @"AXNextTextMarkerForTextMarker",
            @"AXPreviousTextMarkerForTextMarker",
            @"AXLeftWordTextMarkerRangeForTextMarker",
            @"AXRightWordTextMarkerRangeForTextMarker",
            @"AXLeftLineTextMarkerRangeForTextMarker",
            @"AXRightLineTextMarkerRangeForTextMarker",
            @"AXSentenceTextMarkerRangeForTextMarker",
            @"AXParagraphTextMarkerRangeForTextMarker",
            @"AXNextWordEndTextMarkerForTextMarker",
            @"AXPreviousWordStartTextMarkerForTextMarker",
            @"AXNextLineEndTextMarkerForTextMarker",
            @"AXPreviousLineStartTextMarkerForTextMarker",
            @"AXNextSentenceEndTextMarkerForTextMarker",
            @"AXPreviousSentenceStartTextMarkerForTextMarker",
            @"AXNextParagraphEndTextMarkerForTextMarker",
            @"AXPreviousParagraphStartTextMarkerForTextMarker",
            @"AXLengthForTextMarkerRange",
            nil];
    }

    return paramAttributes;
}

- (id)doAXUIElementForTextMarker: (WebCoreTextMarker *) textMarker
{
    VisiblePosition visiblePos = [self visiblePositionForTextMarker:textMarker];
    if (visiblePos.isNull())
        return nil;

    RenderObject * obj = visiblePos.position().node()->renderer();
    if (!obj)
        return nil;
    
    return obj->document()->getAccObjectCache()->accObject(obj);
}

- (id)doAXTextMarkerRangeForUIElement: (id) uiElement
{
    return (id)[uiElement textMarkerRange];
}

- (id)doAXLineForTextMarker: (WebCoreTextMarker *) textMarker
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
    while (visiblePos.isNotNull() && visiblePos != savedVisiblePos) {
        lineCount += 1;
        savedVisiblePos = visiblePos;
        visiblePos = previousLinePosition(visiblePos, 0);
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
    VisiblePosition visiblePos = [self topRenderer]->positionForCoordinates (0, 0);
    VisiblePosition savedVisiblePos;
    while (--lineCount != 0) {
        savedVisiblePos = visiblePos;
        visiblePos = nextLinePosition(visiblePos, 0);
        if (visiblePos.isNull() || visiblePos == savedVisiblePos)
            return nil;
    }
    
    // make a caret selection for the marker position, then extend it to the line
    // NOTE: ignores results of sel.modify because it returns false when
    // starting at an empty line.  The resulting selection in that case
    // will be a caret at visiblePos. 
    Selection sel = Selection(visiblePos, visiblePos);
    (void)sel.modify(Selection::EXTEND, Selection::RIGHT, khtml::LINE_BOUNDARY);

    // return a marker range for the selection start to end
    VisiblePosition startPosition = VisiblePosition(sel.start(), sel.startAffinity());
    VisiblePosition endPosition = VisiblePosition(sel.end(), sel.endAffinity());
    return (id) [self textMarkerRangeFromVisiblePositions:startPosition andEndPos:endPosition];
}

- (id)doAXStringForTextMarkerRange: (WebCoreTextMarkerRange *) textMarkerRange
{
    // extract the start and end VisiblePosition
    VisiblePosition startVisiblePosition = [self visiblePositionForStartOfTextMarkerRange: textMarkerRange];
    if (startVisiblePosition.isNull())
        return nil;
    
    VisiblePosition endVisiblePosition = [self visiblePositionForEndOfTextMarkerRange: textMarkerRange];
    if (endVisiblePosition.isNull())
        return nil;
    
    // get the visible text in the range
    QString qString = plainText(makeRange(startVisiblePosition, endVisiblePosition).get());
    
    // transform it to a CFString and return that
    return (id)qString.getCFString();
}

- (id)doAXTextMarkerForPosition: (NSPoint) point
{
    // convert absolute point to view coordinates
    KHTMLView *docView = [self topView];
    NSView *view = docView->getView();
    RenderObject *renderer = [self topRenderer];
    NodeImpl *innerNode = NULL;
    NSPoint ourpoint;
    
    // locate the node containing the point
    while (1) {
        // ask the document layer to hitTest
        NSPoint windowCoord = [[view window] convertScreenToBase: point];
        ourpoint = [view convertPoint:windowCoord fromView:nil];
        ourpoint.x += docView->contentsX();
        ourpoint.y += docView->contentsY();
        
        RenderObject::NodeInfo nodeInfo(true, true);
        renderer->layer()->hitTest(nodeInfo, (int)ourpoint.x, (int)ourpoint.y);
        innerNode = nodeInfo.innerNode();
        if (!innerNode || !innerNode->renderer())
            return nil;

        // done if hit something other than a widget
        renderer = innerNode->renderer();
        if (!renderer->isWidget())
            break;

        // descend into widget (FRAME, IFRAME, OBJECT...)
        QWidget *widget = static_cast<RenderWidget *>(renderer)->widget();
        if (!widget || !widget->inherits("KHTMLView"))
            break;
        KHTMLPart *part = static_cast<KHTMLView *>(widget)->part();
        if (!part)
            break;
        DocumentImpl *document = part->xmlDocImpl();
        if (!document)
            break;
        renderer = document->renderer();
        docView = static_cast<KHTMLView *>(widget);
        view = docView->getDocumentView();
    }
    
    // get position within the node
    VisiblePosition pos = innerNode->renderer()->positionForCoordinates ((int)ourpoint.x, (int)ourpoint.y);
    return (id) [self textMarkerForVisiblePosition:pos];
}

- (id)doAXBoundsForTextMarkerRange: (WebCoreTextMarkerRange *) textMarkerRange
{

    // extract the start and end VisiblePosition
    VisiblePosition startVisiblePosition = [self visiblePositionForStartOfTextMarkerRange: textMarkerRange];
    if (startVisiblePosition.isNull())
        return nil;
    
    VisiblePosition endVisiblePosition = [self visiblePositionForEndOfTextMarkerRange: textMarkerRange];
    if (endVisiblePosition.isNull())
        return nil;
    
    // use the Selection class to help calculate the corresponding rectangle
    QRect rect1 = Selection(startVisiblePosition, startVisiblePosition).caretRect();
    QRect rect2 = Selection(endVisiblePosition, endVisiblePosition).caretRect();
    QRect ourrect = rect1.unite(rect2);

    // try to use the document view from the selection, so that nested WebAreas work,
    // but fall back to the top level doc if we do not find it easily
    KHTMLView *docView = NULL;
    RenderObject * renderer = startVisiblePosition.deepEquivalent().node()->renderer();
    if (renderer) {
        DocumentImpl* doc = renderer->document();
        if (doc)
            docView = doc->view();
    }
    if (!docView)
        docView = [self topView];
    NSView * view = docView->getView();

    // if the selection spans lines, the rectangle is to extend
    // across the width of the view
    if (rect1.bottom() != rect2.bottom()) {
        ourrect.setX((int)[view frame].origin.x);
        ourrect.setWidth((int)[view frame].size.width);
    }
 
    // convert our rectangle to screen coordinates
    NSRect rect = NSMakeRect(ourrect.left(), ourrect.top(), ourrect.width(), ourrect.height());
    rect = NSOffsetRect(rect, -docView->contentsX(), -docView->contentsY());
    rect = [view convertRect:rect toView:nil];
    rect.origin = [[view window] convertBaseToScreen:rect.origin];
   
    // return the converted rect
    return [NSValue valueWithRect:rect];
}

static CGColorRef CreateCGColorIfDifferent(NSColor *nsColor, CGColorRef existingColor)
{
    // get color information assuming NSDeviceRGBColorSpace 
    NSColor *rgbColor = [nsColor colorUsingColorSpaceName:NSDeviceRGBColorSpace];
    if (rgbColor == nil) {
	rgbColor = [NSColor blackColor];
    }
    float components[4];
    [rgbColor getRed:&components[0] green:&components[1] blue:&components[2] alpha:&components[3]];
    
    // create a new CGColorRef to return
    CGColorSpaceRef cgColorSpace = CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB);
    CGColorRef cgColor = CGColorCreate(cgColorSpace, components);
    CGColorSpaceRelease(cgColorSpace);
    CFMakeCollectable(cgColor);
    
    // check for match with existing color
    if (existingColor && CGColorEqualToColor(cgColor, existingColor))
        cgColor = nil;
        
    return cgColor;
}

static void AXAttributeStringSetColor(NSMutableAttributedString *attrString, NSString *attribute, NSColor* color, NSRange range)
{
    if (color != nil) {
        CGColorRef existingColor = (CGColorRef) [attrString attribute:attribute atIndex:range.location effectiveRange:nil];
        CGColorRef cgColor = CreateCGColorIfDifferent(color, existingColor);
        if (cgColor != NULL) {
            [attrString addAttribute:attribute value:(id)cgColor range:range];
            CGColorRelease(cgColor);
        }
    } else
        [attrString removeAttribute:attribute range:range];
}

static void AXAttributeStringSetNumber(NSMutableAttributedString *attrString, NSString *attribute, NSNumber* number, NSRange range)
{
    if (number != nil) {
	[attrString addAttribute:attribute value:number range:range];
    } else
        [attrString removeAttribute:attribute range:range];
}

static void AXAttributeStringSetFont(NSMutableAttributedString *attrString, NSString *attribute, NSFont* font, NSRange range)
{
    NSDictionary *dict;
    
    if (font != nil) {
        dict = [NSDictionary dictionaryWithObjectsAndKeys:
            [font fontName]                             , NSAccessibilityFontNameKey,
            [font familyName]                           , NSAccessibilityFontFamilyKey,
            [font displayName]                          , NSAccessibilityVisibleNameKey,
            [NSNumber numberWithFloat:[font pointSize]] , NSAccessibilityFontSizeKey,
        nil];

        [attrString addAttribute:attribute value:dict range:range];
    } else
        [attrString removeAttribute:attribute range:range];
    
}

static void AXAttributeStringSetStyle(NSMutableAttributedString *attrString, RenderObject *renderer, NSRange range)
{
    RenderStyle *style = renderer->style();

    // set basic font info
    AXAttributeStringSetFont(attrString, NSAccessibilityFontTextAttribute, style->font().getNSFont(), range);

    // set basic colors
    AXAttributeStringSetColor(attrString, NSAccessibilityForegroundColorTextAttribute, nsColor(style->color()), range);
    AXAttributeStringSetColor(attrString, NSAccessibilityBackgroundColorTextAttribute, nsColor(style->backgroundColor()), range);

    // set super/sub scripting
    EVerticalAlign alignment = style->verticalAlign();
    if (alignment == khtml::SUB)
        AXAttributeStringSetNumber(attrString, NSAccessibilitySuperscriptTextAttribute, [NSNumber numberWithInt:(-1)], range);
    else if (alignment == khtml::SUPER)
        AXAttributeStringSetNumber(attrString, NSAccessibilitySuperscriptTextAttribute, [NSNumber numberWithInt:1], range);
    else
        [attrString removeAttribute:NSAccessibilitySuperscriptTextAttribute range:range];
    
    // set shadow
    if (style->textShadow() != nil)
        AXAttributeStringSetNumber(attrString, NSAccessibilityShadowTextAttribute, [NSNumber numberWithBool:YES], range);
    else
        [attrString removeAttribute:NSAccessibilityShadowTextAttribute range:range];
    
    // set underline and strikethrough
    int decor = style->textDecorationsInEffect();
    if ((decor & khtml::UNDERLINE) == 0) {
        [attrString removeAttribute:NSAccessibilityUnderlineTextAttribute range:range];
        [attrString removeAttribute:NSAccessibilityUnderlineColorTextAttribute range:range];
    }
    
    if ((decor & khtml::LINE_THROUGH) == 0) {
        [attrString removeAttribute:NSAccessibilityStrikethroughTextAttribute range:range];
        [attrString removeAttribute:NSAccessibilityStrikethroughColorTextAttribute range:range];
    }

    if ((decor & (khtml::UNDERLINE | khtml::LINE_THROUGH)) != 0) {
        // find colors using quirk mode approach (strict mode would use current
        // color for all but the root line box, which would use getTextDecorationColors)
        QColor underline, overline, linethrough;
        renderer->getTextDecorationColors(decor, underline, overline, linethrough);
        
        if ((decor & khtml::UNDERLINE) != 0) {
            AXAttributeStringSetNumber(attrString, NSAccessibilityUnderlineTextAttribute, [NSNumber numberWithBool:YES], range);
            AXAttributeStringSetColor(attrString, NSAccessibilityUnderlineColorTextAttribute, nsColor(underline), range);
        }

        if ((decor & khtml::LINE_THROUGH) != 0) {
            AXAttributeStringSetNumber(attrString, NSAccessibilityStrikethroughTextAttribute, [NSNumber numberWithBool:YES], range);
            AXAttributeStringSetColor(attrString, NSAccessibilityStrikethroughColorTextAttribute, nsColor(linethrough), range);
        }
    }
}

static void AXAttributeStringSetElement(NSMutableAttributedString *attrString, NSString *attribute, id element, NSRange range)
{
    if (element != nil) {
        // make a serialiazable AX object
        AXUIElementRef axElement = [[WebCoreViewFactory sharedFactory] AXUIElementForElement:element];
        if (axElement != NULL) {
            [attrString addAttribute:attribute value:(id)axElement range:range];
            CFRelease(axElement);
        }
    } else {
        [attrString removeAttribute:attribute range:range];
    }
}

static KWQAccObject *AXLinkElementForNode (NodeImpl *node)
{
    RenderObject *obj = node->renderer();
    if (!obj)
        return nil;

    KWQAccObject *axObj = obj->document()->getAccObjectCache()->accObject(obj);
    HTMLAnchorElementImpl* anchor = [axObj anchorElement];
    if (!anchor || !anchor->renderer())
        return nil;

    return anchor->renderer()->document()->getAccObjectCache()->accObject(anchor->renderer());
}

static void AXAttributedStringAppendText (NSMutableAttributedString *attrString, NodeImpl *nodeImpl, const QChar *chars, int length)
{
    // easier to calculate the range before appending the string
    NSRange attrStringRange = NSMakeRange([attrString length], length);
    
    // append the string from this node
    [[attrString mutableString] appendString:[NSString stringWithCharacters:(const UniChar*)chars length:length]];

    // add new attributes and remove irrelevant inherited ones
    // NOTE: color attributes are handled specially because -[NSMutableAttributedString addAttribute: value: range:] does not merge
    // identical colors.  Workaround is to not replace an existing color attribute if it matches what we are adding.  This also means
    // we can not just pre-remove all inherited attributes on the appended string, so we have to remove the irrelevant ones individually.

    // remove inherited attachment from prior AXAttributedStringAppendReplaced
    [attrString removeAttribute:NSAccessibilityAttachmentTextAttribute range:attrStringRange];
    
    // set new attributes
    AXAttributeStringSetStyle(attrString, nodeImpl->renderer(), attrStringRange);
    AXAttributeStringSetElement(attrString, NSAccessibilityLinkTextAttribute, AXLinkElementForNode(nodeImpl), attrStringRange);
}

static void AXAttributedStringAppendReplaced (NSMutableAttributedString *attrString, NodeImpl *replacedNode)
{
    static const UniChar attachmentChar = NSAttachmentCharacter;

    // we should always be given a rendered node, but be safe
    if (!replacedNode || !replacedNode->renderer()) {
        ASSERT_NOT_REACHED();
        return;
    }
    
    // we should always be given a replaced node, but be safe
    // replaced nodes are either attachments (widgets) or images
    if (!replacedNode->renderer()->isReplaced()) {
        ASSERT_NOT_REACHED();
        return;
    }
        
    // create an AX object, but skip it if it is not supposed to be seen
    KWQAccObject *obj = replacedNode->renderer()->document()->getAccObjectCache()->accObject(replacedNode->renderer());
    if ([obj accessibilityIsIgnored])
        return;
    
    // easier to calculate the range before appending the string
    NSRange attrStringRange = NSMakeRange([attrString length], 1);
    
    // append the placeholder string
    [[attrString mutableString] appendString:[NSString stringWithCharacters:&attachmentChar length:1]];
    
    // remove all inherited attributes
    [attrString setAttributes:nil range:attrStringRange];

    // add the attachment attribute
    AXAttributeStringSetElement(attrString, NSAccessibilityAttachmentTextAttribute, obj, attrStringRange);
}

- (id)doAXAttributedStringForTextMarkerRange: (WebCoreTextMarkerRange *) textMarkerRange
{
    // extract the start and end VisiblePosition
    VisiblePosition startVisiblePosition = [self visiblePositionForStartOfTextMarkerRange: textMarkerRange];
    if (startVisiblePosition.isNull())
        return nil;
    
    VisiblePosition endVisiblePosition = [self visiblePositionForEndOfTextMarkerRange: textMarkerRange];
    if (endVisiblePosition.isNull())
        return nil;
    
    // iterate over the range to build the AX attributed string
    NSMutableAttributedString *attrString = [[NSMutableAttributedString alloc] init];
    TextIterator it(makeRange(startVisiblePosition, endVisiblePosition).get());
    while (!it.atEnd()) {
        // locate the node for this range
        int exception = 0;
        NodeImpl *nodeImpl = it.range()->startContainer(exception);
        ASSERT(nodeImpl == it.range()->endContainer(exception));
        
        // non-zero length means textual node, zero length means replaced node (AKA "attachments" in AX)
        if (it.length() != 0) {
            AXAttributedStringAppendText (attrString, nodeImpl, it.characters(), it.length());
        } else {
            AXAttributedStringAppendReplaced (attrString, nodeImpl->childNode(it.range()->startOffset(exception)));
        }
        
        it.advance();
    }

    return [attrString autorelease];
}

- (id)doAXTextMarkerRangeForUnorderedTextMarkers: (NSArray *) markers
{
#if MARKERARRAY_SELF_TEST
    WebCoreTextMarkerRange *tmr = [self getSelectedTextMarkerRange];
    WebCoreTextMarker *tm1 = AXTextMarkerRangeCopyEndMarker(tmr);
    WebCoreTextMarker *tm2 = AXTextMarkerRangeCopyStartMarker(tmr);
    markers = [NSArray arrayWithObjects: (id) tm1, (id) tm2, nil];
#endif
    // get and validate the markers
    if ([markers count] < 2)
        return nil;
    
    WebCoreTextMarker *textMarker1 = (WebCoreTextMarker *) [markers objectAtIndex:0];
    WebCoreTextMarker *textMarker2 = (WebCoreTextMarker *) [markers objectAtIndex:1];
    if (![[WebCoreViewFactory sharedFactory] objectIsTextMarker:textMarker1] || ![[WebCoreViewFactory sharedFactory] objectIsTextMarker:textMarker2])
        return nil;
    
    // convert to VisiblePosition
    VisiblePosition visiblePos1 = [self visiblePositionForTextMarker:textMarker1];
    VisiblePosition visiblePos2 = [self visiblePositionForTextMarker:textMarker2];
    if (visiblePos1.isNull() || visiblePos2.isNull())
        return nil;
    
    // use the Selection class to do the ordering
    // NOTE: Perhaps we could add a Selection method to indicate direction, based on m_baseIsStart
    WebCoreTextMarker *startTextMarker;
    WebCoreTextMarker *endTextMarker;
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

- (id)doAXNextTextMarkerForTextMarker: (WebCoreTextMarker *) textMarker
{
    VisiblePosition visiblePos = [self visiblePositionForTextMarker:textMarker];
    VisiblePosition nextVisiblePos = visiblePos.next();
    if (nextVisiblePos.isNull())
        return nil;
    
    return (id) [self textMarkerForVisiblePosition:nextVisiblePos];
}

- (id)doAXPreviousTextMarkerForTextMarker: (WebCoreTextMarker *) textMarker
{
    VisiblePosition visiblePos = [self visiblePositionForTextMarker:textMarker];
    VisiblePosition previousVisiblePos = visiblePos.previous();
    if (previousVisiblePos.isNull())
        return nil;
    
    return (id) [self textMarkerForVisiblePosition:previousVisiblePos];
}

- (id)doAXLeftWordTextMarkerRangeForTextMarker: (WebCoreTextMarker *) textMarker
{
    VisiblePosition visiblePos = [self visiblePositionForTextMarker:textMarker];
    VisiblePosition startPosition = startOfWord(visiblePos, khtml::LeftWordIfOnBoundary);
    VisiblePosition endPosition = endOfWord(startPosition);

    return (id) [self textMarkerRangeFromVisiblePositions:startPosition andEndPos:endPosition];
}

- (id)doAXRightWordTextMarkerRangeForTextMarker: (WebCoreTextMarker *) textMarker
{
    VisiblePosition visiblePos = [self visiblePositionForTextMarker:textMarker];
    VisiblePosition startPosition = startOfWord(visiblePos, khtml::RightWordIfOnBoundary);
    VisiblePosition endPosition = endOfWord(startPosition);

    return (id) [self textMarkerRangeFromVisiblePositions:startPosition andEndPos:endPosition];
}

- (id)doAXLeftLineTextMarkerRangeForTextMarker: (WebCoreTextMarker *) textMarker
{
    VisiblePosition visiblePos = [self visiblePositionForTextMarker:textMarker];
    if (visiblePos.isNull())
        return nil;
    
    // make a caret selection for the position before marker position (to make sure
    // we move off of a line start)
    VisiblePosition prevVisiblePos = visiblePos.previous();
    if (prevVisiblePos.isNull())
        return nil;
    
    VisiblePosition startPosition = startOfLine(prevVisiblePos);
    VisiblePosition endPosition = endOfLine(prevVisiblePos);
    return (id) [self textMarkerRangeFromVisiblePositions:startPosition andEndPos:endPosition];
}

- (id)doAXRightLineTextMarkerRangeForTextMarker: (WebCoreTextMarker *) textMarker
{
    VisiblePosition visiblePos = [self visiblePositionForTextMarker:textMarker];
    if (visiblePos.isNull())
        return nil;
    
    // make sure we move off of a line end
    VisiblePosition nextVisiblePos = visiblePos.next();
    if (nextVisiblePos.isNull())
        return nil;
        
    VisiblePosition startPosition = startOfLine(nextVisiblePos);
    VisiblePosition endPosition = endOfLine(nextVisiblePos);
    return (id) [self textMarkerRangeFromVisiblePositions:startPosition andEndPos:endPosition];
}

- (id)doAXSentenceTextMarkerRangeForTextMarker: (WebCoreTextMarker *) textMarker
{
    // NOTE: BUG FO 2 IMPLEMENT (currently returns incorrect answer)
    // Related? <rdar://problem/3927736> Text selection broken in 8A336
    VisiblePosition visiblePos = [self visiblePositionForTextMarker:textMarker];
    VisiblePosition startPosition = startOfSentence(visiblePos);
    VisiblePosition endPosition = endOfSentence(startPosition);

    return (id) [self textMarkerRangeFromVisiblePositions:startPosition andEndPos:endPosition];
}

- (id)doAXParagraphTextMarkerRangeForTextMarker: (WebCoreTextMarker *) textMarker
{
    VisiblePosition visiblePos = [self visiblePositionForTextMarker:textMarker];
    VisiblePosition startPosition = startOfParagraph(visiblePos);
    VisiblePosition endPosition = endOfParagraph(startPosition);

    return (id) [self textMarkerRangeFromVisiblePositions:startPosition andEndPos:endPosition];
}

- (id)doAXNextWordEndTextMarkerForTextMarker: (WebCoreTextMarker *) textMarker
{
    VisiblePosition visiblePos = [self visiblePositionForTextMarker:textMarker];
    if (visiblePos.isNull())
        return nil;

    // make sure we move off of a word end
    visiblePos = visiblePos.next();
    if (visiblePos.isNull())
        return nil;

    VisiblePosition endPosition = endOfWord(visiblePos, khtml::LeftWordIfOnBoundary);
    return (id) [self textMarkerForVisiblePosition:endPosition];
}

- (id)doAXPreviousWordStartTextMarkerForTextMarker: (WebCoreTextMarker *) textMarker
{
    VisiblePosition visiblePos = [self visiblePositionForTextMarker:textMarker];
    if (visiblePos.isNull())
        return nil;

    // make sure we move off of a word start
    visiblePos = visiblePos.previous();
    if (visiblePos.isNull())
        return nil;
    
    VisiblePosition startPosition = startOfWord(visiblePos, khtml::RightWordIfOnBoundary);
    return (id) [self textMarkerForVisiblePosition:startPosition];
}

- (id)doAXNextLineEndTextMarkerForTextMarker: (WebCoreTextMarker *) textMarker
{
    VisiblePosition visiblePos = [self visiblePositionForTextMarker:textMarker];
    if (visiblePos.isNull())
        return nil;
    
    // to make sure we move off of a line end
    VisiblePosition nextVisiblePos = visiblePos.next();
    if (nextVisiblePos.isNull())
        return nil;
        
    VisiblePosition endPosition = endOfLine(nextVisiblePos);
    return (id) [self textMarkerForVisiblePosition: endPosition];
}

- (id)doAXPreviousLineStartTextMarkerForTextMarker: (WebCoreTextMarker *) textMarker
{
    VisiblePosition visiblePos = [self visiblePositionForTextMarker:textMarker];
    if (visiblePos.isNull())
        return nil;
    
    // make sure we move off of a line start
    VisiblePosition prevVisiblePos = visiblePos.previous();
    if (prevVisiblePos.isNull())
        return nil;
        
    VisiblePosition startPosition = startOfLine(prevVisiblePos);
    return (id) [self textMarkerForVisiblePosition: startPosition];
}

- (id)doAXNextSentenceEndTextMarkerForTextMarker: (WebCoreTextMarker *) textMarker
{
    // NOTE: BUG FO 2 IMPLEMENT (currently returns incorrect answer)
    // Related? <rdar://problem/3927736> Text selection broken in 8A336
    VisiblePosition visiblePos = [self visiblePositionForTextMarker:textMarker];
    if (visiblePos.isNull())
        return nil;
    
    // make sure we move off of a sentence end
    visiblePos = visiblePos.next();
    if (visiblePos.isNull())
        return nil;

    VisiblePosition endPosition = endOfSentence(visiblePos);
    return (id) [self textMarkerForVisiblePosition: endPosition];
}

- (id)doAXPreviousSentenceStartTextMarkerForTextMarker: (WebCoreTextMarker *) textMarker
{
    // NOTE: BUG FO 2 IMPLEMENT (currently returns incorrect answer)
    // Related? <rdar://problem/3927736> Text selection broken in 8A336
    VisiblePosition visiblePos = [self visiblePositionForTextMarker:textMarker];
    if (visiblePos.isNull())
        return nil;

    // make sure we move off of a sentence start
    visiblePos = visiblePos.previous();
    if (visiblePos.isNull())
        return nil;

    VisiblePosition startPosition = startOfSentence(visiblePos);
    return (id) [self textMarkerForVisiblePosition: startPosition];
}

- (id)doAXNextParagraphEndTextMarkerForTextMarker: (WebCoreTextMarker *) textMarker
{
    VisiblePosition visiblePos = [self visiblePositionForTextMarker:textMarker];
    if (visiblePos.isNull())
        return nil;
    
    // make sure we move off of a paragraph end
    visiblePos = visiblePos.next();
    if (visiblePos.isNull())
        return nil;

    VisiblePosition endPosition = endOfParagraph(visiblePos);
    return (id) [self textMarkerForVisiblePosition: endPosition];
}

- (id)doAXPreviousParagraphStartTextMarkerForTextMarker: (WebCoreTextMarker *) textMarker
{
    VisiblePosition visiblePos = [self visiblePositionForTextMarker:textMarker];
    if (visiblePos.isNull())
        return nil;

    // make sure we move off of a paragraph start
    visiblePos = visiblePos.previous();
    if (visiblePos.isNull())
        return nil;

    VisiblePosition startPosition = startOfParagraph(visiblePos);
    return (id) [self textMarkerForVisiblePosition: startPosition];
}

- (id)doAXLengthForTextMarkerRange: (WebCoreTextMarkerRange *) textMarkerRange
{
    // NOTE: BUG Multi-byte support
    CFStringRef string = (CFStringRef) [self doAXStringForTextMarkerRange: textMarkerRange];
    if (!string)
        return nil;

    return [NSNumber numberWithInt:CFStringGetLength(string)];
}

- (id)accessibilityAttributeValue:(NSString *)attribute forParameter:(id)parameter
{
    WebCoreTextMarker *     textMarker = nil;
    WebCoreTextMarkerRange *textMarkerRange = nil;
    NSNumber *              number = nil;
    NSArray *               array = nil;
    KWQAccObject *          uiElement = nil;
    NSPoint                 point = {0.0, 0.0};
    bool                    pointSet = false;
    
    // basic parameter validation
    if (!m_renderer || !attribute || !parameter)
        return nil;

    // common parameter type check/casting.  Nil checks in handlers catch wrong type case.
    // NOTE: This assumes nil is not a valid parameter, because it is indistinguishable from
    // a parameter of the wrong type.
    if ([[WebCoreViewFactory sharedFactory] objectIsTextMarker:parameter])
        textMarker = (WebCoreTextMarker *) parameter;

    else if ([[WebCoreViewFactory sharedFactory] objectIsTextMarkerRange:parameter])
        textMarkerRange = (WebCoreTextMarkerRange *) parameter;

    else if ([parameter isKindOfClass:[KWQAccObject self]])
        uiElement = (KWQAccObject *) parameter;

    else if ([parameter isKindOfClass:[NSNumber self]])
        number = parameter;

    else if ([parameter isKindOfClass:[NSArray self]])
        array = parameter;
    
    else if ([parameter isKindOfClass:[NSValue self]] && strcmp([(NSValue *)parameter objCType], @encode(NSPoint)) == 0) {
        pointSet = true;
        point = [(NSValue *)parameter pointValue];

    } else {
        // got a parameter of a type we never use
        // NOTE: No ASSERT_NOT_REACHED because this can happen accidentally 
        // while using accesstool (e.g.), forcing you to start over
        return nil;
    }
  
    // dispatch
    if ([attribute isEqualToString: @"AXUIElementForTextMarker"])
        return [self doAXUIElementForTextMarker: textMarker];

    if ([attribute isEqualToString: @"AXTextMarkerRangeForUIElement"])
        return [self doAXTextMarkerRangeForUIElement: uiElement];

    if ([attribute isEqualToString: @"AXLineForTextMarker"])
        return [self doAXLineForTextMarker: textMarker];

    if ([attribute isEqualToString: @"AXTextMarkerRangeForLine"])
        return [self doAXTextMarkerRangeForLine: number];

    if ([attribute isEqualToString: @"AXStringForTextMarkerRange"])
        return [self doAXStringForTextMarkerRange: textMarkerRange];

    if ([attribute isEqualToString: @"AXTextMarkerForPosition"])
        return pointSet ? [self doAXTextMarkerForPosition: point] : nil;

    if ([attribute isEqualToString: @"AXBoundsForTextMarkerRange"])
        return [self doAXBoundsForTextMarkerRange: textMarkerRange];

    if ([attribute isEqualToString: @"AXAttributedStringForTextMarkerRange"])
        return [self doAXAttributedStringForTextMarkerRange: textMarkerRange];

    if ([attribute isEqualToString: @"AXTextMarkerRangeForUnorderedTextMarkers"])
        return [self doAXTextMarkerRangeForUnorderedTextMarkers: array];

    if ([attribute isEqualToString: @"AXNextTextMarkerForTextMarker"])
        return [self doAXNextTextMarkerForTextMarker: textMarker];

    if ([attribute isEqualToString: @"AXPreviousTextMarkerForTextMarker"])
        return [self doAXPreviousTextMarkerForTextMarker: textMarker];

    if ([attribute isEqualToString: @"AXLeftWordTextMarkerRangeForTextMarker"])
        return [self doAXLeftWordTextMarkerRangeForTextMarker: textMarker];

    if ([attribute isEqualToString: @"AXRightWordTextMarkerRangeForTextMarker"])
        return [self doAXRightWordTextMarkerRangeForTextMarker: textMarker];

    if ([attribute isEqualToString: @"AXLeftLineTextMarkerRangeForTextMarker"])
        return [self doAXLeftLineTextMarkerRangeForTextMarker: textMarker];

    if ([attribute isEqualToString: @"AXRightLineTextMarkerRangeForTextMarker"])
        return [self doAXRightLineTextMarkerRangeForTextMarker: textMarker];

    if ([attribute isEqualToString: @"AXSentenceTextMarkerRangeForTextMarker"])
        return [self doAXSentenceTextMarkerRangeForTextMarker: textMarker];

    if ([attribute isEqualToString: @"AXParagraphTextMarkerRangeForTextMarker"])
        return [self doAXParagraphTextMarkerRangeForTextMarker: textMarker];

    if ([attribute isEqualToString: @"AXNextWordEndTextMarkerForTextMarker"])
        return [self doAXNextWordEndTextMarkerForTextMarker: textMarker];

    if ([attribute isEqualToString: @"AXPreviousWordStartTextMarkerForTextMarker"])
        return [self doAXPreviousWordStartTextMarkerForTextMarker: textMarker];
        
    if ([attribute isEqualToString: @"AXNextLineEndTextMarkerForTextMarker"])
        return [self doAXNextLineEndTextMarkerForTextMarker: textMarker];
        
    if ([attribute isEqualToString: @"AXPreviousLineStartTextMarkerForTextMarker"])
        return [self doAXPreviousLineStartTextMarkerForTextMarker: textMarker];
        
    if ([attribute isEqualToString: @"AXNextSentenceEndTextMarkerForTextMarker"])
        return [self doAXNextSentenceEndTextMarkerForTextMarker: textMarker];
        
    if ([attribute isEqualToString: @"AXPreviousSentenceStartTextMarkerForTextMarker"])
        return [self doAXPreviousSentenceStartTextMarkerForTextMarker: textMarker];
        
    if ([attribute isEqualToString: @"AXNextParagraphEndTextMarkerForTextMarker"])
        return [self doAXNextParagraphEndTextMarkerForTextMarker: textMarker];

    if ([attribute isEqualToString: @"AXPreviousParagraphStartTextMarkerForTextMarker"])
        return [self doAXPreviousParagraphStartTextMarkerForTextMarker: textMarker];
        
    if ([attribute isEqualToString: @"AXLengthForTextMarkerRange"])
        return [self doAXLengthForTextMarkerRange: textMarkerRange];

    return nil;
}

- (id)accessibilityHitTest:(NSPoint)point
{
    if (!m_renderer)
        return NSAccessibilityUnignoredAncestor(self);
    
    RenderObject::NodeInfo nodeInfo(true, true);
    m_renderer->layer()->hitTest(nodeInfo, (int)point.x, (int)point.y);
    if (!nodeInfo.innerNode())
        return NSAccessibilityUnignoredAncestor(self);
    RenderObject* obj = nodeInfo.innerNode()->renderer();
    if (!obj)
        return NSAccessibilityUnignoredAncestor(self);
    
    KWQAccObject * accObject = obj->document()->getAccObjectCache()->accObject(obj);
    return NSAccessibilityUnignoredAncestor(accObject);
}

- (RenderObject *) rendererForView:(NSView *)view
{
    // check for WebCore NSView that lets us find its widget
    KHTMLPart* docPart = m_renderer->document()->part();
    if (docPart) {
        DOMElement *domElement = [KWQ(docPart)->bridge() elementForView:view];
        if (domElement)
            return [domElement _elementImpl]->renderer();
    }
    
    // check for WebKit NSView that lets us find its bridge
    WebCoreBridge *bridge = nil;
    if ([view conformsToProtocol:@protocol(WebCoreBridgeHolder)]) {
        NSView<WebCoreBridgeHolder>* bridgeHolder = (NSView<WebCoreBridgeHolder>*)view;
        bridge = [bridgeHolder webCoreBridge];
    }

    KWQKHTMLPart *part = [bridge part];
    if (!part)
        return NULL;
        
    DocumentImpl *document = part->xmlDocImpl();
    if (!document)
        return NULL;
        
    NodeImpl* node = document->ownerElement();
    if (!node)
        return NULL;

    return node->renderer();
}

// _accessibilityParentForSubview is called by AppKit when moving up the tree
// we override it so that we can return our KWQAccObject parent of an AppKit AX object
- (id)_accessibilityParentForSubview:(NSView *)subview
{   
    RenderObject *renderer = [self rendererForView:subview];
    if (!renderer)
        return nil;
        
    KWQAccObject* obj = renderer->document()->getAccObjectCache()->accObject(renderer);
    return [obj parentObjectUnignored];
}

- (id)accessibilityFocusedUIElement
{
    // NOTE: BUG support nested WebAreas
    NodeImpl *focusNode = m_renderer->document()->focusNode();
    if (!focusNode || !focusNode->renderer())
        return nil;

    KWQAccObject* obj = focusNode->renderer()->document()->getAccObjectCache()->accObject(focusNode->renderer());
    
    // the HTML element, for example, is focusable but has an AX object that is ignored
    if ([obj accessibilityIsIgnored])
        obj = [obj parentObjectUnignored];
    
    return obj;
}

- (BOOL)accessibilityIsAttributeSettable:(NSString*)attributeName
{
    if ([attributeName isEqualToString: @"AXSelectedTextMarkerRangeAttribute"])
        return YES;
    if ([attributeName isEqualToString: NSAccessibilityFocusedAttribute]) {
        if ([[self role] isEqualToString:@"AXLink"] ||
            (([[self role] isEqualToString:NSAccessibilityCheckBoxRole] ||
             ([[self role] isEqualToString:NSAccessibilityRadioButtonRole])) &&
              m_renderer->element()->isEnabled()))
            return YES;
    }

    return NO;
}

- (void)doSetAXSelectedTextMarkerRange: (WebCoreTextMarkerRange *)textMarkerRange
{
    // extract the start and end VisiblePosition
    VisiblePosition startVisiblePosition = [self visiblePositionForStartOfTextMarkerRange: textMarkerRange];
    if (startVisiblePosition.isNull())
        return;
    
    VisiblePosition endVisiblePosition = [self visiblePositionForEndOfTextMarkerRange: textMarkerRange];
    if (endVisiblePosition.isNull())
        return;
    
    // make selection and tell the document to use it
    // NOTE: BUG support nested WebAreas
    Selection sel = Selection(startVisiblePosition, endVisiblePosition);
    [self topDocument]->part()->setSelection(sel);
}

- (void)accessibilitySetValue:(id)value forAttribute:(NSString *)attributeName;
{
    WebCoreTextMarkerRange *textMarkerRange = nil;
    NSNumber *              number = nil;

    // decode the parameter
    if ([[WebCoreViewFactory sharedFactory] objectIsTextMarkerRange:value])
        textMarkerRange = (WebCoreTextMarkerRange *) value;

    else if ([value isKindOfClass:[NSNumber self]])
        number = value;
    
    // handle the command
    if ([attributeName isEqualToString: @"AXSelectedTextMarkerRange"]) {
        ASSERT(textMarkerRange);
        [self doSetAXSelectedTextMarkerRange:textMarkerRange];
        
    } else if ([attributeName isEqualToString: NSAccessibilityFocusedAttribute]) {
        ASSERT(number);
        if ([[self role] isEqualToString:@"AXLink"] ||
            (([[self role] isEqualToString:NSAccessibilityCheckBoxRole] ||
             ([[self role] isEqualToString:NSAccessibilityRadioButtonRole])) &&
             m_renderer->element()->isEnabled())) {
            if ([number intValue] != 0)
                m_renderer->document()->setFocusNode(m_renderer->element());
            else
                m_renderer->document()->setFocusNode(0);
        }
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
