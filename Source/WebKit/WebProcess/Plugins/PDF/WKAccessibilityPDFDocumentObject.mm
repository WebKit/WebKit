/*
 * Copyright (C) 2024-2025 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WKAccessibilityPDFDocumentObject.h"

#if ENABLE(UNIFIED_PDF)

#include "PDFKitSPI.h"
#include "PDFPluginAnnotation.h"
#include "PDFPluginBase.h"
#include "UnifiedPDFPlugin.h"
#include <PDFKit/PDFKit.h>
#include <WebCore/AXObjectCache.h>
#include <WebCore/HTMLPlugInElement.h>

#if PLATFORM(MAC)
#include <WebCore/WebAccessibilityObjectWrapperMac.h>
#include <pal/spi/cocoa/NSAccessibilitySPI.h>
#endif // PLATFORM(MAC)

#include <wtf/CheckedPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/WeakObjCPtr.h>

#if PLATFORM(IOS_FAMILY)
@interface NSObject (AXPriv)
- (id)accessibilityHitTest:(CGPoint)point;
- (NSArray *)accessibilityElementsWithPlugin:(id)plugin;
@end
#endif // PLATFORM(IOS_FAMILY)

@implementation WKAccessibilityPDFDocumentObject

@synthesize pluginElement = _pluginElement;


- (id)initWithPDFDocument:(RetainPtr<PDFDocument>)document andElement:(WebCore::HTMLPlugInElement*)element
{
    if (!(self = [super init]))
        return nil;

    _pdfDocument = document;
    _pluginElement = element;
    _axElements = adoptNS([[NSMutableArray alloc] init]);
    // We are setting the presenter ID of the WKAccessibilityPDFDocumentObject to the hosting application's PID.
    // This way VoiceOver can set AX observers on all the PDF AX nodes which are descendant of this element.
#if PLATFORM(MAC)
    if ([self respondsToSelector:@selector(accessibilitySetPresenterProcessIdentifier:)])
        [(id)self accessibilitySetPresenterProcessIdentifier:legacyPresentingApplicationPID()];
#endif // PLATFORM(MAC)
    return self;
}

- (void)setPDFPlugin:(WebKit::UnifiedPDFPlugin*)plugin
{
    _pdfPlugin = plugin;
}

- (void)setPDFDocument:(RetainPtr<PDFDocument>)document
{
    _pdfDocument = document;
}

- (void)setParent:(NSObject *)parent
{
    _parent = parent;
}

- (PDFDocument *)document
{
    return _pdfDocument.get();
}

- (NSRect)convertFromPDFPageToScreenForAccessibility:(NSRect)rectInPageCoordinates pageIndex:(WebKit::PDFDocumentLayout::PageIndex)pageIndex
{
    if (RefPtr plugin = _pdfPlugin.get())
        return plugin->convertFromPDFPageToScreenForAccessibility(rectInPageCoordinates, pageIndex);
    return rectInPageCoordinates;
}

#if PLATFORM(IOS_FAMILY)
- (void)setAXElements
{
    if (!_pdfDocument) {
        if (RefPtr plugin = _pdfPlugin.get())
            _pdfDocument = plugin->pdfDocument();
    }

    auto pageCount = [_pdfDocument pageCount];
    for (NSUInteger pageIndex = 0; pageIndex < pageCount; pageIndex ++) {
        PDFPage *page = [_pdfDocument pageAtIndex:pageIndex];

        if ([page respondsToSelector:@selector(accessibilityElementsWithPlugin:)])
            [_axElements addObjectsFromArray:[page accessibilityElementsWithPlugin:self]];
    }
}

- (id)accessibilityHitTest:(CGPoint)point
{
    if (!_pdfDocument) {
        if (RefPtr plugin = _pdfPlugin.get())
            _pdfDocument = plugin->pdfDocument();
    }

    if (![_axElements count])
        [self setAXElements];

    if (RefPtr plugin = _pdfPlugin.get())
        return plugin->accessibilityHitTestInPageForIOS(point);
    return [_pdfDocument accessibilityHitTest:point];
}

- (NSArray *)accessibilityElements
{
    if (![_axElements count])
        [self setAXElements];

    return _axElements.get();
}

- (BOOL)accessibilityScroll:(UIAccessibilityScrollDirection)direction
{
    if (RefPtr plugin = _pdfPlugin.get()) {
        if (auto coreObject = plugin->accessibilityCoreObject())
            [coreObject->wrapper() accessibilityScroll:direction];
    }
    return YES;
}
#endif // PLATFORM(IOS_FAMILY)

#if PLATFORM(MAC)
- (BOOL)isAccessibilityElement
{
    return YES;
}

- (id)accessibilityFocusedUIElement
{
    if (RefPtr plugin = _pdfPlugin.get()) {
        if (RefPtr activeAnnotation = plugin->activeAnnotation()) {
            if (CheckedPtr existingCache = plugin->axObjectCache()) {
                if (RefPtr object = existingCache->getOrCreate(activeAnnotation->protectedElement().get())) {
                ALLOW_DEPRECATED_DECLARATIONS_BEGIN
                    return [object->wrapper() accessibilityAttributeValue:@"_AXAssociatedPluginParent"];
                ALLOW_DEPRECATED_DECLARATIONS_END
                }
            }
        }
    }
    for (id page in [self accessibilityChildren]) {
        id focusedElement = [page accessibilityFocusedUIElement];
        if (focusedElement)
            return focusedElement;
    }
    return nil;
}

- (id)accessibilityWindow
{
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    return [[self accessibilityParent]  accessibilityAttributeValue:NSAccessibilityWindowAttribute];
ALLOW_DEPRECATED_DECLARATIONS_END
}

- (id)accessibilityTopLevelUIElement
{
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    return [[self accessibilityParent] accessibilityAttributeValue:NSAccessibilityTopLevelUIElementAttribute];
ALLOW_DEPRECATED_DECLARATIONS_END
}

- (NSArray *)accessibilityVisibleChildren
{
    RetainPtr<NSMutableArray> visiblePageElements = adoptNS([[NSMutableArray alloc] init]);
    for (id page in [self accessibilityChildren]) {
        id focusedElement = [page accessibilityFocusedUIElement];
        if (focusedElement)
            [visiblePageElements addObject:page];
    }
    return visiblePageElements.autorelease();
}

- (NSString *)accessibilitySubrole
{
    return @"AXPDFPluginSubrole";
}

- (NSRect)accessibilityFrame
{
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    id accessibilityParent = [self accessibilityParent];
    NSSize size = [[accessibilityParent accessibilityAttributeValue:NSAccessibilitySizeAttribute] sizeValue];
    NSPoint origin = [[accessibilityParent accessibilityAttributeValue:NSAccessibilityPositionAttribute] pointValue];
ALLOW_DEPRECATED_DECLARATIONS_END
    return NSMakeRect(origin.x, origin.y, size.width, size.height);
}

- (NSObject *)accessibilityParent
{
    RetainPtr protectedSelf = self;
    if (!protectedSelf->_parent) {
        callOnMainRunLoopAndWait([protectedSelf] {
            if (CheckedPtr axObjectCache = protectedSelf->_pdfPlugin.get()->axObjectCache()) {
                if (RefPtr pluginAxObject = axObjectCache->getOrCreate(RefPtr { protectedSelf->_pluginElement.get() }.get()))
                    protectedSelf->_parent = pluginAxObject->wrapper();
            }
        });
    }
    return protectedSelf->_parent.get().get();
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (id)accessibilityAttributeValue:(NSString *)attribute
{
    if ([attribute isEqualToString:NSAccessibilityParentAttribute])
        return [self accessibilityParent];
    if ([attribute isEqualToString:NSAccessibilityChildrenAttribute])
        return [self accessibilityChildren];
    if ([attribute isEqualToString:NSAccessibilityVisibleChildrenAttribute])
        return [self accessibilityVisibleChildren];
    if ([attribute isEqualToString:NSAccessibilityTopLevelUIElementAttribute])
        return [self accessibilityTopLevelUIElement];
    if ([attribute isEqualToString:NSAccessibilityWindowAttribute])
        return [self accessibilityWindow];
    if ([attribute isEqualToString:NSAccessibilityEnabledAttribute])
        return [[self accessibilityParent] accessibilityAttributeValue:NSAccessibilityEnabledAttribute];
    if ([attribute isEqualToString:NSAccessibilityRoleAttribute])
        return NSAccessibilityGroupRole;
    if ([attribute isEqualToString:NSAccessibilityPrimaryScreenHeightAttribute])
        return [[self accessibilityParent] accessibilityAttributeValue:NSAccessibilityPrimaryScreenHeightAttribute];
    if ([attribute isEqualToString:NSAccessibilitySubroleAttribute])
        return [self accessibilitySubrole];
    if ([attribute isEqualToString:NSAccessibilitySizeAttribute]) {
        if (RefPtr plugin = _pdfPlugin.get())
            return [NSValue valueWithSize:plugin->boundsOnScreen().size()];
    }
    if ([attribute isEqualToString:NSAccessibilityPositionAttribute]) {
        if (RefPtr plugin = _pdfPlugin.get())
            return [NSValue valueWithPoint:plugin->boundsOnScreen().location()];
    }
    return nil;
}
ALLOW_DEPRECATED_IMPLEMENTATIONS_END

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (NSArray *)accessibilityAttributeNames
{
    static NeverDestroyed<RetainPtr<NSArray>> attributeNames = @[
        NSAccessibilityParentAttribute,
        NSAccessibilityWindowAttribute,
        NSAccessibilityTopLevelUIElementAttribute,
        NSAccessibilityRoleDescriptionAttribute,
        NSAccessibilitySizeAttribute,
        NSAccessibilityEnabledAttribute,
        NSAccessibilityPositionAttribute,
        NSAccessibilityFocusedAttribute,
        NSAccessibilityChildrenAttribute,
        NSAccessibilityPrimaryScreenHeightAttribute,
        NSAccessibilitySubroleAttribute
    ];
    return attributeNames.get().get();
}
ALLOW_DEPRECATED_IMPLEMENTATIONS_END

- (BOOL)accessibilityShouldUseUniqueId
{
    return YES;
}

- (NSUInteger)accessibilityArrayAttributeCount:(NSString *)attribute
{
    if (!_pdfDocument) {
        if (RefPtr plugin = _pdfPlugin.get())
            _pdfDocument = plugin->pdfDocument();
    }
    if ([attribute isEqualToString:NSAccessibilityChildrenAttribute])
        return [_pdfDocument.get() pageCount];
    if ([attribute isEqualToString:NSAccessibilityVisibleChildrenAttribute])
        return [self accessibilityVisibleChildren].count;
    return [super accessibilityArrayAttributeCount:attribute];
}

- (NSArray*)accessibilityChildren
{
    if (!_pdfDocument) {
        if (RefPtr plugin = _pdfPlugin.get())
            _pdfDocument = plugin->pdfDocument();
    }

    if ([_pdfDocument respondsToSelector:@selector(accessibilityChildren:)])
        return [_pdfDocument accessibilityChildren:self];

    return nil;
}

- (id)accessibilityAssociatedControlForAnnotation:(PDFAnnotation *)annotation
{
    RetainPtr<id> wrapper;
    callOnMainRunLoopAndWait([protectedSelf = retainPtr(self), &wrapper] {
        RefPtr activeAnnotation = protectedSelf->_pdfPlugin.get()->activeAnnotation();
        if (!activeAnnotation)
            return;

        if (CheckedPtr axObjectCache = protectedSelf->_pdfPlugin.get()->axObjectCache()) {
            if (RefPtr annotationElementAxObject = axObjectCache->getOrCreate(activeAnnotation->protectedElement().get()))
                wrapper = annotationElementAxObject->wrapper();
        }
    });
    return wrapper.autorelease();
}

- (void)setActiveAnnotation:(PDFAnnotation *)annotation
{
    RefPtr plugin = _pdfPlugin.get();
    plugin->setActiveAnnotation({ WTFMove(annotation) });
}

- (id)accessibilityHitTest:(NSPoint)point
{
    for (id element in [self accessibilityChildren]) {
        id result = [element accessibilityHitTest:point];
        if (result)
            return result;
    }
    return self;
}

// this function allows VoiceOver to scroll to the current page with VO cursor
- (void)gotoDestination:(PDFDestination *)destination
{
    RefPtr plugin = _pdfPlugin.get();
    if (!plugin)
        return;

    WebKit::PDFDocumentLayout::PageIndex pageIndex = [_pdfDocument indexForPage:[destination page]];

    callOnMainRunLoop([plugin, pageIndex] {
        plugin->accessibilityScrollToPage(pageIndex);
    });
}

#endif // PLATFORM(MAC)

@end

#endif // ENABLE(UNIFIED_PDF)
