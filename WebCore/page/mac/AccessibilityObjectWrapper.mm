/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
 
#import "config.h"
#import "AccessibilityObjectWrapper.h"

#import "AXObjectCache.h"
#import "Frame.h"
#import "HTMLAnchorElement.h"
#import "HTMLImageElement.h"
#import "HTMLInputElement.h"
#import "HTMLTextAreaElement.h"
#import "RenderTextControl.h"
#import "RenderView.h"
#import "RenderWidget.h"
#import "SelectionController.h"
#import "TextIterator.h"
#import "WebCoreFrameView.h"
#import "WebCoreObjCExtras.h"
#import "WebCoreViewFactory.h"
#import "htmlediting.h"
#import "visible_units.h"

using namespace WebCore;
using namespace HTMLNames;

@implementation AccessibilityObjectWrapper

#ifndef BUILDING_ON_TIGER
+ (void)initialize
{
    WebCoreObjCFinalizeOnMainThread(self);
}
#endif

- (id)initWithAccessibilityObject:(AccessibilityObject*)axObject
{
    [super init];

    m_object = axObject;
    return self;
}

- (void)unregisterUniqueIdForUIElement
{
    [[WebCoreViewFactory sharedFactory] unregisterUniqueIdForUIElement:self];
}

- (void)detach
{
    // Send unregisterUniqueIdForUIElement unconditionally because if it is
    // ever accidently not done (via other bugs in our AX implementation) you
    // end up with a crash like <rdar://problem/4273149>.  It is safe and not
    // expensive to send even if the object is not registered.
    [self unregisterUniqueIdForUIElement];
    m_object = 0;
}

- (AccessibilityObject*)accessibilityObject
{
    return m_object;
}

- (NSArray*)accessibilityActionNames
{
    static NSArray* actions = nil;
    
    if (actions == nil) {
        if (m_object->actionElement()) 
            actions = [[NSArray alloc] initWithObjects: NSAccessibilityPressAction, nil];
        else if (m_object->isAttachment())
            actions = [[m_object->attachmentView() accessibilityActionNames] retain];
    }

    return actions;
}

- (NSArray*)accessibilityAttributeNames
{
    if (m_object->isAttachment())
        return [m_object->attachmentView() accessibilityAttributeNames];
        
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
            NSAccessibilityLinkedUIElementsAttribute,
            nil];
    }
    if (anchorAttrs == nil) {
        tempArray = [[NSMutableArray alloc] initWithArray:attributes];
        [tempArray addObject:NSAccessibilityURLAttribute];
        anchorAttrs = [[NSArray alloc] initWithArray:tempArray];
        [tempArray release];
    }
    if (webAreaAttrs == nil) {
        tempArray = [[NSMutableArray alloc] initWithArray:attributes];
        [tempArray addObject:@"AXLinkUIElements"];
        [tempArray addObject:@"AXLoaded"];
        [tempArray addObject:@"AXLayoutCount"];
        webAreaAttrs = [[NSArray alloc] initWithArray:tempArray];
        [tempArray release];
    }
    if (textAttrs == nil) {
        tempArray = [[NSMutableArray alloc] initWithArray:attributes];
        [tempArray addObject:NSAccessibilityNumberOfCharactersAttribute];
        [tempArray addObject:NSAccessibilitySelectedTextAttribute];
        [tempArray addObject:NSAccessibilitySelectedTextRangeAttribute];
        [tempArray addObject:NSAccessibilityVisibleCharacterRangeAttribute];
        [tempArray addObject:NSAccessibilityInsertionPointLineNumberAttribute];
        textAttrs = [[NSArray alloc] initWithArray:tempArray];
        [tempArray release];
    }
    
    if (m_object->isPasswordField())
        return attributes;

    if (m_object->isWebArea())
        return webAreaAttrs;
    
    if (m_object->isTextControl())
        return textAttrs;

    if (m_object->isAnchor() || m_object->isImage())
        return anchorAttrs;

    return attributes;
}

