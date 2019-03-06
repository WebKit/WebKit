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

#if HAVE(ACCESSIBILITY)

#import "AXIsolatedTree.h"
#import "AXIsolatedTreeNode.h"
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
#import "Chrome.h"
#import "ChromeClient.h"
#import "ColorMac.h"
#import "ContextMenuController.h"
#import "Editing.h"
#import "Font.h"
#import "FontCascade.h"
#import "Frame.h"
#import "FrameLoaderClient.h"
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
#import "TextCheckingHelper.h"
#import "VisibleUnits.h"
#import "WAKView.h"
#import "WAKWindow.h"
#import "WebCoreFrameView.h"

#if PLATFORM(MAC)
#import <pal/spi/mac/HIServicesSPI.h>
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

#define _axBackingObject self.axBackingObject

static NSArray *convertMathPairsToNSArray(const AccessibilityObject::AccessibilityMathMultiscriptPairs& pairs, NSString *subscriptKey, NSString *superscriptKey)
{
    NSMutableArray *array = [NSMutableArray arrayWithCapacity:pairs.size()];
    for (const auto& pair : pairs) {
        NSMutableDictionary *pairDictionary = [NSMutableDictionary dictionary];
        if (pair.first && pair.first->wrapper() && !pair.first->accessibilityIsIgnored())
            [pairDictionary setObject:pair.first->wrapper() forKey:subscriptKey];
        if (pair.second && pair.second->wrapper() && !pair.second->accessibilityIsIgnored())
            [pairDictionary setObject:pair.second->wrapper() forKey:superscriptKey];
        [array addObject:pairDictionary];
    }
    return array;
}

static void addChildToArray(AccessibilityObjectInterface& child, RetainPtr<NSMutableArray> array)
{
    WebAccessibilityObjectWrapper *wrapper = child.wrapper();
    // We want to return the attachment view instead of the object representing the attachment,
    // otherwise, we get palindrome errors in the AX hierarchy.
    if (child.isAttachment() && [wrapper attachmentView])
        [array.get() addObject:[wrapper attachmentView]];
    else if (wrapper)
        [array.get() addObject:wrapper];
}

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
RetainPtr<NSArray> convertToNSArray(const Vector<RefPtr<WebCore::AXIsolatedTreeNode>>& children)
{
    RetainPtr<NSMutableArray> result = [[NSMutableArray alloc] initWithCapacity:children.size()];
    for (auto& child : children)
        addChildToArray(*child, result)
    return result;
}
#endif

RetainPtr<NSArray> convertToNSArray(const WebCore::AccessibilityObject::AccessibilityChildrenVector& children)
{
    RetainPtr<NSMutableArray> result = [[NSMutableArray alloc] initWithCapacity:children.size()];
    for (auto& child : children)
        addChildToArray(*child, result);
    return result;
}

@implementation WebAccessibilityObjectWrapperBase

@synthesize identifier=_identifier;
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
@synthesize isolatedTreeIdentifier=_isolatedTreeIdentifier;
#endif

- (id)initWithAccessibilityObject:(AccessibilityObject*)axObject
{
    if (!(self = [super init]))
        return nil;

    m_object = axObject;
    _identifier = m_object->axObjectID();

    return self;
}

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
- (RefPtr<WebCore::AXIsolatedTreeNode>)isolatedTreeNode
{
    RELEASE_ASSERT(!isMainThread());

    if (!_identifier)
        return nullptr;

    if (m_isolatedTreeNode)
        return m_isolatedTreeNode;

    m_isolatedTreeNode = AXIsolatedTree::nodeInTreeForID(_isolatedTreeIdentifier, _identifier);
    return m_isolatedTreeNode;
}
#endif

- (void)detach
{
    m_object = nullptr;
    _identifier = 0;

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    m_isolatedTreeNode = nullptr;
    _isolatedTreeIdentifier = 0;
#endif
}

- (BOOL)updateObjectBackingStore
{
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (_AXUIElementRequestServicedBySecondaryAXThread()) {
        RELEASE_ASSERT(!isMainThread());
        if (auto treeNode = self.isolatedTreeNode) {
            if (auto tree = treeNode->tree())
                tree->applyPendingChanges();
        }
        return _identifier;
    }
#endif
    
    // Calling updateBackingStore() can invalidate this element so self must be retained.
    // If it does become invalidated, m_object will be nil.
    CFRetain((__bridge CFTypeRef)self);
    CFAutorelease((__bridge CFTypeRef)self);

    if (!m_object)
        return NO;
    
    m_object->updateBackingStore();
    if (!m_object)
        return NO;
    
    return YES;
}

- (id)attachmentView
{
    return nil;
}

- (AccessibilityObject*)accessibilityObject
{
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    ASSERT(!_AXUIElementRequestServicedBySecondaryAXThread());
#endif
    return m_object;
}

// This should be the "visible" text that's actually on the screen if possible.
// If there's alternative text, that can override the title.
- (NSString *)baseAccessibilityTitle
{
    return _axBackingObject->titleAttributeValue();
}

