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
#import <WebCore/Frame.h>
#import <WebCore/FrameView.h>
#import <WebCore/Page.h>
#import <WebCore/PageOverlayController.h>
#import <WebCore/ScrollView.h>
#import <WebCore/Scrollbar.h>
#import <WebCore/WebAccessibilityObjectWrapperMac.h>
#import <pal/spi/mac/NSAccessibilitySPI.h>
#import <wtf/ObjcRuntimeExtras.h>

using namespace WebCore;
using namespace WebKit;

@implementation WKAccessibilityWebPageObject

- (void)dealloc
{
    NSAccessibilityUnregisterUniqueIdForUIElement(self);
    [m_parent release];
    [super dealloc];
}

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (BOOL)accessibilityIsIgnored
IGNORE_WARNINGS_END
{
    return NO;
}

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (NSArray *)accessibilityAttributeNames
IGNORE_WARNINGS_END
{
    if (!m_attributeNames)
        m_attributeNames = adoptNS([[NSArray alloc] initWithObjects:
                            NSAccessibilityRoleAttribute, NSAccessibilityRoleDescriptionAttribute, NSAccessibilityFocusedAttribute,
                            NSAccessibilityParentAttribute, NSAccessibilityWindowAttribute, NSAccessibilityTopLevelUIElementAttribute,
                            NSAccessibilityPositionAttribute, NSAccessibilitySizeAttribute, NSAccessibilityChildrenAttribute, NSAccessibilityPrimaryScreenHeightAttribute, nil]);
    
    return m_attributeNames.get();
}

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (NSArray *)accessibilityParameterizedAttributeNames
IGNORE_WARNINGS_END
{
    Vector<String> result = m_page->corePage()->pageOverlayController().copyAccessibilityAttributesNames(true);
    if (result.isEmpty())
        return nil;
    
    NSMutableArray *names = [NSMutableArray array];
    for (auto& name : result)
        [names addObject:(NSString *)name];
    
    return names;
}

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (BOOL)accessibilityIsAttributeSettable:(NSString *)attribute
IGNORE_WARNINGS_END
{
    return NO;
}

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (void)accessibilitySetValue:(id)value forAttribute:(NSString *)attribute
IGNORE_WARNINGS_END
{
}

- (NSPoint)convertScreenPointToRootView:(NSPoint)point
{
    return m_page->screenToRootView(IntPoint(point.x, point.y));
}

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (NSArray *)accessibilityActionNames
IGNORE_WARNINGS_END
{
    return [NSArray array];
}

- (NSArray *)accessibilityChildren
{
    id wrapper = [self accessibilityRootObjectWrapper];
    if (!wrapper)
        return [NSArray array];
    
    return [NSArray arrayWithObject:wrapper];
}

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (id)accessibilityAttributeValue:(NSString *)attribute
IGNORE_WARNINGS_END
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
    
    if (!m_page)
        return nil;
    
    if ([attribute isEqualToString:NSAccessibilityPositionAttribute]) {
        const WebCore::FloatPoint& point = m_page->accessibilityPosition();
        return [NSValue valueWithPoint:NSMakePoint(point.x(), point.y())];
    }
    
    if ([attribute isEqualToString:NSAccessibilityPrimaryScreenHeightAttribute])
        return [[self accessibilityRootObjectWrapper] accessibilityAttributeValue:attribute];
    
    if ([attribute isEqualToString:NSAccessibilitySizeAttribute]) {
        const IntSize& s = m_page->size();
        return [NSValue valueWithSize:NSMakeSize(s.width(), s.height())];
    }
    
    if ([attribute isEqualToString:NSAccessibilityChildrenAttribute])
        return [self accessibilityChildren];
    
    return nil;
}

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (id)accessibilityAttributeValue:(NSString *)attribute forParameter:(id)parameter
IGNORE_WARNINGS_END
{
    WebCore::FloatPoint pageOverlayPoint;
    if ([parameter isKindOfClass:[NSValue class]] && !strcmp([(NSValue *)parameter objCType], @encode(NSPoint)))
        pageOverlayPoint = [self convertScreenPointToRootView:[(NSValue *)parameter pointValue]];
    else
        return nil;

    if ([attribute isEqualToString:@"AXDataDetectorExistsAtPoint"] || [attribute isEqualToString:@"AXDidShowDataDetectorMenuAtPoint"]) {
        bool value;
        if (!m_page->corePage()->pageOverlayController().copyAccessibilityAttributeBoolValueForPoint(attribute, pageOverlayPoint, value))
            return nil;
        return [NSNumber numberWithBool:value];
    }

    if ([attribute isEqualToString:@"AXDataDetectorTypeAtPoint"]) {
        String value;
        if (!m_page->corePage()->pageOverlayController().copyAccessibilityAttributeStringValueForPoint(attribute, pageOverlayPoint, value))
            return nil;
        return [NSString stringWithString:value];
    }

    return nil;
}

- (BOOL)accessibilityShouldUseUniqueId
{
    return YES;
}

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
- (id)accessibilityHitTest:(NSPoint)point
{
    if (!m_page)
        return nil;

    IntPoint convertedPoint = m_page->screenToRootView(IntPoint(point));
    
    // Some plugins may be able to figure out the scroll position and inset on their own.
    bool applyContentOffset = true;
    if (auto pluginView = WebPage::pluginViewForFrame(m_page->mainFrame()))
        applyContentOffset = !pluginView->plugin()->pluginHandlesContentOffsetForAccessibilityHitTest();

    if (applyContentOffset) {
        if (WebCore::FrameView* frameView = m_page->mainFrameView())
            convertedPoint.moveBy(frameView->scrollPosition());
        if (WebCore::Page* page = m_page->corePage())
            convertedPoint.move(0, -page->topContentInset());
    }

    return [[self accessibilityRootObjectWrapper] accessibilityHitTest:convertedPoint];
}
ALLOW_DEPRECATED_DECLARATIONS_END

@end

#endif // PLATFORM(MAC)

