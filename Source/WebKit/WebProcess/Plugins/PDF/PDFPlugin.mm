/*
 * Copyright (C) 2009-2023 Apple Inc. All rights reserved.
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
#import "PDFPlugin.h"

#if ENABLE(LEGACY_PDFKIT_PLUGIN)

#import "ArgumentCoders.h"
#import "FrameInfoData.h"
#import "Logging.h"
#import "MessageSenderInlines.h"
#import "PDFAnnotationTypeHelpers.h"
#import "PDFContextMenu.h"
#import "PDFIncrementalLoader.h"
#import "PDFKitSPI.h"
#import "PDFLayerControllerSPI.h"
#import "PDFPluginAnnotation.h"
#import "PDFPluginPasswordField.h"
#import "PluginView.h"
#import "WKAccessibilityWebPageObjectMac.h"
#import "WKPageFindMatchesClient.h"
#import "WebCoreArgumentCoders.h"
#import "WebEventConversion.h"
#import "WebFindOptions.h"
#import "WebFrame.h"
#import "WebHitTestResultData.h"
#import "WebKeyboardEvent.h"
#import "WebMouseEvent.h"
#import "WebPage.h"
#import "WebPageProxyMessages.h"
#import "WebProcess.h"
#import "WebWheelEvent.h"
#import <Quartz/Quartz.h>
#import <QuartzCore/QuartzCore.h>
#import <WebCore/AXObjectCache.h>
#import <WebCore/CSSPropertyNames.h>
#import <WebCore/Chrome.h>
#import <WebCore/Color.h>
#import <WebCore/ColorCocoa.h>
#import <WebCore/ColorSerialization.h>
#import <WebCore/Cursor.h>
#import <WebCore/DictionaryLookup.h>
#import <WebCore/DocumentLoader.h>
#import <WebCore/EventNames.h>
#import <WebCore/FontCocoa.h>
#import <WebCore/FormState.h>
#import <WebCore/FrameLoader.h>
#import <WebCore/GraphicsContextCG.h>
#import <WebCore/HTMLBodyElement.h>
#import <WebCore/HTMLElement.h>
#import <WebCore/HTMLFormElement.h>
#import <WebCore/HTMLPlugInElement.h>
#import <WebCore/LegacyNSPasteboardTypes.h>
#import <WebCore/LocalDefaultSystemAppearance.h>
#import <WebCore/LocalFrame.h>
#import <WebCore/LocalFrameView.h>
#import <WebCore/LocalizedStrings.h>
#import <WebCore/MouseEvent.h>
#import <WebCore/PDFDocumentImage.h>
#import <WebCore/Page.h>
#import <WebCore/PagePasteboardContext.h>
#import <WebCore/Pasteboard.h>
#import <WebCore/PlatformScreen.h>
#import <WebCore/PluginData.h>
#import <WebCore/PluginDocument.h>
#import <WebCore/RenderBoxModelObject.h>
#import <WebCore/ScrollAnimator.h>
#import <WebCore/ScrollbarTheme.h>
#import <WebCore/Settings.h>
#import <WebCore/SharedBuffer.h>
#import <WebCore/TextIndicator.h>
#import <WebCore/WebAccessibilityObjectWrapperMac.h>
#import <WebCore/WheelEventTestMonitor.h>
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <pal/spi/mac/NSMenuSPI.h>
#import <wtf/HexNumber.h>
#import <wtf/Scope.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/UUID.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/WorkQueue.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/TextStream.h>

#import "PDFKitSoftLink.h"

// In non-continuous modes, a single scroll event with a magnitude of >= 20px
// will jump to the next or previous page, to match PDFKit behavior.
static const int defaultScrollMagnitudeThresholdForPageFlip = 20;

@interface WKPDFPluginAccessibilityObject : NSObject {
    PDFLayerController *_pdfLayerController;
    WeakObjCPtr<NSObject> _parent;
    WebKit::PDFPlugin* _pdfPlugin;
    WeakPtr<WebCore::HTMLPlugInElement, WebCore::WeakPtrImplWithEventTargetData> _pluginElement;
}

@property (assign) PDFLayerController *pdfLayerController;
@property (assign) WebKit::PDFPlugin* pdfPlugin;
@property (assign) WeakPtr<WebCore::HTMLPlugInElement, WebCore::WeakPtrImplWithEventTargetData> pluginElement;

- (id)initWithPDFPlugin:(WebKit::PDFPlugin *)plugin andElement:(WebCore::HTMLPlugInElement *)element;

@end

@implementation WKPDFPluginAccessibilityObject

@synthesize pdfPlugin = _pdfPlugin;
@synthesize pluginElement = _pluginElement;

- (id)initWithPDFPlugin:(WebKit::PDFPlugin *)plugin andElement:(WebCore::HTMLPlugInElement *)element
{
    if (!(self = [super init]))
        return nil;

    _pdfPlugin = plugin;
    _pluginElement = element;

    return self;
}

- (NSObject *)parent
{
    RetainPtr<WKPDFPluginAccessibilityObject> protectedSelf = retainPtr(self);
    if (!protectedSelf->_parent) {
        callOnMainRunLoopAndWait([&protectedSelf] {
            if (auto* axObjectCache = protectedSelf->_pdfPlugin->axObjectCache()) {
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

- (PDFLayerController *)pdfLayerController
{
    return _pdfLayerController;
}

- (void)setPdfLayerController:(PDFLayerController *)pdfLayerController
{
    _pdfLayerController = pdfLayerController;
    [_pdfLayerController setAccessibilityParent:self];
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (BOOL)accessibilityIsIgnored
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    return NO;
}

// This is called by PDFAccessibilityNodes from inside PDFKit to get final bounds.
- (NSRect)convertRectToScreenSpace:(NSRect)rect
{
    return _pdfPlugin->convertFromPDFViewToScreen(rect);
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (id)accessibilityAttributeValue:(NSString *)attribute
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    if ([attribute isEqualToString:NSAccessibilityParentAttribute])
        return [self parent];
    if ([attribute isEqualToString:NSAccessibilityTopLevelUIElementAttribute])
        return [[self parent] accessibilityAttributeValue:NSAccessibilityTopLevelUIElementAttribute];
    if ([attribute isEqualToString:NSAccessibilityWindowAttribute])
        return [[self parent] accessibilityAttributeValue:NSAccessibilityWindowAttribute];
    if ([attribute isEqualToString:NSAccessibilitySizeAttribute])
        return [NSValue valueWithSize:_pdfPlugin->boundsOnScreen().size()];
    if ([attribute isEqualToString:NSAccessibilityEnabledAttribute])
        return [[self parent] accessibilityAttributeValue:NSAccessibilityEnabledAttribute];
    if ([attribute isEqualToString:NSAccessibilityPositionAttribute])
        return [NSValue valueWithPoint:_pdfPlugin->boundsOnScreen().location()];
    if ([attribute isEqualToString:NSAccessibilityChildrenAttribute])
        return @[ _pdfLayerController ];
    if ([attribute isEqualToString:NSAccessibilityRoleAttribute])
        return NSAccessibilityGroupRole;
    if ([attribute isEqualToString:NSAccessibilityPrimaryScreenHeightAttribute])
        return [[self parent] accessibilityAttributeValue:NSAccessibilityPrimaryScreenHeightAttribute];
    if ([attribute isEqualToString:NSAccessibilitySubroleAttribute])
        return @"AXPDFPluginSubrole";
    return nil;
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (id)accessibilityAttributeValue:(NSString *)attribute forParameter:(id)parameter
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    if ([attribute isEqualToString:NSAccessibilityBoundsForRangeParameterizedAttribute]) {
        NSRect boundsInPDFViewCoordinates = [[_pdfLayerController accessibilityBoundsForRangeAttributeForParameter:parameter] rectValue];
        return [NSValue valueWithRect:_pdfPlugin->convertFromPDFViewToScreen(boundsInPDFViewCoordinates)];
    }
    if ([attribute isEqualToString:NSAccessibilityLineForIndexParameterizedAttribute])
        return [_pdfLayerController accessibilityLineForIndexAttributeForParameter:parameter];
    if ([attribute isEqualToString:NSAccessibilityRangeForLineParameterizedAttribute])
        return [_pdfLayerController accessibilityRangeForLineAttributeForParameter:parameter];
    if ([attribute isEqualToString:NSAccessibilityStringForRangeParameterizedAttribute])
        return [_pdfLayerController accessibilityStringForRangeAttributeForParameter:parameter];
    return nil;
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (NSArray *)accessibilityAttributeNames
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
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
        // PDFLayerController has its own accessibilityChildren.
        NSAccessibilityChildrenAttribute,
        NSAccessibilityPrimaryScreenHeightAttribute,
        NSAccessibilitySubroleAttribute
    ];

    return attributeNames.get().get();
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (NSArray *)accessibilityActionNames
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    return nil;
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (void)accessibilityPerformAction:(NSString *)action
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    if ([action isEqualToString:NSAccessibilityShowMenuAction])
        _pdfPlugin->showContextMenuAtPoint(WebCore::IntRect(WebCore::IntPoint(), _pdfPlugin->size()).center());
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (BOOL)accessibilityIsAttributeSettable:(NSString *)attribute
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    return [_pdfLayerController accessibilityIsAttributeSettable:attribute];
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (void)accessibilitySetValue:(id)value forAttribute:(NSString *)attribute
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    return [_pdfLayerController accessibilitySetValue:value forAttribute:attribute];
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (NSArray *)accessibilityParameterizedAttributeNames
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    return nil;
}

- (id)accessibilityFocusedUIElement
{
    if (RefPtr activeAnnotation = _pdfPlugin->activeAnnotation()) {
        if (WebCore::AXObjectCache* existingCache = _pdfPlugin->axObjectCache()) {
            if (RefPtr object = existingCache->getOrCreate(activeAnnotation->element()))
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
                return [object->wrapper() accessibilityAttributeValue:@"_AXAssociatedPluginParent"];
ALLOW_DEPRECATED_DECLARATIONS_END
        }
    }
    return nil;
}

- (id)accessibilityAssociatedControlForAnnotation:(PDFAnnotation *)annotation
{
    RefPtr activeAnnotation = _pdfPlugin->activeAnnotation();
    if (!activeAnnotation)
        return nil;

    id wrapper = nil;
    callOnMainRunLoopAndWait([activeAnnotation, protectedSelf = retainPtr(self), &wrapper] {
        if (auto* axObjectCache = protectedSelf->_pdfPlugin->axObjectCache()) {
            if (RefPtr annotationElementAxObject = axObjectCache->getOrCreate(activeAnnotation->element()))
                wrapper = annotationElementAxObject->wrapper();
        }
    });

    return wrapper;
}

- (id)accessibilityHitTestIntPoint:(const WebCore::IntPoint&)point
{
    auto convertedPoint = _pdfPlugin->convertFromRootViewToPDFView(point);
    return [_pdfLayerController accessibilityHitTest:convertedPoint];
}

- (id)accessibilityHitTest:(NSPoint)point
{
    return [self accessibilityHitTestIntPoint:WebCore::IntPoint(point)];
}

@end


@interface WKPDFPluginScrollbarLayer : CALayer {
    WebKit::PDFPlugin* _pdfPlugin;
}

@property (assign) WebKit::PDFPlugin* pdfPlugin;

- (id)initWithPDFPlugin:(WebKit::PDFPlugin *)plugin shouldFlip:(BOOL)shouldFlip;

@end

@implementation WKPDFPluginScrollbarLayer

@synthesize pdfPlugin = _pdfPlugin;

- (id)initWithPDFPlugin:(WebKit::PDFPlugin *)plugin shouldFlip:(BOOL)shouldFlip
{
    if (!(self = [super init]))
        return nil;
    
    _pdfPlugin = plugin;
    
    // Due to an issue where the scrollbars are flipped using UI-side compositng
    // flip the geometry in this case.
    if (shouldFlip)
        [self setGeometryFlipped:YES];
    
    return self;
}

- (id<CAAction>)actionForKey:(NSString *)key
{
    return nil;
}

- (void)drawInContext:(CGContextRef)ctx
{
    _pdfPlugin->paintControlForLayerInContext(self, ctx);
}

@end

@interface WKPDFLayerControllerDelegate : NSObject<PDFLayerControllerDelegate> {
    WebKit::PDFPlugin* _pdfPlugin;
}

@property (assign) WebKit::PDFPlugin* pdfPlugin;

@property (nonatomic) BOOL shouldFlipAnnotations;

- (id)initWithPDFPlugin:(WebKit::PDFPlugin *)plugin;

@end

static WebCore::Cursor::Type toWebCoreCursorType(PDFLayerControllerCursorType cursorType)
{
    switch (cursorType) {
    case kPDFLayerControllerCursorTypePointer: return WebCore::Cursor::Type::Pointer;
    case kPDFLayerControllerCursorTypeHand: return WebCore::Cursor::Type::Hand;
    case kPDFLayerControllerCursorTypeIBeam: return WebCore::Cursor::Type::IBeam;
    }

    RELEASE_ASSERT_NOT_REACHED();
    return WebCore::Cursor::Type::Pointer;
}

@implementation WKPDFLayerControllerDelegate

@synthesize pdfPlugin = _pdfPlugin;

- (id)initWithPDFPlugin:(WebKit::PDFPlugin *)plugin
{
    if (!(self = [super init]))
        return nil;
    
    _pdfPlugin = plugin;
    return self;
}

- (void)updateScrollPosition:(CGPoint)newPosition
{
    // This can be called from the secondary accessibility thread when VoiceOver is enabled,
    // so dispatch to the main runloop to allow safe access to main-thread-only timers downstream of
    // notifyScrollPositionChanged (avoiding a crash). The timer causing the crash at the time of
    // this change was ScrollbarsControllerMac::m_sendContentAreaScrolledTimer.
    //
    // This must be run on the main run loop synchronously. If it were dispatched asynchronously,
    // updates to the scroll position could happen between the time the request is dispatched and when
    // it is serviced, meaning we would overwrite the scroll position to a stale value.
    callOnMainRunLoopAndWait([protectedPlugin = Ref { *_pdfPlugin }, newPosition] {
        protectedPlugin->notifyScrollPositionChanged(WebCore::IntPoint(newPosition));
    });
}

- (void)writeItemsToPasteboard:(NSArray *)items withTypes:(NSArray *)types
{
    _pdfPlugin->writeItemsToPasteboard(NSPasteboardNameGeneral, items, types);
}

- (void)showDefinitionForAttributedString:(NSAttributedString *)string atPoint:(CGPoint)point
{
    _pdfPlugin->showDefinitionForAttributedString(string, point);
}

- (void)performWebSearch:(NSString *)string
{
    _pdfPlugin->performWebSearch({ string });
}

- (void)performSpotlightSearch:(NSString *)string
{
}

- (void)openWithNativeApplication
{
}

- (void)saveToPDF
{
}

- (void)pdfLayerController:(PDFLayerController *)pdfLayerController clickedLinkWithURL:(NSURL *)url
{
    URL coreURL = url;
    _pdfPlugin->navigateToURL(coreURL);
}

- (void)pdfLayerController:(PDFLayerController *)pdfLayerController didChangeActiveAnnotation:(PDFAnnotation *)annotation
{
    _pdfPlugin->setActiveAnnotation({ annotation });
}

- (void)pdfLayerController:(PDFLayerController *)pdfLayerController didChangeContentScaleFactor:(CGFloat)scaleFactor
{
    _pdfPlugin->notifyContentScaleFactorChanged(scaleFactor);
}

- (void)pdfLayerController:(PDFLayerController *)pdfLayerController didChangeDisplayMode:(int)mode
{
    _pdfPlugin->notifyDisplayModeChanged(mode);
}

- (void)pdfLayerController:(PDFLayerController *)pdfLayerController didChangeSelection:(PDFSelection *)selection
{
    _pdfPlugin->notifySelectionChanged();
}

- (void)pdfLayerController:(PDFLayerController *)pdfLayerController didUpdateLayer:(CALayer *)layer forAnnotation:(PDFAnnotation *)annotation
{
    // Due to an issue where the annotations are flipped using UI-side compositing
    // flip the transform in this case.
    if (_shouldFlipAnnotations)
        [layer setAffineTransform:CGAffineTransformMakeScale(1, -1)];
}

- (void)setMouseCursor:(PDFLayerControllerCursorType)cursorType
{
    _pdfPlugin->notifyCursorChanged(toWebCoreCursorType(cursorType));
}

- (void)didChangeAnnotationState
{
    _pdfPlugin->didMutatePDFDocument();
}

@end

@interface PDFViewLayout
- (NSPoint)convertPoint:(NSPoint)point toPage:(PDFPage *)page forScaleFactor:(CGFloat)scaleFactor;
- (NSRect)convertRect:(NSRect)rect fromPage:(PDFPage *) page forScaleFactor:(CGFloat) scaleFactor;
- (PDFPage *)pageNearestPoint:(NSPoint)point currentPage:(PDFPage *)currentPage;
@end

namespace WebKit {
using namespace WebCore;
using namespace HTMLNames;
using namespace WebKit::PDFAnnotationTypeHelpers;

WTF_MAKE_TZONE_ALLOCATED_IMPL(PDFPlugin);

bool PDFPlugin::pdfKitLayerControllerIsAvailable()
{
    return getPDFLayerControllerClass();
}

Ref<PDFPlugin> PDFPlugin::create(HTMLPlugInElement& pluginElement)
{
    return adoptRef(*new PDFPlugin(pluginElement));
}

PDFPlugin::PDFPlugin(HTMLPlugInElement& element)
    : PDFPluginBase(element)
    , m_containerLayer(adoptNS([[CALayer alloc] init]))
    , m_contentLayer(adoptNS([[CALayer alloc] init]))
    , m_scrollCornerLayer(adoptNS([[WKPDFPluginScrollbarLayer alloc] initWithPDFPlugin:this shouldFlip:NO]))
    , m_pdfLayerController(adoptNS([allocPDFLayerControllerInstance() init]))
    , m_pdfLayerControllerDelegate(adoptNS([[WKPDFLayerControllerDelegate alloc] initWithPDFPlugin:this]))
{
    Ref document = element.document();

    if ([m_pdfLayerController respondsToSelector:@selector(setDisplaysPDFHUDController:)])
        [m_pdfLayerController setDisplaysPDFHUDController:NO];

    m_pdfLayerController.get().delegate = m_pdfLayerControllerDelegate.get();
    m_pdfLayerController.get().parentLayer = m_contentLayer.get();

    bool isFullFrame = isFullFramePlugin();
    if (isFullFrame) {
        // FIXME: <rdar://problem/75332948> get the background color from PDFKit instead of hardcoding it
        RefPtr { document->bodyOrFrameset() }->setInlineStyleProperty(WebCore::CSSPropertyBackgroundColor, WebCore::serializationForHTML(WebCore::roundAndClampToSRGBALossy([WebCore::CocoaColor grayColor].CGColor)));
    }

    if (supportsForms()) {
        m_annotationContainer = document->createElement(divTag, false);
        m_annotationContainer->setAttributeWithoutSynchronization(idAttr, "annotationContainer"_s);

        auto annotationStyleElement = document->createElement(styleTag, false);
        annotationStyleElement->setTextContent(annotationStyle);

        m_annotationContainer->appendChild(annotationStyleElement);
        RefPtr { document->bodyOrFrameset() }->appendChild(*m_annotationContainer);
    }

    m_accessibilityObject = adoptNS([[WKPDFPluginAccessibilityObject alloc] initWithPDFPlugin:this andElement:&element]);
    [m_accessibilityObject setPdfLayerController:m_pdfLayerController.get()];
    if (isFullFrame && m_frame->isMainFrame())
        [m_accessibilityObject setParent:m_frame->page()->accessibilityRemoteObject()];
    // If this is not a main-frame (e.g. it originated from an iframe) full-frame plugin, we'll need to set the parent later after the AXObjectCache has been initialized.

    [m_containerLayer addSublayer:m_contentLayer.get()];
    [m_containerLayer addSublayer:m_scrollCornerLayer.get()];
    if ([m_pdfLayerController respondsToSelector:@selector(setDeviceColorSpace:)])
        [m_pdfLayerController setDeviceColorSpace:screenColorSpace(m_frame->coreLocalFrame()->view()).platformColorSpace()];
    
    if ([getPDFLayerControllerClass() respondsToSelector:@selector(setUseIOSurfaceForTiles:)])
        [getPDFLayerControllerClass() setUseIOSurfaceForTiles:false];
}

PDFPlugin::~PDFPlugin() = default;

void PDFPlugin::updateScrollbars()
{
    PDFPluginBase::updateScrollbars();

    if (m_verticalScrollbarLayer) {
        m_verticalScrollbarLayer.get().frame = verticalScrollbar()->frameRect();
        [m_verticalScrollbarLayer setContents:nil];
        [m_verticalScrollbarLayer setNeedsDisplay];
    }
    
    if (m_horizontalScrollbarLayer) {
        m_horizontalScrollbarLayer.get().frame = horizontalScrollbar()->frameRect();
        [m_horizontalScrollbarLayer setContents:nil];
        [m_horizontalScrollbarLayer setNeedsDisplay];
    }
    
    if (m_scrollCornerLayer) {
        m_scrollCornerLayer.get().frame = scrollCornerRect();
        [m_scrollCornerLayer setNeedsDisplay];
    }
}

Ref<Scrollbar> PDFPlugin::createScrollbar(ScrollbarOrientation orientation)
{
    auto scrollbar = PDFPluginBase::createScrollbar(orientation);

    auto shouldFlip = m_view->isUsingUISideCompositing();
    if (orientation == ScrollbarOrientation::Horizontal) {
        m_horizontalScrollbarLayer = adoptNS([[WKPDFPluginScrollbarLayer alloc] initWithPDFPlugin:this shouldFlip:shouldFlip]);
        [m_containerLayer addSublayer:m_horizontalScrollbarLayer.get()];
    } else {
        m_verticalScrollbarLayer = adoptNS([[WKPDFPluginScrollbarLayer alloc] initWithPDFPlugin:this shouldFlip:shouldFlip]);
        [m_containerLayer addSublayer:m_verticalScrollbarLayer.get()];
    }

    return scrollbar;
}

void PDFPlugin::destroyScrollbar(ScrollbarOrientation orientation)
{
    PDFPluginBase::destroyScrollbar(orientation);

    if (orientation == ScrollbarOrientation::Horizontal) {
        [m_horizontalScrollbarLayer removeFromSuperlayer];
        m_horizontalScrollbarLayer = 0;
    } else {
        [m_verticalScrollbarLayer removeFromSuperlayer];
        m_verticalScrollbarLayer = 0;
    }
}

void PDFPlugin::installPDFDocument()
{
    ASSERT(isMainRunLoop());
    LOG(IncrementalPDF, "Installing PDF document");

    if (!m_pdfDocument)
        return;

    if (m_hasBeenDestroyed)
        return;

    if (!m_view) {
        RELEASE_LOG(IncrementalPDF, "PDFPlugin::installPDFDocument called - Plug-in has not been destroyed, but there's also no view.");
        return;
    }

#if HAVE(INCREMENTAL_PDF_APIS)
    maybeClearHighLatencyDataProviderFlag();
#endif

    updatePageAndDeviceScaleFactors();

    [m_pdfLayerController setFrameSize:size()];
    m_pdfLayerController.get().document = m_pdfDocument.get();

    if (handlesPageScaleFactor())
        m_view->setPageScaleFactor([m_pdfLayerController contentScaleFactor], std::nullopt);

    notifyScrollPositionChanged(IntPoint([m_pdfLayerController scrollPosition]));

    updateHUDVisibility();
    updateHUDLocation();
    updateScrollbars();

    tryRunScriptsInPDFDocument();

    if (isLocked())
        createPasswordEntryForm();

    if (m_frame && [m_pdfLayerController respondsToSelector:@selector(setURLFragment:)])
        [m_pdfLayerController setURLFragment:m_frame->url().fragmentIdentifier().createNSString().get()];
}

void PDFPlugin::createPasswordEntryForm()
{
    if (!supportsForms())
        return;

    auto passwordField = PDFPluginPasswordField::create(this);
    m_passwordField = passwordField.ptr();
    passwordField->attach(m_annotationContainer.get());
}

void PDFPlugin::teardownPasswordEntryForm()
{
    m_passwordField = nullptr;
}

void PDFPlugin::attemptToUnlockPDF(const String& password)
{
    [m_pdfLayerController attemptToUnlockWithPassword:password];

    if (!isLocked()) {
        m_passwordField = nullptr;

        updateHUDVisibility();
        updateHUDLocation();
        updateScrollbars();
    }
}

void PDFPlugin::updatePageAndDeviceScaleFactors()
{
    if (!m_view)
        return;

    CGFloat newScaleFactor = deviceScaleFactor();
    if (!handlesPageScaleFactor()) {
        if (auto* page = m_frame ? m_frame->page() : nullptr)
            newScaleFactor *= page->pageScaleFactor();
    }

    if (newScaleFactor != [m_pdfLayerController deviceScaleFactor])
        [m_pdfLayerController setDeviceScaleFactor:newScaleFactor];
}

void PDFPlugin::deviceScaleFactorChanged(float)
{
    updatePageAndDeviceScaleFactors();
}

IntSize PDFPlugin::contentsSize() const
{
    if (isLocked())
        return { 0, 0 };
    return IntSize([m_pdfLayerController contentSizeRespectingZoom]);
}

unsigned PDFPlugin::firstPageHeight() const
{
    if (isLocked() || ![m_pdfDocument pageCount])
        return 0;
    return static_cast<unsigned>(CGCeiling([[m_pdfDocument pageAtIndex:0] boundsForBox:kPDFDisplayBoxCropBox].size.height));
}

void PDFPlugin::setView(PluginView& view)
{
    PDFPluginBase::setView(view);

    if (view.isUsingUISideCompositing())
        [m_pdfLayerControllerDelegate setShouldFlipAnnotations:YES];
}

void PDFPlugin::teardown()
{
    PDFPluginBase::teardown();

    m_pdfLayerController.get().delegate = nil;

    if (auto* frameView = m_frame && m_frame->coreLocalFrame() ? m_frame->coreLocalFrame()->view() : nullptr)
        frameView->removeScrollableArea(this);

    m_activeAnnotation = nullptr;
    m_annotationContainer = nullptr;
    
    [m_scrollCornerLayer removeFromSuperlayer];
    [m_contentLayer removeFromSuperlayer];
}

void PDFPlugin::paintControlForLayerInContext(CALayer *layer, CGContextRef context)
{
#if PLATFORM(MAC)
    RefPtr page = this->page();
    if (!page)
        return;
    LocalDefaultSystemAppearance localAppearance(page->useDarkAppearance());
#endif

    GraphicsContextCG graphicsContext(context, WebCore::GraphicsContextCG::CGContextFromCALayer);
    GraphicsContextStateSaver stateSaver(graphicsContext);

    if (layer == m_scrollCornerLayer) {
        IntRect scrollCornerRect = this->scrollCornerRect();
        graphicsContext.translate(-scrollCornerRect.x(), -scrollCornerRect.y());
        ScrollbarTheme::theme().paintScrollCorner(*this, graphicsContext, scrollCornerRect);
        return;
    }

    Scrollbar* scrollbar = nullptr;

    if (layer == m_verticalScrollbarLayer)
        scrollbar = verticalScrollbar();
    else if (layer == m_horizontalScrollbarLayer)
        scrollbar = horizontalScrollbar();

    if (!scrollbar)
        return;

    graphicsContext.translate(-scrollbar->x(), -scrollbar->y());
    scrollbar->paint(graphicsContext, scrollbar->frameRect());
}

RefPtr<ShareableBitmap> PDFPlugin::snapshot()
{
    if (size().isEmpty())
        return nullptr;

    float contentsScaleFactor = deviceScaleFactor();
    IntSize backingStoreSize = size();
    backingStoreSize.scale(contentsScaleFactor);

    auto bitmap = ShareableBitmap::create({ backingStoreSize });
    if (!bitmap)
        return nullptr;
    auto context = bitmap->createGraphicsContext();
    if (!context)
        return nullptr;

    context->scale(FloatSize(contentsScaleFactor, -contentsScaleFactor));
    context->translate(-m_scrollOffset.width(), -contentsSize().height() + m_scrollOffset.height());
    [m_pdfLayerController snapshotInContext:context->platformContext()];

    return bitmap;
}

PlatformLayer* PDFPlugin::platformLayer() const
{
    return m_containerLayer.get();
}

bool PDFPlugin::geometryDidChange(const IntSize& pluginSize, const AffineTransform& pluginToRootViewTransform)
{
    if (!PDFPluginBase::geometryDidChange(pluginSize, pluginToRootViewTransform))
        return false;

    [m_pdfLayerController setFrameSize:pluginSize];

    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    CATransform3D transform = CATransform3DMakeScale(1, -1, 1);
    transform = CATransform3DTranslate(transform, 0, -pluginSize.height(), 0);
    
    updateHUDLocation();
    updateScrollbars();

    if (m_activeAnnotation)
        m_activeAnnotation->updateGeometry();

    [m_contentLayer setSublayerTransform:transform];
    [CATransaction commit];

    return true;
}

IntPoint PDFPlugin::convertFromPluginToPDFView(const IntPoint& point) const
{
    return IntPoint(point.x(), size().height() - point.y());
}

IntPoint PDFPlugin::convertFromPDFViewToRootView(const IntPoint& point) const
{
    IntPoint pointInPluginCoordinates(point.x(), size().height() - point.y());
    return valueOrDefault(m_rootViewToPluginTransform.inverse()).mapPoint(pointInPluginCoordinates);
}

IntRect PDFPlugin::convertFromPDFViewToRootView(const IntRect& rect) const
{
    IntRect rectInPluginCoordinates(rect.x(), rect.y(), rect.width(), rect.height());
    return valueOrDefault(m_rootViewToPluginTransform.inverse()).mapRect(rectInPluginCoordinates);
}

IntPoint PDFPlugin::convertFromRootViewToPDFView(const IntPoint& point) const
{
    IntPoint pointInPluginCoordinates = m_rootViewToPluginTransform.mapPoint(point);
    return IntPoint(pointInPluginCoordinates.x(), size().height() - pointInPluginCoordinates.y());
}

FloatRect PDFPlugin::convertFromPDFViewToScreen(const FloatRect& rect) const
{
    return WebCore::Accessibility::retrieveValueFromMainThread<WebCore::FloatRect>([&] () -> WebCore::FloatRect {
        FloatRect updatedRect = rect;
        updatedRect.setLocation(convertFromPDFViewToRootView(IntPoint(updatedRect.location())));
        RefPtr page = this->page();
        if (!page)
            return { };
        return page->chrome().rootViewToScreen(enclosingIntRect(updatedRect));
    });
}

void PDFPlugin::setPageScaleFactor(double scale, std::optional<WebCore::IntPoint> origin)
{
    if (!handlesPageScaleFactor()) {
        // If we don't handle page scale ourselves, we need to respect our parent page's scale.
        updatePageAndDeviceScaleFactors();
        return;
    }

    if (scale == [m_pdfLayerController contentScaleFactor])
        return;

    if (!origin)
        origin = IntRect({ }, size()).center();

    if (CGFloat magnification = scale - [m_pdfLayerController contentScaleFactor])
        [m_pdfLayerController magnifyWithMagnification:magnification atPoint:convertFromPluginToPDFView(*origin) immediately:NO];
}

static NSUInteger modifierFlagsFromWebEvent(const WebEvent& event)
{
    return (event.shiftKey() ? NSEventModifierFlagShift : 0)
        | (event.controlKey() ? NSEventModifierFlagControl : 0)
        | (event.altKey() ? NSEventModifierFlagOption : 0)
        | (event.metaKey() ? NSEventModifierFlagCommand : 0);
}
    
static bool getEventTypeFromWebEvent(const WebEvent& event, NSEventType& eventType)
{
    switch (event.type()) {
    case WebEventType::KeyDown:
        eventType = NSEventTypeKeyDown;
        return true;
    case WebEventType::KeyUp:
        eventType = NSEventTypeKeyUp;
        return true;
    case WebEventType::MouseDown:
        switch (static_cast<const WebMouseEvent&>(event).button()) {
        case WebMouseEventButton::Left:
            eventType = NSEventTypeLeftMouseDown;
            return true;
        case WebMouseEventButton::Right:
            eventType = NSEventTypeRightMouseDown;
            return true;
        default:
            return false;
        }
    case WebEventType::MouseUp:
        switch (static_cast<const WebMouseEvent&>(event).button()) {
        case WebMouseEventButton::Left:
            eventType = NSEventTypeLeftMouseUp;
            return true;
        case WebMouseEventButton::Right:
            eventType = NSEventTypeRightMouseUp;
            return true;
        default:
            return false;
        }
    case WebEventType::MouseMove:
        switch (static_cast<const WebMouseEvent&>(event).button()) {
        case WebMouseEventButton::Left:
            eventType = NSEventTypeLeftMouseDragged;
            return true;
        case WebMouseEventButton::Right:
            eventType = NSEventTypeRightMouseDragged;
            return true;
        case WebMouseEventButton::None:
            eventType = NSEventTypeMouseMoved;
            return true;
        default:
            return false;
        }
    default:
        return false;
    }
}
    
NSEvent *PDFPlugin::nsEventForWebMouseEvent(const WebMouseEvent& event)
{
    IntPoint positionInPDFViewCoordinates(convertFromPluginToPDFView(lastKnownMousePositionInView()));

    NSEventType eventType;

    if (!getEventTypeFromWebEvent(event, eventType))
        return 0;

    NSUInteger modifierFlags = modifierFlagsFromWebEvent(event);

    return [NSEvent mouseEventWithType:eventType location:positionInPDFViewCoordinates modifierFlags:modifierFlags timestamp:0 windowNumber:0 context:nil eventNumber:0 clickCount:event.clickCount() pressure:0];
}

bool PDFPlugin::handleMouseEvent(const WebMouseEvent& event)
{
    m_lastMouseEvent = event;

    PlatformMouseEvent platformEvent = platform(event);
    IntPoint mousePosition = convertFromRootViewToPlugin(event.position());

    RefPtr<Scrollbar> targetScrollbar;
    RefPtr<Scrollbar> targetScrollbarForLastMousePosition;

    if (m_verticalScrollbarLayer) {
        IntRect verticalScrollbarFrame(m_verticalScrollbarLayer.get().frame);
        if (verticalScrollbarFrame.contains(mousePosition))
            targetScrollbar = verticalScrollbar();
        if (verticalScrollbarFrame.contains(lastKnownMousePositionInView()))
            targetScrollbarForLastMousePosition = verticalScrollbar();
    }

    if (m_horizontalScrollbarLayer) {
        IntRect horizontalScrollbarFrame(m_horizontalScrollbarLayer.get().frame);
        if (horizontalScrollbarFrame.contains(mousePosition))
            targetScrollbar = horizontalScrollbar();
        if (horizontalScrollbarFrame.contains(lastKnownMousePositionInView()))
            targetScrollbarForLastMousePosition = horizontalScrollbar();
    }

    if (m_scrollCornerLayer && IntRect(m_scrollCornerLayer.get().frame).contains(mousePosition))
        return false;

    if (isLocked())
        return false;

    // Right-clicks and Control-clicks always call handleContextMenuEvent as well.
    if (event.button() == WebMouseEventButton::Right || (event.button() == WebMouseEventButton::Left && event.controlKey()))
        return true;

    NSEvent *nsEvent = nsEventForWebMouseEvent(event);

    switch (event.type()) {
    case WebEventType::MouseMove:
        mouseMovedInContentArea();

        if (targetScrollbar) {
            if (!targetScrollbarForLastMousePosition) {
                targetScrollbar->mouseEntered();
                return true;
            }
            return targetScrollbar->mouseMoved(platformEvent);
        }

        if (!targetScrollbar && targetScrollbarForLastMousePosition)
            targetScrollbarForLastMousePosition->mouseExited();

        switch (event.button()) {
        case WebMouseEventButton::Left:
            [m_pdfLayerController mouseDragged:nsEvent];
            return true;
        case WebMouseEventButton::Right:
        case WebMouseEventButton::Middle:
            return false;
        case WebMouseEventButton::None:
            [m_pdfLayerController mouseMoved:nsEvent];
            return true;
        default:
            return false;
        }
        break;
    case WebEventType::MouseDown:
        switch (event.button()) {
        case WebMouseEventButton::Left:
            if (targetScrollbar)
                return targetScrollbar->mouseDown(platformEvent);

            [m_pdfLayerController mouseDown:nsEvent];
            return true;
        case WebMouseEventButton::Right:
            [m_pdfLayerController rightMouseDown:nsEvent];
            return true;
        case WebMouseEventButton::Middle:
        case WebMouseEventButton::None:
            return false;
        default:
            return false;
        }
        break;
    case WebEventType::MouseUp:
        switch (event.button()) {
        case WebMouseEventButton::Left:
            if (targetScrollbar)
                return targetScrollbar->mouseUp(platformEvent);

            [m_pdfLayerController mouseUp:nsEvent];
            return true;
        case WebMouseEventButton::Right:
        case WebMouseEventButton::Middle:
        case WebMouseEventButton::None:
            return false;
        default:
            return false;
        }
        break;
    default:
        break;
    }

    return false;
}

bool PDFPlugin::handleMouseEnterEvent(const WebMouseEvent& event)
{
    mouseEnteredContentArea();
    return false;
}

bool PDFPlugin::handleMouseLeaveEvent(const WebMouseEvent&)
{
    mouseExitedContentArea();
    return false;
}
    
bool PDFPlugin::showContextMenuAtPoint(const IntPoint& point)
{
    auto* frameView = m_frame ? m_frame->coreLocalFrame()->view() : nullptr;
    if (!frameView)
        return false;
    IntPoint contentsPoint = frameView->contentsToRootView(point);
    WebMouseEvent event({ WebEventType::MouseDown, OptionSet<WebEventModifier> { }, WallTime::now() }, WebMouseEventButton::Right, 0, contentsPoint, contentsPoint, 0, 0, 0, 1, WebCore::ForceAtClick);
    return handleContextMenuEvent(event);
}

bool PDFPlugin::handleContextMenuEvent(const WebMouseEvent& event)
{
    if (!m_frame || !m_frame->coreLocalFrame())
        return false;
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return false;
    RefPtr frameView = m_frame->coreLocalFrame()->view();
    if (!frameView)
        return false;

    IntPoint point = frameView->contentsToScreen(IntRect(frameView->windowToContents(event.position()), IntSize())).location();

    NSUserInterfaceLayoutDirection uiLayoutDirection = webPage->userInterfaceLayoutDirection() == UserInterfaceLayoutDirection::LTR ? NSUserInterfaceLayoutDirectionLeftToRight : NSUserInterfaceLayoutDirectionRightToLeft;
    RetainPtr nsMenu = [m_pdfLayerController menuForEvent:nsEventForWebMouseEvent(event) withUserInterfaceLayoutDirection:uiLayoutDirection];

    if (!nsMenu)
        return false;
    
    std::optional<int> openInPreviewTag;
    Vector<PDFContextMenuItem> items;
    auto itemCount = [nsMenu numberOfItems];
    for (int i = 0; i < itemCount; i++) {
        auto item = [nsMenu itemAtIndex:i];
        if ([item submenu])
            continue;
        if ([NSStringFromSelector(item.action) isEqualToString:@"openWithPreview"])
            openInPreviewTag = i;
        PDFContextMenuItem menuItem { String([item title]), static_cast<int>([item state]), i,
            [item isEnabled] ? ContextMenuItemEnablement::Enabled : ContextMenuItemEnablement::Disabled,
            [item action] ? ContextMenuItemHasAction::Yes : ContextMenuItemHasAction::No,
            [item isSeparatorItem] ? ContextMenuItemIsSeparator::Yes : ContextMenuItemIsSeparator::No
        };
        items.append(WTFMove(menuItem));
    }
    PDFContextMenu contextMenu { point, WTFMove(items), WTFMove(openInPreviewTag) };

    webPage->sendWithAsyncReply(Messages::WebPageProxy::ShowPDFContextMenu { contextMenu, identifier() }, [itemCount, nsMenu = WTFMove(nsMenu), weakThis = WeakPtr { *this }](std::optional<int32_t>&& selectedIndex) {
        if (RefPtr protectedThis = weakThis.get()) {
            if (selectedIndex && selectedIndex.value() >= 0 && selectedIndex.value() < itemCount)
                [nsMenu performActionForItemAtIndex:*selectedIndex];
        }
    });

    return true;
}

bool PDFPlugin::handleKeyboardEvent(const WebKeyboardEvent& event)
{
    NSEventType eventType;

    if (!getEventTypeFromWebEvent(event, eventType))
        return false;

    NSUInteger modifierFlags = modifierFlagsFromWebEvent(event);
    
    NSEvent *fakeEvent = [NSEvent keyEventWithType:eventType location:NSZeroPoint modifierFlags:modifierFlags timestamp:0 windowNumber:0 context:0 characters:event.text() charactersIgnoringModifiers:event.unmodifiedText() isARepeat:event.isAutoRepeat() keyCode:event.nativeVirtualKeyCode()];
    
    if (event.type() == WebEventType::KeyDown)
        return [m_pdfLayerController keyDown:fakeEvent];
    
    return false;
}
    
bool PDFPlugin::handleEditingCommand(const String& commandName, const String&)
{
    if (commandName == "copy"_s)
        [m_pdfLayerController copySelection];
    else if (commandName == "selectAll"_s)
        [m_pdfLayerController selectAll];
    else if (commandName == "takeFindStringFromSelection"_s) {
        NSString *string = [m_pdfLayerController currentSelection].string;
        if (string.length)
            writeItemsToPasteboard(NSPasteboardNameFind, @[ [string dataUsingEncoding:NSUTF8StringEncoding] ], @[ NSPasteboardTypeString ]);
    }

    return true;
}

bool PDFPlugin::isEditingCommandEnabled(const String& commandName)
{
    if (commandName == "copy"_s || commandName == "takeFindStringFromSelection"_s)
        return [m_pdfLayerController currentSelection];

    if (commandName == "selectAll"_s)
        return true;

    return false;
}

void PDFPlugin::didChangeScrollOffset()
{
    [CATransaction begin];
    [m_pdfLayerController setScrollPosition:IntPoint(m_scrollOffset.width(), m_scrollOffset.height())];

    if (m_activeAnnotation)
        m_activeAnnotation->updateGeometry();

    [CATransaction commit];
}

void PDFPlugin::invalidateScrollbarRect(Scrollbar& scrollbar, const IntRect& rect)
{
    if (&scrollbar == horizontalScrollbar())
        [m_horizontalScrollbarLayer setNeedsDisplay];
    else if (&scrollbar == verticalScrollbar())
        [m_verticalScrollbarLayer setNeedsDisplay];
}

void PDFPlugin::invalidateScrollCornerRect(const IntRect& rect)
{
    [m_scrollCornerLayer setNeedsDisplay];
}

void PDFPlugin::setActiveAnnotation(SetActiveAnnotationParams&& setActiveAnnotationParams)
{
    // This may be called off the main thread if VoiceOver is running, thus dispatch to the main runloop since it involves main thread only objects.
    callOnMainRunLoopAndWait([annotation = WTFMove(setActiveAnnotationParams.annotation), this] {
        if (!supportsForms())
            return;

        if (m_activeAnnotation)
            m_activeAnnotation->commit();

        if (annotation) {
            if (annotationIsWidgetOfType(annotation.get(), WidgetType::Text) && [annotation isReadOnly]) {
                m_activeAnnotation = nullptr;
                return;
            }

            auto activeAnnotation = PDFPluginAnnotation::create(annotation.get(), this);
            m_activeAnnotation = activeAnnotation.get();
            activeAnnotation->attach(m_annotationContainer.get());
        } else
            m_activeAnnotation = nullptr;
    });
}

void PDFPlugin::notifyContentScaleFactorChanged(CGFloat scaleFactor)
{
    if (handlesPageScaleFactor())
        m_view->setPageScaleFactor(scaleFactor, std::nullopt);

    updateHUDLocation();
    updateScrollbars();
}

void PDFPlugin::notifyDisplayModeChanged(int)
{
    updateHUDLocation();
    updateScrollbars();
}

RefPtr<FragmentedSharedBuffer> PDFPlugin::liveResourceData() const
{
    NSData *pdfData = liveData();

    if (!pdfData)
        return nullptr;

    return SharedBuffer::create(pdfData);
}

void PDFPlugin::zoomIn()
{
    [m_pdfLayerController zoomIn:nil];
}

void PDFPlugin::zoomOut()
{
    [m_pdfLayerController zoomOut:nil];
}

void PDFPlugin::showDefinitionForAttributedString(NSAttributedString *string, CGPoint point)
{
    DictionaryPopupInfo dictionaryPopupInfo;
    dictionaryPopupInfo.origin = convertFromPDFViewToRootView(IntPoint(point));
    dictionaryPopupInfo.platformData.attributedString = WebCore::AttributedString::fromNSAttributedString(string);
    
    NSRect rangeRect;
    rangeRect.origin = NSMakePoint(point.x, point.y);
    auto scaleFactor = PDFPlugin::scaleFactor();

    rangeRect.size.height = string.size.height * scaleFactor;
    rangeRect.size.width = string.size.width * scaleFactor;

    rangeRect.origin.y -= rangeRect.size.height;

    TextIndicatorData dataForSelection;
    dataForSelection.selectionRectInRootViewCoordinates = rangeRect;
    dataForSelection.textBoundingRectInRootViewCoordinates = rangeRect;
    dataForSelection.contentImageScaleFactor = scaleFactor;
    dataForSelection.presentationTransition = TextIndicatorPresentationTransition::FadeIn;
    dictionaryPopupInfo.textIndicator = dataForSelection;
    
    if (!m_frame || !m_frame->page())
        return;

    m_frame->page()->send(Messages::WebPageProxy::DidPerformDictionaryLookup(dictionaryPopupInfo));
}

unsigned PDFPlugin::countFindMatches(const String& target, WebCore::FindOptions options, unsigned /*maxMatchCount*/)
{
    // FIXME: Why is it OK to ignore the passed-in maximum match count?

    if (!target.length())
        return 0;

    NSStringCompareOptions nsOptions = options.contains(FindOption::CaseInsensitive) ? NSCaseInsensitiveSearch : 0;
    return [[m_pdfDocument findString:target withOptions:nsOptions] count];
}