- (VisiblePositionRange)visiblePositionRangeForTextMarkerRange:(WebCoreTextMarkerRange*) textMarkerRange
{
    return VisiblePositionRange(m_object->visiblePositionForStartOfTextMarkerRange(textMarkerRange), m_object->visiblePositionForEndOfTextMarkerRange(textMarkerRange));
}

- (NSArray*)renderWidgetChildren
{
    Widget* widget = m_object->widget();
    if (!widget)
        return nil;
    return [(widget->getOuterView()) accessibilityAttributeValue: NSAccessibilityChildrenAttribute];
}

static NSMutableArray* convertToNSArray(const Vector<RefPtr<AccessibilityObject> >& vector)
{
    unsigned length = vector.size();
    NSMutableArray* array = [NSMutableArray arrayWithCapacity: length];
    for (unsigned i = 0; i < length; ++i)
        [array addObject:vector[i]->wrapper()];
    return array;
}

// FIXME: split up this function in a better way.  
// suggestions: Use a hash table that maps attribute names to function calls,
// or maybe pointers to member functions
- (id)accessibilityAttributeValue:(NSString*)attributeName
{
    RenderObject* renderer = m_object->renderer();
    if (!renderer)
        return nil;

    if ([attributeName isEqualToString: NSAccessibilityRoleAttribute])
        return m_object->role();

    if ([attributeName isEqualToString: NSAccessibilitySubroleAttribute])
        return m_object->subrole();

    if ([attributeName isEqualToString: NSAccessibilityRoleDescriptionAttribute])
        return m_object->roleDescription();

    if ([attributeName isEqualToString: NSAccessibilityParentAttribute]) {
        FrameView* fv = m_object->frameViewIfRenderView();
        if (fv)
            return fv->getView();
        return m_object->parentObjectUnignored()->wrapper();
    }

    if ([attributeName isEqualToString: NSAccessibilityChildrenAttribute]) {
        if (!m_object->hasChildren()) {
            NSArray* children = [self renderWidgetChildren];
            if (children != nil)
                return children;
            m_object->addChildren();
        }
        return convertToNSArray(m_object->children());
    }

    if (m_object->isWebArea()) {
        if ([attributeName isEqualToString: @"AXLinkUIElements"]) {
            Vector<RefPtr<AccessibilityObject> > links;
            m_object->documentLinks(links);
            return convertToNSArray(links);
        }
        if ([attributeName isEqualToString: @"AXLoaded"])
            return [NSNumber numberWithBool: m_object->loaded()];
        if ([attributeName isEqualToString: @"AXLayoutCount"])
            return [NSNumber numberWithInt: m_object->layoutCount()];
    }
    
    if (m_object->isTextControl()) {
        // FIXME: refactor code to eliminate need for this textControl
        RenderTextControl* textControl = static_cast<RenderTextControl*>(renderer);
        if ([attributeName isEqualToString: NSAccessibilityNumberOfCharactersAttribute]) {
            int length = m_object->textLength();
            if (length < 0)
                return nil;
            return [NSNumber numberWithUnsignedInt:length];
        }
        if ([attributeName isEqualToString: NSAccessibilitySelectedTextAttribute]) {
            String selectedText = m_object->selectedText();
            if (selectedText.isNull())
                return nil;
            return (NSString*)selectedText;
        }
        if ([attributeName isEqualToString: NSAccessibilitySelectedTextRangeAttribute]) {
            AccessibilityObject::PlainTextRange textRange = m_object->selectedTextRange();
            if (textRange.isNull())
                return nil;
            return [NSValue valueWithRange:NSMakeRange(textRange.start, textRange.length)];
        }
        // TODO: Get actual visible range. <rdar://problem/4712101>
        if ([attributeName isEqualToString: NSAccessibilityVisibleCharacterRangeAttribute])
            return m_object->isPasswordField() ? nil : [NSValue valueWithRange: NSMakeRange(0, textControl->text().length())];
        if ([attributeName isEqualToString: NSAccessibilityInsertionPointLineNumberAttribute]) {
            if (m_object->isPasswordField() || textControl->selectionStart() != textControl->selectionEnd())
                return nil;
            return [NSNumber numberWithInt:m_object->doAXLineForTextMarker(m_object->textMarkerForIndex(textControl->selectionStart(), true))];
        }
    }
    
    if ([attributeName isEqualToString: NSAccessibilityURLAttribute]) {
        KURL url = m_object->url();
        if (url.isNull())
            return nil;
        return (NSURL*)url;
    }

    if ([attributeName isEqualToString: @"AXVisited"])
        return [NSNumber numberWithBool: m_object->isVisited()];
    
    if ([attributeName isEqualToString: NSAccessibilityTitleAttribute]) {
        if (m_object->isAttachment()) {
            if ([[m_object->attachmentView() accessibilityAttributeNames] containsObject:NSAccessibilityTitleAttribute]) 
                return [m_object->attachmentView() accessibilityAttributeValue:NSAccessibilityTitleAttribute];
        }
        return m_object->title();
    }
    
    if ([attributeName isEqualToString: NSAccessibilityDescriptionAttribute]) {
        if (m_object->isAttachment()) {
            if ([[m_object->attachmentView() accessibilityAttributeNames] containsObject:NSAccessibilityDescriptionAttribute])
                return [m_object->attachmentView() accessibilityAttributeValue:NSAccessibilityDescriptionAttribute];
        }
        return m_object->accessibilityDescription();
    }
    
    if ([attributeName isEqualToString: NSAccessibilityValueAttribute]) {
        if (m_object->isAttachment()) {
            if ([[m_object->attachmentView() accessibilityAttributeNames] containsObject:NSAccessibilityValueAttribute]) 
                return [m_object->attachmentView() accessibilityAttributeValue:NSAccessibilityValueAttribute];
        }    
        if (m_object->hasIntValue())
            return [NSNumber numberWithInt:m_object->intValue()];
        return m_object->stringValue();
    }

    if ([attributeName isEqualToString: NSAccessibilityHelpAttribute])
        return m_object->helpText();
   
    if ([attributeName isEqualToString: NSAccessibilityFocusedAttribute])
        return [NSNumber numberWithBool: m_object->isFocused()];

    if ([attributeName isEqualToString: NSAccessibilityEnabledAttribute])
        return [NSNumber numberWithBool: m_object->isEnabled()];
   
    if ([attributeName isEqualToString: NSAccessibilitySizeAttribute]) {
        IntSize s = m_object->size();
        return [NSValue valueWithSize: NSMakeSize(s.width(), s.height())];
    }

    if ([attributeName isEqualToString: NSAccessibilityPositionAttribute])
        return m_object->position();

    if ([attributeName isEqualToString: NSAccessibilityWindowAttribute]) {
        FrameView* fv = m_object->documentFrameView();
        if (fv)
            return [fv->getView() window];
        return nil;
    }
    
    if ([attributeName isEqualToString: @"AXSelectedTextMarkerRange"])
        return m_object->textMarkerRangeForSelection();
    if ([attributeName isEqualToString: @"AXStartTextMarker"])
        return m_object->startTextMarker();
    if ([attributeName isEqualToString: @"AXEndTextMarker"])
        return m_object->textMarkerForVisiblePosition(endOfDocument(renderer->document()));

    if ([attributeName isEqualToString: NSAccessibilityLinkedUIElementsAttribute]) {
        AccessibilityObject* obj = m_object->linkedUIElement();
        if (obj)
            return (id) obj->wrapper();
        return nil;
    }

    return nil;
}

