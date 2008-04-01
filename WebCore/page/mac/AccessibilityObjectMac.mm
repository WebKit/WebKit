/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
#import "AccessibilityObject.h"

#import "AXObjectCache.h"
#import "ColorMac.h"
#import "HTMLAreaElement.h"
#import "LocalizedStrings.h"
#import "RenderView.h"
#import "RenderWidget.h"
#import "RenderTextControl.h"
#import "Selection.h"
#import "SimpleFontData.h"
#import "TextIterator.h"
#import "WebCoreViewFactory.h"
#import "htmlediting.h"
#import "visible_units.h"

using namespace WebCore;
using namespace HTMLNames;

typedef HashMap<int, NSString*> AccessibilityRoleMap;

static const AccessibilityRoleMap& createAccessibilityRoleMap()
{
    struct RoleEntry {
        AccessibilityRole value;
        NSString* string;
    };
    
    static const RoleEntry roles[] = {
        { UnknownRole, NSAccessibilityUnknownRole },
        { ButtonRole, NSAccessibilityButtonRole },
        { RadioButtonRole, NSAccessibilityRadioButtonRole },
        { CheckBoxRole, NSAccessibilityCheckBoxRole },
        { SliderRole, NSAccessibilitySliderRole },
        { TabGroupRole, NSAccessibilityTabGroupRole },
        { TextFieldRole, NSAccessibilityTextFieldRole },
        { StaticTextRole, NSAccessibilityStaticTextRole },
        { TextAreaRole, NSAccessibilityTextAreaRole },
        { ScrollAreaRole, NSAccessibilityScrollAreaRole },
        { PopUpButtonRole, NSAccessibilityPopUpButtonRole },
        { MenuButtonRole, NSAccessibilityMenuButtonRole },
        { TableRole, NSAccessibilityTableRole },
        { ApplicationRole, NSAccessibilityApplicationRole },
        { GroupRole, NSAccessibilityGroupRole },
        { RadioGroupRole, NSAccessibilityRadioGroupRole },
        { ListRole, NSAccessibilityListRole },
        { ScrollBarRole, NSAccessibilityScrollBarRole },
        { ValueIndicatorRole, NSAccessibilityValueIndicatorRole },
        { ImageRole, NSAccessibilityImageRole },
        { MenuBarRole, NSAccessibilityMenuBarRole },
        { MenuRole, NSAccessibilityMenuRole },
        { MenuItemRole, NSAccessibilityMenuItemRole },
        { ColumnRole, NSAccessibilityColumnRole },
        { RowRole, NSAccessibilityRowRole },
        { ToolbarRole, NSAccessibilityToolbarRole },
        { BusyIndicatorRole, NSAccessibilityBusyIndicatorRole },
        { ProgressIndicatorRole, NSAccessibilityProgressIndicatorRole },
        { WindowRole, NSAccessibilityWindowRole },
        { DrawerRole, NSAccessibilityDrawerRole },
        { SystemWideRole, NSAccessibilitySystemWideRole },
        { OutlineRole, NSAccessibilityOutlineRole },
        { IncrementorRole, NSAccessibilityIncrementorRole },
        { BrowserRole, NSAccessibilityBrowserRole },
        { ComboBoxRole, NSAccessibilityComboBoxRole },
        { SplitGroupRole, NSAccessibilitySplitGroupRole },
        { SplitterRole, NSAccessibilitySplitterRole },
        { ColorWellRole, NSAccessibilityColorWellRole },
        { GrowAreaRole, NSAccessibilityGrowAreaRole },
        { SheetRole, NSAccessibilitySheetRole },
        { HelpTagRole, NSAccessibilityHelpTagRole },
        { MatteRole, NSAccessibilityMatteRole }, 
        { RulerRole, NSAccessibilityRulerRole },
        { RulerMarkerRole, NSAccessibilityRulerMarkerRole },
        { SortButtonRole, NSAccessibilitySortButtonRole },
        { LinkRole, NSAccessibilityLinkRole },
#ifndef BUILDING_ON_TIGER        
        { DisclosureTriangleRole, NSAccessibilityDisclosureTriangleRole },
        { GridRole, NSAccessibilityGridRole },
#endif
        { WebCoreLinkRole, @"AXLink" }, // why isn't this just NSAccessibilityLinkRole ?
        { ImageMapRole, @"AXImageMap" },
        { ListMarkerRole, @"AXListMarker" },
        { WebAreaRole, @"AXWebArea" },
        { HeadingRole, @"AXHeading" }
    };
    AccessibilityRoleMap& roleMap = *new AccessibilityRoleMap;
    
    const unsigned numRoles = sizeof(roles) / sizeof(roles[0]);
    for (unsigned i = 0; i < numRoles; ++i)
        roleMap.set(roles[i].value, roles[i].string);
    return roleMap;
}