PDFSelection *PDFPlugin::nextMatchForString(const String& target, bool searchForward, bool caseSensitive, bool wrapSearch, PDFSelection *initialSelection, bool startInSelection)
{
    if (!target.length())
        return nil;

    NSStringCompareOptions options = 0;
    if (!searchForward)
        options |= NSBackwardsSearch;
    if (!caseSensitive)
        options |= NSCaseInsensitiveSearch;

    RetainPtr<PDFSelection> selectionForInitialSearch = adoptNS([initialSelection copy]);
    if (startInSelection) {
        // Initially we want to include the selected text in the search. So we must modify the starting search
        // selection to fit PDFDocument's search requirements: selection must have a length >= 1, begin before
        // the current selection (if searching forwards) or after (if searching backwards).
        int initialSelectionLength = [[initialSelection string] length];
        if (searchForward) {
            [selectionForInitialSearch extendSelectionAtStart:1];
            [selectionForInitialSearch extendSelectionAtEnd:-initialSelectionLength];
        } else {
            [selectionForInitialSearch extendSelectionAtEnd:1];
            [selectionForInitialSearch extendSelectionAtStart:-initialSelectionLength];
        }
    }

    PDFSelection *foundSelection = [m_pdfDocument findString:target fromSelection:selectionForInitialSearch.get() withOptions:options];

    // If we first searched in the selection, and we found the selection, search again from just past the selection.
    if (startInSelection && [foundSelection isEqual:initialSelection])
        foundSelection = [m_pdfDocument findString:target fromSelection:initialSelection withOptions:options];

    if (!foundSelection && wrapSearch) {
        auto emptySelection = adoptNS([allocPDFSelectionInstance() initWithDocument:m_pdfDocument.get()]);
        foundSelection = [m_pdfDocument findString:target fromSelection:emptySelection.get() withOptions:options];
    }

    return foundSelection;
}