- (id)accessibilityFocusedUIElement
{
    RefPtr<AccessibilityObject> focusedObj = m_object->focusedUIElement();

    if (!focusedObj)
        return nil;
    
    return focusedObj->wrapper();
}

- (id)accessibilityHitTest:(NSPoint)point
{
    RefPtr<AccessibilityObject> axObject = m_object->doAccessibilityHitTest(IntPoint(point));
    if (axObject)
        return NSAccessibilityUnignoredAncestor(axObject->wrapper());
    return NSAccessibilityUnignoredAncestor(self);
}

- (BOOL)accessibilityIsAttributeSettable:(NSString*)attributeName
{
    if ([attributeName isEqualToString: @"AXSelectedTextMarkerRange"])
        return YES;
        
    if ([attributeName isEqualToString: NSAccessibilityFocusedAttribute])
        return m_object->canSetFocusAttribute();

    if ([attributeName isEqualToString: NSAccessibilityValueAttribute])
        return m_object->canSetValueAttribute();

    if ([attributeName isEqualToString: NSAccessibilitySelectedTextAttribute] ||
        [attributeName isEqualToString: NSAccessibilitySelectedTextRangeAttribute] ||
        [attributeName isEqualToString: NSAccessibilityVisibleCharacterRangeAttribute])
        return m_object->canSetTextRangeAttributes();
    
    return NO;
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
- (BOOL)accessibilityIsIgnored
{
    if (m_object->isAttachment())
        return [m_object->attachmentView() accessibilityIsIgnored];
    return m_object->accessibilityIsIgnored();
}

- (NSArray* )accessibilityParameterizedAttributeNames
{
    if (m_object->isAttachment()) 
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
        [tempArray addObject:(NSString*)kAXLineForIndexParameterizedAttribute];
        [tempArray addObject:(NSString*)kAXRangeForLineParameterizedAttribute];
        [tempArray addObject:(NSString*)kAXStringForRangeParameterizedAttribute];
        [tempArray addObject:(NSString*)kAXRangeForPositionParameterizedAttribute];
        [tempArray addObject:(NSString*)kAXRangeForIndexParameterizedAttribute];
        [tempArray addObject:(NSString*)kAXBoundsForRangeParameterizedAttribute];
        [tempArray addObject:(NSString*)kAXRTFForRangeParameterizedAttribute];
        [tempArray addObject:(NSString*)kAXAttributedStringForRangeParameterizedAttribute];
        [tempArray addObject:(NSString*)kAXStyleRangeForIndexParameterizedAttribute];
        textParamAttrs = [[NSArray alloc] initWithArray:tempArray];
        [tempArray release];
    }
    
    if (m_object->isPasswordField())
        return [NSArray array];
    
    if (!m_object->renderer())
        return paramAttrs;

    if (m_object->isTextControl())
        return textParamAttrs;

    return paramAttrs;
}

