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

#import "AXObjectCache.h"
#import "AccessibilityARIAGridRow.h"
#import "AccessibilityList.h"
#import "AccessibilityListBox.h"
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
#import "Font.h"
#import "FontCascade.h"
#import "Frame.h"
#import "FrameLoaderClient.h"
#import "FrameSelection.h"
#import "HTMLAnchorElement.h"
#import "HTMLAreaElement.h"
#import "HTMLFrameOwnerElement.h"
#import "HTMLImageElement.h"
#import "HTMLInputElement.h"
#import "HTMLNames.h"
#import "HTMLTextAreaElement.h"
#import "LocalizedStrings.h"
#import "Page.h"
#import "RenderTextControl.h"
#import "RenderView.h"
#import "RenderWidget.h"
#import "ScrollView.h"
#import "TextCheckerClient.h"
#import "TextCheckingHelper.h"
#import "VisibleUnits.h"
#import "WebCoreFrameView.h"
#import "WebCoreObjCExtras.h"
#import "WebCoreSystemInterface.h"
#import "htmlediting.h"

using namespace WebCore;
using namespace HTMLNames;

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

@implementation WebAccessibilityObjectWrapperBase

- (id)initWithAccessibilityObject:(AccessibilityObject*)axObject
{
    if (!(self = [super init]))
        return nil;

    m_object = axObject;
    return self;
}

- (void)detach
{
    m_object = nullptr;
}

- (BOOL)updateObjectBackingStore
{
    // Calling updateBackingStore() can invalidate this element so self must be retained.
    // If it does become invalidated, m_object will be nil.
    [[self retain] autorelease];
    
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
    return m_object;
}

// FIXME: Different kinds of elements are putting the title tag to use in different
// AX fields. This should be rectified, but in the initial patch I want to achieve
// parity with existing behavior.
- (BOOL)titleTagShouldBeUsedInDescriptionField
{
    return (m_object->isLink() && !m_object->isImageMapLink()) || m_object->isImage();
}

// On iOS, we don't have to return the value in the title. We can return the actual title, given the API.
- (BOOL)fileUploadButtonReturnsValueInTitle
{
    return YES;
}

// This should be the "visible" text that's actually on the screen if possible.
// If there's alternative text, that can override the title.
- (NSString *)baseAccessibilityTitle
{
    // Static text objects should not have a title. Its content is communicated in its AXValue.
    if (m_object->roleValue() == StaticTextRole)
        return [NSString string];

    // A file upload button presents a challenge because it has button text and a value, but the
    // API doesn't support this paradigm.
    // The compromise is to return the button type in the role description and the value of the file path in the title
    if (m_object->isFileUploadButton() && [self fileUploadButtonReturnsValueInTitle])
        return m_object->stringValue();
    
    Vector<AccessibilityText> textOrder;
    m_object->accessibilityText(textOrder);
    
    for (const auto& text : textOrder) {
        // If we have alternative text, then we should not expose a title.
        if (text.textSource == AlternativeText)
            break;
        
        // Once we encounter visible text, or the text from our children that should be used foremost.
        if (text.textSource == VisibleText || text.textSource == ChildrenText)
            return text.text;
        
        // If there's an element that labels this object and it's not exposed, then we should use
        // that text as our title.
        if (text.textSource == LabelByElementText && !m_object->exposesTitleUIElement())
            return text.text;
    }
    
    return [NSString string];
}

- (NSString *)baseAccessibilityDescription
{
    // Static text objects should not have a description. Its content is communicated in its AXValue.
    // One exception is the media control labels that have a value and a description. Those are set programatically.
    if (m_object->roleValue() == StaticTextRole && !m_object->isMediaControlLabel())
        return [NSString string];
    
    Vector<AccessibilityText> textOrder;
    m_object->accessibilityText(textOrder);

    NSMutableString *returnText = [NSMutableString string];
    bool visibleTextAvailable = false;
    for (const auto& text : textOrder) {
        if (text.textSource == AlternativeText) {
            [returnText appendString:text.text];
            break;
        }
        
        switch (text.textSource) {
        // These are sub-components of one element (Attachment) that are re-combined in OSX and iOS.
        case TitleText:
        case SubtitleText:
        case ActionText: {
            if (!text.text.length())
                break;
            if ([returnText length])
                [returnText appendString:@", "];
            [returnText appendString:text.text];
            break;
        }
        case VisibleText:
        case ChildrenText:
        case LabelByElementText:
            visibleTextAvailable = true;
            break;
        default:
            break;
        }
        
        if (text.textSource == TitleTagText && !visibleTextAvailable) {
            [returnText appendString:text.text];
            break;
        }
    }
    
    return returnText;
}