bool PDFPlugin::findString(const String& target, WebCore::FindOptions options, unsigned maxMatchCount)
{
    bool searchForward = !options.contains(FindOption::Backwards);
    bool caseSensitive = !options.contains(FindOption::CaseInsensitive);
    bool wrapSearch = options.contains(FindOption::WrapAround);

    // If the max was zero, any result means we exceeded the max, so we can skip computing the actual count.
    // FIXME: How can always returning true without searching if passed a max of 0 be right?
    // Even if it is right, why not put that special case inside countFindMatches instead of here?
    bool foundMatch = !maxMatchCount || countFindMatches(target, options, maxMatchCount);
    if (options.contains(FindOption::DoNotSetSelection))
        return foundMatch && !target.isEmpty();

    if (target.isEmpty()) {
        auto searchSelection = [m_pdfLayerController searchSelection];
        [m_pdfLayerController findString:target caseSensitive:caseSensitive highlightMatches:YES];
        [m_pdfLayerController setSearchSelection:searchSelection];
        m_lastFindString = emptyString();
        return false;
    }

    if (m_lastFindString == target) {
        auto selection = nextMatchForString(target, searchForward, caseSensitive, wrapSearch, [m_pdfLayerController searchSelection], NO);
        if (!selection)
            return false;
        [m_pdfLayerController setSearchSelection:selection];
        [m_pdfLayerController gotoSelection:selection];
    } else {
        [m_pdfLayerController findString:target caseSensitive:caseSensitive highlightMatches:YES];
        m_lastFindString = target;
    }

    return foundMatch;
}