- (void)accessibilityPerformAction:(NSString*)action
{
    if ([action isEqualToString:NSAccessibilityPressAction])
        m_object->press();
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
        m_object->doSetAXSelectedTextMarkerRange([self visiblePositionRangeForTextMarkerRange:textMarkerRange]);        
    } else if ([attributeName isEqualToString: NSAccessibilityFocusedAttribute]) {
        ASSERT(number);
        m_object->setFocused([number intValue] != 0);
    } else if ([attributeName isEqualToString: NSAccessibilityValueAttribute]) {
        if (!string)
            return;
        m_object->setValue(string);
   } else if (m_object->isTextControl()) {
        if ([attributeName isEqualToString: NSAccessibilitySelectedTextAttribute]) {
            m_object->setSelectedText(string);
        } else if ([attributeName isEqualToString: NSAccessibilitySelectedTextRangeAttribute]) {
            m_object->setSelectedTextRange(AccessibilityObject::PlainTextRange(range.location, range.length));
        } else if ([attributeName isEqualToString: NSAccessibilityVisibleCharacterRangeAttribute]) {
            m_object->makeRangeVisible(AccessibilityObject::PlainTextRange(range.location, range.length));
        }
    }
}

static RenderObject* rendererForView(NSView* view)
{
    if (![view conformsToProtocol:@protocol(WebCoreFrameView)])
        return 0;

    NSView<WebCoreFrameView>* frameView = (NSView<WebCoreFrameView>*)view;
    Frame* frame = [frameView _web_frame];
    if (!frame)
        return 0;

    Node* node = frame->document()->ownerElement();
    if (!node)
        return 0;

    return node->renderer();
}

- (id)_accessibilityParentForSubview:(NSView*)subview
{   
    RenderObject* renderer = rendererForView(subview);
    if (!renderer)
        return nil;
        
    AccessibilityObject* obj = renderer->document()->axObjectCache()->get(renderer);
    if (obj)
        return obj->parentObjectUnignored()->wrapper();
    return nil;
}

