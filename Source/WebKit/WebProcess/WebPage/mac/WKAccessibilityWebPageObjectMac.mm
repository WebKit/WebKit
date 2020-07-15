/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "WKAccessibilityWebPageObjectMac.h"

#if PLATFORM(MAC)

#import "ApplicationServicesSPI.h"
#import "PluginView.h"
#import "WebFrame.h"
#import "WebPage.h"
#import "WKArray.h"
#import "WKNumber.h"
#import "WKRetainPtr.h"
#import "WKSharedAPICast.h"
#import "WKString.h"
#import "WKStringCF.h"
#import <WebCore/AXObjectCache.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameView.h>
#import <WebCore/Page.h>
#import <WebCore/PageOverlayController.h>
#import <WebCore/ScrollView.h>
#import <WebCore/Scrollbar.h>
#import <WebCore/WebAccessibilityObjectWrapperMac.h>
#import <pal/spi/cocoa/NSAccessibilitySPI.h>
#import <pal/spi/mac/HIServicesSPI.h>
#import <wtf/cocoa/VectorCocoa.h>

namespace ax = WebCore::Accessibility;

@interface WKAccessibilityWebPageObject()
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
@property (nonatomic, strong) NSArray *cachedParameterizedAttributeNames;
#endif
@end

@implementation WKAccessibilityWebPageObject

#define PROTECTED_SELF protectedSelf = RetainPtr<WKAccessibilityWebPageObject>(self)

- (void)dealloc
{
    NSAccessibilityUnregisterUniqueIdForUIElement(self);
    [m_parent release];
    [super dealloc];
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (BOOL)accessibilityIsIgnored
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    return NO;
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (NSArray *)accessibilityAttributeNames
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    if (!m_attributeNames)
        m_attributeNames = adoptNS([[NSArray alloc] initWithObjects:
                            NSAccessibilityRoleAttribute, NSAccessibilityRoleDescriptionAttribute, NSAccessibilityFocusedAttribute,
                            NSAccessibilityParentAttribute, NSAccessibilityWindowAttribute, NSAccessibilityTopLevelUIElementAttribute,
                            NSAccessibilityPositionAttribute, NSAccessibilitySizeAttribute, NSAccessibilityChildrenAttribute, NSAccessibilityPrimaryScreenHeightAttribute, nil]);
    
    return m_attributeNames.get();
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (NSArray *)accessibilityParameterizedAttributeNames
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (id cachedNames = self.cachedParameterizedAttributeNames)
        return cachedNames;
#endif

    id names = ax::retrieveValueFromMainThread<RetainPtr<id>>([PROTECTED_SELF] () -> RetainPtr<id> {
        auto page = protectedSelf->m_page;
        if (!page)
            return @[];

        auto corePage = page->corePage();
        if (!corePage)
            return @[];

        return createNSArray(corePage->pageOverlayController().copyAccessibilityAttributesNames(true));
    }).autorelease();

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    self.cachedParameterizedAttributeNames = names;
#endif

    return names;
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (BOOL)accessibilityIsAttributeSettable:(NSString *)attribute
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    return NO;
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (void)accessibilitySetValue:(id)value forAttribute:(NSString *)attribute
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
}

- (NSPoint)convertScreenPointToRootView:(NSPoint)point
{
    return ax::retrieveValueFromMainThread<NSPoint>([&point, PROTECTED_SELF] () -> NSPoint {
        if (!protectedSelf->m_page)
            return point;
        return protectedSelf->m_page->screenToRootView(WebCore::IntPoint(point.x, point.y));
    });
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (NSArray *)accessibilityActionNames
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    return @[];
}

- (NSArray *)accessibilityChildren
{
    id wrapper = [self accessibilityRootObjectWrapper];
    if (!wrapper)
        return @[];
    
    return @[wrapper];
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (id)accessibilityAttributeValue:(NSString *)attribute
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    if (!WebCore::AXObjectCache::accessibilityEnabled())
        WebCore::AXObjectCache::enableAccessibility();
    
    if ([attribute isEqualToString:NSAccessibilityParentAttribute])
        return m_parent;
    
    if ([attribute isEqualToString:NSAccessibilityWindowAttribute])
        return [m_parent accessibilityAttributeValue:NSAccessibilityWindowAttribute];
    
    if ([attribute isEqualToString:NSAccessibilityTopLevelUIElementAttribute])
        return [m_parent accessibilityAttributeValue:NSAccessibilityTopLevelUIElementAttribute];
    
    if ([attribute isEqualToString:NSAccessibilityRoleAttribute])
        return NSAccessibilityGroupRole;
    
    if ([attribute isEqualToString:NSAccessibilityRoleDescriptionAttribute])
        return NSAccessibilityRoleDescription(NSAccessibilityGroupRole, nil);
    
    if ([attribute isEqualToString:NSAccessibilityFocusedAttribute])
        return @NO;
    
    if (!m_pageID)
        return nil;
    
    if ([attribute isEqualToString:NSAccessibilityPositionAttribute])
        return [self accessibilityAttributePositionValue];
    
    if ([attribute isEqualToString:NSAccessibilityPrimaryScreenHeightAttribute])
        return [[self accessibilityRootObjectWrapper] accessibilityAttributeValue:attribute];
    
    if ([attribute isEqualToString:NSAccessibilitySizeAttribute])
        return [self accessibilityAttributeSizeValue];
    
    if ([attribute isEqualToString:NSAccessibilityChildrenAttribute])
        return [self accessibilityChildren];
    
    return nil;
}