WebCore::DictionaryPopupInfo PDFPlugin::dictionaryPopupInfoForSelection(PDFSelection *selection, WebCore::TextIndicatorPresentationTransition presentationTransition)
{
    DictionaryPopupInfo dictionaryPopupInfo;
    if (!selection.string.length)
        return dictionaryPopupInfo;

    NSAttributedString *nsAttributedString = [selection] {
        static constexpr unsigned maximumSelectionLength = 250;
        if (selection.string.length > maximumSelectionLength)
            return [selection.attributedString attributedSubstringFromRange:NSMakeRange(0, maximumSelectionLength)];
        return selection.attributedString;
    }();
    RetainPtr scaledNSAttributedString = adoptNS([[NSMutableAttributedString alloc] initWithString:[nsAttributedString string]]);

    auto scaleFactor = contentScaleFactor();

    [nsAttributedString enumerateAttributesInRange:NSMakeRange(0, [nsAttributedString length]) options:0 usingBlock:^(NSDictionary *attributes, NSRange range, BOOL *stop) {
        RetainPtr<NSMutableDictionary> scaledAttributes = adoptNS([attributes mutableCopy]);

        CocoaFont *font = [scaledAttributes objectForKey:NSFontAttributeName];
        if (font) {
            auto desiredFontSize = font.pointSize * scaleFactor;
#if USE(APPKIT)
            font = [[NSFontManager sharedFontManager] convertFont:font toSize:desiredFontSize];
#else
            font = [font fontWithSize:desiredFontSize];
#endif
            [scaledAttributes setObject:font forKey:NSFontAttributeName];
        }

        [scaledNSAttributedString addAttributes:scaledAttributes.get() range:range];
    }];

    NSRect rangeRect = rectForSelectionInRootView(selection);
    rangeRect.size.height = nsAttributedString.size.height * scaleFactor;
    rangeRect.size.width = nsAttributedString.size.width * scaleFactor;

    TextIndicatorData dataForSelection;
    dataForSelection.selectionRectInRootViewCoordinates = rangeRect;
    dataForSelection.textBoundingRectInRootViewCoordinates = rangeRect;
    dataForSelection.contentImageScaleFactor = scaleFactor;
    dataForSelection.presentationTransition = presentationTransition;

    dictionaryPopupInfo.origin = rangeRect.origin;
    dictionaryPopupInfo.textIndicator = dataForSelection;
    dictionaryPopupInfo.platformData.attributedString = WebCore::AttributedString::fromNSAttributedString(scaledNSAttributedString);

    return dictionaryPopupInfo;
}