static NSString* roleValueToNSString(AccessibilityRole value)
{
    ASSERT(value);
    static const AccessibilityRoleMap& roleMap = createAccessibilityRoleMap();
    return roleMap.get(value);
}

NSView* AccessibilityObject::attachmentView()
{
    ASSERT(m_renderer->isReplaced() && m_renderer->isWidget() && !m_renderer->isImage());

    Widget* widget = static_cast<RenderWidget*>(m_renderer)->widget();
    if (!widget)
        return nil;
    return widget->getView();
}

void AccessibilityObject::performPressActionForAttachment()
{
    [attachmentView() accessibilityPerformAction:NSAccessibilityPressAction];
}

WebCoreTextMarkerRange* AccessibilityObject::textMarkerRange(WebCoreTextMarker* fromMarker, WebCoreTextMarker* toMarker)
{
    return [[WebCoreViewFactory sharedFactory] textMarkerRangeWithStart:fromMarker end:toMarker];
}

WebCoreTextMarker* AccessibilityObject::textMarkerForVisiblePosition(VisiblePosition visiblePos)
{
    if (visiblePos.isNull())
        return nil;
    
    if (isPasswordFieldElement(visiblePos.deepEquivalent().node()))
        return nil;
        
    return m_renderer->document()->axObjectCache()->textMarkerForVisiblePosition(visiblePos);
}

WebCoreTextMarker* AccessibilityObject::startTextMarker()
{
    return textMarkerForVisiblePosition(startOfDocument(m_renderer->document()));
}

VisiblePosition AccessibilityObject::visiblePositionForTextMarker(WebCoreTextMarker* textMarker)
{
    return m_renderer->document()->axObjectCache()->visiblePositionForTextMarker(textMarker);
}

VisiblePosition AccessibilityObject::visiblePositionForStartOfTextMarkerRange(WebCoreTextMarkerRange* textMarkerRange)
{
    return visiblePositionForTextMarker([[WebCoreViewFactory sharedFactory] startOfTextMarkerRange:textMarkerRange]);
}

VisiblePosition AccessibilityObject::visiblePositionForEndOfTextMarkerRange(WebCoreTextMarkerRange* textMarkerRange)
{
    return visiblePositionForTextMarker([[WebCoreViewFactory sharedFactory] endOfTextMarkerRange:textMarkerRange]);
}

WebCoreTextMarkerRange* AccessibilityObject::textMarkerRangeFromVisiblePositions(VisiblePosition startPosition, VisiblePosition endPosition)
{
    WebCoreTextMarker* startTextMarker = textMarkerForVisiblePosition(startPosition);
    WebCoreTextMarker* endTextMarker   = textMarkerForVisiblePosition(endPosition);
    return textMarkerRangeFromMarkers(startTextMarker, endTextMarker);
}

WebCoreTextMarkerRange* AccessibilityObject::textMarkerRangeForSelection()
{
    Selection selection = this->selection();
    if (selection.isNone())
        return nil;
    return textMarkerRangeFromVisiblePositions(selection.visibleStart(), selection.visibleEnd());
}

WebCoreTextMarkerRange* AccessibilityObject::textMarkerRange()
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
    
    WebCoreTextMarker* startTextMarker = textMarkerForVisiblePosition(visiblePos1);
    WebCoreTextMarker* endTextMarker   = textMarkerForVisiblePosition(visiblePos2);
    return textMarkerRangeFromMarkers(startTextMarker, endTextMarker);
}

WebCoreTextMarkerRange* AccessibilityObject::textMarkerRangeFromMarkers(WebCoreTextMarker* textMarker1, WebCoreTextMarker* textMarker2)
{
    return [[WebCoreViewFactory sharedFactory] textMarkerRangeWithStart:textMarker1 end:textMarker2];
}

IntRect AccessibilityObject::convertViewRectToScreenCoords(const IntRect& ourrect)
{
    // convert our rectangle to screen coordinates
    FrameView* fv = m_renderer->document()->view();
    NSView *view = fv->getView();
    NSRect rect = ourrect;
    rect = NSOffsetRect(rect, -fv->contentsX(), -fv->contentsY());
    rect = [view convertRect:rect toView:nil];
    rect.origin = [[view window] convertBaseToScreen:rect.origin];

    // return the converted rect
    return enclosingIntRect(rect);
}

IntPoint AccessibilityObject::convertAbsolutePointToViewCoords(const IntPoint& point, const FrameView* frameView)
{
    NSView* view = frameView->documentView();
    NSPoint windowCoord = [[view window] convertScreenToBase: point];
    return IntPoint([view convertPoint:windowCoord fromView:nil]);
}

