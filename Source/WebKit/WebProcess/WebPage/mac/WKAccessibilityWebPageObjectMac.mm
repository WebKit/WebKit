/*
 * Copyright (C) 2011-2025 Apple Inc. All rights reserved.
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
#import <WebCore/DocumentView.h>
#import <WebCore/LocalFrame.h>
#import <WebCore/LocalFrameView.h>
#import <WebCore/Page.h>
#import <WebCore/PageOverlayController.h>
#import <WebCore/PlatformScreen.h>
#import <WebCore/ScrollView.h>
#import <WebCore/Scrollbar.h>
#import <WebCore/WebAccessibilityObjectWrapperMac.h>
#import <pal/spi/cocoa/NSAccessibilitySPI.h>
#import <wtf/ObjCRuntimeExtras.h>
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

    RefPtr corePage = page->corePage();
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
        if (RefPtr page = protectedSelf->m_page.get())
            return page->screenToRootView(WebCore::IntPoint(point.x, point.y));
        return point;
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
    RetainPtr wrapper = [self accessibilityRootObjectWrapper:protect([self focusedLocalFrame]).get()];
    return wrapper ? @[wrapper.get()] : @[];
}

- (NSArray *)accessibilityChildrenInNavigationOrder
{
    return [self accessibilityChildren];
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (id)accessibilityAttributeValue:(NSString *)attribute
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    static std::atomic<bool> didInitialize { false };
    static std::atomic<unsigned> screenHeight { 0 };
    if (!didInitialize) [[unlikely]] {
        didInitialize = true;
        callOnMainRunLoopAndWait([protectedSelf = retainPtr(self)] {
            if (!WebCore::AXObjectCache::accessibilityEnabled())
                [protectedSelf enableAccessibilityForAllProcesses];

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
            if (WebCore::AXObjectCache::isIsolatedTreeEnabled())
                [protectedSelf _buildIsolatedTreeIfNeeded];
#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)

            float roundedHeight = std::round(WebCore::screenRectForPrimaryScreen().size().height());
            screenHeight = std::max(0u, static_cast<unsigned>(roundedHeight));
        });
    }

    // The following attributes can be handled off the main thread.

    if ([attribute isEqualToString:NSAccessibilityRoleAttribute])
        return NSAccessibilityGroupRole;

    if ([attribute isEqualToString:NSAccessibilityRoleDescriptionAttribute])
        return NSAccessibilityRoleDescription(NSAccessibilityGroupRole, nil);

    if ([attribute isEqualToString:NSAccessibilityFocusedAttribute])
        return @NO;

    if ([attribute isEqualToString:NSAccessibilityPositionAttribute])
        return [self accessibilityAttributePositionValue];

    if ([attribute isEqualToString:NSAccessibilitySizeAttribute])
        return [self accessibilityAttributeSizeValue];

    if ([attribute isEqualToString:NSAccessibilityChildrenAttribute]
        || [attribute isEqualToString:NSAccessibilityChildrenInNavigationOrderAttribute]) {
        // The root object is the only child.
        return [self accessibilityChildren];
    }

    if ([attribute isEqualToString:NSAccessibilityParentAttribute])
        return [self accessibilityAttributeParentValue].autorelease();

    if ([attribute isEqualToString:NSAccessibilityPrimaryScreenHeightAttribute])
        return @(screenHeight.load());

    if ([attribute isEqualToString:NSAccessibilityWindowAttribute])
        return [self accessibilityAttributeWindowValue].autorelease();

    if ([attribute isEqualToString:NSAccessibilityTopLevelUIElementAttribute])
        return [self accessibilityAttributeTopLevelUIElementValue].autorelease();

    return nil;
}

- (NSValue *)accessibilityAttributeSizeValue
{
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (!isMainRunLoop()) {
        Locker lock { m_cacheLock };
        return [NSValue valueWithSize:(NSSize)m_size];
    }
#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)

    return m_page ? [NSValue valueWithSize:m_page->size()] : nil;
}

- (NSValue *)accessibilityAttributePositionValue
{
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (!isMainRunLoop()) {
        Locker lock { m_cacheLock };
        return [NSValue valueWithPoint:(NSPoint)m_position];
    }
#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)

    return m_page ? [NSValue valueWithPoint:m_page->accessibilityPosition()] : nil;
}

- (RetainPtr<id>)accessibilityAttributeParentValue
{
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (!isMainRunLoop()) {
        Locker lock { m_parentLock };
        return m_parent;
    }
#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)

    return m_parent;
}

// FIXME: accessibilityAttributeWindowValue and accessibilityAttributeTopLevelUIElementValue
// always return nil for instances of this class when set up by WebPage::registerRemoteFrameAccessibilityTokens,
// as nothing there sets m_window, setWindowUIElement, and setTopLevelUIElement.
- (RetainPtr<id>)accessibilityAttributeWindowValue
{
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (!isMainRunLoop()) {
        // Use the cached window to avoid using m_parent (which is possibly an AppKit object) off the main-thread.
        Locker lock { m_windowLock };
        return m_window.get();
    }
#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    return [m_parent accessibilityAttributeValue:NSAccessibilityWindowAttribute];
    ALLOW_DEPRECATED_DECLARATIONS_END
}

- (RetainPtr<id>)accessibilityAttributeTopLevelUIElementValue
{
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (!isMainRunLoop()) {
        // Use the cached window to avoid using m_parent (which is possibly an AppKit object) off the main-thread.
        // The TopLevelUIElement is the window, as we set it as such in WebPage::registerUIProcessAccessibilityTokens,
        // so we can return m_window here.
        Locker lock { m_windowLock };
        return m_window.get();
    }
#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    return [m_parent accessibilityAttributeValue:NSAccessibilityTopLevelUIElementAttribute];
    ALLOW_DEPRECATED_DECLARATIONS_END
}

- (id)accessibilityDataDetectorValue:(NSString *)attribute point:(WebCore::FloatPoint&)point
{
    return ax::retrieveValueFromMainThread<RetainPtr<id>>([attribute = retainPtr(attribute), point, PROTECTED_SELF] () -> RetainPtr<id> {
        if (!protectedSelf->m_page)
            return nil;
        RetainPtr<id> value;
        if ([attribute isEqualToString:@"AXDataDetectorExistsAtPoint"] || [attribute isEqualToString:@"AXDidShowDataDetectorMenuAtPoint"]) {
            bool boolValue;
            if (protectedSelf->m_page->corePage()->pageOverlayController().copyAccessibilityAttributeBoolValueForPoint(attribute.get(), point, boolValue))
                value = [NSNumber numberWithBool:boolValue];
        }
        if ([attribute isEqualToString:@"AXDataDetectorTypeAtPoint"]) {
            String stringValue;
            if (protectedSelf->m_page->corePage()->pageOverlayController().copyAccessibilityAttributeStringValueForPoint(attribute.get(), point, stringValue))
                value = stringValue.createNSString();
        }
        return value;
    }).autorelease();
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (id)accessibilityAttributeValue:(NSString *)attribute forParameter:(id)parameter
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    WebCore::FloatPoint pageOverlayPoint;
    if ([parameter isKindOfClass:[NSValue class]] && nsValueHasObjCType<NSPoint>((NSValue *)parameter))
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
    RetainPtr protectedSelf = self;
    WebCore::IntPoint convertedPoint;

    if (isMainRunLoop() || !WebCore::AXObjectCache::isAXThreadHitTestingEnabled()) {
        return ax::retrieveAutoreleasedValueFromMainThread<id>([&point, &protectedSelf, &convertedPoint] () -> id {
            bool shouldFallbackToWebContentAXObject = !protectedSelf->m_hasMainFramePlugin || [protectedSelf shouldFallbackToWebContentAXObjectForMainFramePlugin];
            if (!shouldFallbackToWebContentAXObject) {
                // PDF plug-in handles the scroll view offset natively as part of the layer conversions, so don't
                // do a coordinate conversion for those hit tests.
                convertedPoint = WebCore::IntPoint { point };
            } else {
                RefPtr webPage = protectedSelf->m_page.get();
                convertedPoint = webPage->screenToRootView(WebCore::IntPoint(point));
                if (CheckedPtr localFrameView = webPage->localMainFrameView())
                    convertedPoint.moveBy(localFrameView->scrollPosition());
                else if (RefPtr focusedLocalFrame = [protectedSelf focusedLocalFrame]) {
                    if (CheckedPtr frameView = focusedLocalFrame->view())
                        convertedPoint.moveBy(frameView->scrollPosition());
                }
                if (RefPtr page = webPage->corePage()) {
                    auto obscuredContentInsets = page->obscuredContentInsets();
                    convertedPoint.move(-obscuredContentInsets.left(), -obscuredContentInsets.top());
                }
            }

            return [retainPtr([protectedSelf accessibilityRootObjectWrapper:protect([protectedSelf focusedLocalFrame]).get()]) accessibilityHitTest:convertedPoint];
        });
    }

    // If we're here, we are doing a hit-test off the main-thread. Accessibility thread hit tests
    // can handle the screen-relative point given to us by ATs, so pass it along with any conversion.
    return [retainPtr([protectedSelf accessibilityRootObjectWrapper:protect([protectedSelf focusedLocalFrame]).get()]) accessibilityHitTest:WebCore::IntPoint(point)];
}
ALLOW_DEPRECATED_DECLARATIONS_END

@end

#endif // PLATFORM(MAC)