bool PDFPlugin::performDictionaryLookupAtLocation(const WebCore::FloatPoint& point)
{
    IntPoint localPoint = convertFromRootViewToPlugin(roundedIntPoint(point));
    PDFSelection* lookupSelection = [m_pdfLayerController getSelectionForWordAtPoint:convertFromPluginToPDFView(localPoint)];
    if ([[lookupSelection string] length])
        [m_pdfLayerController searchInDictionaryWithSelection:lookupSelection];
    return true;
}

CGRect PDFPlugin::pluginBoundsForAnnotation(RetainPtr<PDFAnnotation>& annotation) const
{
    auto documentSize = contentSizeRespectingZoom();
    auto annotationBounds = [m_pdfLayerController boundsForAnnotation:annotation.get()];
    annotationBounds.origin.y = documentSize.height - annotationBounds.origin.y - annotationBounds.size.height - m_scrollOffset.height();
    annotationBounds.origin.x -= m_scrollOffset.width();
    return annotationBounds;
}

void PDFPlugin::focusNextAnnotation()
{
    [m_pdfLayerController activateNextAnnotation:false];
}

void PDFPlugin::focusPreviousAnnotation()
{
    [m_pdfLayerController activateNextAnnotation:true];
}

String PDFPlugin::selectionString() const
{
    return [[m_pdfLayerController currentSelection] string];
}

