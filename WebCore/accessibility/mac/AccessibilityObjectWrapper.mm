/*
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
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

#if HAVE(ACCESSIBILITY)

#import "AXObjectCache.h"
#import "AccessibilityListBox.h"
#import "AccessibilityList.h"
#import "AccessibilityRenderObject.h"
#import "AccessibilityTable.h"
#import "AccessibilityTableCell.h"
#import "AccessibilityTableRow.h"
#import "AccessibilityTableColumn.h"
#import "ColorMac.h"
#import "Frame.h"
#import "HTMLAnchorElement.h"
#import "HTMLAreaElement.h"
#import "HTMLImageElement.h"
#import "HTMLInputElement.h"
#import "HTMLTextAreaElement.h"
#import "LocalizedStrings.h"
#import "RenderTextControl.h"
#import "RenderView.h"
#import "RenderWidget.h"
#import "SelectionController.h"
#import "SimpleFontData.h"
#import "TextIterator.h"
#import "WebCoreFrameView.h"
#import "WebCoreObjCExtras.h"
#import "WebCoreViewFactory.h"
#import "htmlediting.h"
#import "visible_units.h"
#import <runtime/InitializeThreading.h>

using namespace WebCore;
using namespace HTMLNames;
using namespace std;

// Cell Tables
#ifndef NSAccessibilitySelectedCellsAttribute
#define NSAccessibilitySelectedCellsAttribute @"AXSelectedCells"
#endif

#ifndef NSAccessibilityVisibleCellsAttribute
#define NSAccessibilityVisibleCellsAttribute @"AXVisibleCells"
#endif

#ifndef NSAccessibilityRowHeaderUIElementsAttribute
#define NSAccessibilityRowHeaderUIElementsAttribute @"AXRowHeaderUIElements"
#endif

#ifndef NSAccessibilityRowIndexRangeAttribute
#define NSAccessibilityRowIndexRangeAttribute @"AXRowIndexRange"
#endif

#ifndef NSAccessibilityColumnIndexRangeAttribute
#define NSAccessibilityColumnIndexRangeAttribute @"AXColumnIndexRange"
#endif

#ifndef NSAccessibilityCellForColumnAndRowParameterizedAttribute
#define NSAccessibilityCellForColumnAndRowParameterizedAttribute @"AXCellForColumnAndRow"
#endif

#ifndef NSAccessibilityCellRole
#define NSAccessibilityCellRole @"AXCell"
#endif

// Lists
#ifndef NSAccessibilityContentListSubrole
#define NSAccessibilityContentListSubrole @"AXContentList"
#endif

#ifndef NSAccessibilityDefinitionListSubrole
#define NSAccessibilityDefinitionListSubrole @"AXDefinitionList"
#endif

// Miscellaneous
#ifndef NSAccessibilityBlockQuoteLevelAttribute
#define NSAccessibilityBlockQuoteLevelAttribute @"AXBlockQuoteLevel"
#endif

#ifndef NSAccessibilityAccessKeyAttribute
#define NSAccessibilityAccessKeyAttribute @"AXAccessKey"
#endif

#ifndef NSAccessibilityLanguageAttribute
#define NSAccessibilityLanguageAttribute @"AXLanguage"
#endif

#ifndef NSAccessibilityRequiredAttribute
#define NSAccessibilityRequiredAttribute @"AXRequired"
#endif

#ifdef BUILDING_ON_TIGER
typedef unsigned NSUInteger;
#define NSAccessibilityValueDescriptionAttribute @"AXValueDescription"
#define NSAccessibilityTimelineSubrole @"AXTimeline"
#endif

@interface NSObject (WebKitAccessibilityArrayCategory)

- (NSUInteger)accessibilityIndexOfChild:(id)child;
- (NSUInteger)accessibilityArrayAttributeCount:(NSString *)attribute;
- (NSArray *)accessibilityArrayAttributeValues:(NSString *)attribute index:(NSUInteger)index maxCount:(NSUInteger)maxCount;

@end

@implementation AccessibilityObjectWrapper

+ (void)initialize
{
    JSC::initializeThreading();
#ifndef BUILDING_ON_TIGER
    WebCoreObjCFinalizeOnMainThread(self);
#endif
}

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

- (NSView*)attachmentView
{
    ASSERT(m_object->isAttachment());
    Widget* widget = m_object->widgetForAttachmentView();
    if (!widget)
        return nil;
    return NSAccessibilityUnignoredDescendant(widget->platformWidget());
}

static WebCoreTextMarker* textMarkerForVisiblePosition(const VisiblePosition& visiblePos)
{
    TextMarkerData textMarkerData;
    AXObjectCache::textMarkerDataForVisiblePosition(textMarkerData, visiblePos);
    if (!textMarkerData.axID)
        return nil;
    
    return [[WebCoreViewFactory sharedFactory] textMarkerWithBytes:&textMarkerData length:sizeof(textMarkerData)];
}

static VisiblePosition visiblePositionForTextMarker(WebCoreTextMarker* textMarker)
{
    TextMarkerData textMarkerData;
    if (![[WebCoreViewFactory sharedFactory] getBytes:&textMarkerData fromTextMarker:textMarker length:sizeof(textMarkerData)])
        return VisiblePosition();
    
    return AXObjectCache::visiblePositionForTextMarkerData(textMarkerData);
}

static VisiblePosition visiblePositionForStartOfTextMarkerRange(WebCoreTextMarkerRange* textMarkerRange)
{
    return visiblePositionForTextMarker([[WebCoreViewFactory sharedFactory] startOfTextMarkerRange:textMarkerRange]);
}

static VisiblePosition visiblePositionForEndOfTextMarkerRange(WebCoreTextMarkerRange* textMarkerRange)
{
    return visiblePositionForTextMarker([[WebCoreViewFactory sharedFactory] endOfTextMarkerRange:textMarkerRange]);
}

static WebCoreTextMarkerRange* textMarkerRangeFromMarkers(WebCoreTextMarker* textMarker1, WebCoreTextMarker* textMarker2)
{
    if (!textMarker1 || !textMarker2)
        return nil;
        
    return [[WebCoreViewFactory sharedFactory] textMarkerRangeWithStart:textMarker1 end:textMarker2];
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

static CGColorRef CreateCGColorIfDifferent(NSColor* nsColor, CGColorRef existingColor)
{
    // get color information assuming NSDeviceRGBColorSpace 
    NSColor* rgbColor = [nsColor colorUsingColorSpaceName:NSDeviceRGBColorSpace];
    if (rgbColor == nil)
        rgbColor = [NSColor blackColor];
    CGFloat components[4];
    [rgbColor getRed:&components[0] green:&components[1] blue:&components[2] alpha:&components[3]];
    
    // create a new CGColorRef to return
    CGColorSpaceRef cgColorSpace = CGColorSpaceCreateDeviceRGB();
    CGColorRef cgColor = CGColorCreate(cgColorSpace, components);
    CGColorSpaceRelease(cgColorSpace);
    
    // check for match with existing color
    if (existingColor && CGColorEqualToColor(cgColor, existingColor)) {
        CGColorRelease(cgColor);
        cgColor = 0;
    }
    
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

static int blockquoteLevel(RenderObject* renderer)
{
    if (!renderer)
        return 0;
    
    int result = 0;
    for (Node* node = renderer->node(); node; node = node->parent()) {
        if (node->hasTagName(blockquoteTag))
            result += 1;
    }
    
    return result;
}

static void AXAttributeStringSetBlockquoteLevel(NSMutableAttributedString* attrString, RenderObject* renderer, NSRange range)
{
    int quoteLevel = blockquoteLevel(renderer);
    
    if (quoteLevel)
        [attrString addAttribute:NSAccessibilityBlockQuoteLevelAttribute value:[NSNumber numberWithInt:quoteLevel] range:range];
    else
        [attrString removeAttribute:NSAccessibilityBlockQuoteLevelAttribute range:range];
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
        int rLength = min(marker.endOffset, endOffset) - marker.startOffset;
        NSRange spellRange = NSMakeRange(rStart, rLength);
        AXAttributeStringSetNumber(attrString, NSAccessibilityMisspelledTextAttribute, [NSNumber numberWithBool:YES], spellRange);
        
        if (marker.endOffset > endOffset + 1)
            break;
    }
}

static void AXAttributeStringSetHeadingLevel(NSMutableAttributedString* attrString, RenderObject* renderer, NSRange range)
{
    if (!renderer)
        return;
    
    AccessibilityObject* parentObject = renderer->document()->axObjectCache()->getOrCreate(renderer->parent());
    int parentHeadingLevel = parentObject->headingLevel();
    
    if (parentHeadingLevel)
        [attrString addAttribute:@"AXHeadingLevel" value:[NSNumber numberWithInt:parentHeadingLevel] range:range];
    else
        [attrString removeAttribute:@"AXHeadingLevel" range:range];
}

static void AXAttributeStringSetElement(NSMutableAttributedString* attrString, NSString* attribute, AccessibilityObject* object, NSRange range)
{
    if (object && object->isAccessibilityRenderObject()) {
        // make a serialiazable AX object
        
        RenderObject* renderer = static_cast<AccessibilityRenderObject*>(object)->renderer();
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
    // we cannot just pre-remove all inherited attributes on the appended string, so we have to remove the irrelevant ones individually.

    // remove inherited attachment from prior AXAttributedStringAppendReplaced
    [attrString removeAttribute:NSAccessibilityAttachmentTextAttribute range:attrStringRange];
    
    // set new attributes
    AXAttributeStringSetStyle(attrString, node->renderer(), attrStringRange);
    AXAttributeStringSetHeadingLevel(attrString, node->renderer(), attrStringRange);
    AXAttributeStringSetBlockquoteLevel(attrString, node->renderer(), attrStringRange);
    AXAttributeStringSetElement(attrString, NSAccessibilityLinkTextAttribute, AccessibilityObject::anchorElementForNode(node), attrStringRange);
    
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
    RefPtr<AccessibilityObject> obj = replacedNode->renderer()->document()->axObjectCache()->getOrCreate(replacedNode->renderer());
    if (obj->accessibilityIsIgnored())
        return nil;
    
    // use the attachmentCharacter to represent the replaced node
    const UniChar attachmentChar = NSAttachmentCharacter;
    return [NSString stringWithCharacters:&attachmentChar length:1];
}

- (NSAttributedString*)doAXAttributedStringForTextMarkerRange:(WebCoreTextMarkerRange*)textMarkerRange
{
    if (!m_object)
        return nil;
    
    // extract the start and end VisiblePosition
    VisiblePosition startVisiblePosition = visiblePositionForStartOfTextMarkerRange(textMarkerRange);
    if (startVisiblePosition.isNull())
        return nil;

    VisiblePosition endVisiblePosition = visiblePositionForEndOfTextMarkerRange(textMarkerRange);
    if (endVisiblePosition.isNull())
        return nil;

    VisiblePositionRange visiblePositionRange(startVisiblePosition, endVisiblePosition);
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
            // Add the text of the list marker item if necessary.
            String listMarkerText = m_object->listMarkerTextForNodeAndPosition(node, VisiblePosition(it.range()->startPosition()));
            if (!listMarkerText.isEmpty())
                AXAttributedStringAppendText(attrString, node, offset, listMarkerText.characters(), listMarkerText.length());
            
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
                AccessibilityObject* obj = replacedNode->renderer()->document()->axObjectCache()->getOrCreate(replacedNode->renderer());
                AXAttributeStringSetElement(attrString, NSAccessibilityAttachmentTextAttribute, obj, attrStringRange);
            }
        }
        it.advance();
    }

    return [attrString autorelease];
}

static WebCoreTextMarkerRange* textMarkerRangeFromVisiblePositions(VisiblePosition startPosition, VisiblePosition endPosition)
{
    WebCoreTextMarker* startTextMarker = textMarkerForVisiblePosition(startPosition);
    WebCoreTextMarker* endTextMarker   = textMarkerForVisiblePosition(endPosition);
    return textMarkerRangeFromMarkers(startTextMarker, endTextMarker);
}

- (NSArray*)accessibilityActionNames
{
    if (!m_object)
        return nil;

    m_object->updateBackingStore();
    if (!m_object)
        return nil;

    static NSArray* actionElementActions = [[NSArray alloc] initWithObjects: NSAccessibilityPressAction, NSAccessibilityShowMenuAction, nil];
    static NSArray* defaultElementActions = [[NSArray alloc] initWithObjects: NSAccessibilityShowMenuAction, nil];
    static NSArray* menuElementActions = [[NSArray alloc] initWithObjects: NSAccessibilityCancelAction, NSAccessibilityPressAction, nil];
    static NSArray* sliderActions = [[NSArray alloc] initWithObjects: NSAccessibilityIncrementAction, NSAccessibilityDecrementAction, nil];

    NSArray *actions;
    if (m_object->actionElement()) 
        actions = actionElementActions;
    else if (m_object->isMenuRelated())
        actions = menuElementActions;
    else if (m_object->isSlider())
        actions = sliderActions;
    else if (m_object->isAttachment())
        actions = [[self attachmentView] accessibilityActionNames];
    else
        actions = defaultElementActions;

    return actions;
}

- (NSArray*)accessibilityAttributeNames
{
    if (!m_object)
        return nil;
    
    m_object->updateBackingStore();
    if (!m_object)
        return nil;
    
    if (m_object->isAttachment())
        return [[self attachmentView] accessibilityAttributeNames];

    static NSArray* attributes = nil;
    static NSArray* anchorAttrs = nil;
    static NSArray* webAreaAttrs = nil;
    static NSArray* textAttrs = nil;
    static NSArray* listBoxAttrs = nil;
    static NSArray* rangeAttrs = nil;
    static NSArray* commonMenuAttrs = nil;
    static NSArray* menuAttrs = nil;
    static NSArray* menuBarAttrs = nil;
    static NSArray* menuItemAttrs = nil;
    static NSArray* menuButtonAttrs = nil;
    static NSArray* controlAttrs = nil;
    static NSArray* tableAttrs = nil;
    static NSArray* tableRowAttrs = nil;
    static NSArray* tableColAttrs = nil;
    static NSArray* tableCellAttrs = nil;
    static NSArray* groupAttrs = nil;
    static NSArray* inputImageAttrs = nil;
    static NSArray* passwordFieldAttrs = nil;
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
                      NSAccessibilitySelectedAttribute,
                      NSAccessibilityBlockQuoteLevelAttribute,
                      NSAccessibilityTopLevelUIElementAttribute,
                      nil];
    }
    if (commonMenuAttrs == nil) {
        commonMenuAttrs = [[NSArray alloc] initWithObjects: NSAccessibilityRoleAttribute,
                            NSAccessibilityRoleDescriptionAttribute,
                            NSAccessibilityChildrenAttribute,
                            NSAccessibilityParentAttribute,
                            NSAccessibilityEnabledAttribute,
                            NSAccessibilityPositionAttribute,
                            NSAccessibilitySizeAttribute,
                            nil];
    }
    if (anchorAttrs == nil) {
        tempArray = [[NSMutableArray alloc] initWithArray:attributes];
        [tempArray addObject:NSAccessibilityURLAttribute];
        [tempArray addObject:NSAccessibilityAccessKeyAttribute];
        anchorAttrs = [[NSArray alloc] initWithArray:tempArray];
        [tempArray release];
    }
    if (webAreaAttrs == nil) {
        tempArray = [[NSMutableArray alloc] initWithArray:attributes];
        [tempArray addObject:@"AXLinkUIElements"];
        [tempArray addObject:@"AXLoaded"];
        [tempArray addObject:@"AXLayoutCount"];
        [tempArray addObject:NSAccessibilityURLAttribute];
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
        [tempArray addObject:NSAccessibilityTitleUIElementAttribute];
        [tempArray addObject:NSAccessibilityAccessKeyAttribute];
        [tempArray addObject:NSAccessibilityRequiredAttribute];
        textAttrs = [[NSArray alloc] initWithArray:tempArray];
        [tempArray release];
    }
    if (listBoxAttrs == nil) {
        tempArray = [[NSMutableArray alloc] initWithArray:attributes];
        [tempArray addObject:NSAccessibilitySelectedChildrenAttribute];
        [tempArray addObject:NSAccessibilityVisibleChildrenAttribute];
        [tempArray addObject:NSAccessibilityOrientationAttribute];
        [tempArray addObject:NSAccessibilityTitleUIElementAttribute];
        [tempArray addObject:NSAccessibilityAccessKeyAttribute];
        [tempArray addObject:NSAccessibilityRequiredAttribute];
        listBoxAttrs = [[NSArray alloc] initWithArray:tempArray];
        [tempArray release];
    }
    if (rangeAttrs == nil) {
        tempArray = [[NSMutableArray alloc] initWithArray:attributes];
        [tempArray addObject:NSAccessibilityTopLevelUIElementAttribute];
        [tempArray addObject:NSAccessibilityValueAttribute];
        [tempArray addObject:NSAccessibilityMinValueAttribute];
        [tempArray addObject:NSAccessibilityMaxValueAttribute];
        [tempArray addObject:NSAccessibilityOrientationAttribute];
        [tempArray addObject:NSAccessibilityValueDescriptionAttribute];
        rangeAttrs = [[NSArray alloc] initWithArray:tempArray];
        [tempArray release];
    }
    if (menuBarAttrs == nil) {
        tempArray = [[NSMutableArray alloc] initWithArray:commonMenuAttrs];
        [tempArray addObject:NSAccessibilitySelectedChildrenAttribute];
        [tempArray addObject:NSAccessibilityVisibleChildrenAttribute];
        [tempArray addObject:NSAccessibilityTitleUIElementAttribute];
        menuBarAttrs = [[NSArray alloc] initWithArray:tempArray];
        [tempArray release];
    }
    if (menuAttrs == nil) {
        tempArray = [[NSMutableArray alloc] initWithArray:commonMenuAttrs];
        [tempArray addObject:NSAccessibilitySelectedChildrenAttribute];
        [tempArray addObject:NSAccessibilityVisibleChildrenAttribute];
        [tempArray addObject:NSAccessibilityTitleUIElementAttribute];
        menuAttrs = [[NSArray alloc] initWithArray:tempArray];
        [tempArray release];
    }
    if (menuItemAttrs == nil) {
        tempArray = [[NSMutableArray alloc] initWithArray:commonMenuAttrs];
        [tempArray addObject:NSAccessibilityTitleAttribute];
        [tempArray addObject:NSAccessibilityHelpAttribute];
        [tempArray addObject:NSAccessibilitySelectedAttribute];
        [tempArray addObject:(NSString*)kAXMenuItemCmdCharAttribute];
        [tempArray addObject:(NSString*)kAXMenuItemCmdVirtualKeyAttribute];
        [tempArray addObject:(NSString*)kAXMenuItemCmdGlyphAttribute];
        [tempArray addObject:(NSString*)kAXMenuItemCmdModifiersAttribute];
        [tempArray addObject:(NSString*)kAXMenuItemMarkCharAttribute];
        [tempArray addObject:(NSString*)kAXMenuItemPrimaryUIElementAttribute];
        [tempArray addObject:NSAccessibilityServesAsTitleForUIElementsAttribute];
        menuItemAttrs = [[NSArray alloc] initWithArray:tempArray];
        [tempArray release];
    }
    if (menuButtonAttrs == nil) {
        menuButtonAttrs = [[NSArray alloc] initWithObjects:NSAccessibilityRoleAttribute,
            NSAccessibilityRoleDescriptionAttribute,
            NSAccessibilityParentAttribute,
            NSAccessibilityPositionAttribute,
            NSAccessibilitySizeAttribute,
            NSAccessibilityWindowAttribute,
            NSAccessibilityTopLevelUIElementAttribute,
            NSAccessibilityEnabledAttribute,
            NSAccessibilityFocusedAttribute,
            NSAccessibilityTitleAttribute,
            NSAccessibilityChildrenAttribute, nil];
    }
    if (controlAttrs == nil) {
        tempArray = [[NSMutableArray alloc] initWithArray:attributes];
        [tempArray addObject:NSAccessibilityTitleUIElementAttribute];
        [tempArray addObject:NSAccessibilityAccessKeyAttribute];
        [tempArray addObject:NSAccessibilityRequiredAttribute];
        controlAttrs = [[NSArray alloc] initWithArray:tempArray];
        [tempArray release];
    }
    if (tableAttrs == nil) {
        tempArray = [[NSMutableArray alloc] initWithArray:attributes];
        [tempArray addObject:NSAccessibilityRowsAttribute];
        [tempArray addObject:NSAccessibilityVisibleRowsAttribute];
        [tempArray addObject:NSAccessibilityColumnsAttribute];
        [tempArray addObject:NSAccessibilityVisibleColumnsAttribute];
        [tempArray addObject:NSAccessibilityVisibleCellsAttribute];
        [tempArray addObject:(NSString *)kAXColumnHeaderUIElementsAttribute];
        [tempArray addObject:NSAccessibilityRowHeaderUIElementsAttribute];
        [tempArray addObject:NSAccessibilityHeaderAttribute];
        tableAttrs = [[NSArray alloc] initWithArray:tempArray];
        [tempArray release];
    }
    if (tableRowAttrs == nil) {
        tempArray = [[NSMutableArray alloc] initWithArray:attributes];
        [tempArray addObject:NSAccessibilityIndexAttribute];
        tableRowAttrs = [[NSArray alloc] initWithArray:tempArray];
        [tempArray release];
    }
    if (tableColAttrs == nil) {
        tempArray = [[NSMutableArray alloc] initWithArray:attributes];
        [tempArray addObject:NSAccessibilityIndexAttribute];
        [tempArray addObject:NSAccessibilityHeaderAttribute];
        [tempArray addObject:NSAccessibilityRowsAttribute];
        [tempArray addObject:NSAccessibilityVisibleRowsAttribute];
        tableColAttrs = [[NSArray alloc] initWithArray:tempArray];
        [tempArray release];
    }
    if (tableCellAttrs == nil) {
        tempArray = [[NSMutableArray alloc] initWithArray:attributes];
        [tempArray addObject:NSAccessibilityRowIndexRangeAttribute];
        [tempArray addObject:NSAccessibilityColumnIndexRangeAttribute];
        tableCellAttrs = [[NSArray alloc] initWithArray:tempArray];
        [tempArray release];        
    }
    if (groupAttrs == nil) {
        tempArray = [[NSMutableArray alloc] initWithArray:attributes];
        [tempArray addObject:NSAccessibilityTitleUIElementAttribute];
        groupAttrs = [[NSArray alloc] initWithArray:tempArray];
        [tempArray release];
    }
    if (inputImageAttrs == nil) {
        tempArray = [[NSMutableArray alloc] initWithArray:controlAttrs];
        [tempArray addObject:NSAccessibilityURLAttribute];
        [tempArray addObject:NSAccessibilityAccessKeyAttribute];
        inputImageAttrs = [[NSArray alloc] initWithArray:tempArray];
        [tempArray release];
    }
    if (passwordFieldAttrs == nil) {
        tempArray = [[NSMutableArray alloc] initWithArray:attributes];
        [tempArray addObject:NSAccessibilityTitleUIElementAttribute];
        [tempArray addObject:NSAccessibilityRequiredAttribute];
        passwordFieldAttrs = [[NSArray alloc] initWithArray:tempArray];
        [tempArray release];
    }
    
    if (m_object->isPasswordField())
        return passwordFieldAttrs;

    if (m_object->isWebArea())
        return webAreaAttrs;
    
    if (m_object->isTextControl())
        return textAttrs;

    if (m_object->isAnchor() || m_object->isImage() || m_object->isLink())
        return anchorAttrs;

    if (m_object->isDataTable())
        return tableAttrs;
    if (m_object->isTableRow())
        return tableRowAttrs;
    if (m_object->isTableColumn())
        return tableColAttrs;
    if (m_object->isTableCell())
        return tableCellAttrs;
    
    if (m_object->isListBox() || m_object->isList())
        return listBoxAttrs;

    if (m_object->isProgressIndicator() || m_object->isSlider())
        return rangeAttrs;

    if (m_object->isInputImage())
        return inputImageAttrs;
    
    if (m_object->isControl())
        return controlAttrs;
    
    if (m_object->isGroup())
        return groupAttrs;
    
    if (m_object->isMenu())
        return menuAttrs;
    if (m_object->isMenuBar())
        return menuBarAttrs;
    if (m_object->isMenuButton())
        return menuButtonAttrs;
    if (m_object->isMenuItem())
        return menuItemAttrs;

    return attributes;
}

- (VisiblePositionRange)visiblePositionRangeForTextMarkerRange:(WebCoreTextMarkerRange*) textMarkerRange
{
    return VisiblePositionRange(visiblePositionForStartOfTextMarkerRange(textMarkerRange), visiblePositionForEndOfTextMarkerRange(textMarkerRange));
}

- (NSArray*)renderWidgetChildren
{
    Widget* widget = m_object->widget();
    if (!widget)
        return nil;
    return [(widget->platformWidget()) accessibilityAttributeValue: NSAccessibilityChildrenAttribute];
}

static void convertToVector(NSArray* array, AccessibilityObject::AccessibilityChildrenVector& vector)
{
    unsigned length = [array count];
    vector.reserveInitialCapacity(length);
    for (unsigned i = 0; i < length; ++i) {
        AccessibilityObject* obj = [[array objectAtIndex:i] accessibilityObject];
        if (obj)
            vector.append(obj);
    }
}

static NSMutableArray* convertToNSArray(const AccessibilityObject::AccessibilityChildrenVector& vector)
{
    unsigned length = vector.size();
    NSMutableArray* array = [NSMutableArray arrayWithCapacity: length];
    for (unsigned i = 0; i < length; ++i) {
        AccessibilityObjectWrapper* wrapper = vector[i]->wrapper();
        ASSERT(wrapper);
        if (wrapper) {
            // we want to return the attachment view instead of the object representing the attachment.
            // otherwise, we get palindrome errors in the AX hierarchy
            if (vector[i]->isAttachment() && [wrapper attachmentView])
                [array addObject:[wrapper attachmentView]];
            else
                [array addObject:wrapper];
        }
    }
    return array;
}

- (WebCoreTextMarkerRange*)textMarkerRangeForSelection
{
    VisibleSelection selection = m_object->selection();
    if (selection.isNone())
        return nil;
    return textMarkerRangeFromVisiblePositions(selection.visibleStart(), selection.visibleEnd());
}

- (NSValue*)position
{
    IntRect rect = m_object->elementRect();
    
    // The Cocoa accessibility API wants the lower-left corner.
    NSPoint point = NSMakePoint(rect.x(), rect.bottom());
    FrameView* frameView = m_object->documentFrameView();
    if (frameView) {
        NSView* view = frameView->documentView();
        point = [[view window] convertBaseToScreen: [view convertPoint: point toView:nil]];
    }

    return [NSValue valueWithPoint: point];
}

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
        { LinkRole, NSAccessibilityLinkRole },
#ifndef BUILDING_ON_TIGER        
        { DisclosureTriangleRole, NSAccessibilityDisclosureTriangleRole },
        { GridRole, NSAccessibilityGridRole },
#endif
        { WebCoreLinkRole, NSAccessibilityLinkRole }, 
        { ImageMapLinkRole, NSAccessibilityLinkRole },
        { ImageMapRole, @"AXImageMap" },
        { ListMarkerRole, @"AXListMarker" },
        { WebAreaRole, @"AXWebArea" },
        { HeadingRole, @"AXHeading" },
        { ListBoxRole, NSAccessibilityListRole },
        { ListBoxOptionRole, NSAccessibilityStaticTextRole },
#if ACCESSIBILITY_TABLES
        { CellRole, NSAccessibilityCellRole },
#else
        { CellRole, NSAccessibilityGroupRole },
#endif
        { TableHeaderContainerRole, NSAccessibilityGroupRole },
        { DefinitionListDefinitionRole, NSAccessibilityGroupRole },
        { DefinitionListTermRole, NSAccessibilityGroupRole },
        { SliderThumbRole, NSAccessibilityValueIndicatorRole },
        { LandmarkApplicationRole, NSAccessibilityGroupRole },
        { LandmarkBannerRole, NSAccessibilityGroupRole },
        { LandmarkComplementaryRole, NSAccessibilityGroupRole },
        { LandmarkContentInfoRole, NSAccessibilityGroupRole },
        { LandmarkMainRole, NSAccessibilityGroupRole },
        { LandmarkNavigationRole, NSAccessibilityGroupRole },
        { LandmarkSearchRole, NSAccessibilityGroupRole },
        { ApplicationLogRole, NSAccessibilityGroupRole },
        { ApplicationMarqueeRole, NSAccessibilityGroupRole },
        { ApplicationStatusRole, NSAccessibilityGroupRole },
        { ApplicationTimerRole, NSAccessibilityGroupRole },
        { DocumentRole, NSAccessibilityGroupRole },
        { DocumentArticleRole, NSAccessibilityGroupRole },
        { DocumentNoteRole, NSAccessibilityGroupRole },
        { DocumentRegionRole, NSAccessibilityGroupRole },
        { UserInterfaceTooltipRole, NSAccessibilityGroupRole },
        

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

- (NSString*)role
{
    if (m_object->isAttachment())
        return [[self attachmentView] accessibilityAttributeValue:NSAccessibilityRoleAttribute];
    NSString* string = roleValueToNSString(m_object->roleValue());
    if (string != nil)
        return string;
    return NSAccessibilityUnknownRole;
}

- (NSString*)subrole
{
    if (m_object->isPasswordField())
        return NSAccessibilitySecureTextFieldSubrole;
    
    if (m_object->isAttachment()) {
        NSView* attachView = [self attachmentView];
        if ([[attachView accessibilityAttributeNames] containsObject:NSAccessibilitySubroleAttribute]) {
            return [attachView accessibilityAttributeValue:NSAccessibilitySubroleAttribute];
        }
    }
    
    if (m_object->isList()) {
        AccessibilityList* listObject = static_cast<AccessibilityList*>(m_object);
        if (listObject->isUnorderedList() || listObject->isOrderedList())
            return NSAccessibilityContentListSubrole;
        if (listObject->isDefinitionList())
            return NSAccessibilityDefinitionListSubrole;
    }
    
    // ARIA content subroles.
    switch (m_object->roleValue()) {
        case LandmarkApplicationRole:
            return @"AXLandmarkApplication";
        case LandmarkBannerRole:
            return @"AXLandmarkBanner";
        case LandmarkComplementaryRole:
            return @"AXLandmarkComplementary";
        case LandmarkContentInfoRole:
            return @"AXLandmarkContentInfo";
        case LandmarkMainRole:
            return @"AXLandmarkMain";
        case LandmarkNavigationRole:
            return @"AXLandmarkNavigation";
        case LandmarkSearchRole:
            return @"AXLandmarkSearch";
        case ApplicationLogRole:
            return @"AXApplicationLog";
        case ApplicationMarqueeRole:
            return @"AXApplicationMarquee";
        case ApplicationStatusRole:
            return @"AXApplicationStatus";
        case ApplicationTimerRole:
            return @"AXApplicationTimer";
        case DocumentRole:
            return @"AXDocument";
        case DocumentArticleRole:
            return @"AXDocumentArticle";
        case DocumentNoteRole:
            return @"AXDocumentNote";
        case DocumentRegionRole:
            return @"AXDocumentRegion";
        case UserInterfaceTooltipRole:
            return @"AXUserInterfaceTooltip";
        default:
            return nil;
    }
    
    if (m_object->isMediaTimeline())
        return NSAccessibilityTimelineSubrole;

    return nil;
}

- (NSString*)roleDescription
{
    if (!m_object)
        return nil;

    // attachments have the AXImage role, but a different subrole
    if (m_object->isAttachment())
        return [[self attachmentView] accessibilityAttributeValue:NSAccessibilityRoleDescriptionAttribute];
    
    // FIXME 3447564: It would be better to call some AppKit API to get these strings
    // (which would be the best way to localize them)
    
    NSString* axRole = [self role];
    if ([axRole isEqualToString:NSAccessibilityButtonRole])
        return NSAccessibilityRoleDescription(NSAccessibilityButtonRole, [self subrole]);
    
    if ([axRole isEqualToString:NSAccessibilityPopUpButtonRole])
        return NSAccessibilityRoleDescription(NSAccessibilityPopUpButtonRole, [self subrole]);

    if ([axRole isEqualToString:NSAccessibilityStaticTextRole])
        return NSAccessibilityRoleDescription(NSAccessibilityStaticTextRole, [self subrole]);

    if ([axRole isEqualToString:NSAccessibilityImageRole])
        return NSAccessibilityRoleDescription(NSAccessibilityImageRole, [self subrole]);
    
    if ([axRole isEqualToString:NSAccessibilityGroupRole]) {
        switch (m_object->roleValue()) {
            default:
                return NSAccessibilityRoleDescription(NSAccessibilityGroupRole, [self subrole]);
            case LandmarkApplicationRole:
                return AXARIAContentGroupText(@"ARIALandmarkApplication");
            case LandmarkBannerRole:
                return AXARIAContentGroupText(@"ARIALandmarkBanner");
            case LandmarkComplementaryRole:
                return AXARIAContentGroupText(@"ARIALandmarkComplementary");
            case LandmarkContentInfoRole:
                return AXARIAContentGroupText(@"ARIALandmarkContentInfo");
            case LandmarkMainRole:
                return AXARIAContentGroupText(@"ARIALandmarkMain");
            case LandmarkNavigationRole:
                return AXARIAContentGroupText(@"ARIALandmarkNavigation");
            case LandmarkSearchRole:
                return AXARIAContentGroupText(@"ARIALandmarkSearch");
            case ApplicationLogRole:
                return AXARIAContentGroupText(@"ARIAApplicationLog");
            case ApplicationMarqueeRole:
                return AXARIAContentGroupText(@"ARIAApplicationMarquee");
            case ApplicationStatusRole:
                return AXARIAContentGroupText(@"ARIAApplicationStatus");
            case ApplicationTimerRole:
                return AXARIAContentGroupText(@"ARIAApplicationTimer");
            case DocumentRole:
                return AXARIAContentGroupText(@"ARIADocument");
            case DocumentArticleRole:
                return AXARIAContentGroupText(@"ARIADocumentArticle");
            case DocumentNoteRole:
                return AXARIAContentGroupText(@"ARIADocumentNote");
            case DocumentRegionRole:
                return AXARIAContentGroupText(@"ARIADocumentRegion");
            case UserInterfaceTooltipRole:
                return AXARIAContentGroupText(@"ARIAUserInterfaceTooltip");
        }
    }        
    
    if ([axRole isEqualToString:NSAccessibilityCheckBoxRole])
        return NSAccessibilityRoleDescription(NSAccessibilityCheckBoxRole, [self subrole]);

    if ([axRole isEqualToString:NSAccessibilityRadioButtonRole])
        return NSAccessibilityRoleDescription(NSAccessibilityRadioButtonRole, [self subrole]);

    if ([axRole isEqualToString:NSAccessibilityRadioGroupRole])
        return NSAccessibilityRoleDescription(NSAccessibilityRadioGroupRole, [self subrole]);

    if ([axRole isEqualToString:NSAccessibilityTextFieldRole])
        return NSAccessibilityRoleDescription(NSAccessibilityTextFieldRole, [self subrole]);

    if ([axRole isEqualToString:NSAccessibilityTextAreaRole])
        return NSAccessibilityRoleDescription(NSAccessibilityTextAreaRole, [self subrole]);

    if ([axRole isEqualToString:NSAccessibilityListRole])
        return NSAccessibilityRoleDescription(NSAccessibilityListRole, [self subrole]);
    
    if ([axRole isEqualToString:NSAccessibilityTableRole])
        return NSAccessibilityRoleDescription(NSAccessibilityTableRole, [self subrole]);

    if ([axRole isEqualToString:NSAccessibilityRowRole])
        return NSAccessibilityRoleDescription(NSAccessibilityRowRole, [self subrole]);

    if ([axRole isEqualToString:NSAccessibilityColumnRole])
        return NSAccessibilityRoleDescription(NSAccessibilityColumnRole, [self subrole]);

    if ([axRole isEqualToString:NSAccessibilityCellRole])
        return NSAccessibilityRoleDescription(NSAccessibilityCellRole, [self subrole]);

    if ([axRole isEqualToString:NSAccessibilitySliderRole])
        return NSAccessibilityRoleDescription(NSAccessibilitySliderRole, [self subrole]);

    if ([axRole isEqualToString:NSAccessibilityValueIndicatorRole])
        return NSAccessibilityRoleDescription(NSAccessibilityValueIndicatorRole, [self subrole]);

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
        
    if ([axRole isEqualToString:(NSString*)kAXMenuBarItemRole] ||
        [axRole isEqualToString:NSAccessibilityMenuRole])
        return nil;

    if ([axRole isEqualToString:NSAccessibilityMenuButtonRole])
        return NSAccessibilityRoleDescription(NSAccessibilityMenuButtonRole, [self subrole]);
    
    if ([axRole isEqualToString:NSAccessibilityToolbarRole])
        return NSAccessibilityRoleDescription(NSAccessibilityToolbarRole, [self subrole]);

    if ([axRole isEqualToString:NSAccessibilitySplitterRole])
        return NSAccessibilityRoleDescription(NSAccessibilitySplitterRole, [self subrole]);
    
    return NSAccessibilityRoleDescription(NSAccessibilityUnknownRole, nil);
}

// FIXME: split up this function in a better way.  
// suggestions: Use a hash table that maps attribute names to function calls,
// or maybe pointers to member functions
- (id)accessibilityAttributeValue:(NSString*)attributeName
{
    if (!m_object)
        return nil;

    m_object->updateBackingStore();
    if (!m_object)
        return nil;
    
    if ([attributeName isEqualToString: NSAccessibilityRoleAttribute])
        return [self role];

    if ([attributeName isEqualToString: NSAccessibilitySubroleAttribute])
        return [self subrole];

    if ([attributeName isEqualToString: NSAccessibilityRoleDescriptionAttribute])
        return [self roleDescription];

    if ([attributeName isEqualToString: NSAccessibilityParentAttribute]) {
        if (m_object->isAccessibilityRenderObject()) {
            FrameView* fv = static_cast<AccessibilityRenderObject*>(m_object)->frameViewIfRenderView();
            if (fv)
                return fv->platformWidget();
        }
        
        return m_object->parentObjectUnignored()->wrapper();
    }

    if ([attributeName isEqualToString: NSAccessibilityChildrenAttribute]) {
        if (m_object->children().isEmpty()) {
            NSArray* children = [self renderWidgetChildren];
            if (children != nil)
                return children;
        }
        return convertToNSArray(m_object->children());
    }
    
    if ([attributeName isEqualToString: NSAccessibilitySelectedChildrenAttribute]) {
        if (m_object->isListBox()) {
            AccessibilityObject::AccessibilityChildrenVector selectedChildrenCopy;
            m_object->selectedChildren(selectedChildrenCopy);
            return convertToNSArray(selectedChildrenCopy);
        }
        return nil;
    }
    
    if ([attributeName isEqualToString: NSAccessibilityVisibleChildrenAttribute]) {
        if (m_object->isListBox()) {
            AccessibilityObject::AccessibilityChildrenVector visibleChildrenCopy;
            m_object->visibleChildren(visibleChildrenCopy);
            return convertToNSArray(visibleChildrenCopy);
        }
        else if (m_object->isList())
            return [self accessibilityAttributeValue:NSAccessibilityChildrenAttribute];

        return nil;
    }
    
    
    if (m_object->isWebArea()) {
        if ([attributeName isEqualToString: @"AXLinkUIElements"]) {
            AccessibilityObject::AccessibilityChildrenVector links;
            static_cast<AccessibilityRenderObject*>(m_object)->getDocumentLinks(links);
            return convertToNSArray(links);
        }
        if ([attributeName isEqualToString: @"AXLoaded"])
            return [NSNumber numberWithBool: m_object->isLoaded()];
        if ([attributeName isEqualToString: @"AXLayoutCount"])
            return [NSNumber numberWithInt: m_object->layoutCount()];
    }
    
    if (m_object->isTextControl()) {
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
            PlainTextRange textRange = m_object->selectedTextRange();
            if (textRange.isNull())
                return nil;
            return [NSValue valueWithRange:NSMakeRange(textRange.start, textRange.length)];
        }
        // TODO: Get actual visible range. <rdar://problem/4712101>
        if ([attributeName isEqualToString: NSAccessibilityVisibleCharacterRangeAttribute])
            return m_object->isPasswordField() ? nil : [NSValue valueWithRange: NSMakeRange(0, m_object->textLength())];
        if ([attributeName isEqualToString: NSAccessibilityInsertionPointLineNumberAttribute]) {
            // if selectionEnd > 0, then there is selected text and this question should not be answered
            if (m_object->isPasswordField() || m_object->selectionEnd() > 0)
                return nil;
            int lineNumber = m_object->lineForPosition(m_object->visiblePositionForIndex(m_object->selectionStart(), true));
            if (lineNumber < 0)
                return nil;
            return [NSNumber numberWithInt:lineNumber];
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
            if ([[[self attachmentView] accessibilityAttributeNames] containsObject:NSAccessibilityTitleAttribute]) 
                return [[self attachmentView] accessibilityAttributeValue:NSAccessibilityTitleAttribute];
        }
        return m_object->title();
    }
    
    if ([attributeName isEqualToString: NSAccessibilityDescriptionAttribute]) {
        if (m_object->isAttachment()) {
            if ([[[self attachmentView] accessibilityAttributeNames] containsObject:NSAccessibilityDescriptionAttribute])
                return [[self attachmentView] accessibilityAttributeValue:NSAccessibilityDescriptionAttribute];
        }
        return m_object->accessibilityDescription();
    }

    if ([attributeName isEqualToString: NSAccessibilityValueAttribute]) {
        if (m_object->isAttachment()) {
            if ([[[self attachmentView] accessibilityAttributeNames] containsObject:NSAccessibilityValueAttribute]) 
                return [[self attachmentView] accessibilityAttributeValue:NSAccessibilityValueAttribute];
        }
        if (m_object->isProgressIndicator() || m_object->isSlider())
            return [NSNumber numberWithFloat:m_object->valueForRange()];
        if (m_object->hasIntValue())
            return [NSNumber numberWithInt:m_object->intValue()];

        // radio groups return the selected radio button as the AXValue
        if (m_object->isRadioGroup()) {
            AccessibilityObject* radioButton = m_object->selectedRadioButton();
            if (!radioButton)
                return nil;
            return radioButton->wrapper();
        }
        
        return m_object->stringValue();
    }

    if ([attributeName isEqualToString: NSAccessibilityMinValueAttribute])
        return [NSNumber numberWithFloat:m_object->minValueForRange()];

    if ([attributeName isEqualToString: NSAccessibilityMaxValueAttribute])
        return [NSNumber numberWithFloat:m_object->maxValueForRange()];

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
        return [self position];

    if ([attributeName isEqualToString: NSAccessibilityWindowAttribute] ||
        [attributeName isEqualToString: NSAccessibilityTopLevelUIElementAttribute]) {
        FrameView* fv = m_object->documentFrameView();
        if (fv)
            return [fv->platformWidget() window];
        return nil;
    }
    
    if ([attributeName isEqualToString:NSAccessibilityAccessKeyAttribute]) {
        AtomicString accessKey = m_object->accessKey();
        if (accessKey.isNull())
            return nil;
        return accessKey;
    }
    
    if (m_object->isDataTable()) {
        // TODO: distinguish between visible and non-visible rows
        if ([attributeName isEqualToString:NSAccessibilityRowsAttribute] || 
            [attributeName isEqualToString:NSAccessibilityVisibleRowsAttribute]) {
            return convertToNSArray(static_cast<AccessibilityTable*>(m_object)->rows());
        }
        // TODO: distinguish between visible and non-visible columns
        if ([attributeName isEqualToString:NSAccessibilityColumnsAttribute] || 
            [attributeName isEqualToString:NSAccessibilityVisibleColumnsAttribute]) {
            return convertToNSArray(static_cast<AccessibilityTable*>(m_object)->columns());
        }
        
        // HTML tables don't support these
        if ([attributeName isEqualToString:NSAccessibilitySelectedColumnsAttribute] || 
            [attributeName isEqualToString:NSAccessibilitySelectedRowsAttribute] ||
            [attributeName isEqualToString:NSAccessibilitySelectedCellsAttribute])
            return nil;
        
        if ([attributeName isEqualToString:(NSString *)kAXColumnHeaderUIElementsAttribute]) {
            AccessibilityObject::AccessibilityChildrenVector columnHeaders;
            static_cast<AccessibilityTable*>(m_object)->columnHeaders(columnHeaders);
            return convertToNSArray(columnHeaders);            
        }
        
        if ([attributeName isEqualToString:NSAccessibilityHeaderAttribute]) {
            AccessibilityObject* headerContainer = static_cast<AccessibilityTable*>(m_object)->headerContainer();
            if (headerContainer)
                return headerContainer->wrapper();
            return nil;
        }

        if ([attributeName isEqualToString:NSAccessibilityRowHeaderUIElementsAttribute]) {
            AccessibilityObject::AccessibilityChildrenVector rowHeaders;
            static_cast<AccessibilityTable*>(m_object)->rowHeaders(rowHeaders);
            return convertToNSArray(rowHeaders);                        
        }
        
        if ([attributeName isEqualToString:NSAccessibilityVisibleCellsAttribute]) {
            AccessibilityObject::AccessibilityChildrenVector cells;
            static_cast<AccessibilityTable*>(m_object)->cells(cells);
            return convertToNSArray(cells);
        }        
    }
    
    if (m_object->isTableRow()) {
        if ([attributeName isEqualToString:NSAccessibilityIndexAttribute])
            return [NSNumber numberWithInt:static_cast<AccessibilityTableRow*>(m_object)->rowIndex()];
    }
    
    if (m_object->isTableColumn()) {
        if ([attributeName isEqualToString:NSAccessibilityIndexAttribute])
            return [NSNumber numberWithInt:static_cast<AccessibilityTableColumn*>(m_object)->columnIndex()];
        
        // rows attribute for a column is the list of all the elements in that column at each row
        if ([attributeName isEqualToString:NSAccessibilityRowsAttribute] || 
            [attributeName isEqualToString:NSAccessibilityVisibleRowsAttribute]) {
            return convertToNSArray(static_cast<AccessibilityTableColumn*>(m_object)->children());
        }
        if ([attributeName isEqualToString:NSAccessibilityHeaderAttribute]) {
            AccessibilityObject* header = static_cast<AccessibilityTableColumn*>(m_object)->headerObject();
            if (!header)
                return nil;
            return header->wrapper();
        }
    }
    
    if (m_object->isTableCell()) {
        if ([attributeName isEqualToString:NSAccessibilityRowIndexRangeAttribute]) {
            pair<int, int> rowRange;
            static_cast<AccessibilityTableCell*>(m_object)->rowIndexRange(rowRange);
            return [NSValue valueWithRange:NSMakeRange(rowRange.first, rowRange.second)];
        }  
        if ([attributeName isEqualToString:NSAccessibilityColumnIndexRangeAttribute]) {
            pair<int, int> columnRange;
            static_cast<AccessibilityTableCell*>(m_object)->columnIndexRange(columnRange);
            return [NSValue valueWithRange:NSMakeRange(columnRange.first, columnRange.second)];
        }  
    }
    
    if ((m_object->isListBox() || m_object->isList()) && [attributeName isEqualToString:NSAccessibilityOrientationAttribute])
        return NSAccessibilityVerticalOrientationValue;

    if ([attributeName isEqualToString: @"AXSelectedTextMarkerRange"])
        return [self textMarkerRangeForSelection];
    
    if (m_object->isAccessibilityRenderObject()) {
        RenderObject* renderer = static_cast<AccessibilityRenderObject*>(m_object)->renderer();
        if (!renderer)
            return nil;
        
        if ([attributeName isEqualToString: @"AXStartTextMarker"])
            return textMarkerForVisiblePosition(startOfDocument(renderer->document()));
        if ([attributeName isEqualToString: @"AXEndTextMarker"])
            return textMarkerForVisiblePosition(endOfDocument(renderer->document()));

        if ([attributeName isEqualToString:NSAccessibilityBlockQuoteLevelAttribute])
            return [NSNumber numberWithInt:blockquoteLevel(renderer)];
    } else {
        if ([attributeName isEqualToString:NSAccessibilityBlockQuoteLevelAttribute]) {
            AccessibilityObject* parent = m_object->parentObjectUnignored();
            if (!parent)
                return [NSNumber numberWithInt:0];
            return [parent->wrapper() accessibilityAttributeValue:NSAccessibilityBlockQuoteLevelAttribute];        
        }
    }
    
    if ([attributeName isEqualToString: NSAccessibilityLinkedUIElementsAttribute]) {
        AccessibilityObject::AccessibilityChildrenVector linkedUIElements;
        m_object->linkedUIElements(linkedUIElements);
        if (linkedUIElements.size() == 0)
            return nil;
        return convertToNSArray(linkedUIElements);
    }

    if ([attributeName isEqualToString: NSAccessibilitySelectedAttribute])
        return [NSNumber numberWithBool:m_object->isSelected()];

    if ([attributeName isEqualToString: NSAccessibilityServesAsTitleForUIElementsAttribute] && m_object->isMenuButton()) {
        AccessibilityObject* uiElement = static_cast<AccessibilityRenderObject*>(m_object)->menuForMenuButton();
        if (uiElement)
            return [NSArray arrayWithObject:uiElement->wrapper()];
    }

    if ([attributeName isEqualToString:NSAccessibilityTitleUIElementAttribute]) {
        AccessibilityObject* obj = m_object->titleUIElement();
        if (obj)
            return obj->wrapper();
        return nil;
    }
    
    if ([attributeName isEqualToString:NSAccessibilityValueDescriptionAttribute])
        return m_object->valueDescription();
    
    if ([attributeName isEqualToString:NSAccessibilityOrientationAttribute]) {
        AccessibilityOrientation elementOrientation = m_object->orientation();
        if (elementOrientation == AccessibilityOrientationVertical)
            return NSAccessibilityVerticalOrientationValue;
        if (elementOrientation == AccessibilityOrientationHorizontal)
            return NSAccessibilityHorizontalOrientationValue;
        return nil;
    }
    
    if ([attributeName isEqualToString:NSAccessibilityLanguageAttribute]) 
        return m_object->language();
    
    if ([attributeName isEqualToString:NSAccessibilityRequiredAttribute])
        return [NSNumber numberWithBool:m_object->isRequired()];

    // this is used only by DumpRenderTree for testing
    if ([attributeName isEqualToString:@"AXClickPoint"])
        return [NSValue valueWithPoint:m_object->clickPoint()];
    
    return nil;
}

- (id)accessibilityFocusedUIElement
{
    if (!m_object)
        return nil;

    m_object->updateBackingStore();
    if (!m_object)
        return nil;

    RefPtr<AccessibilityObject> focusedObj = m_object->focusedUIElement();

    if (!focusedObj)
        return nil;
    
    return focusedObj->wrapper();
}

- (id)accessibilityHitTest:(NSPoint)point
{
    if (!m_object)
        return nil;

    m_object->updateBackingStore();
    if (!m_object)
        return nil;

    RefPtr<AccessibilityObject> axObject = m_object->doAccessibilityHitTest(IntPoint(point));
    if (axObject)
        return NSAccessibilityUnignoredAncestor(axObject->wrapper());
    return NSAccessibilityUnignoredAncestor(self);
}

- (BOOL)accessibilityIsAttributeSettable:(NSString*)attributeName
{
    if (!m_object)
        return nil;

    m_object->updateBackingStore();
    if (!m_object)
        return nil;

    if ([attributeName isEqualToString: @"AXSelectedTextMarkerRange"])
        return YES;

    if ([attributeName isEqualToString: NSAccessibilityFocusedAttribute])
        return m_object->canSetFocusAttribute();

    if ([attributeName isEqualToString: NSAccessibilityValueAttribute])
        return m_object->canSetValueAttribute();

    if ([attributeName isEqualToString: NSAccessibilitySelectedAttribute])
        return m_object->canSetSelectedAttribute();
    
    if ([attributeName isEqualToString: NSAccessibilitySelectedChildrenAttribute])
        return m_object->canSetSelectedChildrenAttribute();

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
    if (!m_object)
        return nil;

    m_object->updateBackingStore();
    if (!m_object)
        return nil;

    if (m_object->isAttachment())
        return [[self attachmentView] accessibilityIsIgnored];
    return m_object->accessibilityIsIgnored();
}

- (NSArray* )accessibilityParameterizedAttributeNames
{
    if (!m_object)
        return nil;

    m_object->updateBackingStore();
    if (!m_object)
        return nil;

    if (m_object->isAttachment()) 
        return nil;

    static NSArray* paramAttrs = nil;
    static NSArray* textParamAttrs = nil;
    static NSArray* tableParamAttrs = nil;
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
                      NSAccessibilityBoundsForRangeParameterizedAttribute,
                      NSAccessibilityStringForRangeParameterizedAttribute,
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
    if (tableParamAttrs == nil) {
        NSMutableArray* tempArray = [[NSMutableArray alloc] initWithArray:paramAttrs];
        [tempArray addObject:NSAccessibilityCellForColumnAndRowParameterizedAttribute];
        tableParamAttrs = [[NSArray alloc] initWithArray:tempArray];
        [tempArray release];
    }
    
    if (m_object->isPasswordField())
        return [NSArray array];
    
    if (!m_object->isAccessibilityRenderObject())
        return paramAttrs;

    if (m_object->isTextControl())
        return textParamAttrs;
    
    if (m_object->isDataTable())
        return tableParamAttrs;
    
    if (m_object->isMenuRelated())
        return nil;

    return paramAttrs;
}

- (void)accessibilityPerformPressAction
{
    if (!m_object)
        return;

    m_object->updateBackingStore();
    if (!m_object)
        return;

    if (m_object->isAttachment())
        [[self attachmentView] accessibilityPerformAction:NSAccessibilityPressAction];
    else
        m_object->press();    
}

- (void)accessibilityPerformIncrementAction
{
    if (!m_object)
        return;

    m_object->updateBackingStore();
    if (!m_object)
        return;

    if (m_object->isAttachment())
        [[self attachmentView] accessibilityPerformAction:NSAccessibilityIncrementAction];
    else
        m_object->increment();    
}

- (void)accessibilityPerformDecrementAction
{
    if (!m_object)
        return;

    m_object->updateBackingStore();
    if (!m_object)
        return;

    if (m_object->isAttachment())
        [[self attachmentView] accessibilityPerformAction:NSAccessibilityDecrementAction];
    else
        m_object->decrement();    
}

- (void)accessibilityPerformShowMenuAction
{
    // This needs to be performed in an iteration of the run loop that did not start from an AX call. 
    // If it's the same run loop iteration, the menu open notification won't be sent
    [self performSelector:@selector(accessibilityShowContextMenu) withObject:nil afterDelay:0.0];
}

- (void)accessibilityShowContextMenu
{    
    FrameView* frameView = m_object->documentFrameView();
    if (!frameView)
        return;

    // simulate a click in the middle of the object
    IntPoint clickPoint = m_object->clickPoint();
    NSPoint nsClickPoint = NSMakePoint(clickPoint.x(), clickPoint.y());
    
    NSView* view = nil;
    if (m_object->isAttachment())
        view = [self attachmentView];
    else
        view = frameView->documentView();
    
    if (!view)
        return;
    
    NSPoint nsScreenPoint = [view convertPoint:nsClickPoint toView:nil];
    
    // Show the contextual menu for this event.
    NSEvent* event = [NSEvent mouseEventWithType:NSRightMouseDown location:nsScreenPoint modifierFlags:0 timestamp:0 windowNumber:[[view window] windowNumber] context:0 eventNumber:0 clickCount:1 pressure:1];
    NSMenu* menu = [view menuForEvent:event];
    
    if (menu)
        [NSMenu popUpContextMenu:menu withEvent:event forView:view];    
}

- (void)accessibilityPerformAction:(NSString*)action
{
    if (!m_object)
        return;

    m_object->updateBackingStore();
    if (!m_object)
        return;

    if ([action isEqualToString:NSAccessibilityPressAction])
        [self accessibilityPerformPressAction];
    
    else if ([action isEqualToString:NSAccessibilityShowMenuAction])
        [self accessibilityPerformShowMenuAction];

    else if ([action isEqualToString:NSAccessibilityIncrementAction])
        [self accessibilityPerformIncrementAction];

    else if ([action isEqualToString:NSAccessibilityDecrementAction])
        [self accessibilityPerformDecrementAction];
}

- (void)accessibilitySetValue:(id)value forAttribute:(NSString*)attributeName
{
    if (!m_object)
        return;

    m_object->updateBackingStore();
    if (!m_object)
        return;

    WebCoreTextMarkerRange* textMarkerRange = nil;
    NSNumber*               number = nil;
    NSString*               string = nil;
    NSRange                 range = {0, 0};
    NSArray*                array = nil;
    
    // decode the parameter
    if ([[WebCoreViewFactory sharedFactory] objectIsTextMarkerRange:value])
        textMarkerRange = (WebCoreTextMarkerRange*) value;

    else if ([value isKindOfClass:[NSNumber self]])
        number = value;

    else if ([value isKindOfClass:[NSString self]])
        string = value;
    
    else if ([value isKindOfClass:[NSValue self]])
        range = [value rangeValue];
    
    else if ([value isKindOfClass:[NSArray self]])
        array = value;
    
    // handle the command
    if ([attributeName isEqualToString: @"AXSelectedTextMarkerRange"]) {
        ASSERT(textMarkerRange);
        m_object->setSelectedVisiblePositionRange([self visiblePositionRangeForTextMarkerRange:textMarkerRange]);        
    } else if ([attributeName isEqualToString: NSAccessibilityFocusedAttribute]) {
        ASSERT(number);
        m_object->setFocused([number intValue] != 0);
    } else if ([attributeName isEqualToString: NSAccessibilityValueAttribute]) {
        if (!string)
            return;
        m_object->setValue(string);
    } else if ([attributeName isEqualToString: NSAccessibilitySelectedAttribute]) {
        if (!number)
            return;
        m_object->setSelected([number boolValue]);
    } else if ([attributeName isEqualToString: NSAccessibilitySelectedChildrenAttribute]) {
        if (!array || m_object->roleValue() != ListBoxRole)
            return;
        AccessibilityObject::AccessibilityChildrenVector selectedChildren;
        convertToVector(array, selectedChildren);
        static_cast<AccessibilityListBox*>(m_object)->setSelectedChildren(selectedChildren);
    } else if (m_object->isTextControl()) {
        if ([attributeName isEqualToString: NSAccessibilitySelectedTextAttribute]) {
            m_object->setSelectedText(string);
        } else if ([attributeName isEqualToString: NSAccessibilitySelectedTextRangeAttribute]) {
            m_object->setSelectedTextRange(PlainTextRange(range.location, range.length));
        } else if ([attributeName isEqualToString: NSAccessibilityVisibleCharacterRangeAttribute]) {
            m_object->makeRangeVisible(PlainTextRange(range.location, range.length));
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

    AccessibilityObject* obj = renderer->document()->axObjectCache()->getOrCreate(renderer);
    if (obj)
        return obj->parentObjectUnignored()->wrapper();
    return nil;
}

- (NSString*)accessibilityActionDescription:(NSString*)action
{
    // we have no custom actions
    return NSAccessibilityActionDescription(action);
}

// The CFAttributedStringType representation of the text associated with this accessibility
// object that is specified by the given range.
- (NSAttributedString*)doAXAttributedStringForRange:(NSRange)range
{
    PlainTextRange textRange = PlainTextRange(range.location, range.length);
    VisiblePositionRange visiblePosRange = m_object->visiblePositionRangeForRange(textRange);
    return [self doAXAttributedStringForTextMarkerRange:textMarkerRangeFromVisiblePositions(visiblePosRange.start, visiblePosRange.end)];
}

// The RTF representation of the text associated with this accessibility object that is
// specified by the given range.
- (NSData*)doAXRTFForRange:(NSRange)range
{
    NSAttributedString* attrString = [self doAXAttributedStringForRange:range];
    return [attrString RTFFromRange: NSMakeRange(0, [attrString length]) documentAttributes: nil];
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
    
    // basic parameter validation
    if (!m_object || !attribute || !parameter)
        return nil;

    m_object->updateBackingStore();
    if (!m_object)
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
    // FIXME: prepping all of these values as WebCore types is unnecessary in many 
    // cases. Re-organization of this function or performing the conversion on a 
    // need basis are possible improvements. 
    VisiblePosition visiblePos;
    if (textMarker)
        visiblePos = visiblePositionForTextMarker(textMarker);
    int intNumber = [number intValue];
    VisiblePositionRange visiblePosRange;
    if (textMarkerRange)
        visiblePosRange = [self visiblePositionRangeForTextMarkerRange:textMarkerRange];
    IntPoint webCorePoint = IntPoint(point);
    PlainTextRange plainTextRange = PlainTextRange(range.location, range.length);

    // dispatch
    if ([attribute isEqualToString: @"AXUIElementForTextMarker"])
        return m_object->accessibilityObjectForPosition(visiblePos)->wrapper();

    if ([attribute isEqualToString: @"AXTextMarkerRangeForUIElement"]) {
        VisiblePositionRange vpRange = uiElement.get()->visiblePositionRange();
        return (id)textMarkerRangeFromVisiblePositions(vpRange.start, vpRange.end);
    }

    if ([attribute isEqualToString: @"AXLineForTextMarker"])
        return [NSNumber numberWithUnsignedInt:m_object->lineForPosition(visiblePos)];

    if ([attribute isEqualToString: @"AXTextMarkerRangeForLine"]) {
        VisiblePositionRange vpRange = m_object->visiblePositionRangeForLine(intNumber);
        return (id)textMarkerRangeFromVisiblePositions(vpRange.start, vpRange.end);
    }

    if ([attribute isEqualToString: @"AXStringForTextMarkerRange"])
        return m_object->stringForVisiblePositionRange(visiblePosRange);

    if ([attribute isEqualToString: @"AXTextMarkerForPosition"])
        return pointSet ? textMarkerForVisiblePosition(m_object->visiblePositionForPoint(webCorePoint)) : nil;

    if ([attribute isEqualToString: @"AXBoundsForTextMarkerRange"]) {
        NSRect rect = m_object->boundsForVisiblePositionRange(visiblePosRange);
        return [NSValue valueWithRect:rect];
    }
    
    if ([attribute isEqualToString:NSAccessibilityBoundsForRangeParameterizedAttribute]) {
        VisiblePosition start = m_object->visiblePositionForIndex(range.location);
        VisiblePosition end = m_object->visiblePositionForIndex(range.location+range.length);
        if (start.isNull() || end.isNull())
            return nil;
        NSRect rect = m_object->boundsForVisiblePositionRange(VisiblePositionRange(start, end));
        return [NSValue valueWithRect:rect];
    }
    
    if ([attribute isEqualToString:NSAccessibilityStringForRangeParameterizedAttribute]) {
        VisiblePosition start = m_object->visiblePositionForIndex(range.location);
        VisiblePosition end = m_object->visiblePositionForIndex(range.location+range.length);
        if (start.isNull() || end.isNull())
            return nil;
        return m_object->stringForVisiblePositionRange(VisiblePositionRange(start, end));
    }

    if ([attribute isEqualToString: @"AXAttributedStringForTextMarkerRange"])
        return [self doAXAttributedStringForTextMarkerRange:textMarkerRange];

    if ([attribute isEqualToString: @"AXTextMarkerRangeForUnorderedTextMarkers"]) {
        if ([array count] < 2)
            return nil;

        WebCoreTextMarker* textMarker1 = (WebCoreTextMarker*) [array objectAtIndex:0];
        WebCoreTextMarker* textMarker2 = (WebCoreTextMarker*) [array objectAtIndex:1];
        if (![[WebCoreViewFactory sharedFactory] objectIsTextMarker:textMarker1] 
            || ![[WebCoreViewFactory sharedFactory] objectIsTextMarker:textMarker2])
            return nil;

        VisiblePosition visiblePos1 = visiblePositionForTextMarker(textMarker1);
        VisiblePosition visiblePos2 = visiblePositionForTextMarker(textMarker2);
        VisiblePositionRange vpRange = m_object->visiblePositionRangeForUnorderedPositions(visiblePos1, visiblePos2);
        return (id)textMarkerRangeFromVisiblePositions(vpRange.start, vpRange.end);
    }

    if ([attribute isEqualToString: @"AXNextTextMarkerForTextMarker"])
        return textMarkerForVisiblePosition(m_object->nextVisiblePosition(visiblePos));

    if ([attribute isEqualToString: @"AXPreviousTextMarkerForTextMarker"])
        return textMarkerForVisiblePosition(m_object->previousVisiblePosition(visiblePos));

    if ([attribute isEqualToString: @"AXLeftWordTextMarkerRangeForTextMarker"]) {
        VisiblePositionRange vpRange = m_object->positionOfLeftWord(visiblePos);
        return (id)textMarkerRangeFromVisiblePositions(vpRange.start, vpRange.end);
    }

    if ([attribute isEqualToString: @"AXRightWordTextMarkerRangeForTextMarker"]) {
        VisiblePositionRange vpRange = m_object->positionOfRightWord(visiblePos);
        return (id)textMarkerRangeFromVisiblePositions(vpRange.start, vpRange.end);
    }

    if ([attribute isEqualToString: @"AXLeftLineTextMarkerRangeForTextMarker"]) {
        VisiblePositionRange vpRange = m_object->leftLineVisiblePositionRange(visiblePos);
        return (id)textMarkerRangeFromVisiblePositions(vpRange.start, vpRange.end);
    }

    if ([attribute isEqualToString: @"AXRightLineTextMarkerRangeForTextMarker"]) {
        VisiblePositionRange vpRange = m_object->rightLineVisiblePositionRange(visiblePos);
        return (id)textMarkerRangeFromVisiblePositions(vpRange.start, vpRange.end);
    }

    if ([attribute isEqualToString: @"AXSentenceTextMarkerRangeForTextMarker"]) {
        VisiblePositionRange vpRange = m_object->sentenceForPosition(visiblePos);
        return (id)textMarkerRangeFromVisiblePositions(vpRange.start, vpRange.end);
    }

    if ([attribute isEqualToString: @"AXParagraphTextMarkerRangeForTextMarker"]) {
        VisiblePositionRange vpRange = m_object->paragraphForPosition(visiblePos);
        return (id)textMarkerRangeFromVisiblePositions(vpRange.start, vpRange.end);
    }

    if ([attribute isEqualToString: @"AXNextWordEndTextMarkerForTextMarker"])
        return textMarkerForVisiblePosition(m_object->nextWordEnd(visiblePos));

    if ([attribute isEqualToString: @"AXPreviousWordStartTextMarkerForTextMarker"])
        return textMarkerForVisiblePosition(m_object->previousWordStart(visiblePos));

    if ([attribute isEqualToString: @"AXNextLineEndTextMarkerForTextMarker"])
        return textMarkerForVisiblePosition(m_object->nextLineEndPosition(visiblePos));

    if ([attribute isEqualToString: @"AXPreviousLineStartTextMarkerForTextMarker"])
        return textMarkerForVisiblePosition(m_object->previousLineStartPosition(visiblePos));

    if ([attribute isEqualToString: @"AXNextSentenceEndTextMarkerForTextMarker"])
        return textMarkerForVisiblePosition(m_object->nextSentenceEndPosition(visiblePos));

    if ([attribute isEqualToString: @"AXPreviousSentenceStartTextMarkerForTextMarker"])
        return textMarkerForVisiblePosition(m_object->previousSentenceStartPosition(visiblePos));

    if ([attribute isEqualToString: @"AXNextParagraphEndTextMarkerForTextMarker"])
        return textMarkerForVisiblePosition(m_object->nextParagraphEndPosition(visiblePos));

    if ([attribute isEqualToString: @"AXPreviousParagraphStartTextMarkerForTextMarker"])
        return textMarkerForVisiblePosition(m_object->previousParagraphStartPosition(visiblePos));

    if ([attribute isEqualToString: @"AXStyleTextMarkerRangeForTextMarker"]) {
        VisiblePositionRange vpRange = m_object->styleRangeForPosition(visiblePos);
        return (id)textMarkerRangeFromVisiblePositions(vpRange.start, vpRange.end);
    }

    if ([attribute isEqualToString: @"AXLengthForTextMarkerRange"]) {
        int length = m_object->lengthForVisiblePositionRange(visiblePosRange);
        if (length < 0)
            return nil;
        return [NSNumber numberWithInt:length];
    }

    if (m_object->isDataTable()) {
        if ([attribute isEqualToString:NSAccessibilityCellForColumnAndRowParameterizedAttribute]) {
            if (array == nil || [array count] != 2)
                return nil;
            AccessibilityTableCell* cell = static_cast<AccessibilityTable*>(m_object)->cellForColumnAndRow([[array objectAtIndex:0] unsignedIntValue], [[array objectAtIndex:1] unsignedIntValue]);
            if (!cell)
                return nil;
            
            return cell->wrapper();
        }
    }

    if (m_object->isTextControl()) {
        if ([attribute isEqualToString: (NSString *)kAXLineForIndexParameterizedAttribute]) {
            int lineNumber = m_object->doAXLineForIndex(intNumber);
            if (lineNumber < 0)
                return nil;
            return [NSNumber numberWithUnsignedInt:lineNumber];
        }

        if ([attribute isEqualToString: (NSString *)kAXRangeForLineParameterizedAttribute]) {
            PlainTextRange textRange = m_object->doAXRangeForLine(intNumber);
            return [NSValue valueWithRange: NSMakeRange(textRange.start, textRange.length)];
        }

        if ([attribute isEqualToString: (NSString*)kAXStringForRangeParameterizedAttribute])
            return rangeSet ? (id)(m_object->doAXStringForRange(plainTextRange)) : nil;

        if ([attribute isEqualToString: (NSString*)kAXRangeForPositionParameterizedAttribute]) {
            if (!pointSet)
                return nil;
            PlainTextRange textRange = m_object->doAXRangeForPosition(webCorePoint);
            return [NSValue valueWithRange: NSMakeRange(textRange.start, textRange.length)];
        }

        if ([attribute isEqualToString: (NSString*)kAXRangeForIndexParameterizedAttribute]) {
            PlainTextRange textRange = m_object->doAXRangeForIndex(intNumber);
            return [NSValue valueWithRange: NSMakeRange(textRange.start, textRange.length)];
        }

        if ([attribute isEqualToString: (NSString*)kAXBoundsForRangeParameterizedAttribute]) {
            if (!rangeSet)
                return nil;
            NSRect rect = m_object->doAXBoundsForRange(plainTextRange);
            return [NSValue valueWithRect:rect];
        }

        if ([attribute isEqualToString: (NSString*)kAXRTFForRangeParameterizedAttribute])
            return rangeSet ? [self doAXRTFForRange:range] : nil;

        if ([attribute isEqualToString: (NSString*)kAXAttributedStringForRangeParameterizedAttribute])
            return rangeSet ? [self doAXAttributedStringForRange:range] : nil;

        if ([attribute isEqualToString: (NSString*)kAXStyleRangeForIndexParameterizedAttribute]) {
            PlainTextRange textRange = m_object->doAXStyleRangeForIndex(intNumber);
            return [NSValue valueWithRange: NSMakeRange(textRange.start, textRange.length)];
        }
    }

    return nil;
}

- (BOOL)accessibilityShouldUseUniqueId
{
    return m_object->accessibilityShouldUseUniqueId();
}

// API that AppKit uses for faster access
- (NSUInteger)accessibilityIndexOfChild:(id)child
{
    if (!m_object)
        return NSNotFound;

    m_object->updateBackingStore();
    if (!m_object)
        return NSNotFound;

    const AccessibilityObject::AccessibilityChildrenVector& children = m_object->children();
       
    if (children.isEmpty())
        return [[self renderWidgetChildren] indexOfObject:child];
    
    unsigned count = children.size();
    for (unsigned k = 0; k < count; ++k) {
        AccessibilityObjectWrapper* wrapper = children[k]->wrapper();
        if (wrapper == child || (children[k]->isAttachment() && [wrapper attachmentView] == child)) 
            return k;
    }

    return NSNotFound;
}

- (NSUInteger)accessibilityArrayAttributeCount:(NSString *)attribute
{
    if (!m_object)
        return 0;

    m_object->updateBackingStore();
    if (!m_object)
        return 0;
    
    if ([attribute isEqualToString:NSAccessibilityChildrenAttribute]) {
        const AccessibilityObject::AccessibilityChildrenVector& children = m_object->children();
        if (children.isEmpty())
            return [[self renderWidgetChildren] count];
        
        return children.size();
    }
    
    return [super accessibilityArrayAttributeCount:attribute];
}

- (NSArray *)accessibilityArrayAttributeValues:(NSString *)attribute index:(NSUInteger)index maxCount:(NSUInteger)maxCount 
{
    if (!m_object)
        return nil;

    m_object->updateBackingStore();
    if (!m_object)
        return nil;
    
    if ([attribute isEqualToString:NSAccessibilityChildrenAttribute]) {
        if (m_object->children().isEmpty()) {
            NSArray *children = [self renderWidgetChildren];
            if (!children) 
                return nil;
            
            NSUInteger childCount = [children count];
            if (index >= childCount)
                return nil;
            
            NSUInteger arrayLength = min(childCount - index, maxCount);
            return [children subarrayWithRange:NSMakeRange(index, arrayLength)];
        }
        
        const AccessibilityObject::AccessibilityChildrenVector& children = m_object->children();
        unsigned childCount = children.size();
        if (index >= childCount)
            return nil;
        
        unsigned available = min(childCount - index, maxCount);
        
        NSMutableArray *subarray = [NSMutableArray arrayWithCapacity:available];
        for (unsigned added = 0; added < available; ++index, ++added) {
            AccessibilityObjectWrapper* wrapper = children[index]->wrapper();
            if (wrapper) {
                // The attachment view should be returned, otherwise AX palindrome errors occur.
                if (children[index]->isAttachment() && [wrapper attachmentView])
                    [subarray addObject:[wrapper attachmentView]];
                else
                    [subarray addObject:wrapper];
            }
        }
        
        return subarray;
    }
    
    return [super accessibilityArrayAttributeValues:attribute index:index maxCount:maxCount];
}

@end

#endif // HAVE(ACCESSIBILITY)
