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
#import <WebCore/LocalFrame.h>
#import <WebCore/LocalFrameView.h>
#import <WebCore/Page.h>
#import <WebCore/PageOverlayController.h>
#import <WebCore/PlatformScreen.h>
#import <WebCore/ScrollView.h>
#import <WebCore/Scrollbar.h>
#import <WebCore/WebAccessibilityObjectWrapperMac.h>
#import <pal/spi/cocoa/NSAccessibilitySPI.h>
#import <wtf/cocoa/VectorCocoa.h>

namespace ax = WebCore::Accessibility;

@implementation WKAccessibilityWebPageObject

#define PROTECTED_SELF protectedSelf = RetainPtr<WKAccessibilityWebPageObject>(self)

- (instancetype)init
{
    self = [super init];
    if (!self)
        return self;

    self->m_attributeNames = adoptNS([[NSArray alloc] initWithObjects:
        NSAccessibilityRoleAttribute, NSAccessibilityRoleDescriptionAttribute, NSAccessibilityFocusedAttribute,
        NSAccessibilityParentAttribute, NSAccessibilityWindowAttribute, NSAccessibilityTopLevelUIElementAttribute,
        NSAccessibilityPositionAttribute, NSAccessibilitySizeAttribute, NSAccessibilityChildrenAttribute, NSAccessibilityChildrenInNavigationOrderAttribute, NSAccessibilityPrimaryScreenHeightAttribute, nil]);
    return self;
}

- (void)dealloc
{
    NSAccessibilityUnregisterUniqueIdForUIElement(self);
    [super dealloc];
}

- (void)setWebPage:(NakedPtr<WebKit::WebPage>)page
{
    ASSERT(isMainRunLoop());
    [super setWebPage:page];

    if (!page) {
        m_parameterizedAttributeNames = @[];
        return;
    }

    auto* corePage = page->corePage();
    if (!corePage) {
        m_parameterizedAttributeNames = @[];
        return;
    }

    m_parameterizedAttributeNames = createNSArray(corePage->pageOverlayController().copyAccessibilityAttributesNames(true));
    // FIXME: m_parameterizedAttributeNames needs to be updated when page overlays are added or removed, although this is a property that doesn't change much.
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
    return m_attributeNames.get();
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (NSArray *)accessibilityParameterizedAttributeNames
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    return m_parameterizedAttributeNames.get();
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
    return wrapper ? @[wrapper] : @[];
}

- (NSArray *)accessibilityChildrenInNavigationOrder
{
    return [self accessibilityChildren];
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (id)accessibilityAttributeValue:(NSString *)attribute
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    if (!WebCore::AXObjectCache::accessibilityEnabled())
        WebCore::AXObjectCache::enableAccessibility();

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (WebCore::AXObjectCache::isIsolatedTreeEnabled())
        WebCore::AXObjectCache::initializeAXThreadIfNeeded();
#endif

    if ([attribute isEqualToString:NSAccessibilityParentAttribute])
        return m_parent.get();
    
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
        return @(WebCore::screenRectForPrimaryScreen().size().height());
    
    if ([attribute isEqualToString:NSAccessibilitySizeAttribute])
        return [self accessibilityAttributeSizeValue];
    
    if ([attribute isEqualToString:NSAccessibilityChildrenAttribute])
        return [self accessibilityChildren];
    
    // [self accessibilityChildren] is just the root object, so it's already in navigation order.
    if ([attribute isEqualToString:NSAccessibilityChildrenInNavigationOrderAttribute])
        return [self accessibilityChildren];

    return nil;
}

- (NSValue *)accessibilityAttributeSizeValue
{
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (!isMainRunLoop()) {
        Locker lock { m_cacheLock };
        return [NSValue valueWithSize:(NSSize)m_size];
    }
#endif

    return ax::retrieveAutoreleasedValueFromMainThread<id>([PROTECTED_SELF] () -> RetainPtr<id> {
        if (!protectedSelf->m_page)
            return nil;
        return [NSValue valueWithSize:(NSSize)protectedSelf->m_page->size()];
    });
}

- (NSValue *)accessibilityAttributePositionValue
{
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (!isMainRunLoop()) {
        Locker lock { m_cacheLock };
        return [NSValue valueWithPoint:(NSPoint)m_position];
    }
#endif

    return ax::retrieveAutoreleasedValueFromMainThread<id>([PROTECTED_SELF] () -> RetainPtr<id> {
        if (!protectedSelf->m_page)
            return nil;
        return [NSValue valueWithPoint:(NSPoint)protectedSelf->m_page->accessibilityPosition()];
    });
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

        // PDF plug-in handles the scroll view offset natively as part of the layer conversions.
        if (protectedSelf->m_page->mainFramePlugIn())
            return convertedPoint;

        if (auto* frameView = protectedSelf->m_page->localMainFrameView())
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