bool PDFPlugin::existingSelectionContainsPoint(const WebCore::FloatPoint& locationInViewCoordinates) const
{
    PDFSelection *currentSelection = [m_pdfLayerController currentSelection];
    if (!currentSelection)
        return false;
    
    IntPoint pointInPDFView = convertFromPluginToPDFView(convertFromRootViewToPlugin(roundedIntPoint(locationInViewCoordinates)));
    PDFSelection *selectionForWord = [m_pdfLayerController getSelectionForWordAtPoint:pointInPDFView];

    NSUInteger currentPageIndex = [m_pdfLayerController currentPageIndex];
    
    NSArray *selectionRects = [m_pdfLayerController rectsForSelectionInLayoutSpace:currentSelection];
    if (!selectionRects || !selectionRects.count)
        return false;
    
    if (currentPageIndex >= selectionRects.count)
        currentPageIndex = selectionRects.count - 1;

    NSArray *wordSelectionRects = [m_pdfLayerController rectsForSelectionInLayoutSpace:selectionForWord];
    if (!wordSelectionRects || !wordSelectionRects.count)
        return false;

    NSValue *selectionBounds = [selectionRects objectAtIndex:currentPageIndex];
    NSValue *wordSelectionBounds = [wordSelectionRects objectAtIndex:0];

    NSRect selectionBoundsRect = selectionBounds.rectValue;
    NSRect wordSelectionBoundsRect = wordSelectionBounds.rectValue;
    return NSIntersectsRect(wordSelectionBoundsRect, selectionBoundsRect);
}