- (WebCore::AccessibilityObjectInterface*)axBackingObject
{
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (_AXUIElementRequestServicedBySecondaryAXThread())
        return self.isolatedTreeNode.get();
#endif
    return m_object;
}

- (NSString *)baseAccessibilityDescription
{
    return _axBackingObject->descriptionAttributeValue();
}

- (NSArray<NSString *> *)baseAccessibilitySpeechHint
{
    return [(NSString *)_axBackingObject->speechHintAttributeValue() componentsSeparatedByString:@" "];
}

- (NSString *)baseAccessibilityHelpText
{
    return _axBackingObject->helpTextAttributeValue();
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
    case PathElementMoveToPoint:
    {
        rect = FloatRect(element.points[0], FloatSize());
        CGPoint newPoint = [wrapper convertRectToSpace:rect space:AccessibilityConversionSpace::Screen].origin;
        CGPathMoveToPoint(newPath, nil, newPoint.x, newPoint.y);
        break;
    }
    case PathElementAddLineToPoint:
    {
        rect = FloatRect(element.points[0], FloatSize());
        CGPoint newPoint = [wrapper convertRectToSpace:rect space:AccessibilityConversionSpace::Screen].origin;
        CGPathAddLineToPoint(newPath, nil, newPoint.x, newPoint.y);
        break;
    }
    case PathElementAddQuadCurveToPoint:
    {
        rect = FloatRect(element.points[0], FloatSize());
        CGPoint newPoint1 = [wrapper convertRectToSpace:rect space:AccessibilityConversionSpace::Screen].origin;

        rect = FloatRect(element.points[1], FloatSize());
        CGPoint newPoint2 = [wrapper convertRectToSpace:rect space:AccessibilityConversionSpace::Screen].origin;
        CGPathAddQuadCurveToPoint(newPath, nil, newPoint1.x, newPoint1.y, newPoint2.x, newPoint2.y);
        break;
    }
    case PathElementAddCurveToPoint:
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
    case PathElementCloseSubpath:
    {
        CGPathCloseSubpath(newPath);
        break;
    }
    }
}

- (CGPathRef)convertPathToScreenSpace:(Path &)path
{
    PathConversionInfo conversion = { self, CGPathCreateMutable() };
    path.apply([&conversion](const PathElement& pathElement) {
        convertPathToScreenSpaceFunction(conversion, pathElement);
    });
    CFAutorelease(conversion.path);
    return conversion.path;
}

- (id)_accessibilityWebDocumentView
{
    ASSERT_NOT_REACHED();
    // Overridden by sub-classes
    return nil;
}

- (CGRect)convertRectToSpace:(WebCore::FloatRect &)rect space:(AccessibilityConversionSpace)space
{
    if (!m_object)
        return CGRectZero;
    
    CGSize size = CGSizeMake(rect.size().width(), rect.size().height());
    CGPoint point = CGPointMake(rect.x(), rect.y());
    
    CGRect cgRect = CGRectMake(point.x, point.y, size.width, size.height);

    // WebKit1 code path... platformWidget() exists.
    FrameView* frameView = m_object->documentFrameView();
#if PLATFORM(IOS_FAMILY)
    WAKView* documentView = frameView ? frameView->documentView() : nullptr;
    if (documentView) {
        cgRect = [documentView convertRect:cgRect toView:nil];
        
        // we need the web document view to give us our final screen coordinates
        // because that can take account of the scroller
        id webDocument = [self _accessibilityWebDocumentView];
        if (webDocument)
            cgRect = [webDocument convertRect:cgRect toView:nil];
        return cgRect;
    }
#else
    if (frameView && frameView->platformWidget()) {
        NSRect nsRect = NSRectFromCGRect(cgRect);
        NSView* view = frameView->documentView();
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        nsRect = [[view window] convertRectToScreen:[view convertRect:nsRect toView:nil]];
        ALLOW_DEPRECATED_DECLARATIONS_END
        return NSRectToCGRect(nsRect);
    }
#endif
    else
        return static_cast<CGRect>(m_object->convertFrameToSpace(rect, space));
}