- (NSString*)accessibilityActionDescription:(NSString*)action
{
    // we have no custom actions
    return NSAccessibilityActionDescription(action);
}

- (id)accessibilityAttributeValue:(NSString*)attribute forParameter:(id)parameter
{
    WebCoreTextMarker* textMarker = nil;
    WebCoreTextMarkerRange* textMarkerRange = nil;
    NSNumber* number = nil;
    NSArray* array = nil;
    RefPtr<AccessibilityObject> uiElement = 0;
    NSPoint point = NSZeroPoint;
    bool pointSet = false;
    NSRange range = {0, 0};
    bool rangeSet = false;
    
    RenderObject* renderer = m_object->renderer();

    // basic parameter validation
    if (!renderer || !attribute || !parameter)
        return nil;

    // common parameter type check/casting.  Nil checks in handlers catch wrong type case.
    // NOTE: This assumes nil is not a valid parameter, because it is indistinguishable from
    // a parameter of the wrong type.
    if ([[WebCoreViewFactory sharedFactory] objectIsTextMarker:parameter])
        textMarker = (WebCoreTextMarker*) parameter;

    else if ([[WebCoreViewFactory sharedFactory] objectIsTextMarkerRange:parameter])
        textMarkerRange = (WebCoreTextMarkerRange*) parameter;

    else if ([parameter isKindOfClass:[AccessibilityObjectWrapper self]])
        uiElement = [(AccessibilityObjectWrapper*)parameter accessibilityObject];

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
    
    // Convert values to WebCore types
    VisiblePosition visiblePos = m_object->visiblePositionForTextMarker(textMarker);
    int intNumber = [number intValue];
    VisiblePositionRange visiblePosRange = [self visiblePositionRangeForTextMarkerRange:textMarkerRange];
    IntPoint webCorePoint = IntPoint(point);
    AccessibilityObject::PlainTextRange plainTextRange = AccessibilityObject::PlainTextRange(range.location, range.length);

    // dispatch
    if ([attribute isEqualToString: @"AXUIElementForTextMarker"])
        return m_object->doAXUIElementForTextMarker(visiblePos)->wrapper();

    if ([attribute isEqualToString: @"AXTextMarkerRangeForUIElement"]) {
        VisiblePositionRange vpRange = uiElement.get()->visiblePositionRange();
        return (id)(m_object->textMarkerRangeFromVisiblePositions(vpRange.start, vpRange.end));
    }

    if ([attribute isEqualToString: @"AXLineForTextMarker"])
        return [NSNumber numberWithUnsignedInt:m_object->doAXLineForTextMarker(visiblePos)];

    if ([attribute isEqualToString: @"AXTextMarkerRangeForLine"]) {
        VisiblePositionRange vpRange = m_object->doAXTextMarkerRangeForLine(intNumber);
        return (id)(m_object->textMarkerRangeFromVisiblePositions(vpRange.start, vpRange.end));
    }

    if ([attribute isEqualToString: @"AXStringForTextMarkerRange"])
        return m_object->doAXStringForTextMarkerRange(visiblePosRange);

    if ([attribute isEqualToString: @"AXTextMarkerForPosition"])
        return pointSet ? m_object->textMarkerForVisiblePosition(m_object->doAXTextMarkerForPosition(webCorePoint)) : nil;

    if ([attribute isEqualToString: @"AXBoundsForTextMarkerRange"]) {
        NSRect rect = m_object->doAXBoundsForTextMarkerRange(visiblePosRange);
        return [NSValue valueWithRect:rect];
    }

    if ([attribute isEqualToString: @"AXAttributedStringForTextMarkerRange"])
        return m_object->doAXAttributedStringForTextMarkerRange(textMarkerRange);

    if ([attribute isEqualToString: @"AXTextMarkerRangeForUnorderedTextMarkers"]) {
        if ([array count] < 2)
            return nil;

        WebCoreTextMarker* textMarker1 = (WebCoreTextMarker*) [array objectAtIndex:0];
        WebCoreTextMarker* textMarker2 = (WebCoreTextMarker*) [array objectAtIndex:1];
        if (![[WebCoreViewFactory sharedFactory] objectIsTextMarker:textMarker1] 
            || ![[WebCoreViewFactory sharedFactory] objectIsTextMarker:textMarker2])
            return nil;

        VisiblePosition visiblePos1 = m_object->visiblePositionForTextMarker(textMarker1);
        VisiblePosition visiblePos2 = m_object->visiblePositionForTextMarker(textMarker2);
        VisiblePositionRange vpRange = m_object->doAXTextMarkerRangeForUnorderedTextMarkers(visiblePos1, visiblePos2);
        return (id)(m_object->textMarkerRangeFromVisiblePositions(vpRange.start, vpRange.end));
    }

    if ([attribute isEqualToString: @"AXNextTextMarkerForTextMarker"])
        return m_object->textMarkerForVisiblePosition(m_object->doAXNextTextMarkerForTextMarker(visiblePos));

    if ([attribute isEqualToString: @"AXPreviousTextMarkerForTextMarker"])
        return m_object->textMarkerForVisiblePosition(m_object->doAXPreviousTextMarkerForTextMarker(visiblePos));

    if ([attribute isEqualToString: @"AXLeftWordTextMarkerRangeForTextMarker"]) {
        VisiblePositionRange vpRange = m_object->doAXLeftWordTextMarkerRangeForTextMarker(visiblePos);
        return (id)(m_object->textMarkerRangeFromVisiblePositions(vpRange.start, vpRange.end));
    }

    if ([attribute isEqualToString: @"AXRightWordTextMarkerRangeForTextMarker"]) {
        VisiblePositionRange vpRange = m_object->doAXRightWordTextMarkerRangeForTextMarker(visiblePos);
        return (id)(m_object->textMarkerRangeFromVisiblePositions(vpRange.start, vpRange.end));
    }

    if ([attribute isEqualToString: @"AXLeftLineTextMarkerRangeForTextMarker"]) {
        VisiblePositionRange vpRange = m_object->doAXLeftLineTextMarkerRangeForTextMarker(visiblePos);
        return (id)(m_object->textMarkerRangeFromVisiblePositions(vpRange.start, vpRange.end));
    }

    if ([attribute isEqualToString: @"AXRightLineTextMarkerRangeForTextMarker"]) {
        VisiblePositionRange vpRange = m_object->doAXRightLineTextMarkerRangeForTextMarker(visiblePos);
        return (id)(m_object->textMarkerRangeFromVisiblePositions(vpRange.start, vpRange.end));
    }

    if ([attribute isEqualToString: @"AXSentenceTextMarkerRangeForTextMarker"]) {
        VisiblePositionRange vpRange = m_object->doAXSentenceTextMarkerRangeForTextMarker(visiblePos);
        return (id)(m_object->textMarkerRangeFromVisiblePositions(vpRange.start, vpRange.end));
    }

    if ([attribute isEqualToString: @"AXParagraphTextMarkerRangeForTextMarker"]) {
        VisiblePositionRange vpRange = m_object->doAXParagraphTextMarkerRangeForTextMarker(visiblePos);
        return (id)(m_object->textMarkerRangeFromVisiblePositions(vpRange.start, vpRange.end));
    }

    if ([attribute isEqualToString: @"AXNextWordEndTextMarkerForTextMarker"])
        return m_object->textMarkerForVisiblePosition(m_object->doAXNextWordEndTextMarkerForTextMarker(visiblePos));

    if ([attribute isEqualToString: @"AXPreviousWordStartTextMarkerForTextMarker"])
        return m_object->textMarkerForVisiblePosition(m_object->doAXPreviousWordStartTextMarkerForTextMarker(visiblePos));

    if ([attribute isEqualToString: @"AXNextLineEndTextMarkerForTextMarker"])
        return m_object->textMarkerForVisiblePosition(m_object->doAXNextLineEndTextMarkerForTextMarker(visiblePos));

    if ([attribute isEqualToString: @"AXPreviousLineStartTextMarkerForTextMarker"])
        return m_object->textMarkerForVisiblePosition(m_object->doAXPreviousLineStartTextMarkerForTextMarker(visiblePos));
       
    if ([attribute isEqualToString: @"AXNextSentenceEndTextMarkerForTextMarker"])
        return m_object->textMarkerForVisiblePosition(m_object->doAXNextSentenceEndTextMarkerForTextMarker(visiblePos));

    if ([attribute isEqualToString: @"AXPreviousSentenceStartTextMarkerForTextMarker"])
        return m_object->textMarkerForVisiblePosition(m_object->doAXPreviousSentenceStartTextMarkerForTextMarker(visiblePos));
       
    if ([attribute isEqualToString: @"AXNextParagraphEndTextMarkerForTextMarker"])
        return m_object->textMarkerForVisiblePosition(m_object->doAXNextParagraphEndTextMarkerForTextMarker(visiblePos));

    if ([attribute isEqualToString: @"AXPreviousParagraphStartTextMarkerForTextMarker"])
        return m_object->textMarkerForVisiblePosition(m_object->doAXPreviousParagraphStartTextMarkerForTextMarker(visiblePos));

    if ([attribute isEqualToString: @"AXStyleTextMarkerRangeForTextMarker"]) {
        VisiblePositionRange vpRange = m_object->doAXStyleTextMarkerRangeForTextMarker(visiblePos);
        return (id)(m_object->textMarkerRangeFromVisiblePositions(vpRange.start, vpRange.end));
    }

    if ([attribute isEqualToString: @"AXLengthForTextMarkerRange"]) {
        int length = m_object->doAXLengthForTextMarkerRange(visiblePosRange);
        if (length < 0)
            return nil;
        return [NSNumber numberWithInt:length];
    }

    if (m_object->isTextControl()) {
        if ([attribute isEqualToString: (NSString*)kAXLineForIndexParameterizedAttribute])
            return [NSNumber numberWithUnsignedInt: m_object->doAXLineForIndex(intNumber)];

        if ([attribute isEqualToString: (NSString*)kAXRangeForLineParameterizedAttribute]) {
            AccessibilityObject::PlainTextRange textRange = m_object->doAXRangeForLine(intNumber);
            return [NSValue valueWithRange: NSMakeRange(textRange.start, textRange.length)];
        }
       
        if ([attribute isEqualToString: (NSString*)kAXStringForRangeParameterizedAttribute])
            return rangeSet ? (id)(m_object->doAXStringForRange(plainTextRange)) : nil;

        if ([attribute isEqualToString: (NSString*)kAXRangeForPositionParameterizedAttribute]) {
            if (!pointSet)
                return nil;
            AccessibilityObject::PlainTextRange textRange = m_object->doAXRangeForPosition(webCorePoint);
            return [NSValue valueWithRange: NSMakeRange(textRange.start, textRange.length)];
        }

        if ([attribute isEqualToString: (NSString*)kAXRangeForIndexParameterizedAttribute]) {
            AccessibilityObject::PlainTextRange textRange = m_object->doAXRangeForIndex(intNumber);
            return [NSValue valueWithRange: NSMakeRange(textRange.start, textRange.length)];
        }

        if ([attribute isEqualToString: (NSString*)kAXBoundsForRangeParameterizedAttribute]) {
            if (!rangeSet)
                return nil;
            NSRect rect = m_object->doAXBoundsForRange(plainTextRange);
            return [NSValue valueWithRect:rect];
        }

        if ([attribute isEqualToString: (NSString*)kAXRTFForRangeParameterizedAttribute])
            return rangeSet ? m_object->doAXRTFForRange(range) : nil;

        if ([attribute isEqualToString: (NSString*)kAXAttributedStringForRangeParameterizedAttribute])
            return rangeSet ? m_object->doAXAttributedStringForRange(range) : nil;
       
        if ([attribute isEqualToString: (NSString*)kAXStyleRangeForIndexParameterizedAttribute]) {
            AccessibilityObject::PlainTextRange textRange = m_object->doAXStyleRangeForIndex(intNumber);
            return [NSValue valueWithRange: NSMakeRange(textRange.start, textRange.length)];
        }
    }

    return nil;
}

-(BOOL) accessibilityShouldUseUniqueId
{
    return m_object->accessibilityShouldUseUniqueId();
}

@end