static NSPoint pointInLayoutSpaceForPointInWindowSpace(PDFLayerController* pdfLayerController, NSPoint pointInView)
{
    CGPoint point = NSPointToCGPoint(pointInView);
    CGPoint scrollOffset = [pdfLayerController scrollPosition];
    CGFloat scaleFactor = [pdfLayerController contentScaleFactor];

    scrollOffset.y = [pdfLayerController contentSizeRespectingZoom].height - NSRectToCGRect([pdfLayerController frame]).size.height - scrollOffset.y;

    CGPoint newPoint = CGPointMake(scrollOffset.x + point.x, scrollOffset.y + point.y);
    newPoint.x /= scaleFactor;
    newPoint.y /= scaleFactor;
    return NSPointFromCGPoint(newPoint);
}

std::pair<String, RetainPtr<PDFSelection>> PDFPlugin::textForImmediateActionHitTestAtPoint(const WebCore::FloatPoint& locationInViewCoordinates, WebHitTestResultData& data)
{
    auto selection = [m_pdfLayerController currentSelection];
    if (existingSelectionContainsPoint(locationInViewCoordinates))
        return { selection.string, selection };

    IntPoint pointInPDFView = convertFromPluginToPDFView(convertFromRootViewToPlugin(roundedIntPoint(locationInViewCoordinates)));
    selection = [m_pdfLayerController getSelectionForWordAtPoint:pointInPDFView];
    if (!selection)
        return { emptyString(), nil };

    NSPoint pointInLayoutSpace = pointInLayoutSpaceForPointInWindowSpace(m_pdfLayerController.get(), pointInPDFView);
    PDFPage *currentPage = [[m_pdfLayerController layout] pageNearestPoint:pointInLayoutSpace currentPage:[m_pdfLayerController currentPage]];
    NSPoint pointInPageSpace = [[m_pdfLayerController layout] convertPoint:pointInLayoutSpace toPage:currentPage forScaleFactor:1.0];

    for (PDFAnnotation *annotation in currentPage.annotations) {
        if (![annotation isKindOfClass:getPDFAnnotationLinkClass()])
            continue;

        NSRect bounds = annotation.bounds;
        if (!NSPointInRect(pointInPageSpace, bounds))
            continue;

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        PDFAnnotationLink *linkAnnotation = (PDFAnnotationLink *)annotation;
ALLOW_DEPRECATED_DECLARATIONS_END
        NSURL *url = linkAnnotation.URL;
        if (!url)
            continue;

        data.absoluteLinkURL = url.absoluteString;
        data.linkLabel = selection.string;
        return { selection.string, selection };
    }

    NSString *lookupText = DictionaryLookup::stringForPDFSelection(selection);
    if (!lookupText.length)
        return { emptyString(), selection };

    [m_pdfLayerController setCurrentSelection:selection];
    return { lookupText, selection };
}