- (NSString *)ariaLandmarkRoleDescription
{
    switch (_axBackingObject->roleValue()) {
    case AccessibilityRole::LandmarkBanner:
        return AXARIAContentGroupText(@"ARIALandmarkBanner");
    case AccessibilityRole::LandmarkComplementary:
        return AXARIAContentGroupText(@"ARIALandmarkComplementary");
    case AccessibilityRole::LandmarkContentInfo:
        return AXARIAContentGroupText(@"ARIALandmarkContentInfo");
    case AccessibilityRole::LandmarkMain:
        return AXARIAContentGroupText(@"ARIALandmarkMain");
    case AccessibilityRole::LandmarkNavigation:
        return AXARIAContentGroupText(@"ARIALandmarkNavigation");
    case AccessibilityRole::LandmarkDocRegion:
    case AccessibilityRole::LandmarkRegion:
        return AXARIAContentGroupText(@"ARIALandmarkRegion");
    case AccessibilityRole::LandmarkSearch:
        return AXARIAContentGroupText(@"ARIALandmarkSearch");
    case AccessibilityRole::ApplicationAlert:
        return AXARIAContentGroupText(@"ARIAApplicationAlert");
    case AccessibilityRole::ApplicationAlertDialog:
        return AXARIAContentGroupText(@"ARIAApplicationAlertDialog");
    case AccessibilityRole::ApplicationDialog:
        return AXARIAContentGroupText(@"ARIAApplicationDialog");
    case AccessibilityRole::ApplicationLog:
        return AXARIAContentGroupText(@"ARIAApplicationLog");
    case AccessibilityRole::ApplicationMarquee:
        return AXARIAContentGroupText(@"ARIAApplicationMarquee");
    case AccessibilityRole::ApplicationStatus:
        return AXARIAContentGroupText(@"ARIAApplicationStatus");
    case AccessibilityRole::ApplicationTimer:
        return AXARIAContentGroupText(@"ARIAApplicationTimer");
    case AccessibilityRole::Document:
        return AXARIAContentGroupText(@"ARIADocument");
    case AccessibilityRole::DocumentArticle:
        return AXARIAContentGroupText(@"ARIADocumentArticle");
    case AccessibilityRole::DocumentMath:
        return AXARIAContentGroupText(@"ARIADocumentMath");
    case AccessibilityRole::DocumentNote:
        return AXARIAContentGroupText(@"ARIADocumentNote");
    case AccessibilityRole::UserInterfaceTooltip:
        return AXARIAContentGroupText(@"ARIAUserInterfaceTooltip");
    case AccessibilityRole::TabPanel:
        return AXARIAContentGroupText(@"ARIATabPanel");
    case AccessibilityRole::WebApplication:
        return AXARIAContentGroupText(@"ARIAWebApplication");
    default:
        return nil;
    }
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
    m_object->mathPostscripts(pairs);
    return convertMathPairsToNSArray(pairs, [self accessibilityPlatformMathSubscriptKey], [self accessibilityPlatformMathSuperscriptKey]);
}

- (NSArray *)accessibilityMathPrescriptPairs
{
    AccessibilityObject::AccessibilityMathMultiscriptPairs pairs;
    m_object->mathPrescripts(pairs);
    return convertMathPairsToNSArray(pairs, [self accessibilityPlatformMathSubscriptKey], [self accessibilityPlatformMathSuperscriptKey]);
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
    NSMutableArray *mutableArray = [array mutableCopy];
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
    return [mutableArray autorelease];
}

static NSDictionary *dictionaryRemovingNonSupportedTypes(NSDictionary *dictionary)
{
    if (!dictionary)
        return nil;
    ASSERT([dictionary isKindOfClass:[NSDictionary class]]);
    NSMutableDictionary *mutableDictionary = [dictionary mutableCopy];
    for (NSString *key in dictionary) {
        id value = [dictionary objectForKey:key];
        if ([value isKindOfClass:[NSDictionary class]])
            [mutableDictionary setObject:dictionaryRemovingNonSupportedTypes(value) forKey:key];
        else if ([value isKindOfClass:[NSArray class]])
            [mutableDictionary setObject:arrayRemovingNonSupportedTypes(value) forKey:key];
        else if (!isValueTypeSupported(value))
            [mutableDictionary removeObjectForKey:key];
    }
    return [mutableDictionary autorelease];
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
    
    AccessibilityObject* startElement = nullptr;
    if ([startElementParameter isKindOfClass:[WebAccessibilityObjectWrapperBase class]])
        startElement = [startElementParameter accessibilityObject];
    
    bool visibleOnly = false;
    if ([visibleOnlyParameter isKindOfClass:[NSNumber class]])
        visibleOnly = [visibleOnlyParameter boolValue];
    
    AccessibilitySearchCriteria criteria = AccessibilitySearchCriteria(startElement, direction, searchText, resultsLimit, visibleOnly, immediateDescendantsOnly);
    
    if ([searchKeyParameter isKindOfClass:[NSString class]])
        criteria.searchKeys.append(accessibilitySearchKeyForString(searchKeyParameter));
    else if ([searchKeyParameter isKindOfClass:[NSArray class]]) {
        size_t searchKeyCount = static_cast<size_t>([searchKeyParameter count]);
        criteria.searchKeys.reserveInitialCapacity(searchKeyCount);
        for (size_t i = 0; i < searchKeyCount; ++i) {
            NSString *searchKey = [searchKeyParameter objectAtIndex:i];
            if ([searchKey isKindOfClass:[NSString class]])
                criteria.searchKeys.uncheckedAppend(accessibilitySearchKeyForString(searchKey));
        }
    }
    
    return criteria;
}

@end

#endif // HAVE(ACCESSIBILITY)