- (NSString *)baseAccessibilityHelpText
{
    Vector<AccessibilityText> textOrder;
    m_object->accessibilityText(textOrder);
    
    bool descriptiveTextAvailable = false;
    for (const auto& text : textOrder) {
        if (text.textSource == HelpText || text.textSource == SummaryText)
            return text.text;
        
        // If an element does NOT have other descriptive text the title tag should be used as its descriptive text.
        // But, if those ARE available, then the title tag should be used for help text instead.
        switch (text.textSource) {
        case AlternativeText:
        case VisibleText:
        case ChildrenText:
        case LabelByElementText:
            descriptiveTextAvailable = true;
            break;
        default:
            break;
        }
        
        if (text.textSource == TitleTagText && descriptiveTextAvailable)
            return text.text;
    }
    
    return [NSString string];
}

struct PathConversionInfo {
    WebAccessibilityObjectWrapperBase *wrapper;
    CGMutablePathRef path;
};

static void convertPathToScreenSpaceFunction(PathConversionInfo& conversion, const PathElement& element)
{
    WebAccessibilityObjectWrapperBase *wrapper = conversion.wrapper;
    CGMutablePathRef newPath = conversion.path;
    switch (element.type) {
    case PathElementMoveToPoint:
    {
        CGPoint newPoint = [wrapper convertPointToScreenSpace:element.points[0]];
        CGPathMoveToPoint(newPath, nil, newPoint.x, newPoint.y);
        break;
    }
    case PathElementAddLineToPoint:
    {
        CGPoint newPoint = [wrapper convertPointToScreenSpace:element.points[0]];
        CGPathAddLineToPoint(newPath, nil, newPoint.x, newPoint.y);
        break;
    }
    case PathElementAddQuadCurveToPoint:
    {
        CGPoint newPoint1 = [wrapper convertPointToScreenSpace:element.points[0]];
        CGPoint newPoint2 = [wrapper convertPointToScreenSpace:element.points[1]];
        CGPathAddQuadCurveToPoint(newPath, nil, newPoint1.x, newPoint1.y, newPoint2.x, newPoint2.y);
        break;
    }
    case PathElementAddCurveToPoint:
    {
        CGPoint newPoint1 = [wrapper convertPointToScreenSpace:element.points[0]];
        CGPoint newPoint2 = [wrapper convertPointToScreenSpace:element.points[1]];
        CGPoint newPoint3 = [wrapper convertPointToScreenSpace:element.points[2]];
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
    return (CGPathRef)[(id)conversion.path autorelease];
}

- (CGPoint)convertPointToScreenSpace:(FloatPoint &)point
{
    UNUSED_PARAM(point);
    ASSERT_NOT_REACHED();
    return CGPointZero;
}

- (NSString *)ariaLandmarkRoleDescription
{
    switch (m_object->roleValue()) {
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
    case ApplicationAlertRole:
        return AXARIAContentGroupText(@"ARIAApplicationAlert");
    case ApplicationAlertDialogRole:
        return AXARIAContentGroupText(@"ARIAApplicationAlertDialog");
    case ApplicationDialogRole:
        return AXARIAContentGroupText(@"ARIAApplicationDialog");
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
    case DocumentMathRole:
        return AXARIAContentGroupText(@"ARIADocumentMath");
    case DocumentNoteRole:
        return AXARIAContentGroupText(@"ARIADocumentNote");
    case DocumentRegionRole:
        return AXARIAContentGroupText(@"ARIADocumentRegion");
    case UserInterfaceTooltipRole:
        return AXARIAContentGroupText(@"ARIAUserInterfaceTooltip");
    case TabPanelRole:
        return AXARIAContentGroupText(@"ARIATabPanel");
    case WebApplicationRole:
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

@end

#endif // HAVE(ACCESSIBILITY)