- (NSValue *)accessibilityAttributeSizeValue
{
    return ax::retrieveValueFromMainThread<RetainPtr<id>>([PROTECTED_SELF] () -> RetainPtr<id> {
        if (!protectedSelf->m_page)
            return nil;
        return [NSValue valueWithSize:(NSSize)protectedSelf->m_page->size()];
    }).autorelease();
}

- (NSValue *)accessibilityAttributePositionValue
{
    return ax::retrieveValueFromMainThread<RetainPtr<id>>([PROTECTED_SELF] () -> RetainPtr<id> {
        if (!protectedSelf->m_page)
            return nil;
        return [NSValue valueWithPoint:(NSPoint)protectedSelf->m_page->accessibilityPosition()];
    }).autorelease();
}

- (id)accessibilityDataDetectorValue:(NSString *)attribute point:(WebCore::FloatPoint&)point
{
    return ax::retrieveValueFromMainThread<RetainPtr<id>>([&attribute, &point, PROTECTED_SELF] () -> RetainPtr<id> {
        if (!protectedSelf->m_page)
            return nil;
        id value = nil;
        if ([attribute isEqualToString:@"AXDataDetectorExistsAtPoint"] || [attribute isEqualToString:@"AXDidShowDataDetectorMenuAtPoint"]) {
            bool boolValue;
            if (protectedSelf->m_page->corePage()->pageOverlayController().copyAccessibilityAttributeBoolValueForPoint(attribute, point, boolValue))
                value = [NSNumber numberWithBool:boolValue];
        }
        if ([attribute isEqualToString:@"AXDataDetectorTypeAtPoint"]) {
            String stringValue;
            if (protectedSelf->m_page->corePage()->pageOverlayController().copyAccessibilityAttributeStringValueForPoint(attribute, point, stringValue))
                value = [NSString stringWithString:stringValue];
        }
        return value;
    }).autorelease();
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (id)accessibilityAttributeValue:(NSString *)attribute forParameter:(id)parameter
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    WebCore::FloatPoint pageOverlayPoint;
    if ([parameter isKindOfClass:[NSValue class]] && !strcmp([(NSValue *)parameter objCType], @encode(NSPoint)))
        pageOverlayPoint = [self convertScreenPointToRootView:[(NSValue *)parameter pointValue]];
    else
        return nil;

    if ([attribute isEqualToString:@"AXDataDetectorExistsAtPoint"] || [attribute isEqualToString:@"AXDidShowDataDetectorMenuAtPoint"] || [attribute isEqualToString:@"AXDataDetectorTypeAtPoint"])
        return [self accessibilityDataDetectorValue:attribute point:pageOverlayPoint];

    return nil;
}

- (BOOL)accessibilityShouldUseUniqueId
{
    return YES;
}

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
- (id)accessibilityHitTest:(NSPoint)point
{
    auto convertedPoint = ax::retrieveValueFromMainThread<WebCore::IntPoint>([&point, PROTECTED_SELF] () -> WebCore::IntPoint {
        if (!protectedSelf->m_page)
            return WebCore::IntPoint(point);

        auto convertedPoint = protectedSelf->m_page->screenToRootView(WebCore::IntPoint(point));

        // Some plugins may be able to figure out the scroll position and inset on their own.
        bool applyContentOffset = true;

        // Isolated tree frames have the offset encoded into them so we don't need to undo here.
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
        applyContentOffset = !WebCore::AXObjectCache::isIsolatedTreeEnabled() || !_AXUIElementRequestServicedBySecondaryAXThread();
#endif
        if (auto pluginView = WebKit::WebPage::pluginViewForFrame(protectedSelf->m_page->mainFrame()))
            applyContentOffset = !pluginView->plugin()->pluginHandlesContentOffsetForAccessibilityHitTest();
        
        if (!applyContentOffset)
            return convertedPoint;

        if (auto* frameView = protectedSelf->m_page->mainFrameView())
            convertedPoint.moveBy(frameView->scrollPosition());
        if (auto* page = protectedSelf->m_page->corePage())
            convertedPoint.move(0, -page->topContentInset());
        return convertedPoint;
    });
    
    return [[self accessibilityRootObjectWrapper] accessibilityHitTest:convertedPoint];
}
ALLOW_DEPRECATED_DECLARATIONS_END

@end

#endif // PLATFORM(MAC)

