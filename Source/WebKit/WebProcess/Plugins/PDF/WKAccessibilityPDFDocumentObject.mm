/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#if ENABLE(UNIFIED_PDF) && PLATFORM(MAC)

#include "PDFKitSPI.h"
#include "PDFPluginAnnotation.h"
#include "PDFPluginBase.h"
#include "UnifiedPDFPlugin.h"
#include <PDFKit/PDFKit.h>
#include <WebCore/AXObjectCache.h>
#include <WebCore/HTMLPlugInElement.h>
#include <WebCore/WebAccessibilityObjectWrapperMac.h>
#include <pal/spi/cocoa/NSAccessibilitySPI.h>
#include <wtf/CheckedPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/WeakObjCPtr.h>

@implementation WKAccessibilityPDFDocumentObject

@synthesize pluginElement = _pluginElement;

- (id)initWithPDFDocument:(RetainPtr<PDFDocument>)document andElement:(WebCore::HTMLPlugInElement*)element
{
    if (!(self = [super init]))
        return nil;

    _pdfDocument = document;
    _pluginElement = element;
    // We are setting the presenter ID of the WKAccessibilityPDFDocumentObject to the hosting application's PID.
    // This way VoiceOver can set AX observers on all the PDF AX nodes which are descendant of this element.
    if ([self respondsToSelector:@selector(accessibilitySetPresenterProcessIdentifier:)])
        [(id)self accessibilitySetPresenterProcessIdentifier:legacyPresentingApplicationPID()];
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

- (BOOL)isAccessibilityElement
{
    return YES;
}

- (id)accessibilityFocusedUIElement
{
    if (RefPtr plugin = _pdfPlugin.get()) {
        if (RefPtr activeAnnotation = plugin->activeAnnotation()) {
            if (WebCore::AXObjectCache* existingCache = plugin->axObjectCache()) {
                if (RefPtr object = existingCache->getOrCreate(activeAnnotation->element())) {
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

- (PDFDocument*)document
{
    return _pdfDocument.get();
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
                if (RefPtr pluginAxObject = axObjectCache->getOrCreate(protectedSelf->_pluginElement.get()))
                    protectedSelf->_parent = pluginAxObject->wrapper();
            }
        });
    }
    return protectedSelf->_parent.get().get();
}

- (void)setParent:(NSObject *)parent
{
    _parent = parent;
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

- (NSRect)convertFromPDFPageToScreenForAccessibility:(NSRect)rectInPageCoordinate pageIndex:(WebKit::PDFDocumentLayout::PageIndex)pageIndex
{
    if (RefPtr plugin = _pdfPlugin.get())
        return plugin->convertFromPDFPageToScreenForAccessibility(rectInPageCoordinate, pageIndex);
    return rectInPageCoordinate;
}

- (id)accessibilityAssociatedControlForAnnotation:(PDFAnnotation *)annotation
{
    RefPtr activeAnnotation = _pdfPlugin.get()->activeAnnotation();
    if (!activeAnnotation)
        return nil;

    id wrapper = nil;
    callOnMainRunLoopAndWait([activeAnnotation, protectedSelf = retainPtr(self), &wrapper] {
        if (auto* axObjectCache = protectedSelf->_pdfPlugin.get()->axObjectCache()) {
            if (RefPtr annotationElementAxObject = axObjectCache->getOrCreate(activeAnnotation->element()))
                wrapper = annotationElementAxObject->wrapper();
        }
    });

    return wrapper;
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
@end

#endif