NSArray* AccessibilityObject::convertWidgetChildrenToNSArray()
{
    if (!m_renderer->isWidget())
        return nil;

    RenderWidget* renderWidget = static_cast<RenderWidget*>(m_renderer);
    Widget* widget = renderWidget->widget();
    if (!widget)
        return nil;

    NSArray* childArr = [(widget->getOuterView()) accessibilityAttributeValue: NSAccessibilityChildrenAttribute];
    return childArr;
}

NSValue* AccessibilityObject::position()
{
    IntRect rect = m_areaElement ? m_areaElement->getRect(m_renderer) : boundingBoxRect();
    
    // The Cocoa accessibility API wants the lower-left corner.
    NSPoint point = NSMakePoint(rect.x(), rect.bottom());
    if (m_renderer && m_renderer->view() && m_renderer->view()->frameView()) {
        NSView* view = m_renderer->view()->frameView()->documentView();
        point = [[view window] convertBaseToScreen: [view convertPoint: point toView:nil]];
    }

    return [NSValue valueWithPoint: point];
}

NSString* AccessibilityObject::role()
{
    if (isAttachment())
        return [attachmentView() accessibilityAttributeValue:NSAccessibilityRoleAttribute];
    NSString* string = roleValueToNSString(roleValue());
    if (string != nil)
        return string;
    return NSAccessibilityUnknownRole;
}

NSString* AccessibilityObject::subrole()
{
    if (isPasswordField())
        return NSAccessibilitySecureTextFieldSubrole;
    
    if (isAttachment()) {
        NSView* attachView = attachmentView();
        if ([[attachView accessibilityAttributeNames] containsObject:NSAccessibilitySubroleAttribute]) {
            return [attachView accessibilityAttributeValue:NSAccessibilitySubroleAttribute];
        }
    }

    return nil;
}

