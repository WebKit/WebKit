/*
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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

#import "config.h"
#import "WebCoreAXObject.h"

#import "DOMInternal.h"
#import "ColorMac.h"
#import "Document.h"
#import "EventNames.h"
#import "FocusController.h"
#import "FontData.h"
#import "Frame.h"
#import "FrameLoader.h"
#import "FrameView.h"
#import "HTMLAreaElement.h"
#import "HTMLCollection.h"
#import "HTMLFrameElementBase.h"
#import "HTMLImageElement.h"
#import "HTMLInputElement.h"
#import "HTMLLabelElement.h"
#import "HTMLMapElement.h"
#import "HTMLNames.h"
#import "HTMLSelectElement.h"
#import "HTMLTextAreaElement.h"
#import "HitTestRequest.h"
#import "HitTestResult.h"
#import "LocalizedStrings.h"
#import "NodeList.h"
#import "Page.h"
#import "RenderImage.h"
#import "RenderListMarker.h"
#import "RenderMenuList.h"
#import "RenderTextControl.h"
#import "RenderTheme.h"
#import "RenderView.h"
#import "RenderWidget.h"
#import "SelectionController.h"
#import "TextIterator.h"
#import "WebCoreFrameBridge.h"
#import "WebCoreFrameView.h"
#import "WebCoreObjCExtras.h"
#import "WebCoreViewFactory.h"
#import "htmlediting.h"
#import "kjs_html.h"
#import "visible_units.h"
#include <mach-o/dyld.h>

using namespace WebCore;
using namespace EventNames;
using namespace HTMLNames;

@interface WebCoreAXObject (PrivateWebCoreAXObject)
// forward declarations as needed
- (WebCoreTextMarker*)textMarkerForIndex: (NSNumber*) index lastIndexOK: (BOOL)lastIndexOK;
- (id)doAXLineForTextMarker: (WebCoreTextMarker* ) textMarker;
@end

@implementation WebCoreAXObject

#ifndef BUILDING_ON_TIGER
+ (void)initialize
{
    WebCoreObjCFinalizeOnMainThread(self);
}
#endif

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
    // Send unregisterUniqueIdForUIElement unconditionally because if it is
    // ever accidently not done (via other bugs in our AX implementation) you
    // end up with a crash like <rdar://problem/4273149>.  It is safe and not
    // expensive to send even if the object is not registered.
    [[WebCoreViewFactory sharedFactory] unregisterUniqueIdForUIElement:self];
    [m_data release];
    m_data = 0;
    [self removeAXObjectID];
    m_renderer = 0;
    [self clearChildren];
}

- (void)dealloc
{
    [self detach];
    [super dealloc];
}

- (void)finalize
{
    [self detach];
    [super finalize];
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

-(HTMLAnchorElement*)anchorElement
{
    // return already-known anchor for image areas
    if (m_areaElement)
        return m_areaElement;

    // search up the render tree for a RenderObject with a DOM node.  Defer to an earlier continuation, though.
    RenderObject* currRenderer;
    for (currRenderer = m_renderer; currRenderer && !currRenderer->element(); currRenderer = currRenderer->parent()) {
        if (currRenderer->continuation())
            return [currRenderer->document()->axObjectCache()->get(currRenderer->continuation()) anchorElement];
    }
    
    // bail of none found
    if (!currRenderer)
        return 0;
    
    // search up the DOM tree for an anchor element
    // NOTE: this assumes that any non-image with an anchor is an HTMLAnchorElement
    Node* elt = currRenderer->element();
    for ( ; elt; elt = elt->parentNode()) {
        if (elt->isLink() && elt->renderer() && !elt->renderer()->isImage())
            return static_cast<HTMLAnchorElement*>(elt);
    }
  
    return 0;
}

-(BOOL)isImageButton
{
    return m_renderer->isImage() && m_renderer->element() && m_renderer->element()->hasTagName(inputTag);
}

-(Element*)mouseButtonListener
{
    // FIXME: Do the continuation search like anchorElement does
    for (EventTargetNode* elt = static_cast<EventTargetNode*>(m_renderer->element()); elt; elt = static_cast<EventTargetNode*>(elt->parentNode())) {
        if (elt->getHTMLEventListener(clickEvent) || elt->getHTMLEventListener(mousedownEvent) || elt->getHTMLEventListener(mouseupEvent))
            return static_cast<Element*>(elt);
    }

    return 0;
}

-(Element*)actionElement
{
    if (m_renderer->element() && m_renderer->element()->hasTagName(inputTag)) {
        HTMLInputElement* input = static_cast<HTMLInputElement*>(m_renderer->element());
        if (!input->disabled() && (input->inputType() == HTMLInputElement::CHECKBOX ||
                                   input->inputType() == HTMLInputElement::RADIO ||
                                   input->isTextButton()))
            return input;
    }

    if ([self isImageButton] || m_renderer->isMenuList())
        return static_cast<Element*>(m_renderer->element());

    Element* elt = [self anchorElement];
    if (!elt)
        elt = [self mouseButtonListener];

    return elt;
}

-(WebCoreAXObject*)firstChild
{
    if (!m_renderer || !m_renderer->firstChild())
        return nil;

    return m_renderer->document()->axObjectCache()->get(m_renderer->firstChild());
}

-(WebCoreAXObject*)lastChild
{
    if (!m_renderer || !m_renderer->lastChild())
        return nil;

    return m_renderer->document()->axObjectCache()->get(m_renderer->lastChild());
}

-(WebCoreAXObject*)previousSibling
{
    if (!m_renderer || !m_renderer->previousSibling())
        return nil;

    return m_renderer->document()->axObjectCache()->get(m_renderer->previousSibling());
}

-(WebCoreAXObject*)nextSibling
{
    if (!m_renderer || !m_renderer->nextSibling())
        return nil;

    return m_renderer->document()->axObjectCache()->get(m_renderer->nextSibling());
}

-(WebCoreAXObject*)parentObject
{
    if (m_areaElement)
        return m_renderer->document()->axObjectCache()->get(m_renderer);

    if (!m_renderer || !m_renderer->parent())
        return nil;

    return m_renderer->document()->axObjectCache()->get(m_renderer->parent());
}

-(WebCoreAXObject*)parentObjectUnignored
{
    WebCoreAXObject* obj = [self parentObject];
    if ([obj accessibilityIsIgnored])
        return [obj parentObjectUnignored];

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
        Widget* widget = renderWidget->widget();
        if (widget) {
            NSArray* childArr = [(widget->getOuterView()) accessibilityAttributeValue: NSAccessibilityChildrenAttribute];
            [array addObjectsFromArray: childArr];
            return;
        }
    }
    
    // add all unignored acc children
    for (WebCoreAXObject* obj = [self firstChild]; obj; obj = [obj nextSibling]) {
        if ([obj accessibilityIsIgnored])
            [obj addChildrenToArray: array];
        else
            [array addObject: obj];
    }
    
    // for a RenderImage, add the <area> elements as individual accessibility objects
    if (m_renderer->isImage() && !m_areaElement) {
        HTMLMapElement* map = static_cast<RenderImage*>(m_renderer)->imageMap();
        if (map) {
            for (Node* current = map->firstChild(); current; current = current->traverseNextNode(map)) {
                // add an <area> element for this child if it has a link
                // NOTE: can't cache these because they all have the same renderer, which is the cache key, right?
                // plus there may be little reason to since they are being added to the handy array
                if (current->isLink()) {
                    WebCoreAXObject* obj = [[[WebCoreAXObject alloc] initWithRenderer: m_renderer] autorelease];
                    obj->m_areaElement = static_cast<HTMLAreaElement*>(current);
                    [array addObject: obj];
                }
            }
        }
    }
}

-(BOOL)isWebArea
{
    return m_renderer->isRenderView();
}

-(BOOL)isAnchor
{
    return m_areaElement || (!m_renderer->isImage() && m_renderer->element() && m_renderer->element()->isLink());
} 

-(BOOL)isTextControl
{
    return m_renderer->isTextField() || m_renderer->isTextArea();
}

static bool isPasswordFieldElement(Node* node)
{
    if (!node || !node->hasTagName(inputTag))
        return false;
    
    HTMLInputElement* input = static_cast<HTMLInputElement*>(node);
    return input->inputType() == HTMLInputElement::PASSWORD;
}

-(BOOL)isPasswordField
{
    return m_renderer && isPasswordFieldElement(m_renderer->element());
}

-(BOOL)isAttachment
{
    // widgets are the replaced elements that we represent to AX as attachments
    BOOL result = m_renderer && m_renderer->isWidget();
    
    // assert that a widget is a replaced element that is not an image
    ASSERT(!result || (m_renderer->isReplaced() && !m_renderer->isImage()));
    return result;
}

-(NSView*)attachmentView
{
    ASSERT(m_renderer->isReplaced() && m_renderer->isWidget() && !m_renderer->isImage());

    RenderWidget* renderWidget = static_cast<RenderWidget*>(m_renderer);
    Widget* widget = renderWidget->widget();
    if (widget)
         return widget->getView();

    return nil;
}

static int blockquoteLevel(RenderObject* renderer)
{
    int result = 0;
    for (Node* node = renderer->element(); node; node = node->parent()) {
        if (node->hasTagName(blockquoteTag))
            result += 1;
    }
    
    return result;
}

static int headingLevel(RenderObject* renderer)
{
    if (!renderer->isBlockFlow())
        return 0;
        
    Node* node = renderer->element();
    if (!node)
        return 0;
    
    if (node->hasTagName(h1Tag))
        return 1;
    
    if (node->hasTagName(h2Tag))
        return 2;
    
    if (node->hasTagName(h3Tag))
        return 3;
    
    if (node->hasTagName(h4Tag))
        return 4;
    
    if (node->hasTagName(h5Tag))
        return 5;
    
    if (node->hasTagName(h6Tag))
        return 6;

    return 0;
}

-(int)headingLevel
{
    return headingLevel(m_renderer);
}

-(BOOL)isHeading
{
    return [self headingLevel] != 0;
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
    if ([self isWebArea])
        return @"AXWebArea";
    
    if (m_renderer->isTextField())
        return NSAccessibilityTextFieldRole;
    
    if (m_renderer->isTextArea())
        return NSAccessibilityTextAreaRole;
    
    if (m_renderer->element() && m_renderer->element()->hasTagName(inputTag)) {
        HTMLInputElement* input = static_cast<HTMLInputElement*>(m_renderer->element());
        if (input->inputType() == HTMLInputElement::CHECKBOX)
            return NSAccessibilityCheckBoxRole;
        if (input->inputType() == HTMLInputElement::RADIO)
            return NSAccessibilityRadioButtonRole;
        if (input->isTextButton())
            return NSAccessibilityButtonRole;
    }
    
    if (m_renderer->isMenuList())
        return NSAccessibilityPopUpButtonRole;

    if ([self isHeading])
        return @"AXHeading";
        
    if (m_renderer->isBlockFlow())
        return NSAccessibilityGroupRole;
    if ([self isAttachment])
        return [[self attachmentView] accessibilityAttributeValue:NSAccessibilityRoleAttribute];

    return NSAccessibilityUnknownRole;
}

-(NSString*)subrole
{
    if ([self isPasswordField])
        return NSAccessibilitySecureTextFieldSubrole;
    
    if ([self isAttachment]) {
        NSView* attachmentView = [self attachmentView];
        if ([[attachmentView accessibilityAttributeNames] containsObject:NSAccessibilitySubroleAttribute]) {
            return [attachmentView accessibilityAttributeValue:NSAccessibilitySubroleAttribute];
        }
    }

    return nil;
}

-(NSString*)roleDescription
{
    if (!m_renderer)
        return nil;

    // attachments have the AXImage role, but a different subrole
    if ([self isAttachment])
        return [[self attachmentView] accessibilityAttributeValue:NSAccessibilityRoleDescriptionAttribute];
    
    // FIXME 3447564: It would be better to call some AppKit API to get these strings
    // (which would be the best way to localize them)
    
    NSString* role = [self role];
    if ([role isEqualToString:NSAccessibilityButtonRole])
        return NSAccessibilityRoleDescription(NSAccessibilityButtonRole, [self subrole]);
    
    if ([role isEqualToString:NSAccessibilityPopUpButtonRole])
        return NSAccessibilityRoleDescription(NSAccessibilityPopUpButtonRole, [self subrole]);
   
    if ([role isEqualToString:NSAccessibilityStaticTextRole])
        return NSAccessibilityRoleDescription(NSAccessibilityStaticTextRole, [self subrole]);

    if ([role isEqualToString:NSAccessibilityImageRole])
        return NSAccessibilityRoleDescription(NSAccessibilityImageRole, [self subrole]);
    
    if ([role isEqualToString:NSAccessibilityGroupRole])
        return NSAccessibilityRoleDescription(NSAccessibilityGroupRole, [self subrole]);
    
    if ([role isEqualToString:NSAccessibilityCheckBoxRole])
        return NSAccessibilityRoleDescription(NSAccessibilityCheckBoxRole, [self subrole]);
        
    if ([role isEqualToString:NSAccessibilityRadioButtonRole])
        return NSAccessibilityRoleDescription(NSAccessibilityRadioButtonRole, [self subrole]);
        
    if ([role isEqualToString:NSAccessibilityTextFieldRole])
        return NSAccessibilityRoleDescription(NSAccessibilityTextFieldRole, [self subrole]);

    if ([role isEqualToString:NSAccessibilityTextAreaRole])
        return NSAccessibilityRoleDescription(NSAccessibilityTextAreaRole, [self subrole]);

    if ([role isEqualToString:@"AXWebArea"])
        return AXWebAreaText();
    
    if ([role isEqualToString:@"AXLink"])
        return AXLinkText();
    
    if ([role isEqualToString:@"AXListMarker"])
        return AXListMarkerText();
    
    if ([role isEqualToString:@"AXImageMap"])
        return AXImageMapText();

    if ([role isEqualToString:@"AXHeading"])
        return AXHeadingText();
    
    return NSAccessibilityRoleDescription(NSAccessibilityUnknownRole, nil);
}

-(NSString*)helpText
{
    if (!m_renderer)
        return nil;

    if (m_areaElement) {
        const AtomicString& summary = static_cast<Element*>(m_areaElement)->getAttribute(summaryAttr);
        if (!summary.isEmpty())
            return summary;
        const AtomicString& title = static_cast<Element*>(m_areaElement)->getAttribute(titleAttr);
        if (!title.isEmpty())
            return title;
    }

    for (RenderObject* curr = m_renderer; curr; curr = curr->parent()) {
        if (curr->element() && curr->element()->isHTMLElement()) {
            const AtomicString& summary = static_cast<Element*>(curr->element())->getAttribute(summaryAttr);
            if (!summary.isEmpty())
                return summary;
            const AtomicString& title = static_cast<Element*>(curr->element())->getAttribute(titleAttr);
            if (!title.isEmpty())
                return title;
        }
    }

    return nil;
}

-(NSString*)textUnderElement
{
    if (!m_renderer)
        return nil;

    Node* e = m_renderer->element();
    Document* d = m_renderer->document();
    if (e && d) {
        Frame* p = d->frame();
        if (p) {
            // catch stale WebCoreAXObject (see <rdar://problem/3960196>)
            if (p->document() != d)
                return nil;
            return plainText(rangeOfContents(e).get()).getNSString();
        }
    }

    // return nil for anonymous text because it is non-trivial to get
    // the actual text and, so far, that is not needed
    return nil;
}

-(id)value
{
    if (!m_renderer || m_areaElement || [self isPasswordField])
        return nil;

    if (m_renderer->isText())
        return [self textUnderElement];
    
    if (m_renderer->isMenuList())
        return static_cast<RenderMenuList*>(m_renderer)->text();
    
    if (m_renderer->isListMarker())
        return static_cast<RenderListMarker*>(m_renderer)->text();

    if ([self isWebArea]) {
        if (m_renderer->document()->frame())
            return nil;
        
        // FIXME: should use startOfDocument and endOfDocument (or rangeForDocument?) here
        VisiblePosition startVisiblePosition = m_renderer->positionForCoordinates(0, 0);
        VisiblePosition endVisiblePosition   = m_renderer->positionForCoordinates(INT_MAX, INT_MAX);
        if (startVisiblePosition.isNull() || endVisiblePosition.isNull())
            return nil;
            
        return plainText(makeRange(startVisiblePosition, endVisiblePosition).get()).getNSString();
    }
    
    if ([self isAttachment]) {
        NSView* attachmentView = [self attachmentView];
        if ([[attachmentView accessibilityAttributeNames] containsObject:NSAccessibilityValueAttribute]) 
            return [attachmentView accessibilityAttributeValue:NSAccessibilityValueAttribute];
        return nil;
    }
    
    if ([self isHeading])
        return [NSNumber numberWithInt:[self headingLevel]];
        
    if ([self isTextControl])
        return (NSString*)(static_cast<RenderTextControl*>(m_renderer)->text());

    if (m_renderer->element() && m_renderer->element()->hasTagName(inputTag)) {
        HTMLInputElement* input = static_cast<HTMLInputElement*>(m_renderer->element());

        // Checkboxes return their state as an integer. 0 for off, 1 for on.
        if (input->inputType() == HTMLInputElement::CHECKBOX ||
            input->inputType() == HTMLInputElement::RADIO)
            return [NSNumber numberWithInt:input->checked()];
    }

    // FIXME: We might need to implement a value here for more types
    // FIXME: It would be better not to advertise a value at all for the types for which we don't implement one;
    // this would require subclassing or making accessibilityAttributeNames do something other than return a
    // single static array.
    return nil;
}

static HTMLLabelElement* labelForElement(Element* element)
{
    RefPtr<NodeList> list = element->document()->getElementsByTagName("label");
    unsigned len = list->length();
    for (unsigned i = 0; i < len; i++) {
        HTMLLabelElement* label = static_cast<HTMLLabelElement*>(list->item(i));
        if (label->correspondingControl() == element)
            return label;
    }
    
    return 0;
}

-(NSString*)title
{
    if (!m_renderer || m_areaElement || !m_renderer->element())
        return nil;
    
    if (m_renderer->element()->hasTagName(buttonTag))
        return [self textUnderElement];
        
    if (m_renderer->element()->hasTagName(inputTag)) {
        HTMLInputElement* input = static_cast<HTMLInputElement*>(m_renderer->element());
        if (input->isTextButton())
            return input->value();

        HTMLLabelElement* label = labelForElement(input);
        if (label)
            return label->innerText();
    }
    
    if (m_renderer->element()->isLink() || [self isHeading])
        return [self textUnderElement];
        
    if ([self isAttachment]) {
        NSView* attachmentView = [self attachmentView];
        if ([[attachmentView accessibilityAttributeNames] containsObject:NSAccessibilityTitleAttribute]) 
            return [attachmentView accessibilityAttributeValue:NSAccessibilityTitleAttribute];
    }
    
    return nil;
}

- (NSString*)accessibilityDescription
{
    if (!m_renderer || m_areaElement)
        return nil;
    
    if (m_renderer->isImage()) {
        if (m_renderer->element() && m_renderer->element()->isHTMLElement()) {
            const AtomicString& alt = static_cast<Element*>(m_renderer->element())->getAttribute(altAttr);
            if (alt.isEmpty())
                return nil;
            return alt;
        }
    } else if ([self isAttachment]) {
        NSView* attachmentView = [self attachmentView];
        if ([[attachmentView accessibilityAttributeNames] containsObject:NSAccessibilityDescriptionAttribute])
            return [attachmentView accessibilityAttributeValue:NSAccessibilityDescriptionAttribute];
    }

    if ([self isWebArea]) {
        Document *document = m_renderer->document();
        Node* owner = document->ownerElement();
        if (owner) {
            if (owner->hasTagName(frameTag) || owner->hasTagName(iframeTag)) {
                HTMLFrameElementBase* frameElement = static_cast<HTMLFrameElementBase*>(owner);
                return frameElement->name();
            } else if (owner->isHTMLElement()) {
                return static_cast<Element*>(owner)->getAttribute(nameAttr);
            }
        } else {
            owner = document->body();
            if (owner && owner->isHTMLElement()) 
                return static_cast<Element*>(owner)->getAttribute(nameAttr);
        } 
    }
    
    return nil;
}

static IntRect boundingBoxRect(RenderObject* obj)
{
    IntRect rect;
    if (obj) {
        if (obj->isInlineContinuation())
            obj = obj->element()->renderer();
        Vector<IntRect> rects;
        int x, y;
        obj->absolutePosition(x, y);
        obj->absoluteRects(rects, x, y);
        const size_t n = rects.size();
        for (size_t i = 0; i < n; ++i) {
            IntRect r = rects[i];
            if (!r.isEmpty()) {
                if (obj->style()->hasAppearance())
                    theme()->adjustRepaintRect(obj, r);
                rect.unite(r);
            }
        }
    }
    return rect;
}

-(NSValue*)position
{
    IntRect rect = m_areaElement ? m_areaElement->getRect(m_renderer) : boundingBoxRect(m_renderer);
    
    // The Cocoa accessibility API wants the lower-left corner.
    NSPoint point = NSMakePoint(rect.x(), rect.bottom());
    if (m_renderer && m_renderer->view() && m_renderer->view()->frameView()) {
        NSView* view = m_renderer->view()->frameView()->getDocumentView();
        point = [[view window] convertBaseToScreen: [view convertPoint: point toView:nil]];
    }

    return [NSValue valueWithPoint: point];
}

-(NSValue*)size
{
    IntRect rect = m_areaElement ? m_areaElement->getRect(m_renderer) : boundingBoxRect(m_renderer);
    return [NSValue valueWithSize: NSMakeSize(rect.width(), rect.height())];
}

// accessibilityShouldUseUniqueId is an AppKit method we override so that
// objects will be given a unique ID, and therefore allow AppKit to know when they
// become obsolete (e.g. when the user navigates to a new web page, making this one
// unrendered but not deallocated because it is in the back/forward cache).
// It is important to call NSAccessibilityUnregisterUniqueIdForUIElement in the
// appropriate place (e.g. dealloc) to remove these non-retained references from
// AppKit's id mapping tables. We do this in detach by calling unregisterUniqueIdForUIElement.
//
// Registering an object is also required for observing notifications. Only registered objects can be observed.
- (BOOL)accessibilityShouldUseUniqueId {
    if (!m_renderer)
        return NO;
    
    if ([self isWebArea])
        return YES;

    if ([self isTextControl])
        return YES;

    return NO;
}

-(BOOL)accessibilityIsIgnored
{
    // ignore invisible element
    if (!m_renderer || m_renderer->style()->visibility() != VISIBLE)
        return YES;

    // ignore popup menu items because AppKit does
    for (RenderObject* parent = m_renderer->parent(); parent; parent = parent->parent()) {
        if (parent->isMenuList())
            return YES;
    }
    
    // NOTE: BRs always have text boxes now, so the text box check here can be removed
    if (m_renderer->isText())
        return m_renderer->isBR() || !static_cast<RenderText*>(m_renderer)->firstTextBox();
    
    // delegate to the attachment
    if ([self isAttachment])
        return [[self attachmentView] accessibilityIsIgnored];
        
    if (m_areaElement || (m_renderer->element() && m_renderer->element()->isLink()))
        return NO;

    // all controls are accessible
    if (m_renderer->element() && m_renderer->element()->isControl())
        return NO;

    if (m_renderer->isBlockFlow() && m_renderer->childrenInline())
        return !static_cast<RenderBlock*>(m_renderer)->firstLineBox() && ![self mouseButtonListener];

    // ignore images seemingly used as spacers
    if (m_renderer->isImage()) {
        // informal standard is to ignore images with zero-length alt strings
        Element* elt = static_cast<Element*>(m_renderer->element());
        if (elt) {
            const AtomicString& alt = elt->getAttribute(altAttr);
            if (alt.isEmpty() && !alt.isNull())
                return YES;
        }
        
        // check for one-dimensional image
        if (m_renderer->height() <= 1 || m_renderer->width() <= 1)
            return YES;
        
        // check whether rendered image was stretched from one-dimensional file image
        RenderImage* image = static_cast<RenderImage*>(m_renderer);
        if (image->cachedImage()) {
            IntSize imageSize = image->cachedImage()->imageSize();
            return (imageSize.height() <= 1 || imageSize.width() <= 1);
        }
        
        return NO;
    }
    
    return (!m_renderer->isListMarker() && ![self isWebArea]);
}

- (NSArray*)accessibilityAttributeNames
{
    if ([self isAttachment])
        return [[self attachmentView] accessibilityAttributeNames];
        
    static NSArray* attributes = nil;
    static NSArray* anchorAttrs = nil;
    static NSArray* webAreaAttrs = nil;
    static NSArray* textAttrs = nil;
    NSMutableArray* tempArray;
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
            @"AXVisited",
            nil];
    }
    if (anchorAttrs == nil) {
        tempArray = [[NSMutableArray alloc] initWithArray:attributes];
        [tempArray addObject: NSAccessibilityURLAttribute];
        anchorAttrs = [[NSArray alloc] initWithArray:tempArray];
        [tempArray release];
    }
    if (webAreaAttrs == nil) {
        tempArray = [[NSMutableArray alloc] initWithArray:attributes];
        [tempArray addObject: @"AXLinkUIElements"];
        [tempArray addObject: @"AXLoaded"];
        [tempArray addObject: @"AXLayoutCount"];
        webAreaAttrs = [[NSArray alloc] initWithArray:tempArray];
        [tempArray release];
    }
    if (textAttrs == nil) {
        tempArray = [[NSMutableArray alloc] initWithArray:attributes];
        [tempArray addObject: NSAccessibilityNumberOfCharactersAttribute];
        [tempArray addObject: NSAccessibilitySelectedTextAttribute];
        [tempArray addObject: NSAccessibilitySelectedTextRangeAttribute];
        [tempArray addObject: NSAccessibilityVisibleCharacterRangeAttribute];
        [tempArray addObject: NSAccessibilityInsertionPointLineNumberAttribute];
        textAttrs = [[NSArray alloc] initWithArray:tempArray];
        [tempArray release];
    }
    
    if (!m_renderer || [self isPasswordField])
        return attributes;

    if ([self isWebArea])
        return webAreaAttrs;
    
    if ([self isTextControl])
        return textAttrs;

    if ([self isAnchor] || m_renderer->isImage())
        return anchorAttrs;

    return attributes;
}

- (NSArray*)accessibilityActionNames
{
    static NSArray* actions = nil;
    
    if (actions == nil) {
        if ([self actionElement]) 
            actions = [[NSArray alloc] initWithObjects: NSAccessibilityPressAction, nil];
        else if ([self isAttachment])
            actions = [[[self attachmentView] accessibilityActionNames] retain];
    }

    return actions;
}

- (NSString*)accessibilityActionDescription:(NSString*)action
{
    // we have no custom actions
    return NSAccessibilityActionDescription(action);
}

- (void)accessibilityPerformAction:(NSString*)action
{
    if ([action isEqualToString:NSAccessibilityPressAction]) {
        if ([self isAttachment]) {
            [[self attachmentView] accessibilityPerformAction:action];
            return;
        }
            
        Element* actionElement = [self actionElement];
        if (!actionElement)
            return;
        if (Frame* f = actionElement->document()->frame())
            f->loader()->resetMultipleFormSubmissionProtection();
        actionElement->accessKeyAction(true);
    }
}

- (WebCoreTextMarkerRange*) textMarkerRangeFromMarkers: (WebCoreTextMarker*) textMarker1 andEndMarker:(WebCoreTextMarker*) textMarker2
{
    return [[WebCoreViewFactory sharedFactory] textMarkerRangeWithStart:textMarker1 end:textMarker2];
}

- (WebCoreTextMarker*) textMarkerForVisiblePosition: (VisiblePosition)visiblePos
{
    if (visiblePos.isNull())
        return nil;
    
    if (isPasswordFieldElement(visiblePos.deepEquivalent().node()))
        return nil;
        
    return m_renderer->document()->axObjectCache()->textMarkerForVisiblePosition(visiblePos);
}

- (VisiblePosition) visiblePositionForTextMarker: (WebCoreTextMarker*)textMarker
{
    return m_renderer->document()->axObjectCache()->visiblePositionForTextMarker(textMarker);
}

- (VisiblePosition) visiblePositionForStartOfTextMarkerRange: (WebCoreTextMarkerRange*)textMarkerRange
{
    return [self visiblePositionForTextMarker:[[WebCoreViewFactory sharedFactory] startOfTextMarkerRange:textMarkerRange]];
}

- (VisiblePosition) visiblePositionForEndOfTextMarkerRange: (WebCoreTextMarkerRange*) textMarkerRange
{
    return [self visiblePositionForTextMarker:[[WebCoreViewFactory sharedFactory] endOfTextMarkerRange:textMarkerRange]];
}

- (WebCoreTextMarkerRange*) textMarkerRangeFromVisiblePositions: (VisiblePosition) startPosition andEndPos: (VisiblePosition) endPosition
{
    WebCoreTextMarker* startTextMarker = [self textMarkerForVisiblePosition: startPosition];
    WebCoreTextMarker* endTextMarker   = [self textMarkerForVisiblePosition: endPosition];
    return [self textMarkerRangeFromMarkers: startTextMarker andEndMarker:endTextMarker];
}

- (WebCoreTextMarkerRange*)textMarkerRange
{
    if (!m_renderer)
        return nil;
    
    // construct VisiblePositions for start and end
    Node* node = m_renderer->element();
    VisiblePosition visiblePos1 = VisiblePosition(node, 0, VP_DEFAULT_AFFINITY);
    VisiblePosition visiblePos2 = VisiblePosition(node, maxDeepOffset(node), VP_DEFAULT_AFFINITY);
    
    // the VisiblePositions are equal for nodes like buttons, so adjust for that
    if (visiblePos1 == visiblePos2) {
        visiblePos2 = visiblePos2.next();
        if (visiblePos2.isNull())
            visiblePos2 = visiblePos1;
    }
    
    WebCoreTextMarker* startTextMarker = [self textMarkerForVisiblePosition: visiblePos1];
    WebCoreTextMarker* endTextMarker   = [self textMarkerForVisiblePosition: visiblePos2];
    return [self textMarkerRangeFromMarkers: startTextMarker andEndMarker:endTextMarker];
}

- (RenderObject*)topRenderer
{
    return m_renderer->document()->topDocument()->renderer();
}

- (FrameView*)frameView
{
    return m_renderer->document()->view();
}

- (FrameView*)topFrameView
{
    return m_renderer->document()->topDocument()->renderer()->view()->frameView();
}

- (id)accessibilityAttributeValue:(NSString*)attributeName
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
        if (m_renderer->isRenderView() && m_renderer->view() && m_renderer->view()->frameView())
            return m_renderer->view()->frameView()->getView();
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

    if ([self isWebArea]) {
        if ([attributeName isEqualToString: @"AXLinkUIElements"]) {
            NSMutableArray* links = [NSMutableArray arrayWithCapacity: 32];
            RefPtr<HTMLCollection> coll = m_renderer->document()->links();
            Node* curr = coll->firstItem();
            while (curr) {
                RenderObject* obj = curr->renderer();
                if (obj) {
                    WebCoreAXObject* axobj = obj->document()->axObjectCache()->get(obj);
                    ASSERT([[axobj role] isEqualToString:@"AXLink"]);
                    if (![axobj accessibilityIsIgnored])
                        [links addObject: axobj];
                }
                curr = coll->nextItem();
            }
            return links;
        }
        if ([attributeName isEqualToString: @"AXLoaded"])
            return [NSNumber numberWithBool: (!m_renderer->document()->tokenizer())];
        if ([attributeName isEqualToString: @"AXLayoutCount"])
            return [NSNumber numberWithInt: (static_cast<RenderView*>(m_renderer)->frameView()->layoutCount())];
    }
    
    if ([self isTextControl]) {
        RenderTextControl* textControl = static_cast<RenderTextControl*>(m_renderer);
        if ([attributeName isEqualToString: NSAccessibilityNumberOfCharactersAttribute])
            return [self isPasswordField] ? nil : [NSNumber numberWithUnsignedInt: textControl->text().length()];
        if ([attributeName isEqualToString: NSAccessibilitySelectedTextAttribute]) {
            if ([self isPasswordField])
                return nil;
            NSString* text = textControl->text();
            return [text substringWithRange: NSMakeRange(textControl->selectionStart(), textControl->selectionEnd() - textControl->selectionStart())];
        }
        if ([attributeName isEqualToString: NSAccessibilitySelectedTextRangeAttribute])
            return [self isPasswordField] ? nil : [NSValue valueWithRange: NSMakeRange(textControl->selectionStart(), textControl->selectionEnd() - textControl->selectionStart())];
        // TODO: Get actual visible range. <rdar://problem/4712101>
        if ([attributeName isEqualToString: NSAccessibilityVisibleCharacterRangeAttribute])
            return [self isPasswordField] ? nil : [NSValue valueWithRange: NSMakeRange(0, textControl->text().length())];
        if ([attributeName isEqualToString: NSAccessibilityInsertionPointLineNumberAttribute]) {
            if ([self isPasswordField] || textControl->selectionStart() != textControl->selectionEnd())
                return nil;
            NSNumber* index = [NSNumber numberWithInt: textControl->selectionStart()];
            return [self doAXLineForTextMarker: [self textMarkerForIndex: index lastIndexOK: YES]];
        }
    }
    
    if ([attributeName isEqualToString: NSAccessibilityURLAttribute]) {
        if ([self isAnchor]) {
            HTMLAnchorElement* anchor = [self anchorElement];
            if (anchor) {
                DeprecatedString s = anchor->getAttribute(hrefAttr).deprecatedString();
                if (!s.isNull()) {
                    s = anchor->document()->completeURL(s);
                    return KURL(s).getNSURL();
                }
            }
        }
        else if (m_renderer->isImage() && m_renderer->element() && m_renderer->element()->hasTagName(imgTag)) {
            DeprecatedString src = static_cast<HTMLImageElement*>(m_renderer->element())->src().deprecatedString();
            if (!src.isNull()) 
                return KURL(src).getNSURL();
        }
        return nil;
    }

    if ([attributeName isEqualToString: @"AXVisited"])
        return [NSNumber numberWithBool: m_renderer->style()->pseudoState() == PseudoVisited];
    
    if ([attributeName isEqualToString: NSAccessibilityTitleAttribute])
        return [self title];
    
    if ([attributeName isEqualToString: NSAccessibilityDescriptionAttribute])
        return [self accessibilityDescription];
    
    if ([attributeName isEqualToString: NSAccessibilityValueAttribute])
        return [self value];

    if ([attributeName isEqualToString: NSAccessibilityHelpAttribute])
        return [self helpText];
    
    if ([attributeName isEqualToString: NSAccessibilityFocusedAttribute])
        return [NSNumber numberWithBool: (m_renderer->element() && m_renderer->document()->focusedNode() == m_renderer->element())];

    if ([attributeName isEqualToString: NSAccessibilityEnabledAttribute])
        return [NSNumber numberWithBool: m_renderer->element() ? m_renderer->element()->isEnabled() : YES];
    
    if ([attributeName isEqualToString: NSAccessibilitySizeAttribute])
        return [self size];

    if ([attributeName isEqualToString: NSAccessibilityPositionAttribute])
        return [self position];

    if ([attributeName isEqualToString: NSAccessibilityWindowAttribute]) {
        if (m_renderer && m_renderer->view() && m_renderer->view()->frameView())
            return [m_renderer->view()->frameView()->getView() window];

        return nil;
    }
    
    if ([attributeName isEqualToString: @"AXSelectedTextMarkerRange"]) {
        // get the selection from the document
        Selection selection = [self frameView]->frame()->selectionController()->selection();
        if (selection.isNone())
            return nil;

        return (id) [self textMarkerRangeFromVisiblePositions:selection.visibleStart() andEndPos:selection.visibleEnd()];        
    }
    
    if ([attributeName isEqualToString: @"AXStartTextMarker"])
        return (id) [self textMarkerForVisiblePosition: startOfDocument(m_renderer->document())];

    if ([attributeName isEqualToString: @"AXEndTextMarker"])
        return (id) [self textMarkerForVisiblePosition: endOfDocument(m_renderer->document())];

    return nil;
}

- (NSArray* )accessibilityParameterizedAttributeNames
{
    if ([self isAttachment]) 
        return nil;
        
    static NSArray* paramAttrs = nil;
    static NSArray* textParamAttrs = nil;
    if (paramAttrs == nil) {
        paramAttrs = [[NSArray alloc] initWithObjects:
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
        @"AXStyleTextMarkerRangeForTextMarker",
        @"AXLengthForTextMarkerRange",
        nil];
    }

    if (textParamAttrs == nil) {
        NSMutableArray* tempArray = [[NSMutableArray alloc] initWithArray:paramAttrs];
        [tempArray addObject: (NSString*)kAXLineForIndexParameterizedAttribute];
        [tempArray addObject: (NSString*)kAXRangeForLineParameterizedAttribute];
        [tempArray addObject: (NSString*)kAXStringForRangeParameterizedAttribute];
        [tempArray addObject: (NSString*)kAXRangeForPositionParameterizedAttribute];
        [tempArray addObject: (NSString*)kAXRangeForIndexParameterizedAttribute];
        [tempArray addObject: (NSString*)kAXBoundsForRangeParameterizedAttribute];
        [tempArray addObject: (NSString*)kAXRTFForRangeParameterizedAttribute];
        [tempArray addObject: (NSString*)kAXAttributedStringForRangeParameterizedAttribute];
        [tempArray addObject: (NSString*)kAXStyleRangeForIndexParameterizedAttribute];
        textParamAttrs = [[NSArray alloc] initWithArray:tempArray];
        [tempArray release];
    }
    
    if ([self isPasswordField])
        return [NSArray array];
    
    if (!m_renderer)
        return paramAttrs;

    if ([self isTextControl])
        return textParamAttrs;

    return paramAttrs;
}

- (id)doAXUIElementForTextMarker: (WebCoreTextMarker* ) textMarker
{
    VisiblePosition visiblePos = [self visiblePositionForTextMarker:textMarker];
    if (visiblePos.isNull())
        return nil;

    RenderObject* obj = visiblePos.deepEquivalent().node()->renderer();
    if (!obj)
        return nil;
    
    return obj->document()->axObjectCache()->get(obj);
}

- (id)doAXTextMarkerRangeForUIElement: (id) uiElement
{
    return (id)[uiElement textMarkerRange];
}

- (id)doAXLineForTextMarker: (WebCoreTextMarker* ) textMarker
{
    unsigned int    lineCount = 0;
    VisiblePosition savedVisiblePos;
    VisiblePosition visiblePos = [self visiblePositionForTextMarker:textMarker];
    if (visiblePos.isNull())
        return nil;

    // move up until we get to the top
    // NOTE: BUG This only takes us to the top of the rootEditableElement, not the top of the
    // top document.
    while (visiblePos.isNotNull() && !(inSameLine(visiblePos, savedVisiblePos))) {
        lineCount += 1;
        savedVisiblePos = visiblePos;
        visiblePos = previousLinePosition(visiblePos, 0);
    }
    
    return [NSNumber numberWithUnsignedInt:(lineCount - 1)];
}

- (id)doAXTextMarkerRangeForLine: (NSNumber*) lineNumber
{
    unsigned lineCount = [lineNumber unsignedIntValue];
    if (lineCount == 0 || !m_renderer)
        return nil;
    
    // iterate over the lines
    // NOTE: BUG this is wrong when lineNumber is lineCount+1,  because nextLinePosition takes you to the
    // last offset of the last line
    VisiblePosition visiblePos = m_renderer->document()->renderer()->positionForCoordinates(0, 0);
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
    SelectionController selectionController;
    selectionController.setSelection(Selection(visiblePos));
    selectionController.modify(SelectionController::EXTEND, SelectionController::RIGHT, LineBoundary);

    // return a marker range for the selection start to end
    VisiblePosition startPosition = selectionController.selection().visibleStart();
    VisiblePosition endPosition = selectionController.selection().visibleEnd();
    return (id) [self textMarkerRangeFromVisiblePositions:startPosition andEndPos:endPosition];
}

static NSString *nsStringForReplacedNode(Node* replacedNode)
{
    // we should always be given a rendered node and a replaced node, but be safe
    // replaced nodes are either attachments (widgets) or images
    if (!replacedNode || !replacedNode->renderer() || !replacedNode->renderer()->isReplaced() || replacedNode->isTextNode()) {
        ASSERT_NOT_REACHED();
        return nil;
    }

    // create an AX object, but skip it if it is not supposed to be seen
    WebCoreAXObject* obj = replacedNode->renderer()->document()->axObjectCache()->get(replacedNode->renderer());
    if ([obj accessibilityIsIgnored])
        return nil;
    
    // use the attachmentCharacter to represent the replaced node
    const UniChar attachmentChar = NSAttachmentCharacter;
    return [NSString stringWithCharacters:&attachmentChar length:1];
}

- (id)doAXStringForTextMarkerRange: (WebCoreTextMarkerRange*) textMarkerRange
{
    // extract the start and end VisiblePosition
    VisiblePosition startVisiblePosition = [self visiblePositionForStartOfTextMarkerRange: textMarkerRange];
    if (startVisiblePosition.isNull())
        return nil;
    
    VisiblePosition endVisiblePosition = [self visiblePositionForEndOfTextMarkerRange: textMarkerRange];
    if (endVisiblePosition.isNull())
        return nil;
    
    NSMutableString* resultString = [[[NSMutableString alloc] init] autorelease];
    TextIterator it(makeRange(startVisiblePosition, endVisiblePosition).get());
    while (!it.atEnd()) {
        // non-zero length means textual node, zero length means replaced node (AKA "attachments" in AX)
        if (it.length() != 0) {
            [resultString appendString:[NSString stringWithCharacters:it.characters() length:it.length()]];
        } else {
            // locate the node and starting offset for this replaced range
            int exception = 0;
            Node* node = it.range()->startContainer(exception);
            ASSERT(node == it.range()->endContainer(exception));
            int offset = it.range()->startOffset(exception);
            NSString* attachmentString = nsStringForReplacedNode(node->childNode(offset));
            
            // append the replacement string
            if (attachmentString)
                [resultString appendString:attachmentString];
        }
        it.advance();
    }
    
    return [resultString length] > 0 ? resultString : nil;
}

- (id)doAXTextMarkerForPosition: (NSPoint) point
{
    // convert absolute point to view coordinates
    FrameView* frameView = [self topFrameView];
    NSView* view = frameView->getDocumentView();
    RenderObject* renderer = [self topRenderer];
    Node* innerNode = 0;
    
    // locate the node containing the point
    IntPoint pointResult;
    while (1) {
        // ask the document layer to hitTest
        NSPoint windowCoord = [[view window] convertScreenToBase: point];
        IntPoint ourpoint([view convertPoint:windowCoord fromView:nil]);

        HitTestRequest request(true, true);
        HitTestResult result(ourpoint);
        renderer->layer()->hitTest(request, result);
        innerNode = result.innerNode();
        if (!innerNode || !innerNode->renderer())
            return nil;

        pointResult = result.localPoint();

        // done if hit something other than a widget
        renderer = innerNode->renderer();
        if (!renderer->isWidget())
            break;

        // descend into widget (FRAME, IFRAME, OBJECT...)
        Widget* widget = static_cast<RenderWidget*>(renderer)->widget();
        if (!widget || !widget->isFrameView())
            break;
        Frame* frame = static_cast<FrameView*>(widget)->frame();
        if (!frame)
            break;
        Document* document = frame->document();
        if (!document)
            break;
        renderer = document->renderer();
        frameView = static_cast<FrameView*>(widget);
        view = frameView->getDocumentView();
    }
    
    // get position within the node
    VisiblePosition pos = innerNode->renderer()->positionForPoint(pointResult);
    return (id) [self textMarkerForVisiblePosition:pos];
}

- (id)doAXBoundsForTextMarkerRange: (WebCoreTextMarkerRange*) textMarkerRange
{
    // extract the start and end VisiblePosition
    VisiblePosition startVisiblePosition = [self visiblePositionForStartOfTextMarkerRange: textMarkerRange];
    if (startVisiblePosition.isNull())
        return nil;
    
    VisiblePosition endVisiblePosition = [self visiblePositionForEndOfTextMarkerRange: textMarkerRange];
    if (endVisiblePosition.isNull())
        return nil;

    IntRect rect1 = startVisiblePosition.caretRect();
    IntRect rect2 = endVisiblePosition.caretRect();

    // readjust for position at the edge of a line.  This is to exclude line rect that doesn't need to be accounted in the range bounds 
    if (rect2.y() != rect1.y()) {
        VisiblePosition endOfFirstLine = endOfLine(startVisiblePosition);
        if (startVisiblePosition == endOfFirstLine) {
            startVisiblePosition.setAffinity(DOWNSTREAM);
            rect1 = startVisiblePosition.caretRect();
        }
        if (endVisiblePosition == endOfFirstLine) {
            endVisiblePosition.setAffinity(UPSTREAM);
            rect2 = endVisiblePosition.caretRect();
        }
    }

    IntRect ourrect = rect1;
    ourrect.unite(rect2);

    // try to use the document view from the first position, so that nested WebAreas work,
    // but fall back to the top level doc if we do not find it easily
    RenderObject* renderer = startVisiblePosition.deepEquivalent().node()->renderer();
    FrameView* frameView = renderer ? renderer->document()->view() : 0;
    if (!frameView)
        frameView = [self frameView];
    NSView *view = frameView->getView();

    // if the rectangle spans lines and contains multiple text chars, use the range's bounding box intead
    if (rect1.bottom() != rect2.bottom()) {
        RefPtr<Range> dataRange = makeRange(startVisiblePosition, endVisiblePosition);
        IntRect boundingBox = dataRange->boundingBox();
        DeprecatedString rangeString = plainText(dataRange.get());
        if (rangeString.length() > 1 && !boundingBox.isEmpty()) 
            ourrect = boundingBox;
    }
 
    // convert our rectangle to screen coordinates
    NSRect rect = ourrect;
    rect = NSOffsetRect(rect, -frameView->contentsX(), -frameView->contentsY());
    rect = [view convertRect:rect toView:nil];
    rect.origin = [[view window] convertBaseToScreen:rect.origin];

    // return the converted rect
    return [NSValue valueWithRect:rect];
}

static CGColorRef CreateCGColorIfDifferent(NSColor* nsColor, CGColorRef existingColor)
{
    // get color information assuming NSDeviceRGBColorSpace 
    NSColor* rgbColor = [nsColor colorUsingColorSpaceName:NSDeviceRGBColorSpace];
    if (rgbColor == nil)
        rgbColor = [NSColor blackColor];
    CGFloat components[4];
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

static void AXAttributeStringSetColor(NSMutableAttributedString* attrString, NSString* attribute, NSColor* color, NSRange range)
{
    if (color) {
        CGColorRef existingColor = (CGColorRef) [attrString attribute:attribute atIndex:range.location effectiveRange:nil];
        CGColorRef cgColor = CreateCGColorIfDifferent(color, existingColor);
        if (cgColor) {
            [attrString addAttribute:attribute value:(id)cgColor range:range];
            CGColorRelease(cgColor);
        }
    } else
        [attrString removeAttribute:attribute range:range];
}

static void AXAttributeStringSetNumber(NSMutableAttributedString* attrString, NSString* attribute, NSNumber* number, NSRange range)
{
    if (number)
        [attrString addAttribute:attribute value:number range:range];
    else
        [attrString removeAttribute:attribute range:range];
}

static void AXAttributeStringSetFont(NSMutableAttributedString* attrString, NSString* attribute, NSFont* font, NSRange range)
{
    NSDictionary* dict;
    
    if (font) {
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

static void AXAttributeStringSetStyle(NSMutableAttributedString* attrString, RenderObject* renderer, NSRange range)
{
    RenderStyle* style = renderer->style();

    // set basic font info
    AXAttributeStringSetFont(attrString, NSAccessibilityFontTextAttribute, style->font().primaryFont()->getNSFont(), range);

    // set basic colors
    AXAttributeStringSetColor(attrString, NSAccessibilityForegroundColorTextAttribute, nsColor(style->color()), range);
    AXAttributeStringSetColor(attrString, NSAccessibilityBackgroundColorTextAttribute, nsColor(style->backgroundColor()), range);

    // set super/sub scripting
    EVerticalAlign alignment = style->verticalAlign();
    if (alignment == SUB)
        AXAttributeStringSetNumber(attrString, NSAccessibilitySuperscriptTextAttribute, [NSNumber numberWithInt:(-1)], range);
    else if (alignment == SUPER)
        AXAttributeStringSetNumber(attrString, NSAccessibilitySuperscriptTextAttribute, [NSNumber numberWithInt:1], range);
    else
        [attrString removeAttribute:NSAccessibilitySuperscriptTextAttribute range:range];
    
    // set shadow
    if (style->textShadow())
        AXAttributeStringSetNumber(attrString, NSAccessibilityShadowTextAttribute, [NSNumber numberWithBool:YES], range);
    else
        [attrString removeAttribute:NSAccessibilityShadowTextAttribute range:range];
    
    // set underline and strikethrough
    int decor = style->textDecorationsInEffect();
    if ((decor & UNDERLINE) == 0) {
        [attrString removeAttribute:NSAccessibilityUnderlineTextAttribute range:range];
        [attrString removeAttribute:NSAccessibilityUnderlineColorTextAttribute range:range];
    }
    
    if ((decor & LINE_THROUGH) == 0) {
        [attrString removeAttribute:NSAccessibilityStrikethroughTextAttribute range:range];
        [attrString removeAttribute:NSAccessibilityStrikethroughColorTextAttribute range:range];
    }

    if ((decor & (UNDERLINE | LINE_THROUGH)) != 0) {
        // find colors using quirk mode approach (strict mode would use current
        // color for all but the root line box, which would use getTextDecorationColors)
        Color underline, overline, linethrough;
        renderer->getTextDecorationColors(decor, underline, overline, linethrough);
        
        if ((decor & UNDERLINE) != 0) {
            AXAttributeStringSetNumber(attrString, NSAccessibilityUnderlineTextAttribute, [NSNumber numberWithBool:YES], range);
            AXAttributeStringSetColor(attrString, NSAccessibilityUnderlineColorTextAttribute, nsColor(underline), range);
        }

        if ((decor & LINE_THROUGH) != 0) {
            AXAttributeStringSetNumber(attrString, NSAccessibilityStrikethroughTextAttribute, [NSNumber numberWithBool:YES], range);
            AXAttributeStringSetColor(attrString, NSAccessibilityStrikethroughColorTextAttribute, nsColor(linethrough), range);
        }
    }
}

static void AXAttributeStringSetHeadingLevel(NSMutableAttributedString* attrString, RenderObject* renderer, NSRange range)
{
    int parentHeadingLevel = headingLevel(renderer->parent());
    
    if (parentHeadingLevel)
        [attrString addAttribute:@"AXHeadingLevel" value:[NSNumber numberWithInt:parentHeadingLevel] range:range];
    else
        [attrString removeAttribute:@"AXHeadingLevel" range:range];
}

static void AXAttributeStringSetBlockquoteLevel(NSMutableAttributedString* attrString, RenderObject* renderer, NSRange range)
{
    int quoteLevel = blockquoteLevel(renderer);
    
    if (quoteLevel)
        [attrString addAttribute:@"AXBlockQuoteLevel" value:[NSNumber numberWithInt:quoteLevel] range:range];
    else
        [attrString removeAttribute:@"AXBlockQuoteLevel" range:range];
}

static void AXAttributeStringSetElement(NSMutableAttributedString* attrString, NSString* attribute, id element, NSRange range)
{
    if (element) {
        // make a serialiazable AX object
        AXUIElementRef axElement = [[WebCoreViewFactory sharedFactory] AXUIElementForElement:element];
        if (axElement) {
            [attrString addAttribute:attribute value:(id)axElement range:range];
            CFRelease(axElement);
        }
    } else
        [attrString removeAttribute:attribute range:range];
}

static WebCoreAXObject* AXLinkElementForNode (Node* node)
{
    RenderObject* obj = node->renderer();
    if (!obj)
        return nil;

    WebCoreAXObject* axObj = obj->document()->axObjectCache()->get(obj);
    HTMLAnchorElement* anchor = [axObj anchorElement];
    if (!anchor || !anchor->renderer())
        return nil;

    return anchor->renderer()->document()->axObjectCache()->get(anchor->renderer());
}

static void AXAttributeStringSetSpelling(NSMutableAttributedString* attrString, Node* node, int offset, NSRange range)
{
    Vector<DocumentMarker> markers = node->renderer()->document()->markersForNode(node);
    Vector<DocumentMarker>::iterator markerIt = markers.begin();

    unsigned endOffset = (unsigned)offset + range.length;
    for ( ; markerIt != markers.end(); markerIt++) {
        DocumentMarker marker = *markerIt;
        
        if (marker.type != DocumentMarker::Spelling)
            continue;
        
        if (marker.endOffset <= (unsigned)offset)
            continue;
        
        if (marker.startOffset > endOffset)
            break;
        
        // add misspelling attribute for the intersection of the marker and the range
        int rStart = range.location + (marker.startOffset - offset);
        int rLength = MIN(marker.endOffset, endOffset) - marker.startOffset;
        NSRange spellRange = NSMakeRange(rStart, rLength);
        AXAttributeStringSetNumber(attrString, NSAccessibilityMisspelledTextAttribute, [NSNumber numberWithBool:YES], spellRange);
        
        if (marker.endOffset > endOffset + 1)
            break;
    }
}

static void AXAttributedStringAppendText(NSMutableAttributedString* attrString, Node* node, int offset, const UChar* chars, int length)
{
    // skip invisible text
    if (!node->renderer())
        return;
        
    // easier to calculate the range before appending the string
    NSRange attrStringRange = NSMakeRange([attrString length], length);
    
    // append the string from this node
    [[attrString mutableString] appendString:[NSString stringWithCharacters:chars length:length]];

    // add new attributes and remove irrelevant inherited ones
    // NOTE: color attributes are handled specially because -[NSMutableAttributedString addAttribute: value: range:] does not merge
    // identical colors.  Workaround is to not replace an existing color attribute if it matches what we are adding.  This also means
    // we can not just pre-remove all inherited attributes on the appended string, so we have to remove the irrelevant ones individually.

    // remove inherited attachment from prior AXAttributedStringAppendReplaced
    [attrString removeAttribute:NSAccessibilityAttachmentTextAttribute range:attrStringRange];
    
    // set new attributes
    AXAttributeStringSetStyle(attrString, node->renderer(), attrStringRange);
    AXAttributeStringSetHeadingLevel(attrString, node->renderer(), attrStringRange);
    AXAttributeStringSetBlockquoteLevel(attrString, node->renderer(), attrStringRange);
    AXAttributeStringSetElement(attrString, NSAccessibilityLinkTextAttribute, AXLinkElementForNode(node), attrStringRange);
    
    // do spelling last because it tends to break up the range
    AXAttributeStringSetSpelling(attrString, node, offset, attrStringRange);
}

- (id)doAXAttributedStringForTextMarkerRange: (WebCoreTextMarkerRange*) textMarkerRange
{
    // extract the start and end VisiblePosition
    VisiblePosition startVisiblePosition = [self visiblePositionForStartOfTextMarkerRange: textMarkerRange];
    if (startVisiblePosition.isNull())
        return nil;
    
    VisiblePosition endVisiblePosition = [self visiblePositionForEndOfTextMarkerRange: textMarkerRange];
    if (endVisiblePosition.isNull())
        return nil;
    
    // iterate over the range to build the AX attributed string
    NSMutableAttributedString* attrString = [[NSMutableAttributedString alloc] init];
    TextIterator it(makeRange(startVisiblePosition, endVisiblePosition).get());
    while (!it.atEnd()) {
        // locate the node and starting offset for this range
        int exception = 0;
        Node* node = it.range()->startContainer(exception);
        ASSERT(node == it.range()->endContainer(exception));
        int offset = it.range()->startOffset(exception);
        
        // non-zero length means textual node, zero length means replaced node (AKA "attachments" in AX)
        if (it.length() != 0) {
            AXAttributedStringAppendText(attrString, node, offset, it.characters(), it.length());
        } else {
            Node* replacedNode = node->childNode(offset);
            NSString *attachmentString = nsStringForReplacedNode(replacedNode);
            if (attachmentString) {
                NSRange attrStringRange = NSMakeRange([attrString length], [attachmentString length]);
                
                // append the placeholder string
                [[attrString mutableString] appendString:attachmentString];
                
                // remove all inherited attributes
                [attrString setAttributes:nil range:attrStringRange];
                
                // add the attachment attribute
                WebCoreAXObject* obj = replacedNode->renderer()->document()->axObjectCache()->get(replacedNode->renderer());
                AXAttributeStringSetElement(attrString, NSAccessibilityAttachmentTextAttribute, obj, attrStringRange);
            }
        }
        it.advance();
    }

    return [attrString autorelease];
}

- (id)doAXTextMarkerRangeForUnorderedTextMarkers: (NSArray*) markers
{
    // get and validate the markers
    if ([markers count] < 2)
        return nil;
    
    WebCoreTextMarker* textMarker1 = (WebCoreTextMarker*) [markers objectAtIndex:0];
    WebCoreTextMarker* textMarker2 = (WebCoreTextMarker*) [markers objectAtIndex:1];
    if (![[WebCoreViewFactory sharedFactory] objectIsTextMarker:textMarker1] || ![[WebCoreViewFactory sharedFactory] objectIsTextMarker:textMarker2])
        return nil;
    
    // convert to VisiblePosition
    VisiblePosition visiblePos1 = [self visiblePositionForTextMarker:textMarker1];
    VisiblePosition visiblePos2 = [self visiblePositionForTextMarker:textMarker2];
    if (visiblePos1.isNull() || visiblePos2.isNull())
        return nil;
    
    WebCoreTextMarker* startTextMarker;
    WebCoreTextMarker* endTextMarker;
    bool alreadyInOrder;
    
    // upstream is ordered before downstream for the same position
    if (visiblePos1 == visiblePos2 && visiblePos2.affinity() == UPSTREAM) 
        alreadyInOrder = false;
    
    // use selection order to see if the positions are in order
    else 
        alreadyInOrder = Selection(visiblePos1, visiblePos2).isBaseFirst();
    
    if (alreadyInOrder) {
        startTextMarker = textMarker1;
        endTextMarker = textMarker2;
    } else {
        startTextMarker = textMarker2;
        endTextMarker = textMarker1;
    }
    
    return (id) [self textMarkerRangeFromMarkers: startTextMarker andEndMarker:endTextMarker];
}

- (id)doAXNextTextMarkerForTextMarker: (WebCoreTextMarker*) textMarker
{
    VisiblePosition visiblePos = [self visiblePositionForTextMarker:textMarker];
    VisiblePosition nextVisiblePos = visiblePos.next();
    if (nextVisiblePos.isNull())
        return nil;
    
    return (id) [self textMarkerForVisiblePosition:nextVisiblePos];
}

- (id)doAXPreviousTextMarkerForTextMarker: (WebCoreTextMarker*) textMarker
{
    VisiblePosition visiblePos = [self visiblePositionForTextMarker:textMarker];
    VisiblePosition previousVisiblePos = visiblePos.previous();
    if (previousVisiblePos.isNull())
        return nil;
    
    return (id) [self textMarkerForVisiblePosition:previousVisiblePos];
}

- (id)doAXLeftWordTextMarkerRangeForTextMarker: (WebCoreTextMarker*) textMarker
{
    VisiblePosition visiblePos = [self visiblePositionForTextMarker:textMarker];
    VisiblePosition startPosition = startOfWord(visiblePos, LeftWordIfOnBoundary);
    VisiblePosition endPosition = endOfWord(startPosition);

    return (id) [self textMarkerRangeFromVisiblePositions:startPosition andEndPos:endPosition];
}

- (id)doAXRightWordTextMarkerRangeForTextMarker: (WebCoreTextMarker*) textMarker
{
    VisiblePosition visiblePos = [self visiblePositionForTextMarker:textMarker];
    VisiblePosition startPosition = startOfWord(visiblePos, RightWordIfOnBoundary);
    VisiblePosition endPosition = endOfWord(startPosition);

    return (id) [self textMarkerRangeFromVisiblePositions:startPosition andEndPos:endPosition];
}


static VisiblePosition updateAXLineStartForVisiblePosition(const VisiblePosition& visiblePosition)
{
    // A line in the accessibility sense should include floating objects, such as aligned image, as part of a line.
    // So let's update the position to include that.
    VisiblePosition tempPosition;
    VisiblePosition startPosition = visiblePosition;
    Position p;
    RenderObject* renderer;
    while (true) {
        tempPosition = startPosition.previous();
        if (tempPosition.isNull())
            break;
        p = tempPosition.deepEquivalent();
        if (!p.node())
            break;
        renderer = p.node()->renderer();
        if (!renderer || renderer->inlineBox(p.offset(), tempPosition.affinity()) || (renderer->isRenderBlock() && p.offset() == 0))
            break;
        startPosition = tempPosition;
    }
    
    return startPosition;
}

- (id)doAXLeftLineTextMarkerRangeForTextMarker: (WebCoreTextMarker*) textMarker
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

    // keep searching for a valid line start position.  Unless the textmarker is at the very beginning, there should
    // always be a valid line range.  However, startOfLine will return null for position next to a floating object, 
    // since floating object doesn't really belong to any line.  
    // This check will reposition the marker before the floating object, to ensure we get a line start.
    if (startPosition.isNull()) {
        while (startPosition.isNull() && prevVisiblePos.isNotNull()) {
            prevVisiblePos = prevVisiblePos.previous();
            startPosition = startOfLine(prevVisiblePos);
        }
    } else 
        startPosition = updateAXLineStartForVisiblePosition(startPosition);
    
    VisiblePosition endPosition = endOfLine(prevVisiblePos);
    return (id) [self textMarkerRangeFromVisiblePositions:startPosition andEndPos:endPosition];
}

- (id)doAXRightLineTextMarkerRangeForTextMarker: (WebCoreTextMarker*) textMarker
{
    VisiblePosition visiblePos = [self visiblePositionForTextMarker:textMarker];
    if (visiblePos.isNull())
        return nil;
    
    // make sure we move off of a line end
    VisiblePosition nextVisiblePos = visiblePos.next();
    if (nextVisiblePos.isNull())
        return nil;
        
    VisiblePosition startPosition = startOfLine(nextVisiblePos);
    
    // fetch for a valid line start position
    if (startPosition.isNull() ) {
        startPosition = visiblePos;
        nextVisiblePos = nextVisiblePos.next();
    } else 
        startPosition = updateAXLineStartForVisiblePosition(startPosition);
    
    VisiblePosition endPosition = endOfLine(nextVisiblePos);

    // as long as the position hasn't reached the end of the doc,  keep searching for a valid line end position
    // Unless the textmarker is at the very end, there should always be a valid line range.  However, endOfLine will 
    // return null for position by a floating object, since floating object doesn't really belong to any line.  
    // This check will reposition the marker after the floating object, to ensure we get a line end.
    while (endPosition.isNull() && nextVisiblePos.isNotNull()) {
        nextVisiblePos = nextVisiblePos.next();
        endPosition = endOfLine(nextVisiblePos);
    }
    
    return (id) [self textMarkerRangeFromVisiblePositions:startPosition andEndPos:endPosition];
}

- (id)doAXSentenceTextMarkerRangeForTextMarker: (WebCoreTextMarker*) textMarker
{
    // NOTE: BUG FO 2 IMPLEMENT (currently returns incorrect answer)
    // Related? <rdar://problem/3927736> Text selection broken in 8A336
    VisiblePosition visiblePos = [self visiblePositionForTextMarker:textMarker];
    VisiblePosition startPosition = startOfSentence(visiblePos);
    VisiblePosition endPosition = endOfSentence(startPosition);
    return (id) [self textMarkerRangeFromVisiblePositions:startPosition andEndPos:endPosition];
}

- (id)doAXParagraphTextMarkerRangeForTextMarker: (WebCoreTextMarker*) textMarker
{
    VisiblePosition visiblePos = [self visiblePositionForTextMarker:textMarker];
    VisiblePosition startPosition = startOfParagraph(visiblePos);
    VisiblePosition endPosition = endOfParagraph(startPosition);
    return (id) [self textMarkerRangeFromVisiblePositions:startPosition andEndPos:endPosition];
}

- (id)doAXNextWordEndTextMarkerForTextMarker: (WebCoreTextMarker*) textMarker
{
    VisiblePosition visiblePos = [self visiblePositionForTextMarker:textMarker];
    if (visiblePos.isNull())
        return nil;

    // make sure we move off of a word end
    visiblePos = visiblePos.next();
    if (visiblePos.isNull())
        return nil;

    VisiblePosition endPosition = endOfWord(visiblePos, LeftWordIfOnBoundary);
    return (id) [self textMarkerForVisiblePosition:endPosition];
}

- (id)doAXPreviousWordStartTextMarkerForTextMarker: (WebCoreTextMarker*) textMarker
{
    VisiblePosition visiblePos = [self visiblePositionForTextMarker:textMarker];
    if (visiblePos.isNull())
        return nil;

    // make sure we move off of a word start
    visiblePos = visiblePos.previous();
    if (visiblePos.isNull())
        return nil;
    
    VisiblePosition startPosition = startOfWord(visiblePos, RightWordIfOnBoundary);
    return (id) [self textMarkerForVisiblePosition:startPosition];
}

- (id)doAXNextLineEndTextMarkerForTextMarker: (WebCoreTextMarker*) textMarker
{
    VisiblePosition visiblePos = [self visiblePositionForTextMarker:textMarker];
    if (visiblePos.isNull())
        return nil;
    
    // to make sure we move off of a line end
    VisiblePosition nextVisiblePos = visiblePos.next();
    if (nextVisiblePos.isNull())
        return nil;
        
    VisiblePosition endPosition = endOfLine(nextVisiblePos);

    // as long as the position hasn't reached the end of the doc,  keep searching for a valid line end position
    // There are cases like when the position is next to a floating object that'll return null for end of line. This code will avoid returning null.
    while (endPosition.isNull() && nextVisiblePos.isNotNull()) {
        nextVisiblePos = nextVisiblePos.next();
        endPosition = endOfLine(nextVisiblePos);
    }
    
    return (id) [self textMarkerForVisiblePosition: endPosition];
}

- (id)doAXPreviousLineStartTextMarkerForTextMarker: (WebCoreTextMarker*) textMarker
{
    VisiblePosition visiblePos = [self visiblePositionForTextMarker:textMarker];
    if (visiblePos.isNull())
        return nil;
    
    // make sure we move off of a line start
    VisiblePosition prevVisiblePos = visiblePos.previous();
    if (prevVisiblePos.isNull())
        return nil;
        
    VisiblePosition startPosition = startOfLine(prevVisiblePos);

    // as long as the position hasn't reached the beginning of the doc,  keep searching for a valid line start position
    // There are cases like when the position is next to a floating object that'll return null for start of line. This code will avoid returning null.
    if (startPosition.isNull()) {
        while (startPosition.isNull() && prevVisiblePos.isNotNull()) {
            prevVisiblePos = prevVisiblePos.previous();
            startPosition = startOfLine(prevVisiblePos);
        }
    } else 
        startPosition = updateAXLineStartForVisiblePosition(startPosition);
    
    return (id) [self textMarkerForVisiblePosition: startPosition];
}

- (id)doAXNextSentenceEndTextMarkerForTextMarker: (WebCoreTextMarker*) textMarker
{
    // NOTE: BUG FO 2 IMPLEMENT (currently returns incorrect answer)
    // Related? <rdar://problem/3927736> Text selection broken in 8A336
    VisiblePosition visiblePos = [self visiblePositionForTextMarker:textMarker];
    if (visiblePos.isNull())
        return nil;
    
    // make sure we move off of a sentence end
    VisiblePosition nextVisiblePos = visiblePos.next();
    if (nextVisiblePos.isNull())
        return nil;

    // an empty line is considered a sentence. If it's skipped, then the sentence parser will not
    // see this empty line.  Instead, return the end position of the empty line. 
    VisiblePosition endPosition;
    DeprecatedString lineString = plainText(makeRange(startOfLine(visiblePos), endOfLine(visiblePos)).get());
    if (lineString.isEmpty())
        endPosition = nextVisiblePos;
    else
        endPosition = endOfSentence(nextVisiblePos);
    
    return (id) [self textMarkerForVisiblePosition: endPosition];
}

- (id)doAXPreviousSentenceStartTextMarkerForTextMarker: (WebCoreTextMarker*) textMarker
{
    // NOTE: BUG FO 2 IMPLEMENT (currently returns incorrect answer)
    // Related? <rdar://problem/3927736> Text selection broken in 8A336
    VisiblePosition visiblePos = [self visiblePositionForTextMarker:textMarker];
    if (visiblePos.isNull())
        return nil;

    // make sure we move off of a sentence start
    VisiblePosition previousVisiblePos = visiblePos.previous();
    if (previousVisiblePos.isNull())
        return nil;
    
    // treat empty line as a separate sentence.  
    VisiblePosition startPosition;
    DeprecatedString lineString = plainText(makeRange(startOfLine(previousVisiblePos), endOfLine(previousVisiblePos)).get());
    if (lineString.isEmpty())
        startPosition = previousVisiblePos;
    else
        startPosition = startOfSentence(previousVisiblePos);
        
    return (id) [self textMarkerForVisiblePosition: startPosition];
}

- (id)doAXNextParagraphEndTextMarkerForTextMarker: (WebCoreTextMarker*) textMarker
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

- (id)doAXPreviousParagraphStartTextMarkerForTextMarker: (WebCoreTextMarker*) textMarker
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

static VisiblePosition startOfStyleRange (const VisiblePosition visiblePos)
{
    RenderObject* renderer = visiblePos.deepEquivalent().node()->renderer();
    RenderObject* startRenderer = renderer;
    RenderStyle* style = renderer->style();
    
    // traverse backward by renderer to look for style change
    for (RenderObject* r = renderer->previousInPreOrder(); r; r = r->previousInPreOrder()) {
        // skip non-leaf nodes
        if (r->firstChild())
            continue;
        
        // stop at style change
        if (r->style() != style)
            break;
            
        // remember match
        startRenderer = r;
    }
    
    return VisiblePosition(startRenderer->node(), 0, VP_DEFAULT_AFFINITY);
}

static VisiblePosition endOfStyleRange (const VisiblePosition visiblePos)
{
    RenderObject* renderer = visiblePos.deepEquivalent().node()->renderer();
    RenderObject* endRenderer = renderer;
    RenderStyle* style = renderer->style();

    // traverse forward by renderer to look for style change
    for (RenderObject* r = renderer->nextInPreOrder(); r; r = r->nextInPreOrder()) {
        // skip non-leaf nodes
        if (r->firstChild())
            continue;
        
        // stop at style change
        if (r->style() != style)
            break;
        
        // remember match
        endRenderer = r;
    }
    
    return VisiblePosition(endRenderer->node(), maxDeepOffset(endRenderer->node()), VP_DEFAULT_AFFINITY);
}

- (id)doAXStyleTextMarkerRangeForTextMarker: (WebCoreTextMarker*) textMarker
{
    VisiblePosition visiblePos = [self visiblePositionForTextMarker:textMarker];
    if (visiblePos.isNull())
        return nil;

    VisiblePosition startPosition = startOfStyleRange(visiblePos);
    VisiblePosition endPosition = endOfStyleRange(visiblePos);
    return (id) [self textMarkerRangeFromVisiblePositions:startPosition andEndPos:endPosition];
}

- (id)doAXLengthForTextMarkerRange: (WebCoreTextMarkerRange*) textMarkerRange
{
    // NOTE: BUG Multi-byte support
    CFStringRef string = (CFStringRef) [self doAXStringForTextMarkerRange: textMarkerRange];
    if (!string)
        return nil;

    return [NSNumber numberWithInt:CFStringGetLength(string)];
}

// NOTE: Consider providing this utility method as AX API
- (WebCoreTextMarker*)textMarkerForIndex: (NSNumber*) index lastIndexOK: (BOOL)lastIndexOK
{
    ASSERT(m_renderer->isTextField() || m_renderer->isTextArea());
    RenderTextControl* textControl = static_cast<RenderTextControl*>(m_renderer);
    unsigned int indexValue = [index unsignedIntValue];
    
    // lastIndexOK specifies whether the position after the last character is acceptable
    if (indexValue >= textControl->text().length()) {
        if (!lastIndexOK || indexValue > textControl->text().length())
            return nil;
    }
    VisiblePosition position = textControl->visiblePositionForIndex(indexValue);
    position.setAffinity(DOWNSTREAM);
    return [self textMarkerForVisiblePosition: position];
}

// NOTE: Consider providing this utility method as AX API
- (NSNumber*)indexForTextMarker: (WebCoreTextMarker*) marker
{
    ASSERT(m_renderer->isTextField() || m_renderer->isTextArea());
    RenderTextControl* textControl = static_cast<RenderTextControl*>(m_renderer);

    VisiblePosition position = [self visiblePositionForTextMarker: marker];
    Node* node = position.deepEquivalent().node();
    if (!node)
        return nil;
    
    for (RenderObject* renderer = node->renderer(); renderer && renderer->element(); renderer = renderer->parent()) {
        if (renderer == textControl)
            return [NSNumber numberWithInt: textControl->indexForVisiblePosition(position)];
    }
    
    return nil;
}

// NOTE: Consider providing this utility method as AX API
- (WebCoreTextMarkerRange*)textMarkerRangeForRange: (NSRange) range
{
    ASSERT(m_renderer->isTextField() || m_renderer->isTextArea());
    RenderTextControl* textControl = static_cast<RenderTextControl*>(m_renderer);
    if (range.location + range.length > textControl->text().length())
        return nil;
    
    VisiblePosition startPosition = textControl->visiblePositionForIndex(range.location);
    startPosition.setAffinity(DOWNSTREAM);
    VisiblePosition endPosition = textControl->visiblePositionForIndex(range.location + range.length);
    return [self textMarkerRangeFromVisiblePositions:startPosition andEndPos:endPosition];
}

// NOTE: Consider providing this utility method as AX API
- (NSValue*)rangeForTextMarkerRange: (WebCoreTextMarkerRange*) textMarkerRange
{
    WebCoreTextMarker* textMarker1 = [[WebCoreViewFactory sharedFactory] startOfTextMarkerRange:textMarkerRange];
    WebCoreTextMarker* textMarker2 = [[WebCoreViewFactory sharedFactory] endOfTextMarkerRange:textMarkerRange];
    NSNumber* index1 = [self indexForTextMarker: textMarker1];
    NSNumber* index2 = [self indexForTextMarker: textMarker2];
    if (!index1 || !index2 || [index1 unsignedIntValue] > [index2 unsignedIntValue])
        return nil;
    
    return [NSValue valueWithRange: NSMakeRange([index1 unsignedIntValue], [index2 unsignedIntValue] - [index1 unsignedIntValue])];
}

// Given an indexed character, the line number of the text associated with this accessibility
// object that contains the character.
- (id)doAXLineForIndex: (NSNumber*) index
{
    return [self doAXLineForTextMarker: [self textMarkerForIndex: index lastIndexOK: NO]];
}

// Given a line number, the range of characters of the text associated with this accessibility
// object that contains the line number.
- (id)doAXRangeForLine: (NSNumber*) lineNumber
{
    ASSERT(m_renderer->isTextField() || m_renderer->isTextArea());
    RenderTextControl* textControl = static_cast<RenderTextControl*>(m_renderer);
    
    // iterate to the specified line
    VisiblePosition visiblePos = textControl->visiblePositionForIndex(0);
    VisiblePosition savedVisiblePos;
    for (unsigned lineCount = [lineNumber unsignedIntValue]; lineCount != 0; lineCount -= 1) {
        savedVisiblePos = visiblePos;
        visiblePos = nextLinePosition(visiblePos, 0);
        if (visiblePos.isNull() || visiblePos == savedVisiblePos)
            return nil;
    }
    
    // make a caret selection for the marker position, then extend it to the line
    // NOTE: ignores results of selectionController.modify because it returns false when
    // starting at an empty line.  The resulting selection in that case
    // will be a caret at visiblePos.
    SelectionController selectionController;
    selectionController.setSelection(Selection(visiblePos));
    selectionController.modify(SelectionController::EXTEND, SelectionController::LEFT, LineBoundary);
    selectionController.modify(SelectionController::EXTEND, SelectionController::RIGHT, LineBoundary);
    
    // calculate the indices for the selection start and end
    VisiblePosition startPosition = selectionController.selection().visibleStart();
    VisiblePosition endPosition = selectionController.selection().visibleEnd();
    int index1 = textControl->indexForVisiblePosition(startPosition);
    int index2 = textControl->indexForVisiblePosition(endPosition);
    
    // add one to the end index for a line break not caused by soft line wrap (to match AppKit)
    if (endPosition.affinity() == DOWNSTREAM && endPosition.next().isNotNull())
        index2 += 1;

    // return nil rather than an zero-length range (to match AppKit)
    if (index1 == index2)
        return nil;
    
    return [NSValue valueWithRange: NSMakeRange(index1, index2 - index1)];
}

// A substring of the text associated with this accessibility object that is
// specified by the given character range.
- (id)doAXStringForRange: (NSRange) range
{
    if ([self isPasswordField])
        return nil;
        
    if (range.length == 0)
        return @"";
        
    ASSERT(m_renderer->isTextField() || m_renderer->isTextArea());
    RenderTextControl* textControl = static_cast<RenderTextControl*>(m_renderer);
    String text = textControl->text();
    if (range.location + range.length > text.length())
        return nil;
        
    return text.substring(range.location, range.length);
}

// The composed character range in the text associated with this accessibility object that
// is specified by the given screen coordinates. This parameterized attribute returns the
// complete range of characters (including surrogate pairs of multi-byte glyphs) at the given
// screen coordinates.
// NOTE: This varies from AppKit when the point is below the last line. AppKit returns an
// an error in that case. We return textControl->text().length(), 1. Does this matter?
- (id)doAXRangeForPosition: (NSPoint) point
{
    NSNumber* index = [self indexForTextMarker: [self doAXTextMarkerForPosition: point]];
    if (!index)
        return nil;
        
    return [NSValue valueWithRange: NSMakeRange([index unsignedIntValue], 1)];
}

// The composed character range in the text associated with this accessibility object that
// is specified by the given index value. This parameterized attribute returns the complete
// range of characters (including surrogate pairs of multi-byte glyphs) at the given index.
- (id)doAXRangeForIndex: (NSNumber*) number
{
    ASSERT(m_renderer->isTextField() || m_renderer->isTextArea());
    RenderTextControl* textControl = static_cast<RenderTextControl*>(m_renderer);
    String text = textControl->text();
    if (!text.length() || [number unsignedIntValue] > text.length() - 1)
        return nil;
    
    return [NSValue valueWithRange: NSMakeRange([number unsignedIntValue], 1)];
}

// The bounding rectangle of the text associated with this accessibility object that is
// specified by the given range. This is the bounding rectangle a sighted user would see
// on the display screen, in pixels.
- (id)doAXBoundsForRange: (NSRange) range
{
    return [self doAXBoundsForTextMarkerRange: [self textMarkerRangeForRange:range]];
}

// The CFAttributedStringType representation of the text associated with this accessibility
// object that is specified by the given range.
- (id)doAXAttributedStringForRange: (NSRange) range
{
    return [self doAXAttributedStringForTextMarkerRange: [self textMarkerRangeForRange:range]];
}

// The RTF representation of the text associated with this accessibility object that is
// specified by the given range.
- (id)doAXRTFForRange: (NSRange) range
{
    NSAttributedString* attrString = [self doAXAttributedStringForRange: range];
    return [attrString RTFFromRange: NSMakeRange(0, [attrString length]) documentAttributes: nil];
}

// Given a character index, the range of text associated with this accessibility object
// over which the style in effect at that character index applies.
- (id)doAXStyleRangeForIndex: (NSNumber*) index
{
    WebCoreTextMarkerRange* textMarkerRange = [self doAXStyleTextMarkerRangeForTextMarker: [self textMarkerForIndex: index lastIndexOK: NO]];
    return [self rangeForTextMarkerRange: textMarkerRange];
}

- (id)accessibilityAttributeValue:(NSString*)attribute forParameter:(id)parameter
{
    WebCoreTextMarker*      textMarker = nil;
    WebCoreTextMarkerRange* textMarkerRange = nil;
    NSNumber*               number = nil;
    NSArray*                array = nil;
    WebCoreAXObject*        uiElement = nil;
    NSPoint                 point = NSZeroPoint;
    bool                    pointSet = false;
    NSRange                 range = {0, 0};
    bool                    rangeSet = false;
    
    // basic parameter validation
    if (!m_renderer || !attribute || !parameter)
        return nil;

    // common parameter type check/casting.  Nil checks in handlers catch wrong type case.
    // NOTE: This assumes nil is not a valid parameter, because it is indistinguishable from
    // a parameter of the wrong type.
    if ([[WebCoreViewFactory sharedFactory] objectIsTextMarker:parameter])
        textMarker = (WebCoreTextMarker*) parameter;

    else if ([[WebCoreViewFactory sharedFactory] objectIsTextMarkerRange:parameter])
        textMarkerRange = (WebCoreTextMarkerRange*) parameter;

    else if ([parameter isKindOfClass:[WebCoreAXObject self]])
        uiElement = (WebCoreAXObject*) parameter;

    else if ([parameter isKindOfClass:[NSNumber self]])
        number = parameter;

    else if ([parameter isKindOfClass:[NSArray self]])
        array = parameter;
    
    else if ([parameter isKindOfClass:[NSValue self]] && strcmp([(NSValue*)parameter objCType], @encode(NSPoint)) == 0) {
        pointSet = true;
        point = [(NSValue*)parameter pointValue];

    } else if ([parameter isKindOfClass:[NSValue self]] && strcmp([(NSValue*)parameter objCType], @encode(NSRange)) == 0) {
        rangeSet = true;
        range = [(NSValue*)parameter rangeValue];

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
    
    if ([attribute isEqualToString: @"AXStyleTextMarkerRangeForTextMarker"])
        return [self doAXStyleTextMarkerRangeForTextMarker: textMarker];
    
    if ([attribute isEqualToString: @"AXLengthForTextMarkerRange"])
        return [self doAXLengthForTextMarkerRange: textMarkerRange];
    
    if ([self isTextControl]) {
        if ([attribute isEqualToString: (NSString*)kAXLineForIndexParameterizedAttribute])
            return [self doAXLineForIndex: number];
        
        if ([attribute isEqualToString: (NSString*)kAXRangeForLineParameterizedAttribute])
            return [self doAXRangeForLine: number];
        
        if ([attribute isEqualToString: (NSString*)kAXStringForRangeParameterizedAttribute])
            return rangeSet ? [self doAXStringForRange: range] : nil;
        
        if ([attribute isEqualToString: (NSString*)kAXRangeForPositionParameterizedAttribute])
            return pointSet ? [self doAXRangeForPosition: point] : nil;
        
        if ([attribute isEqualToString: (NSString*)kAXRangeForIndexParameterizedAttribute])
            return [self doAXRangeForIndex: number];
        
        if ([attribute isEqualToString: (NSString*)kAXBoundsForRangeParameterizedAttribute])
            return rangeSet ? [self doAXBoundsForRange: range] : nil;
       
        if ([attribute isEqualToString: (NSString*)kAXRTFForRangeParameterizedAttribute])
            return rangeSet ? [self doAXRTFForRange: range] : nil;
        
        if ([attribute isEqualToString: (NSString*)kAXAttributedStringForRangeParameterizedAttribute])
            return rangeSet ? [self doAXAttributedStringForRange: range] : nil;
        
        if ([attribute isEqualToString: (NSString*)kAXStyleRangeForIndexParameterizedAttribute])
            return [self doAXStyleRangeForIndex: number];
    }
    
    return nil;
}

- (id)accessibilityHitTest:(NSPoint)point
{
    if (!m_renderer)
        return NSAccessibilityUnignoredAncestor(self);
    
    HitTestRequest request(true, true);
    HitTestResult result = HitTestResult(IntPoint(point));
    m_renderer->layer()->hitTest(request, result);
    if (!result.innerNode())
        return NSAccessibilityUnignoredAncestor(self);
    Node* node = result.innerNode()->shadowAncestorNode();
    RenderObject* obj = node->renderer();
    if (!obj)
        return NSAccessibilityUnignoredAncestor(self);
    
    return NSAccessibilityUnignoredAncestor(obj->document()->axObjectCache()->get(obj));
}

- (RenderObject*)rendererForView:(NSView*)view
{
    // check for WebKit NSView that lets us find its bridge
    WebCoreFrameBridge* bridge = nil;
    if ([view conformsToProtocol:@protocol(WebCoreBridgeHolder)]) {
        NSView<WebCoreBridgeHolder>* bridgeHolder = (NSView<WebCoreBridgeHolder>*)view;
        bridge = [bridgeHolder webCoreBridge];
    }

    Frame* frame = [bridge _frame];
    if (!frame)
        return nil;
        
    Document* document = frame->document();
    if (!document)
        return nil;
        
    Node* node = document->ownerElement();
    if (!node)
        return nil;

    return node->renderer();
}

// _accessibilityParentForSubview is called by AppKit when moving up the tree
// we override it so that we can return our WebCoreAXObject parent of an AppKit AX object
- (id)_accessibilityParentForSubview:(NSView*)subview
{   
    RenderObject* renderer = [self rendererForView:subview];
    if (!renderer)
        return nil;
        
    WebCoreAXObject* obj = renderer->document()->axObjectCache()->get(renderer);
    return [obj parentObjectUnignored];
}

- (id)accessibilityFocusedUIElement
{
    // get the focused node in the page
    Page* page = m_renderer->document()->page();
    if (!page)
        return nil;

    Document* focusedDocument = page->focusController()->focusedOrMainFrame()->document();
    Node* focusedNode = focusedDocument->focusedNode();
    if (!focusedNode || !focusedNode->renderer())
        focusedNode = focusedDocument;

    WebCoreAXObject* obj = focusedNode->renderer()->document()->axObjectCache()->get(focusedNode->renderer());
    
    // the HTML element, for example, is focusable but has an AX object that is ignored
    if ([obj accessibilityIsIgnored])
        obj = [obj parentObjectUnignored];
    
    return obj;
}

- (void)doSetAXSelectedTextMarkerRange: (WebCoreTextMarkerRange*)textMarkerRange
{
    // extract the start and end VisiblePosition
    VisiblePosition startVisiblePosition = [self visiblePositionForStartOfTextMarkerRange: textMarkerRange];
    if (startVisiblePosition.isNull())
        return;
    
    VisiblePosition endVisiblePosition = [self visiblePositionForEndOfTextMarkerRange: textMarkerRange];
    if (endVisiblePosition.isNull())
        return;
    
    // make selection and tell the document to use it
    Selection newSelection = Selection(startVisiblePosition, endVisiblePosition);
    m_renderer->document()->frame()->selectionController()->setSelection(newSelection);
}

- (BOOL)canSetFocusAttribute
{
    // NOTE: It would be more accurate to ask the document whether setFocusedNode() would 
    // do anything.  For example, it setFocusedNode() will do nothing if the current focused
    // node will not relinquish the focus.
    if (!m_renderer->element() || !m_renderer->element()->isEnabled())
        return NO;
        
    NSString* role = [self role];
    if ([role isEqualToString:@"AXLink"] ||
        [role isEqualToString:NSAccessibilityTextFieldRole] ||
        [role isEqualToString:NSAccessibilityTextAreaRole] ||
        [role isEqualToString:NSAccessibilityButtonRole] ||
        [role isEqualToString:NSAccessibilityPopUpButtonRole] ||
        [role isEqualToString:NSAccessibilityCheckBoxRole] ||
        [role isEqualToString:NSAccessibilityRadioButtonRole])
        return YES;

    return NO;
}

- (BOOL)canSetValueAttribute
{
    return [self isTextControl];
}

- (BOOL)canSetTextRangeAttributes
{
    return [self isTextControl];
}

- (BOOL)accessibilityIsAttributeSettable:(NSString*)attributeName
{
    if ([attributeName isEqualToString: @"AXSelectedTextMarkerRange"])
        return YES;
        
    if ([attributeName isEqualToString: NSAccessibilityFocusedAttribute])
        return [self canSetFocusAttribute];

    if ([attributeName isEqualToString: NSAccessibilityValueAttribute])
        return [self canSetValueAttribute];

    if ([attributeName isEqualToString: NSAccessibilitySelectedTextAttribute] ||
        [attributeName isEqualToString: NSAccessibilitySelectedTextRangeAttribute] ||
        [attributeName isEqualToString: NSAccessibilityVisibleCharacterRangeAttribute])
        return [self canSetTextRangeAttributes];
    
    return NO;
}

- (void)accessibilitySetValue:(id)value forAttribute:(NSString*)attributeName
{
    WebCoreTextMarkerRange* textMarkerRange = nil;
    NSNumber*               number = nil;
    NSString*               string = nil;
    NSRange                 range = {0, 0};

    // decode the parameter
    if ([[WebCoreViewFactory sharedFactory] objectIsTextMarkerRange:value])
        textMarkerRange = (WebCoreTextMarkerRange*) value;

    else if ([value isKindOfClass:[NSNumber self]])
        number = value;

    else if ([value isKindOfClass:[NSString self]])
        string = value;
    
    else if ([value isKindOfClass:[NSValue self]])
        range = [value rangeValue];
    
    // handle the command
    if ([attributeName isEqualToString: @"AXSelectedTextMarkerRange"]) {
        ASSERT(textMarkerRange);
        [self doSetAXSelectedTextMarkerRange:textMarkerRange];
        
    } else if ([attributeName isEqualToString: NSAccessibilityFocusedAttribute]) {
        ASSERT(number);
        if ([self canSetFocusAttribute] && number) {
            if ([number intValue] == 0)
                m_renderer->document()->setFocusedNode(0);
            else {
                if (m_renderer->element()->isElementNode())
                    static_cast<Element*>(m_renderer->element())->focus();
                else
                    m_renderer->document()->setFocusedNode(m_renderer->element());
            }
        }
    } else if ([attributeName isEqualToString: NSAccessibilityValueAttribute]) {
        if (!string)
            return;
        if (m_renderer->isTextField()) {
            HTMLInputElement* input = static_cast<HTMLInputElement*>(m_renderer->element());
            input->setValue(string);
        } else if (m_renderer->isTextArea()) {
            HTMLTextAreaElement* textArea = static_cast<HTMLTextAreaElement*>(m_renderer->element());
            textArea->setValue(string);
      }
    } else if ([self isTextControl]) {
        RenderTextControl* textControl = static_cast<RenderTextControl*>(m_renderer);
        if ([attributeName isEqualToString: NSAccessibilitySelectedTextAttribute]) {
            // TODO: set selected text (ReplaceSelectionCommand). <rdar://problem/4712125>
        } else if ([attributeName isEqualToString: NSAccessibilitySelectedTextRangeAttribute]) {
            textControl->setSelectionRange(range.location, range.location + range.length);
        } else if ([attributeName isEqualToString: NSAccessibilityVisibleCharacterRangeAttribute]) {
            // TODO: make range visible (scrollRectToVisible).  <rdar://problem/4712101>
        }
    }
}

- (WebCoreAXObject*)observableObject
{
    for (RenderObject* renderer = m_renderer; renderer && renderer->element(); renderer = renderer->parent()) {
        if (renderer->isTextField() || renderer->isTextArea())
            return renderer->document()->axObjectCache()->get(renderer);
    }
    
    return nil;
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

-(AXID)axObjectID
{
    return m_id;
}

-(void)setAXObjectID:(AXID) axObjectID
{
    m_id = axObjectID;
}

- (void)removeAXObjectID
{
    if (!m_id)
        return;
        
    m_renderer->document()->axObjectCache()->removeAXID(self);
}

@end