static NSRect rectInViewSpaceForRectInLayoutSpace(PDFLayerController* pdfLayerController, NSRect layoutSpaceRect)
{
    CGRect newRect = NSRectToCGRect(layoutSpaceRect);
    CGFloat scaleFactor = pdfLayerController.contentScaleFactor;
    CGPoint scrollOffset = pdfLayerController.scrollPosition;

    scrollOffset.y = pdfLayerController.contentSizeRespectingZoom.height - NSRectToCGRect(pdfLayerController.frame).size.height - scrollOffset.y;

    newRect.origin.x *= scaleFactor;
    newRect.origin.y *= scaleFactor;
    newRect.size.width *= scaleFactor;
    newRect.size.height *= scaleFactor;

    newRect.origin.x -= scrollOffset.x;
    newRect.origin.y -= scrollOffset.y;

    return NSRectFromCGRect(newRect);
}

WebCore::FloatRect PDFPlugin::rectForSelectionInRootView(PDFSelection *selection) const
{
    PDFPage *currentPage = nil;
    NSArray *pages = selection.pages;
    if (pages.count)
        currentPage = (PDFPage *)[pages objectAtIndex:0];

    if (!currentPage)
        currentPage = [m_pdfLayerController currentPage];

    NSRect rectInPageSpace = [selection boundsForPage:currentPage];
    NSRect rectInLayoutSpace = [[m_pdfLayerController layout] convertRect:rectInPageSpace fromPage:currentPage forScaleFactor:1.0];
    NSRect rectInView = rectInViewSpaceForRectInLayoutSpace(m_pdfLayerController.get(), rectInLayoutSpace);

    rectInView.origin = convertFromPDFViewToRootView(IntPoint(rectInView.origin));

    return rectInView;
}

CGSize PDFPlugin::contentSizeRespectingZoom() const
{
    return [m_pdfLayerController contentSizeRespectingZoom];
}

double PDFPlugin::scaleFactor() const
{
    return [m_pdfLayerController contentScaleFactor];
}

double PDFPlugin::contentScaleFactor() const
{
    return scaleFactor();
}

bool PDFPlugin::handleWheelEvent(const WebWheelEvent& event)
{
    PDFDisplayMode displayMode = [m_pdfLayerController displayMode];

    if (displayMode == kPDFDisplaySinglePageContinuous || displayMode == kPDFDisplayTwoUpContinuous)
        return ScrollableArea::handleWheelEventForScrolling(platform(event), { });

    NSUInteger currentPageIndex = [m_pdfLayerController currentPageIndex];
    bool inFirstPage = !currentPageIndex;
    bool inLastPage = [m_pdfLayerController lastPageIndex] == currentPageIndex;

    bool atScrollTop = !scrollPosition().y();
    bool atScrollBottom = scrollPosition().y() == maximumScrollPosition().y();

    bool inMomentumScroll = event.momentumPhase() != WebWheelEvent::PhaseNone;

    int scrollMagnitudeThresholdForPageFlip = defaultScrollMagnitudeThresholdForPageFlip;

    // Imprecise input devices should have a lower threshold so that "clicky" scroll wheels can flip pages.
    if (!event.hasPreciseScrollingDeltas())
        scrollMagnitudeThresholdForPageFlip = 0;

    if (atScrollBottom && !inLastPage && event.delta().height() < 0) {
        if (event.delta().height() <= -scrollMagnitudeThresholdForPageFlip && !inMomentumScroll)
            [m_pdfLayerController gotoNextPage];
        return true;
    }

    if (atScrollTop && !inFirstPage && event.delta().height() > 0) {
        if (event.delta().height() >= scrollMagnitudeThresholdForPageFlip && !inMomentumScroll) {
            [CATransaction begin];
            [m_pdfLayerController gotoPreviousPage];
            scrollToOffsetWithoutAnimation(maximumScrollPosition());
            [CATransaction commit];
        }
        return true;
    }

    return ScrollableArea::handleWheelEventForScrolling(platform(event), { });
}

NSData *PDFPlugin::liveData() const
{
    if (m_activeAnnotation)
        m_activeAnnotation->commit();

    // Save data straight from the resource instead of PDFKit if the document is
    // untouched by the user, so that PDFs which PDFKit can't display will still be downloadable.
    if (m_pdfDocumentWasMutated)
        return [m_pdfDocument dataRepresentation];

    return originalData();
}

id PDFPlugin::accessibilityHitTest(const WebCore::IntPoint& point) const
{
    return [m_accessibilityObject accessibilityHitTestIntPoint:point];
}

id PDFPlugin::accessibilityObject() const
{
    return m_accessibilityObject.get();
}

} // namespace WebKit

#endif // ENABLE(LEGACY_PDFKIT_PLUGIN)