NSString* AccessibilityObject::roleDescription()
{
    if (!m_renderer)
        return nil;

    // attachments have the AXImage role, but a different subrole
    if (isAttachment())
        return [attachmentView() accessibilityAttributeValue:NSAccessibilityRoleDescriptionAttribute];
    
    // FIXME 3447564: It would be better to call some AppKit API to get these strings
    // (which would be the best way to localize them)
    
    NSString* axRole = role();
    if ([axRole isEqualToString:NSAccessibilityButtonRole])
        return NSAccessibilityRoleDescription(NSAccessibilityButtonRole, subrole());
    
    if ([axRole isEqualToString:NSAccessibilityPopUpButtonRole])
        return NSAccessibilityRoleDescription(NSAccessibilityPopUpButtonRole, subrole());
   
    if ([axRole isEqualToString:NSAccessibilityStaticTextRole])
        return NSAccessibilityRoleDescription(NSAccessibilityStaticTextRole, subrole());

    if ([axRole isEqualToString:NSAccessibilityImageRole])
        return NSAccessibilityRoleDescription(NSAccessibilityImageRole, subrole());
    
    if ([axRole isEqualToString:NSAccessibilityGroupRole])
        return NSAccessibilityRoleDescription(NSAccessibilityGroupRole, subrole());
    
    if ([axRole isEqualToString:NSAccessibilityCheckBoxRole])
        return NSAccessibilityRoleDescription(NSAccessibilityCheckBoxRole, subrole());
        
    if ([axRole isEqualToString:NSAccessibilityRadioButtonRole])
        return NSAccessibilityRoleDescription(NSAccessibilityRadioButtonRole, subrole());
        
    if ([axRole isEqualToString:NSAccessibilityTextFieldRole])
        return NSAccessibilityRoleDescription(NSAccessibilityTextFieldRole, subrole());

    if ([axRole isEqualToString:NSAccessibilityTextAreaRole])
        return NSAccessibilityRoleDescription(NSAccessibilityTextAreaRole, subrole());

    if ([axRole isEqualToString:@"AXWebArea"])
        return AXWebAreaText();
    
    if ([axRole isEqualToString:@"AXLink"])
        return AXLinkText();
    
    if ([axRole isEqualToString:@"AXListMarker"])
        return AXListMarkerText();
    
    if ([axRole isEqualToString:@"AXImageMap"])
        return AXImageMapText();

    if ([axRole isEqualToString:@"AXHeading"])
        return AXHeadingText();
    
    return NSAccessibilityRoleDescription(NSAccessibilityUnknownRole, nil);
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

void AccessibilityObject::AXAttributeStringSetElement(NSMutableAttributedString* attrString, NSString* attribute, AccessibilityObject* object, NSRange range)
{
    if (object) {
        // make a serialiazable AX object
        RenderObject* renderer = object->renderer();
        if (!renderer)
            return;
        
        Document* doc = renderer->document();
        if (!doc)
            return;
        
        AXObjectCache* cache = doc->axObjectCache();
        if (!cache)
            return;

        AXUIElementRef axElement = [[WebCoreViewFactory sharedFactory] AXUIElementForElement:object->wrapper()];
        if (axElement) {
            [attrString addAttribute:attribute value:(id)axElement range:range];
            CFRelease(axElement);
        }
    } else
        [attrString removeAttribute:attribute range:range];
}

static void AXAttributeStringSetBlockquoteLevel(NSMutableAttributedString* attrString, RenderObject* renderer, NSRange range)
{
    int quoteLevel = blockquoteLevel(renderer);
    
    if (quoteLevel)
        [attrString addAttribute:@"AXBlockQuoteLevel" value:[NSNumber numberWithInt:quoteLevel] range:range];
    else
        [attrString removeAttribute:@"AXBlockQuoteLevel" range:range];
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

void AccessibilityObject::AXAttributeStringSetHeadingLevel(NSMutableAttributedString* attrString, RenderObject* renderer, NSRange range)
{
    int parentHeadingLevel = headingLevel(renderer->parent()->element());
    
    if (parentHeadingLevel)
        [attrString addAttribute:@"AXHeadingLevel" value:[NSNumber numberWithInt:parentHeadingLevel] range:range];
    else
        [attrString removeAttribute:@"AXHeadingLevel" range:range];
}

AccessibilityObject* AccessibilityObject::AXLinkElementForNode (Node* node)
{
    RenderObject* obj = node->renderer();
    if (!obj)
        return 0;

    RefPtr<AccessibilityObject> axObj = obj->document()->axObjectCache()->get(obj);
    HTMLAnchorElement* anchor = axObj->anchorElement();
    if (!anchor || !anchor->renderer())
        return 0;

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

void AccessibilityObject::AXAttributedStringAppendText(NSMutableAttributedString* attrString, Node* node, int offset, const UChar* chars, int length)
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
    // we cannot just pre-remove all inherited attributes on the appended string, so we have to remove the irrelevant ones individually.

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

static NSString* nsStringForReplacedNode(Node* replacedNode)
{
    // we should always be given a rendered node and a replaced node, but be safe
    // replaced nodes are either attachments (widgets) or images
    if (!replacedNode || !replacedNode->renderer() || !replacedNode->renderer()->isReplaced() || replacedNode->isTextNode()) {
        ASSERT_NOT_REACHED();
        return nil;
    }

    // create an AX object, but skip it if it is not supposed to be seen
    RefPtr<AccessibilityObject> obj = replacedNode->renderer()->document()->axObjectCache()->get(replacedNode->renderer());
    if (obj->accessibilityIsIgnored())
        return nil;
    
    // use the attachmentCharacter to represent the replaced node
    const UniChar attachmentChar = NSAttachmentCharacter;
    return [NSString stringWithCharacters:&attachmentChar length:1];
}

NSAttributedString* AccessibilityObject::doAXAttributedStringForTextMarkerRange(WebCoreTextMarkerRange* textMarkerRange)
{
    // extract the start and end VisiblePosition
    VisiblePosition startVisiblePosition = visiblePositionForStartOfTextMarkerRange(textMarkerRange);
    if (startVisiblePosition.isNull())
        return nil;

    VisiblePosition endVisiblePosition = visiblePositionForEndOfTextMarkerRange(textMarkerRange);
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
                AccessibilityObject* obj = replacedNode->renderer()->document()->axObjectCache()->get(replacedNode->renderer());
                AXAttributeStringSetElement(attrString, NSAccessibilityAttachmentTextAttribute, obj, attrStringRange);
            }
        }
        it.advance();
    }

    return [attrString autorelease];
}

// The CFAttributedStringType representation of the text associated with this accessibility
// object that is specified by the given range.
NSAttributedString* AccessibilityObject::doAXAttributedStringForRange(NSRange range)
{
    AccessibilityObject::PlainTextRange textRange = AccessibilityObject::PlainTextRange(range.location, range.length);
    VisiblePositionRange visiblePosRange = textMarkerRangeForRange(textRange, static_cast<RenderTextControl*>(m_renderer));
    return doAXAttributedStringForTextMarkerRange(textMarkerRangeFromVisiblePositions(visiblePosRange.start, visiblePosRange.end));
}

// The RTF representation of the text associated with this accessibility object that is
// specified by the given range.
NSData* AccessibilityObject::doAXRTFForRange(NSRange range)
{
    NSAttributedString* attrString = doAXAttributedStringForRange(range);
    return [attrString RTFFromRange: NSMakeRange(0, [attrString length]) documentAttributes: nil];
}
