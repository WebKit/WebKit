/*
 * Copyright (C) 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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
#import "WebAccessibilityObjectWrapperBase.h"

#if ENABLE(ACCESSIBILITY)

#import "AXIsolatedObject.h"
#import "AXObjectCache.h"
#import "AccessibilityARIAGridRow.h"
#import "AccessibilityList.h"
#import "AccessibilityListBox.h"
#import "AccessibilityObjectInterface.h"
#import "AccessibilityRenderObject.h"
#import "AccessibilityScrollView.h"
#import "AccessibilitySpinButton.h"
#import "AccessibilityTable.h"
#import "AccessibilityTableCell.h"
#import "AccessibilityTableColumn.h"
#import "AccessibilityTableRow.h"
#import "ColorMac.h"
#import "ContextMenuController.h"
#import "Editing.h"
#import "FrameSelection.h"
#import "HTMLNames.h"
#import "LayoutRect.h"
#import "LocalizedStrings.h"
#import "Page.h"
#import "RenderTextControl.h"
#import "RenderView.h"
#import "RenderWidget.h"
#import "ScrollView.h"
#import "TextCheckerClient.h"
#import "TextIterator.h"
#import "VisibleUnits.h"
#import <Accessibility/Accessibility.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <pal/cocoa/AccessibilitySoftLink.h>

#if PLATFORM(MAC)
#import "WebAccessibilityObjectWrapperMac.h"
#else
#import "WebAccessibilityObjectWrapperIOS.h"
#endif

using namespace WebCore;
using namespace HTMLNames;

// Search Keys
#ifndef NSAccessibilityAnyTypeSearchKey
#define NSAccessibilityAnyTypeSearchKey @"AXAnyTypeSearchKey"
#endif

#ifndef NSAccessibilityArticleSearchKey
#define NSAccessibilityArticleSearchKey @"AXArticleSearchKey"
#endif

#ifndef NSAccessibilityBlockquoteSameLevelSearchKey
#define NSAccessibilityBlockquoteSameLevelSearchKey @"AXBlockquoteSameLevelSearchKey"
#endif

#ifndef NSAccessibilityBlockquoteSearchKey
#define NSAccessibilityBlockquoteSearchKey @"AXBlockquoteSearchKey"
#endif

#ifndef NSAccessibilityBoldFontSearchKey
#define NSAccessibilityBoldFontSearchKey @"AXBoldFontSearchKey"
#endif

#ifndef NSAccessibilityButtonSearchKey
#define NSAccessibilityButtonSearchKey @"AXButtonSearchKey"
#endif

#ifndef NSAccessibilityCheckBoxSearchKey
#define NSAccessibilityCheckBoxSearchKey @"AXCheckBoxSearchKey"
#endif

#ifndef NSAccessibilityControlSearchKey
#define NSAccessibilityControlSearchKey @"AXControlSearchKey"
#endif

#ifndef NSAccessibilityDifferentTypeSearchKey
#define NSAccessibilityDifferentTypeSearchKey @"AXDifferentTypeSearchKey"
#endif

#ifndef NSAccessibilityFontChangeSearchKey
#define NSAccessibilityFontChangeSearchKey @"AXFontChangeSearchKey"
#endif

#ifndef NSAccessibilityFontColorChangeSearchKey
#define NSAccessibilityFontColorChangeSearchKey @"AXFontColorChangeSearchKey"
#endif

#ifndef NSAccessibilityFrameSearchKey
#define NSAccessibilityFrameSearchKey @"AXFrameSearchKey"
#endif

#ifndef NSAccessibilityGraphicSearchKey
#define NSAccessibilityGraphicSearchKey @"AXGraphicSearchKey"
#endif

#ifndef NSAccessibilityHeadingLevel1SearchKey
#define NSAccessibilityHeadingLevel1SearchKey @"AXHeadingLevel1SearchKey"
#endif

#ifndef NSAccessibilityHeadingLevel2SearchKey
#define NSAccessibilityHeadingLevel2SearchKey @"AXHeadingLevel2SearchKey"
#endif

#ifndef NSAccessibilityHeadingLevel3SearchKey
#define NSAccessibilityHeadingLevel3SearchKey @"AXHeadingLevel3SearchKey"
#endif

#ifndef NSAccessibilityHeadingLevel4SearchKey
#define NSAccessibilityHeadingLevel4SearchKey @"AXHeadingLevel4SearchKey"
#endif

#ifndef NSAccessibilityHeadingLevel5SearchKey
#define NSAccessibilityHeadingLevel5SearchKey @"AXHeadingLevel5SearchKey"
#endif

#ifndef NSAccessibilityHeadingLevel6SearchKey
#define NSAccessibilityHeadingLevel6SearchKey @"AXHeadingLevel6SearchKey"
#endif

#ifndef NSAccessibilityHeadingSameLevelSearchKey
#define NSAccessibilityHeadingSameLevelSearchKey @"AXHeadingSameLevelSearchKey"
#endif

#ifndef NSAccessibilityHeadingSearchKey
#define NSAccessibilityHeadingSearchKey @"AXHeadingSearchKey"
#endif

#ifndef NSAccessibilityHighlightedSearchKey
#define NSAccessibilityHighlightedSearchKey @"AXHighlightedSearchKey"
#endif

#ifndef NSAccessibilityKeyboardFocusableSearchKey
#define NSAccessibilityKeyboardFocusableSearchKey @"AXKeyboardFocusableSearchKey"
#endif

#ifndef NSAccessibilityItalicFontSearchKey
#define NSAccessibilityItalicFontSearchKey @"AXItalicFontSearchKey"
#endif

#ifndef NSAccessibilityLandmarkSearchKey
#define NSAccessibilityLandmarkSearchKey @"AXLandmarkSearchKey"
#endif

#ifndef NSAccessibilityLinkSearchKey
#define NSAccessibilityLinkSearchKey @"AXLinkSearchKey"
#endif

#ifndef NSAccessibilityListSearchKey
#define NSAccessibilityListSearchKey @"AXListSearchKey"
#endif

#ifndef NSAccessibilityLiveRegionSearchKey
#define NSAccessibilityLiveRegionSearchKey @"AXLiveRegionSearchKey"
#endif

#ifndef NSAccessibilityMisspelledWordSearchKey
#define NSAccessibilityMisspelledWordSearchKey @"AXMisspelledWordSearchKey"
#endif

#ifndef NSAccessibilityOutlineSearchKey
#define NSAccessibilityOutlineSearchKey @"AXOutlineSearchKey"
#endif

#ifndef NSAccessibilityPlainTextSearchKey
#define NSAccessibilityPlainTextSearchKey @"AXPlainTextSearchKey"
#endif

#ifndef NSAccessibilityRadioGroupSearchKey
#define NSAccessibilityRadioGroupSearchKey @"AXRadioGroupSearchKey"
#endif

#ifndef NSAccessibilitySameTypeSearchKey
#define NSAccessibilitySameTypeSearchKey @"AXSameTypeSearchKey"
#endif

#ifndef NSAccessibilityStaticTextSearchKey
#define NSAccessibilityStaticTextSearchKey @"AXStaticTextSearchKey"
#endif

#ifndef NSAccessibilityStyleChangeSearchKey
#define NSAccessibilityStyleChangeSearchKey @"AXStyleChangeSearchKey"
#endif

#ifndef NSAccessibilityTableSameLevelSearchKey
#define NSAccessibilityTableSameLevelSearchKey @"AXTableSameLevelSearchKey"
#endif

#ifndef NSAccessibilityTableSearchKey
#define NSAccessibilityTableSearchKey @"AXTableSearchKey"
#endif

#ifndef NSAccessibilityTextFieldSearchKey
#define NSAccessibilityTextFieldSearchKey @"AXTextFieldSearchKey"
#endif

#ifndef NSAccessibilityUnderlineSearchKey
#define NSAccessibilityUnderlineSearchKey @"AXUnderlineSearchKey"
#endif

#ifndef NSAccessibilityUnvisitedLinkSearchKey
#define NSAccessibilityUnvisitedLinkSearchKey @"AXUnvisitedLinkSearchKey"
#endif

#ifndef NSAccessibilityVisitedLinkSearchKey
#define NSAccessibilityVisitedLinkSearchKey @"AXVisitedLinkSearchKey"
#endif

// Search
#ifndef NSAccessibilityImmediateDescendantsOnly
#define NSAccessibilityImmediateDescendantsOnly @"AXImmediateDescendantsOnly"
#endif

static NSArray *convertMathPairsToNSArray(const AccessibilityObject::AccessibilityMathMultiscriptPairs& pairs, NSString *subscriptKey, NSString *superscriptKey)
{
    return createNSArray(pairs, [&] (auto& pair) {
        WebAccessibilityObjectWrapper *wrappers[2];
        NSString *keys[2];
        NSUInteger count = 0;
        if (pair.first && pair.first->wrapper() && !pair.first->accessibilityIsIgnored()) {
            wrappers[0] = pair.first->wrapper();
            keys[0] = subscriptKey;
            count = 1;
        }
        if (pair.second && pair.second->wrapper() && !pair.second->accessibilityIsIgnored()) {
            wrappers[count] = pair.second->wrapper();
            keys[count] = superscriptKey;
            count += 1;
        }
        return adoptNS([[NSDictionary alloc] initWithObjects:wrappers forKeys:keys count:count]);
    }).autorelease();
}

NSArray *makeNSArray(const WebCore::AXCoreObject::AccessibilityChildrenVector& children)
{
    return createNSArray(children, [] (const auto& child) -> id {
        if (!child)
            return nil;

        auto wrapper = child->wrapper();

        // We want to return the attachment view instead of the object representing the attachment,
        // otherwise, we get palindrome errors in the AX hierarchy.
        if (child->isAttachment() && wrapper.attachmentView)
            return wrapper.attachmentView;

        return wrapper;
    }).autorelease();
}

@implementation WebAccessibilityObjectWrapperBase

@synthesize identifier = _identifier;

- (id)initWithAccessibilityObject:(AccessibilityObject*)axObject
{
    ASSERT(isMainThread());

    if (!(self = [super init]))
        return nil;
    [self attachAXObject:axObject];
    return self;
}

- (void)attachAXObject:(AccessibilityObject*)axObject
{
    ASSERT(axObject && (!_identifier.isValid() || _identifier == axObject->objectID()));
    m_axObject = axObject;
    if (!_identifier.isValid())
        _identifier = m_axObject->objectID();
}

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
- (void)attachIsolatedObject:(AXIsolatedObject*)isolatedObject
{
    ASSERT(isolatedObject && (!_identifier.isValid() || _identifier == isolatedObject->objectID()));
    m_isolatedObject = isolatedObject;
    if (isMainThread())
        m_isolatedObjectInitialized = true;

    if (!_identifier.isValid())
        _identifier = m_isolatedObject->objectID();
}

- (BOOL)hasIsolatedObject
{
    return !!m_isolatedObject;
}
#endif

- (void)detach
{
    ASSERT(isMainThread());
    _identifier = { };
    m_axObject = nullptr;
}

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
- (void)detachIsolatedObject:(AccessibilityDetachmentType)detachmentType
{
    m_isolatedObject = nullptr;
}
#endif

#if PLATFORM(MAC)
- (WebCore::AXCoreObject*)updateObjectBackingStore
{
    // Calling updateBackingStore() can invalidate this element so self must be retained.
    // If it does become invalidated, self.axBackingObject will be nil.
    retainPtr(self).autorelease();

    auto* backingObject = self.axBackingObject;
    if (!backingObject)
        return nil;

    backingObject->updateBackingStore();

    return self.axBackingObject;
}
#else
- (BOOL)_prepareAccessibilityCall
{
    // rdar://7980318 if we start a call, then block in WebThreadLock(), then we're dealloced on another, thread, we could
    // crash, so we should retain ourself for the duration of usage here.
    retainPtr(self).autorelease();

    WebThreadLock();

    // If we came back from our thread lock and we were detached, we will no longer have an self.axBackingObject.
    if (!self.axBackingObject)
        return NO;

    self.axBackingObject->updateBackingStore();
    if (!self.axBackingObject)
        return NO;

    return YES;
}
#endif

- (id)attachmentView
{
    return nil;
}

- (WebCore::AXCoreObject*)axBackingObject
{
    if (isMainThread())
        return m_axObject;

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    ASSERT(AXObjectCache::isIsolatedTreeEnabled());
    return m_isolatedObject;
#else
    ASSERT_NOT_REACHED();
    return nullptr;
#endif
}

- (BOOL)isIsolatedObject
{
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    auto* backingObject = self.axBackingObject;
    return backingObject && backingObject->isAXIsolatedObjectInstance();
#else
    return NO;
#endif
}

- (NSArray<NSString *> *)baseAccessibilitySpeechHint
{
    return [(NSString *)self.axBackingObject->speechHintAttributeValue() componentsSeparatedByString:@" "];
}

#if HAVE(ACCESSIBILITY_FRAMEWORK)
- (NSArray<AXCustomContent *> *)accessibilityCustomContent
{
    RefPtr<AXCoreObject> backingObject = [self baseUpdateBackingStore];
    if (!backingObject)
        return nil;
    
    RetainPtr<NSMutableArray<AXCustomContent *>> accessibilityCustomContent = nil;
    auto extendedDescription = backingObject->extendedDescription();
    if (extendedDescription.length()) {
        accessibilityCustomContent = adoptNS([[NSMutableArray alloc] init]);
        [accessibilityCustomContent addObject:[PAL::getAXCustomContentClass() customContentWithLabel:WEB_UI_STRING("description", "description detail") value:extendedDescription]];
    }
    
    return accessibilityCustomContent.autorelease();
}
#endif

- (NSString *)baseAccessibilityHelpText
{
    return self.axBackingObject->helpTextAttributeValue();
}

struct PathConversionInfo {
    WebAccessibilityObjectWrapperBase *wrapper;
    CGMutablePathRef path;
};

static void convertPathToScreenSpaceFunction(PathConversionInfo& conversion, const PathElement& element)
{
    WebAccessibilityObjectWrapperBase *wrapper = conversion.wrapper;
    CGMutablePathRef newPath = conversion.path;
    FloatRect rect;
    switch (element.type) {
    case PathElement::Type::MoveToPoint:
    {
        rect = FloatRect(element.points[0], FloatSize());
        CGPoint newPoint = [wrapper convertRectToSpace:rect space:AccessibilityConversionSpace::Screen].origin;
        CGPathMoveToPoint(newPath, nil, newPoint.x, newPoint.y);
        break;
    }
    case PathElement::Type::AddLineToPoint:
    {
        rect = FloatRect(element.points[0], FloatSize());
        CGPoint newPoint = [wrapper convertRectToSpace:rect space:AccessibilityConversionSpace::Screen].origin;
        CGPathAddLineToPoint(newPath, nil, newPoint.x, newPoint.y);
        break;
    }
    case PathElement::Type::AddQuadCurveToPoint:
    {
        rect = FloatRect(element.points[0], FloatSize());
        CGPoint newPoint1 = [wrapper convertRectToSpace:rect space:AccessibilityConversionSpace::Screen].origin;

        rect = FloatRect(element.points[1], FloatSize());
        CGPoint newPoint2 = [wrapper convertRectToSpace:rect space:AccessibilityConversionSpace::Screen].origin;
        CGPathAddQuadCurveToPoint(newPath, nil, newPoint1.x, newPoint1.y, newPoint2.x, newPoint2.y);
        break;
    }
    case PathElement::Type::AddCurveToPoint:
    {
        rect = FloatRect(element.points[0], FloatSize());
        CGPoint newPoint1 = [wrapper convertRectToSpace:rect space:AccessibilityConversionSpace::Screen].origin;

        rect = FloatRect(element.points[1], FloatSize());
        CGPoint newPoint2 = [wrapper convertRectToSpace:rect space:AccessibilityConversionSpace::Screen].origin;

        rect = FloatRect(element.points[2], FloatSize());
        CGPoint newPoint3 = [wrapper convertRectToSpace:rect space:AccessibilityConversionSpace::Screen].origin;
        CGPathAddCurveToPoint(newPath, nil, newPoint1.x, newPoint1.y, newPoint2.x, newPoint2.y, newPoint3.x, newPoint3.y);
        break;
    }
    case PathElement::Type::CloseSubpath:
    {
        CGPathCloseSubpath(newPath);
        break;
    }
    }
}

- (CGPathRef)convertPathToScreenSpace:(const Path&)path
{
    auto convertedPath = adoptCF(CGPathCreateMutable());
    PathConversionInfo conversion = { self, convertedPath.get() };
    path.apply([&conversion](const PathElement& pathElement) {
        convertPathToScreenSpaceFunction(conversion, pathElement);
    });
    return convertedPath.autorelease();
}

// Determine the visible range by checking intersection of unobscuredContentRect and a range of text by
// advancing forward by line from top and backwards by line from the bottom, until we have a visible range.
- (NSRange)accessibilityVisibleCharacterRange
{
    return Accessibility::retrieveValueFromMainThread<NSRange>([protectedSelf = retainPtr(self)] () -> NSRange {
        auto backingObject = protectedSelf.get().baseUpdateBackingStore;
        if (!backingObject)
            return NSMakeRange(NSNotFound, 0);

        auto elementRange = makeNSRange(backingObject->elementRange());
        if (elementRange.location == NSNotFound)
            return elementRange;
        
        auto visibleRange = makeNSRange(backingObject->visibleCharacterRange());
        if (visibleRange.location == NSNotFound)
            return visibleRange;

        return NSMakeRange(visibleRange.location - elementRange.location, visibleRange.length);
    });
}

- (id)_accessibilityWebDocumentView
{
    ASSERT_NOT_REACHED();
    // Overridden by sub-classes
    return nil;
}

- (CGRect)convertRectToSpace:(const WebCore::FloatRect&)rect space:(AccessibilityConversionSpace)space
{
    auto* backingObject = self.axBackingObject;
    if (!backingObject)
        return CGRectZero;

    return backingObject->convertRectToPlatformSpace(rect, space);
}

static BOOL addObjectWrapperToArray(AccessibilityObject* axObject, NSMutableArray *array)
{
    if (!axObject)
        return NO;

    auto* wrapper = axObject->wrapper();
    if (!wrapper)
        return NO;

    // Don't add the same object twice, but since this has already been added, we should return
    // YES because we want to inform that it's in the array
    if ([array containsObject:wrapper])
        return YES;

#if PLATFORM(IOS_FAMILY)
    // Explicitly set that this is a now element, in case other logic tries to override.
    [wrapper setValue:@YES forKey:@"isAccessibilityElement"];
#endif

    [array addObject:wrapper];
    return YES;
}

static int blockquoteLevel(RenderObject* renderer)
{
    if (!renderer)
        return 0;

    int result = 0;
    for (Node* node = renderer->node(); node; node = node->parentNode()) {
        if (node->hasTagName(blockquoteTag))
            result += 1;
    }

    return result;
}

static void AXAttributeStringSetLanguage(NSMutableAttributedString* attrString, RenderObject* renderer, NSRange range)
{
    if (!renderer)
        return;

    AccessibilityObject* axObject = renderer->document().axObjectCache()->getOrCreate(renderer);
    NSString *language = axObject->language();
    if ([language length])
        [attrString addAttribute:UIAccessibilityTokenLanguage value:language range:range];
    else
        [attrString removeAttribute:UIAccessibilityTokenLanguage range:range];
}

static void AXAttributeStringSetBlockquoteLevel(NSMutableAttributedString* attrString, RenderObject* renderer, NSRange range)
{
    int quoteLevel = blockquoteLevel(renderer);

    if (quoteLevel)
        [attrString addAttribute:UIAccessibilityTokenBlockquoteLevel value:@(quoteLevel) range:range];
    else
        [attrString removeAttribute:UIAccessibilityTokenBlockquoteLevel range:range];
}

static void AXAttributeStringSetHeadingLevel(NSMutableAttributedString* attrString, RenderObject* renderer, NSRange range)
{
    if (!renderer)
        return;

    AccessibilityObject* parentObject = renderer->document().axObjectCache()->getOrCreate(renderer->parent());
    int parentHeadingLevel = parentObject->headingLevel();

    if (parentHeadingLevel)
        [attrString addAttribute:UIAccessibilityTokenHeadingLevel value:@(parentHeadingLevel) range:range];
    else
        [attrString removeAttribute:UIAccessibilityTokenHeadingLevel range:range];
}

// When modifying attributed strings, the range can come from a source which may provide faulty information (e.g. the spell checker).
// To protect against such cases, the range should be validated before adding or removing attributes.
bool AXAttributedStringRangeIsValid(NSAttributedString *attributedString, const NSRange& range)
{
    return NSMaxRange(range) <= [attributedString length];
}

void AXAttributedStringSetFont(NSMutableAttributedString *attributedString, CTFontRef font, const NSRange& range)
{
    if (!AXAttributedStringRangeIsValid(attributedString, range))
        return;

    if (!font) {
#if PLATFORM(MAC)
        [attributedString removeAttribute:NSAccessibilityFontTextAttribute range:range];
#endif
        return;
    }

    auto fontAttributes = adoptNS([[NSMutableDictionary alloc] init]);
    auto familyName = adoptCF(CTFontCopyFamilyName(font));
    NSNumber *size = [NSNumber numberWithFloat:CTFontGetSize(font)];
#if PLATFORM(IOS_FAMILY)
    auto fullName = adoptCF(CTFontCopyFullName(font));
    if (fullName)
        [fontAttributes setValue:bridge_cast(fullName.get()) forKey:UIAccessibilityTokenFontName];
    if (familyName)
        [fontAttributes setValue:bridge_cast(familyName.get()) forKey:UIAccessibilityTokenFontFamily];
    if ([size boolValue])
        [fontAttributes setValue:size forKey:UIAccessibilityTokenFontSize];
    auto traits = CTFontGetSymbolicTraits(font);
    if (traits & kCTFontTraitBold)
        [fontAttributes setValue:@YES forKey:UIAccessibilityTokenBold];
    if (traits & kCTFontTraitItalic)
        [fontAttributes setValue:@YES forKey:UIAccessibilityTokenItalic];

    [attributedString addAttributes:fontAttributes.get() range:range];
#endif

#if PLATFORM(MAC)
    [fontAttributes setValue:size forKey:NSAccessibilityFontSizeKey];

    if (familyName)
        [fontAttributes setValue:bridge_cast(familyName.get()) forKey:NSAccessibilityFontFamilyKey];
    auto postScriptName = adoptCF(CTFontCopyPostScriptName(font));
    if (postScriptName)
        [fontAttributes setValue:bridge_cast(postScriptName.get()) forKey:NSAccessibilityFontNameKey];
    auto displayName = adoptCF(CTFontCopyDisplayName(font));
    if (displayName)
        [fontAttributes setValue:bridge_cast(displayName.get()) forKey:NSAccessibilityVisibleNameKey];
    auto traits = CTFontGetSymbolicTraits(font);
    if (traits & kCTFontTraitBold)
        [fontAttributes setValue:@YES forKey:@"AXFontBold"];
    if (traits & kCTFontTraitItalic)
        [fontAttributes setValue:@YES forKey:@"AXFontItalic"];

    [attributedString addAttribute:NSAccessibilityFontTextAttribute value:fontAttributes.get() range:range];
#endif
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
    auto& style = renderer->style();

    // set basic font info
    AXAttributedStringSetFont(attrString, style.fontCascade().primaryFont().getCTFont(), range);

    auto decor = style.textDecorationsInEffect();
    if (decor & TextDecorationLine::Underline)
        AXAttributeStringSetNumber(attrString, UIAccessibilityTokenUnderline, @YES, range);

    // Add code context if this node is within a <code> block.
    AccessibilityObject* axObject = renderer->document().axObjectCache()->getOrCreate(renderer);
    auto matchFunc = [] (const AXCoreObject& object) {
        return object.node() && object.node()->hasTagName(codeTag);
    };

    if (const AXCoreObject* parent = Accessibility::findAncestor<AXCoreObject>(*axObject, true, WTFMove(matchFunc)))
        [attrString addAttribute:UIAccessibilityTextAttributeContext value:UIAccessibilityTextualContextSourceCode range:range];
}

static void AXAttributedStringAppendText(NSMutableAttributedString* attrString, Node* node, StringView text)
{
    // skip invisible text
    if (!node->renderer())
        return;

    // easier to calculate the range before appending the string
    NSRange attrStringRange = NSMakeRange([attrString length], text.length());

    // append the string from this node
    [[attrString mutableString] appendString:text.createNSStringWithoutCopying().get()];

    // set new attributes
    AXAttributeStringSetStyle(attrString, node->renderer(), attrStringRange);
    AXAttributeStringSetHeadingLevel(attrString, node->renderer(), attrStringRange);
    AXAttributeStringSetBlockquoteLevel(attrString, node->renderer(), attrStringRange);
    AXAttributeStringSetLanguage(attrString, node->renderer(), attrStringRange);
}

NSRange makeNSRange(std::optional<SimpleRange> range)
{
    if (!range)
        return NSMakeRange(NSNotFound, 0);
    
    auto& document = range->start.document();
    auto* frame = document.frame();
    if (!frame)
        return NSMakeRange(NSNotFound, 0);

    auto* rootEditableElement = frame->selection().selection().rootEditableElement();
    auto* scope = rootEditableElement ? rootEditableElement : document.documentElement();
    if (!scope)
        return NSMakeRange(NSNotFound, 0);

    // Mouse events may cause TSM to attempt to create an NSRange for a portion of the view
    // that is not inside the current editable region. These checks ensure we don't produce
    // potentially invalid data when responding to such requests.
    if (!scope->contains(range->start.container.ptr()) || !scope->contains(range->end.container.ptr()))
        return NSMakeRange(NSNotFound, 0);

    return NSMakeRange(characterCount({ { *scope, 0 }, range->start }), characterCount(*range));
}

std::optional<SimpleRange> makeDOMRange(Document* document, NSRange range)
{
    if (range.location == NSNotFound)
        return std::nullopt;

    // our critical assumption is that we are only called by input methods that
    // concentrate on a given area containing the selection
    // We have to do this because of text fields and textareas. The DOM for those is not
    // directly in the document DOM, so serialization is problematic. Our solution is
    // to use the root editable element of the selection start as the positional base.
    // That fits with AppKit's idea of an input context.
    auto selectionRoot = document->frame()->selection().selection().rootEditableElement();
    auto scope = selectionRoot ? selectionRoot : document->documentElement();
    if (!scope)
        return std::nullopt;

    return resolveCharacterRange(makeRangeSelectingNodeContents(*scope), range);
}

// Returns an array of strings and AXObject wrappers corresponding to the text
// runs and replacement nodes included in the given range.
- (NSArray *)contentForSimpleRange:(const SimpleRange&)range attributed:(BOOL)attributed
{
    auto array = adoptNS([[NSMutableArray alloc] init]);

    // Iterate over the range to build the AX attributed string.
    TextIterator it(range);
    for (; !it.atEnd(); it.advance()) {
        Node& node = it.range().start.container;

        // Non-zero length means textual node, zero length means replaced node (AKA "attachments" in AX).
        if (it.text().length()) {
            if (!attributed) {
                // First check if this is represented by a link.
                auto* linkObject = AccessibilityObject::anchorElementForNode(&node);
                if (addObjectWrapperToArray(linkObject, array.get()))
                    continue;

                // Next check if this region is represented by a heading.
                auto* headingObject = AccessibilityObject::headingElementForNode(&node);
                if (addObjectWrapperToArray(headingObject, array.get()))
                    continue;

                StringView listMarkerText = AccessibilityObject::listMarkerTextForNodeAndPosition(&node, makeContainerOffsetPosition(it.range().start));
                if (!listMarkerText.isEmpty())
                    [array addObject:listMarkerText.createNSString().get()];
                // There was not an element representation, so just return the text.
                [array addObject:it.text().createNSString().get()];
            } else {
                StringView listMarkerText = AccessibilityObject::listMarkerTextForNodeAndPosition(&node, makeContainerOffsetPosition(it.range().start));
                if (!listMarkerText.isEmpty()) {
                    auto attrString = adoptNS([[NSMutableAttributedString alloc] init]);
                    AXAttributedStringAppendText(attrString.get(), &node, listMarkerText);
                    [array addObject:attrString.get()];
                }

                auto attrString = adoptNS([[NSMutableAttributedString alloc] init]);
                AXAttributedStringAppendText(attrString.get(), &node, it.text());
                [array addObject:attrString.get()];
            }
        } else {
            if (Node* replacedNode = it.node()) {
                auto* object = self.axBackingObject->axObjectCache()->getOrCreate(replacedNode->renderer());
                if (object)
                    addObjectWrapperToArray(object, array.get());
            }
        }
    }

    return array.autorelease();
}

- (WebCore::AXCoreObject*)baseUpdateBackingStore
{
#if PLATFORM(MAC)
    auto* backingObject = self.updateObjectBackingStore;
    if (!backingObject)
        return nullptr;
#else
    if (![self _prepareAccessibilityCall])
        return nullptr;
    auto* backingObject = self.axBackingObject;
#endif
    return backingObject;
}

- (NSArray<NSDictionary *> *)lineRectsAndText
{
    auto backingObject = self.baseUpdateBackingStore;
    if (!backingObject)
        return nil;
    
    auto range = backingObject->elementRange();
    if (!range)
        return nil;

    Vector<std::pair<IntRect, RetainPtr<NSAttributedString>>> lines;
    auto start = VisiblePosition { makeContainerOffsetPosition(range->start) };
    auto rangeEnd = VisiblePosition { makeContainerOffsetPosition(range->end) };
    while (!start.isNull() && start <= rangeEnd) {
        auto end = backingObject->nextLineEndPosition(start);
        if (end <= start)
            break;

        auto rect = backingObject->boundsForVisiblePositionRange({start, end});

        auto lineRange = makeSimpleRange(start, end);
        if (!lineRange)
            break;

        NSArray *content = [self contentForSimpleRange:*lineRange attributed:YES];
        auto text = adoptNS([[NSMutableAttributedString alloc] init]);
        for (id item in content) {
            if ([item isKindOfClass:NSAttributedString.class])
                [text appendAttributedString:item];
            else if ([item isKindOfClass:WebAccessibilityObjectWrapper.class]) {
#if PLATFORM(MAC)
                auto *wrapper = static_cast<WebAccessibilityObjectWrapper *>(item);
                auto* object = wrapper.axBackingObject;
                if (!object)
                    continue;

                NSString *label;
                switch (object->roleValue()) {
                case AccessibilityRole::StaticText:
                    label = object->stringValue();
                    break;
                case AccessibilityRole::Image: {
                    String name = object->titleAttributeValue();
                    if (name.isEmpty())
                        name = object->descriptionAttributeValue();
                    label = name;
                    break;
                }
                default:
                    label = nil;
                    break;
                }
#else
                NSString *label = static_cast<WebAccessibilityObjectWrapper *>(item).accessibilityLabel;
#endif
                if (!label)
                    continue;

                auto attributedLabel = adoptNS([[NSAttributedString alloc] initWithString:label]);
                [text appendAttributedString:attributedLabel.get()];
            }
        }
        lines.append({rect, text});

        start = end;
        // If start is at a hard breakline "\n", move to the beginning of the next line.
        while (isEndOfLine(start)) {
            end = start.next();
            auto endOfLineRange = makeSimpleRange(start, end);
            if (!endOfLineRange)
                break;

            TextIterator it(*endOfLineRange);
            if (it.atEnd() || it.text().length() != 1 || it.text()[0] != '\n')
                break;

            start = end;
        }
    }

    if (lines.isEmpty())
        return nil;
    return createNSArray(lines, [self] (const auto& line) {
        return @{ @"rect": [NSValue valueWithRect:[self convertRectToSpace:FloatRect(line.first) space:AccessibilityConversionSpace::Screen]],
                  @"text": line.second.get() };
    }).autorelease();
}

- (NSString *)ariaLandmarkRoleDescription
{
    return self.axBackingObject->ariaLandmarkRoleDescription();
}

- (NSString *)accessibilityPlatformMathSubscriptKey
{
    ASSERT_NOT_REACHED();
    return nil;
}

- (NSString *)accessibilityPlatformMathSuperscriptKey
{
    ASSERT_NOT_REACHED();
    return nil;    
}

- (NSArray *)accessibilityMathPostscriptPairs
{
    AccessibilityObject::AccessibilityMathMultiscriptPairs pairs;
    self.axBackingObject->mathPostscripts(pairs);
    return convertMathPairsToNSArray(pairs, [self accessibilityPlatformMathSubscriptKey], [self accessibilityPlatformMathSuperscriptKey]);
}

- (NSArray *)accessibilityMathPrescriptPairs
{
    AccessibilityObject::AccessibilityMathMultiscriptPairs pairs;
    self.axBackingObject->mathPrescripts(pairs);
    return convertMathPairsToNSArray(pairs, [self accessibilityPlatformMathSubscriptKey], [self accessibilityPlatformMathSuperscriptKey]);
}

- (NSDictionary<NSString *, id> *)baseAccessibilityResolvedEditingStyles
{
    NSMutableDictionary<NSString *, id> *results = [NSMutableDictionary dictionary];
    auto editingStyles = self.axBackingObject->resolvedEditingStyles();
    for (String& key : editingStyles.keys()) {
        auto value = editingStyles.get(key);
        id result = WTF::switchOn(value,
            [] (String& typedValue) -> id { return (NSString *)typedValue; },
            [] (bool& typedValue) -> id { return @(typedValue); },
            [] (int& typedValue) -> id { return @(typedValue); },
            [] (auto&) { return nil; }
        );
        results[(NSString *)key] = result;
    }
    return results;
}

// This is set by DRT when it wants to listen for notifications.
static BOOL accessibilityShouldRepostNotifications;
+ (void)accessibilitySetShouldRepostNotifications:(BOOL)repost
{
    accessibilityShouldRepostNotifications = repost;
#if PLATFORM(MAC)
    AXObjectCache::setShouldRepostNotificationsForTests(repost);
#endif
}

- (void)accessibilityPostedNotification:(NSString *)notificationName
{
    if (accessibilityShouldRepostNotifications)
        [self accessibilityPostedNotification:notificationName userInfo:nil];
}

static bool isValueTypeSupported(id value)
{
    return [value isKindOfClass:[NSString class]] || [value isKindOfClass:[NSNumber class]] || [value isKindOfClass:[WebAccessibilityObjectWrapperBase class]];
}

static NSArray *arrayRemovingNonSupportedTypes(NSArray *array)
{
    ASSERT([array isKindOfClass:[NSArray class]]);
    auto mutableArray = adoptNS([array mutableCopy]);
    for (NSUInteger i = 0; i < [mutableArray count];) {
        id value = [mutableArray objectAtIndex:i];
        if ([value isKindOfClass:[NSDictionary class]])
            [mutableArray replaceObjectAtIndex:i withObject:dictionaryRemovingNonSupportedTypes(value)];
        else if ([value isKindOfClass:[NSArray class]])
            [mutableArray replaceObjectAtIndex:i withObject:arrayRemovingNonSupportedTypes(value)];
        else if (!isValueTypeSupported(value)) {
            [mutableArray removeObjectAtIndex:i];
            continue;
        }
        i++;
    }
    return mutableArray.autorelease();
}

static NSDictionary *dictionaryRemovingNonSupportedTypes(NSDictionary *dictionary)
{
    if (!dictionary)
        return nil;
    ASSERT([dictionary isKindOfClass:[NSDictionary class]]);
    auto mutableDictionary = adoptNS([dictionary mutableCopy]);
    for (NSString *key in dictionary) {
        id value = [dictionary objectForKey:key];
        if ([value isKindOfClass:[NSDictionary class]])
            [mutableDictionary setObject:dictionaryRemovingNonSupportedTypes(value) forKey:key];
        else if ([value isKindOfClass:[NSArray class]])
            [mutableDictionary setObject:arrayRemovingNonSupportedTypes(value) forKey:key];
        else if (!isValueTypeSupported(value))
            [mutableDictionary removeObjectForKey:key];
    }
    return mutableDictionary.autorelease();
}

- (void)accessibilityPostedNotification:(NSString *)notificationName userInfo:(NSDictionary *)userInfo
{
    if (accessibilityShouldRepostNotifications) {
        ASSERT(notificationName);
        userInfo = dictionaryRemovingNonSupportedTypes(userInfo);
        NSDictionary *info = [NSDictionary dictionaryWithObjectsAndKeys:notificationName, @"notificationName", userInfo, @"userInfo", nil];
        [[NSNotificationCenter defaultCenter] postNotificationName:@"AXDRTNotification" object:self userInfo:info];
    }
}

- (NSString *)innerHTML
{
    if (RefPtr<AXCoreObject> backingObject = self.axBackingObject)
        return backingObject->innerHTML();
    return nil;
}

- (NSString *)outerHTML
{
    if (RefPtr<AXCoreObject> backingObject = self.axBackingObject)
        return backingObject->outerHTML();
    return nil;
}

#pragma mark Search helpers

typedef HashMap<String, AccessibilitySearchKey> AccessibilitySearchKeyMap;

struct SearchKeyEntry {
    String key;
    AccessibilitySearchKey value;
};

static AccessibilitySearchKeyMap* createAccessibilitySearchKeyMap()
{
    const SearchKeyEntry searchKeys[] = {
        { NSAccessibilityAnyTypeSearchKey, AccessibilitySearchKey::AnyType },
        { NSAccessibilityArticleSearchKey, AccessibilitySearchKey::Article },
        { NSAccessibilityBlockquoteSameLevelSearchKey, AccessibilitySearchKey::BlockquoteSameLevel },
        { NSAccessibilityBlockquoteSearchKey, AccessibilitySearchKey::Blockquote },
        { NSAccessibilityBoldFontSearchKey, AccessibilitySearchKey::BoldFont },
        { NSAccessibilityButtonSearchKey, AccessibilitySearchKey::Button },
        { NSAccessibilityCheckBoxSearchKey, AccessibilitySearchKey::CheckBox },
        { NSAccessibilityControlSearchKey, AccessibilitySearchKey::Control },
        { NSAccessibilityDifferentTypeSearchKey, AccessibilitySearchKey::DifferentType },
        { NSAccessibilityFontChangeSearchKey, AccessibilitySearchKey::FontChange },
        { NSAccessibilityFontColorChangeSearchKey, AccessibilitySearchKey::FontColorChange },
        { NSAccessibilityFrameSearchKey, AccessibilitySearchKey::Frame },
        { NSAccessibilityGraphicSearchKey, AccessibilitySearchKey::Graphic },
        { NSAccessibilityHeadingLevel1SearchKey, AccessibilitySearchKey::HeadingLevel1 },
        { NSAccessibilityHeadingLevel2SearchKey, AccessibilitySearchKey::HeadingLevel2 },
        { NSAccessibilityHeadingLevel3SearchKey, AccessibilitySearchKey::HeadingLevel3 },
        { NSAccessibilityHeadingLevel4SearchKey, AccessibilitySearchKey::HeadingLevel4 },
        { NSAccessibilityHeadingLevel5SearchKey, AccessibilitySearchKey::HeadingLevel5 },
        { NSAccessibilityHeadingLevel6SearchKey, AccessibilitySearchKey::HeadingLevel6 },
        { NSAccessibilityHeadingSameLevelSearchKey, AccessibilitySearchKey::HeadingSameLevel },
        { NSAccessibilityHeadingSearchKey, AccessibilitySearchKey::Heading },
        { NSAccessibilityHighlightedSearchKey, AccessibilitySearchKey::Highlighted },
        { NSAccessibilityKeyboardFocusableSearchKey, AccessibilitySearchKey::KeyboardFocusable },
        { NSAccessibilityItalicFontSearchKey, AccessibilitySearchKey::ItalicFont },
        { NSAccessibilityLandmarkSearchKey, AccessibilitySearchKey::Landmark },
        { NSAccessibilityLinkSearchKey, AccessibilitySearchKey::Link },
        { NSAccessibilityListSearchKey, AccessibilitySearchKey::List },
        { NSAccessibilityLiveRegionSearchKey, AccessibilitySearchKey::LiveRegion },
        { NSAccessibilityMisspelledWordSearchKey, AccessibilitySearchKey::MisspelledWord },
        { NSAccessibilityOutlineSearchKey, AccessibilitySearchKey::Outline },
        { NSAccessibilityPlainTextSearchKey, AccessibilitySearchKey::PlainText },
        { NSAccessibilityRadioGroupSearchKey, AccessibilitySearchKey::RadioGroup },
        { NSAccessibilitySameTypeSearchKey, AccessibilitySearchKey::SameType },
        { NSAccessibilityStaticTextSearchKey, AccessibilitySearchKey::StaticText },
        { NSAccessibilityStyleChangeSearchKey, AccessibilitySearchKey::StyleChange },
        { NSAccessibilityTableSameLevelSearchKey, AccessibilitySearchKey::TableSameLevel },
        { NSAccessibilityTableSearchKey, AccessibilitySearchKey::Table },
        { NSAccessibilityTextFieldSearchKey, AccessibilitySearchKey::TextField },
        { NSAccessibilityUnderlineSearchKey, AccessibilitySearchKey::Underline },
        { NSAccessibilityUnvisitedLinkSearchKey, AccessibilitySearchKey::UnvisitedLink },
        { NSAccessibilityVisitedLinkSearchKey, AccessibilitySearchKey::VisitedLink }
    };
    
    AccessibilitySearchKeyMap* searchKeyMap = new AccessibilitySearchKeyMap;
    for (size_t i = 0; i < WTF_ARRAY_LENGTH(searchKeys); i++)
        searchKeyMap->set(searchKeys[i].key, searchKeys[i].value);
    
    return searchKeyMap;
}

static AccessibilitySearchKey accessibilitySearchKeyForString(const String& value)
{
    if (value.isEmpty())
        return AccessibilitySearchKey::AnyType;
    
    static const AccessibilitySearchKeyMap* searchKeyMap = createAccessibilitySearchKeyMap();
    AccessibilitySearchKey searchKey = searchKeyMap->get(value);    
    return static_cast<int>(searchKey) ? searchKey : AccessibilitySearchKey::AnyType;
}

static std::optional<AccessibilitySearchKey> makeVectorElement(const AccessibilitySearchKey*, id arrayElement)
{
    if (![arrayElement isKindOfClass:NSString.class])
        return std::nullopt;
    return { { accessibilitySearchKeyForString(arrayElement) } };
}

AccessibilitySearchCriteria accessibilitySearchCriteriaForSearchPredicateParameterizedAttribute(const NSDictionary *parameterizedAttribute)
{
    NSString *directionParameter = [parameterizedAttribute objectForKey:@"AXDirection"];
    NSNumber *immediateDescendantsOnlyParameter = [parameterizedAttribute objectForKey:NSAccessibilityImmediateDescendantsOnly];
    NSNumber *resultsLimitParameter = [parameterizedAttribute objectForKey:@"AXResultsLimit"];
    NSString *searchTextParameter = [parameterizedAttribute objectForKey:@"AXSearchText"];
    WebAccessibilityObjectWrapperBase *startElementParameter = [parameterizedAttribute objectForKey:@"AXStartElement"];
    NSNumber *visibleOnlyParameter = [parameterizedAttribute objectForKey:@"AXVisibleOnly"];
    id searchKeyParameter = [parameterizedAttribute objectForKey:@"AXSearchKey"];
    
    AccessibilitySearchDirection direction = AccessibilitySearchDirection::Next;
    if ([directionParameter isKindOfClass:[NSString class]])
        direction = [directionParameter isEqualToString:@"AXDirectionNext"] ? AccessibilitySearchDirection::Next : AccessibilitySearchDirection::Previous;
    
    bool immediateDescendantsOnly = false;
    if ([immediateDescendantsOnlyParameter isKindOfClass:[NSNumber class]])
        immediateDescendantsOnly = [immediateDescendantsOnlyParameter boolValue];
    
    unsigned resultsLimit = 0;
    if ([resultsLimitParameter isKindOfClass:[NSNumber class]])
        resultsLimit = [resultsLimitParameter unsignedIntValue];
    
    String searchText;
    if ([searchTextParameter isKindOfClass:[NSString class]])
        searchText = searchTextParameter;
    
    AXCoreObject* startElement = nullptr;
    if ([startElementParameter isKindOfClass:[WebAccessibilityObjectWrapperBase class]])
        startElement = [startElementParameter axBackingObject];

    bool visibleOnly = false;
    if ([visibleOnlyParameter isKindOfClass:[NSNumber class]])
        visibleOnly = [visibleOnlyParameter boolValue];
    
    AccessibilitySearchCriteria criteria = AccessibilitySearchCriteria(startElement, direction, searchText, resultsLimit, visibleOnly, immediateDescendantsOnly);
    
    if ([searchKeyParameter isKindOfClass:[NSString class]])
        criteria.searchKeys.append(accessibilitySearchKeyForString(searchKeyParameter));
    else if ([searchKeyParameter isKindOfClass:[NSArray class]])
        criteria.searchKeys = makeVector<AccessibilitySearchKey>(searchKeyParameter);

    return criteria;
}

@end

#endif // ENABLE(ACCESSIBILITY)
