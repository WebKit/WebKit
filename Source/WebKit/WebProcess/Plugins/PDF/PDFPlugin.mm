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
#import "DataReference.h"
#import "FrameInfoData.h"
#import "Logging.h"
#import "MessageSenderInlines.h"
#import "PDFAnnotationTextWidgetDetails.h"
#import "PDFContextMenu.h"
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
#import "WebLoaderStrategy.h"
#import "WebMouseEvent.h"
#import "WebPage.h"
#import "WebPageProxyMessages.h"
#import "WebPasteboardProxyMessages.h"
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
#import <WebCore/WebAccessibilityObjectWrapperMac.h>
#import <WebCore/WheelEventTestMonitor.h>
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <pal/spi/mac/NSMenuSPI.h>
#import <wtf/HexNumber.h>
#import <wtf/Scope.h>
#import <wtf/UUID.h>
#import <wtf/WTFSemaphore.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/WorkQueue.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/TextStream.h>

#import "PDFKitSoftLink.h"

#if HAVE(INCREMENTAL_PDF_APIS)
@interface PDFDocument ()
-(instancetype)initWithProvider:(CGDataProviderRef)dataProvider;
-(void)preloadDataOfPagesInRange:(NSRange)range onQueue:(dispatch_queue_t)queue completion:(void (^)(NSIndexSet* loadedPageIndexes))completionBlock;
@property (readwrite, nonatomic) BOOL hasHighLatencyDataProvider;
@end
#endif

// Set overflow: hidden on the annotation container so <input> elements scrolled out of view don't show
// scrollbars on the body. We can't add annotations directly to the body, because overflow: hidden on the body
// will break rubber-banding.
static constexpr auto annotationStyle =
"#annotationContainer {"
"    overflow: hidden; "
"    position: absolute; "
"    pointer-events: none; "
"    top: 0; "
"    left: 0; "
"    right: 0; "
"    bottom: 0; "
"    display: -webkit-box; "
"    -webkit-box-align: center; "
"    -webkit-box-pack: center; "
"} "
".annotation { "
"    position: absolute; "
"    pointer-events: auto; "
"} "
"textarea.annotation { "
"    resize: none; "
"} "
"input.annotation[type='password'] { "
"    position: static; "
"    width: 238px; "
"    height: 20px; "
"    margin-top: 110px; "
"    font-size: 15px; "
"} "_s;

// In non-continuous modes, a single scroll event with a magnitude of >= 20px
// will jump to the next or previous page, to match PDFKit behavior.
static const int defaultScrollMagnitudeThresholdForPageFlip = 20;

// <rdar://problem/61473378> - PDFKit asks for a "way too large" range when the PDF it is loading
// incrementally turns out to be non-linearized.
// We'll assume any size over 4gb is PDFKit noticing non-linearized data.
static const uint32_t nonLinearizedPDFSentinel = std::numeric_limits<uint32_t>::max();

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
    // Only active annotations seem to have their associated controls available.
    RefPtr activeAnnotation = _pdfPlugin->activeAnnotation();
    if (!activeAnnotation || ![activeAnnotation->annotation() isEqual:annotation])
        return nil;
    
    WebCore::AXObjectCache* cache = _pdfPlugin->axObjectCache();
    if (!cache)
        return nil;
    
    RefPtr object = cache->getOrCreate(activeAnnotation->element());
    if (!object)
        return nil;

    return object->wrapper();
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
    _pdfPlugin->performWebSearch(string);
}

- (void)performSpotlightSearch:(NSString *)string
{
    _pdfPlugin->performSpotlightSearch(string);
}

- (void)openWithNativeApplication
{
}

- (void)saveToPDF
{
}

- (void)pdfLayerController:(PDFLayerController *)pdfLayerController clickedLinkWithURL:(NSURL *)url
{
    _pdfPlugin->clickedLink(url);
}

- (void)pdfLayerController:(PDFLayerController *)pdfLayerController didChangeActiveAnnotation:(PDFAnnotation *)annotation
{
    _pdfPlugin->setActiveAnnotation(annotation);
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
    _pdfPlugin->notifySelectionChanged(selection);
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
#if HAVE(INCREMENTAL_PDF_APIS)
    , m_streamLoaderClient(adoptRef(*new PDFPluginStreamLoaderClient(*this)))
    , m_incrementalPDFLoadingEnabled(element.document().settings().incrementalPDFLoadingEnabled())
#endif
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

#if HAVE(INCREMENTAL_PDF_APIS)
    if (m_incrementalPDFLoadingEnabled) {
        m_pdfThread = Thread::create("PDF document thread", [protectedThis = Ref { *this }, this] () mutable {
            threadEntry(WTFMove(protectedThis));
        });
    }
#endif
}

#if HAVE(INCREMENTAL_PDF_APIS)
#if !LOG_DISABLED
void PDFPlugin::pdfLog(const String& message)
{
    if (!isMainRunLoop()) {
        callOnMainRunLoop([this, protectedThis = Ref { *this }, message = message.isolatedCopy()] {
            pdfLog(message);
        });
        return;
    }

    LOG_WITH_STREAM(IncrementalPDF, stream << message);
    verboseLog();
    LOG_WITH_STREAM(IncrementalPDFVerbose, stream << message);
}

void PDFPlugin::logStreamLoader(WTF::TextStream& stream, WebCore::NetscapePlugInStreamLoader& loader)
{
    ASSERT(isMainRunLoop());

    auto* request = byteRangeRequestForLoader(loader);
    stream << "(";
    if (!request) {
        stream << "no range request found) ";
        return;
    }

    stream << request->position() << "-" << request->position() + request->count() - 1 << ") ";
}

void PDFPlugin::verboseLog()
{
    ASSERT(isMainRunLoop());

    TextStream stream;
    stream << "\n";
    if (m_pdfThread)
        stream << "Initial PDF thread is still in progress\n";
    else
        stream << "Initial PDF thread has completed\n";

    stream << "Have completed " << m_completedRangeRequests << " range requests (" << m_completedNetworkRangeRequests << " from the network)\n";
    stream << "There are " << m_threadsWaitingOnCallback << " data provider threads waiting on a reply\n";
    stream << "There are " << m_outstandingByteRangeRequests.size() << " byte range requests outstanding\n";

    stream << "There are " << m_streamLoaderMap.size() << " active network stream loaders: ";
    for (auto& loader : m_streamLoaderMap.keys())
        logStreamLoader(stream, *loader);
    stream << "\n";

    stream << "The main document loader has finished loading " << m_streamedBytes << " bytes, and is";
    if (!m_documentFinishedLoading)
        stream << " not";
    stream << " complete";

    LOG(IncrementalPDFVerbose, "%s", stream.release().utf8().data());
}
#endif // !LOG_DISABLED

void PDFPlugin::receivedNonLinearizedPDFSentinel()
{
    m_incrementalPDFLoadingEnabled = false;

    if (m_hasBeenDestroyed)
        return;

    if (!isMainRunLoop()) {
#if !LOG_DISABLED
        pdfLog("Disabling incremental PDF loading on background thread"_s);
#endif
        callOnMainRunLoop([this, protectedThis = Ref { *this }] {
            receivedNonLinearizedPDFSentinel();
        });
        return;
    }

#if !LOG_DISABLED
    pdfLog(makeString("Cancelling all ", m_streamLoaderMap.size(), " range request loaders"));
#endif

    for (auto iterator = m_streamLoaderMap.begin(); iterator != m_streamLoaderMap.end(); iterator = m_streamLoaderMap.begin()) {
        removeOutstandingByteRangeRequest(iterator->value);
        cancelAndForgetLoader(*iterator->key);
    }

    if (!m_documentFinishedLoading || m_pdfDocument)
        return;

    createPDFDocument();
    installPDFDocument();
    tryRunScriptsInPDFDocument();
}

static size_t dataProviderGetBytesAtPositionCallback(void* info, void* buffer, off_t position, size_t count)
{
    Ref<PDFPlugin> plugin = *((PDFPlugin*)info);

    if (isMainRunLoop()) {
#if !LOG_DISABLED
        plugin->pdfLog(makeString("Handling request for ", count, " bytes at position ", position, " synchronously on the main thread"));
#endif
        return plugin->getResourceBytesAtPositionMainThread(buffer, position, count);
    }

    // It's possible we previously encountered an invalid range and therefore disabled incremental loading,
    // but PDFDocument is still trying to read data using a different strategy.
    // Always ignore such requests.
    if (!plugin->incrementalPDFLoadingEnabled())
        return 0;

#if !LOG_DISABLED
    auto debugPluginRef = plugin.copyRef();
    debugPluginRef->incrementThreadsWaitingOnCallback();
    debugPluginRef->pdfLog(makeString("PDF data provider requesting ", count, " bytes at position ", position));
#endif

    if (position > nonLinearizedPDFSentinel) {
#if !LOG_DISABLED
        plugin->pdfLog(makeString("Received invalid range request for ", count, " bytes at position ", position, ". PDF is probably not linearized. Falling back to non-incremental loading."));
#endif
        plugin->receivedNonLinearizedPDFSentinel();
        return 0;
    }

    WTF::Semaphore dataSemaphore { 0 };
    size_t bytesProvided = 0;

    RunLoop::main().dispatch([plugin = WTFMove(plugin), position, count, buffer, &dataSemaphore, &bytesProvided] {
        plugin->getResourceBytesAtPosition(count, position, [count, buffer, &dataSemaphore, &bytesProvided](const uint8_t* bytes, size_t bytesCount) {
            RELEASE_ASSERT(bytesCount <= count);
            memcpy(buffer, bytes, bytesCount);
            bytesProvided = bytesCount;
            dataSemaphore.signal();
        });
    });

    dataSemaphore.wait();

#if !LOG_DISABLED
    debugPluginRef->decrementThreadsWaitingOnCallback();
    debugPluginRef->pdfLog(makeString("PDF data provider received ", bytesProvided, " bytes of requested ", count));
#endif

    return bytesProvided;
}

static void dataProviderGetByteRangesCallback(void* info, CFMutableArrayRef buffers, const CFRange* ranges, size_t count)
{
    ASSERT(!isMainRunLoop());

#if !LOG_DISABLED
    Ref<PDFPlugin> debugPluginRef = *((PDFPlugin*)info);
    debugPluginRef->incrementThreadsWaitingOnCallback();
    TextStream stream;
    stream << "PDF data provider requesting " << count << " byte ranges (";
    for (size_t i = 0; i < count; ++i) {
        stream << ranges[i].length << " at " << ranges[i].location;
        if (i < count - 1)
            stream << ", ";
    }
    stream << ")";
    debugPluginRef->pdfLog(stream.release());
#endif

    Ref<PDFPlugin> plugin = *((PDFPlugin*)info);
    WTF::Semaphore dataSemaphore { 0 };
    Vector<RetainPtr<CFDataRef>> dataResults(count);

    // FIXME: Once we support multi-range requests, make a single request for all ranges instead of <count> individual requests.
    RunLoop::main().dispatch([plugin = WTFMove(plugin), &dataResults, ranges, count, &dataSemaphore] {
        for (size_t i = 0; i < count; ++i) {
            plugin->getResourceBytesAtPosition(ranges[i].length, ranges[i].location, [i, &dataResults, &dataSemaphore](const uint8_t* bytes, size_t bytesCount) {
                dataResults[i] = adoptCF(CFDataCreate(kCFAllocatorDefault, bytes, bytesCount));
                dataSemaphore.signal();
            });
        }
    });

    for (size_t i = 0; i < count; ++i)
        dataSemaphore.wait();

#if !LOG_DISABLED
    debugPluginRef->decrementThreadsWaitingOnCallback();
    debugPluginRef->pdfLog(makeString("PDF data provider finished receiving the requested ", count, " byte ranges"));
#endif

    for (auto& result : dataResults) {
        if (!result)
            result = adoptCF(CFDataCreate(kCFAllocatorDefault, 0, 0));
        CFArrayAppendValue(buffers, result.get());
    }
}

static void dataProviderReleaseInfoCallback(void* info)
{
    ASSERT(!isMainRunLoop());
    adoptRef((PDFPlugin*)info);
}

void PDFPlugin::maybeClearHighLatencyDataProviderFlag()
{
    if (!m_pdfDocument || !m_documentFinishedLoading)
        return;

    if ([m_pdfDocument.get() respondsToSelector:@selector(setHasHighLatencyDataProvider:)])
        [m_pdfDocument.get() setHasHighLatencyDataProvider:NO];
}

void PDFPlugin::threadEntry(Ref<PDFPlugin>&& protectedPlugin)
{
    CGDataProviderDirectAccessRangesCallbacks dataProviderCallbacks {
        0,
        dataProviderGetBytesAtPositionCallback,
        dataProviderGetByteRangesCallback,
        dataProviderReleaseInfoCallback,
    };

    auto scopeExit = makeScopeExit([protectedPlugin = WTFMove(protectedPlugin)] () mutable {
        // Keep the PDFPlugin alive until the end of this function and the end
        // of the last main thread task submitted by this function.
        callOnMainRunLoop([protectedPlugin = WTFMove(protectedPlugin)] { });
    });

    // Balanced by a deref inside of the dataProviderReleaseInfoCallback
    ref();

    RetainPtr<CGDataProviderRef> dataProvider = adoptCF(CGDataProviderCreateMultiRangeDirectAccess(this, kCGDataProviderIndeterminateSize, &dataProviderCallbacks));
    CGDataProviderSetProperty(dataProvider.get(), kCGDataProviderHasHighLatency, kCFBooleanTrue);
    m_backgroundThreadDocument = adoptNS([allocPDFDocumentInstance() initWithProvider:dataProvider.get()]);

    // [PDFDocument initWithProvider:] will return nil in cases where the PDF is non-linearized.
    // In those cases we'll just keep buffering the entire PDF on the main thread.
    if (!m_backgroundThreadDocument) {
        LOG(IncrementalPDF, "Background thread [PDFDocument initWithProvider:] returned nil. PDF is not linearized. Reverting to main thread.");
        receivedNonLinearizedPDFSentinel();
        return;
    }

    if (!m_incrementalPDFLoadingEnabled) {
        m_backgroundThreadDocument = nil;
        return;
    }

    WTF::Semaphore firstPageSemaphore { 0 };
    auto firstPageQueue = WorkQueue::create("PDF first page work queue");

    [m_backgroundThreadDocument preloadDataOfPagesInRange:NSMakeRange(0, 1) onQueue:firstPageQueue->dispatchQueue() completion:[&firstPageSemaphore, this] (NSIndexSet *) mutable {
        if (m_incrementalPDFLoadingEnabled) {
            callOnMainRunLoop([this] {
                adoptBackgroundThreadDocument();
            });
        } else
            m_backgroundThreadDocument = nil;

        firstPageSemaphore.signal();
    }];

    firstPageSemaphore.wait();

#if !LOG_DISABLED
    pdfLog("Finished preloading first page"_s);
#endif
}

void PDFPlugin::unconditionalCompleteOutstandingRangeRequests()
{
    for (auto& request : m_outstandingByteRangeRequests.values())
        request.completeUnconditionally(*this);
    m_outstandingByteRangeRequests.clear();
}

size_t PDFPlugin::getResourceBytesAtPositionMainThread(void* buffer, off_t position, size_t count)
{
    ASSERT(isMainRunLoop());
    ASSERT(m_documentFinishedLoading);
    ASSERT(position >= 0);

    auto cfLength = m_data ? CFDataGetLength(m_data.get()) : 0;
    ASSERT(cfLength >= 0);

    if ((unsigned)position + count > (unsigned)cfLength) {
        // We could return partial data, but this method should only be called
        // once the entire buffer is known, and therefore PDFKit should only
        // be asking for valid ranges.
        return 0;
    }
    memcpy(buffer, CFDataGetBytePtr(m_data.get()) + position, count);
    return count;
}

void PDFPlugin::getResourceBytesAtPosition(size_t count, off_t position, CompletionHandler<void(const uint8_t*, size_t)>&& completionHandler)
{
    ASSERT(isMainRunLoop());
    ASSERT(position >= 0);

    ByteRangeRequest request = { static_cast<uint64_t>(position), count, WTFMove(completionHandler) };
    if (request.maybeComplete(*this))
        return;

    if (m_documentFinishedLoading) {
        request.completeUnconditionally(*this);
        return;
    }

    auto identifier = request.identifier();
    m_outstandingByteRangeRequests.set(identifier, WTFMove(request));

    if (!m_frame)
        return;
    RefPtr coreFrame = m_frame->coreLocalFrame();
    if (!coreFrame)
        return;

    RefPtr documentLoader = coreFrame->loader().documentLoader();
    if (!documentLoader)
        return;

    auto resourceRequest = documentLoader->request();
    resourceRequest.setURL(m_view->mainResourceURL());
    resourceRequest.setHTTPHeaderField(HTTPHeaderName::Range, makeString("bytes="_s, position, "-"_s, position + count - 1));
    resourceRequest.setCachePolicy(ResourceRequestCachePolicy::DoNotUseAnyCache);

#if !LOG_DISABLED
    pdfLog(makeString("Scheduling a stream loader for request ", identifier, " (", count, " bytes at ", position, ")"));
#endif

    WebProcess::singleton().webLoaderStrategy().schedulePluginStreamLoad(*coreFrame, m_streamLoaderClient, WTFMove(resourceRequest), [this, protectedThis = Ref { *this }, identifier] (RefPtr<WebCore::NetscapePlugInStreamLoader>&& loader) {
        if (!loader)
            return;
        auto iterator = m_outstandingByteRangeRequests.find(identifier);
        if (iterator == m_outstandingByteRangeRequests.end()) {
            loader->cancel(loader->cancelledError());
            return;
        }

        iterator->value.setStreamLoader(loader.get());
        m_streamLoaderMap.set(WTFMove(loader), identifier);

#if !LOG_DISABLED
        pdfLog(makeString("There are now ", m_streamLoaderMap.size(), " stream loaders in flight"));
#endif
    });
}

void PDFPlugin::adoptBackgroundThreadDocument()
{
    if (m_hasBeenDestroyed)
        return;

    ASSERT(!m_pdfDocument);
    ASSERT(m_backgroundThreadDocument);
    ASSERT(isMainRunLoop());

#if !LOG_DISABLED
    pdfLog("Adopting PDFDocument from background thread"_s);
#endif

    m_pdfDocument = WTFMove(m_backgroundThreadDocument);

    // If the plugin was manually destroyed, the m_pdfThread might already be gone.
    if (m_pdfThread) {
        m_pdfThread->waitForCompletion();
        m_pdfThread = nullptr;
    }

    // If the plugin is being destroyed, no point in doing any more PDF work
    if (m_isBeingDestroyed)
        return;

    installPDFDocument();
}

void PDFPlugin::ByteRangeRequest::clearStreamLoader()
{
    ASSERT(m_streamLoader);
    m_streamLoader = nullptr;
    m_accumulatedData.clear();
}

void PDFPlugin::ByteRangeRequest::completeWithBytes(const uint8_t* data, size_t count, PDFPlugin& plugin)
{
#if !LOG_DISABLED
    ++plugin.m_completedRangeRequests;
    plugin.pdfLog(makeString("Completing range request ", identifier()," (", m_count," bytes at ", m_position,") with ", count," bytes from the main PDF buffer"));
#endif
    m_completionHandler(data, count);

    if (m_streamLoader)
        plugin.forgetLoader(*m_streamLoader);
}

void PDFPlugin::ByteRangeRequest::completeWithAccumulatedData(PDFPlugin& plugin)
{
#if !LOG_DISABLED
    ++plugin.m_completedRangeRequests;
    ++plugin.m_completedNetworkRangeRequests;
    plugin.pdfLog(makeString("Completing range request ", identifier()," (", m_count," bytes at ", m_position,") with ", m_accumulatedData.size()," bytes from the network"));
#endif

    auto completionSize = m_accumulatedData.size();
    if (completionSize > m_count) {
        RELEASE_LOG_ERROR(IncrementalPDF, "PDF byte range request got more bytes back from the server than requested. This is likely due to a misconfigured server. Capping result at the requested number of bytes.");
        completionSize = m_count;
    }

    m_completionHandler(m_accumulatedData.data(), completionSize);

    // Fold this data into the main data buffer so that if something in its range is requested again (which happens quite often)
    // we do not need to hit the network layer again.
    plugin.ensureDataBufferLength(m_position + m_accumulatedData.size());
    if (m_accumulatedData.size()) {
        memcpy(CFDataGetMutableBytePtr(plugin.m_data.get()) + m_position, m_accumulatedData.data(), m_accumulatedData.size());
        plugin.m_completedRanges.add({ m_position, m_position + m_accumulatedData.size() - 1});
    }

    if (m_streamLoader)
        plugin.forgetLoader(*m_streamLoader);
}

bool PDFPlugin::ByteRangeRequest::maybeComplete(PDFPlugin& plugin)
{
    if (!plugin.m_data)
        return false;

    if (plugin.m_streamedBytes >= m_position + m_count) {
        completeWithBytes(CFDataGetBytePtr(plugin.m_data.get()) + m_position, m_count, plugin);
        return true;
    }

    if (plugin.m_completedRanges.contains({ m_position, m_position + m_count - 1 })) {
#if !LOG_DISABLED
        plugin.pdfLog(makeString("Completing request %llu with a previously completed range", identifier()));
#endif
        completeWithBytes(CFDataGetBytePtr(plugin.m_data.get()) + m_position, m_count, plugin);
        return true;
    }

    return false;
}

void PDFPlugin::ByteRangeRequest::completeUnconditionally(PDFPlugin& plugin)
{
    if (m_position >= plugin.m_streamedBytes || !plugin.m_data) {
        completeWithBytes(nullptr, 0, plugin);
        return;
    }

    auto count = m_position + m_count > plugin.m_streamedBytes ? plugin.m_streamedBytes - m_position : m_count;
    completeWithBytes(CFDataGetBytePtr(plugin.m_data.get()) + m_position, count, plugin);
}

void PDFPlugin::PDFPluginStreamLoaderClient::willSendRequest(NetscapePlugInStreamLoader* loader, ResourceRequest&&, const ResourceResponse&, CompletionHandler<void(ResourceRequest&&)>&&)
{
    if (!m_pdfPlugin)
        return;

    // Redirections for range requests are unexpected.
    m_pdfPlugin->cancelAndForgetLoader(*loader);
}

void PDFPlugin::PDFPluginStreamLoaderClient::didReceiveResponse(NetscapePlugInStreamLoader* loader, const ResourceResponse& response)
{
    if (!m_pdfPlugin)
        return;

    auto* request = m_pdfPlugin->byteRangeRequestForLoader(*loader);
    if (!request) {
        m_pdfPlugin->cancelAndForgetLoader(*loader);
        return;
    }

    ASSERT(request->streamLoader() == loader);

    // Range success! We'll expect to receive the data in future didReceiveData callbacks.
    if (response.httpStatusCode() == 206)
        return;

    // If the response wasn't a successful range response, we don't need this stream loader anymore.
    // This can happen, for example, if the server doesn't support range requests.
    // We'll still resolve the ByteRangeRequest later once enough of the full resource has loaded.
    m_pdfPlugin->cancelAndForgetLoader(*loader);

    // The server might support range requests and explicitly told us this range was not satisfiable.
    // In this case, we can reject the ByteRangeRequest right away.
    if (response.httpStatusCode() == 416 && request) {
        request->completeWithAccumulatedData(*m_pdfPlugin);
        m_pdfPlugin->removeOutstandingByteRangeRequest(request->identifier());
    }
}

void PDFPlugin::PDFPluginStreamLoaderClient::didReceiveData(NetscapePlugInStreamLoader* loader, const SharedBuffer& data)
{
    if (!m_pdfPlugin)
        return;

    auto* request = m_pdfPlugin->byteRangeRequestForLoader(*loader);
    if (!request)
        return;

    request->addData(data.data(), data.size());
}

void PDFPlugin::PDFPluginStreamLoaderClient::didFail(NetscapePlugInStreamLoader* loader, const ResourceError&)
{
    if (!m_pdfPlugin)
        return;

    if (m_pdfPlugin->documentFinishedLoading()) {
        auto identifier = m_pdfPlugin->identifierForLoader(loader);
        if (identifier)
            m_pdfPlugin->removeOutstandingByteRangeRequest(identifier);
    }

    m_pdfPlugin->forgetLoader(*loader);
}

void PDFPlugin::PDFPluginStreamLoaderClient::didFinishLoading(NetscapePlugInStreamLoader* loader)
{
    if (!m_pdfPlugin)
        return;

    auto* request = m_pdfPlugin->byteRangeRequestForLoader(*loader);
    if (!request)
        return;

    request->completeWithAccumulatedData(*m_pdfPlugin);
    m_pdfPlugin->removeOutstandingByteRangeRequest(request->identifier());
}

PDFPlugin::ByteRangeRequest* PDFPlugin::byteRangeRequestForLoader(NetscapePlugInStreamLoader& loader)
{
    uint64_t identifier = identifierForLoader(&loader);
    if (!identifier)
        return nullptr;

    auto request = m_outstandingByteRangeRequests.find(identifier);
    if (request == m_outstandingByteRangeRequests.end())
        return nullptr;

    return &(request->value);
}

void PDFPlugin::forgetLoader(NetscapePlugInStreamLoader& loader)
{
    uint64_t identifier = identifierForLoader(&loader);
    if (!identifier) {
        ASSERT(!m_streamLoaderMap.contains(&loader));
        return;
    }

    if (auto* request = byteRangeRequestForLoader(loader)) {
        if (request->streamLoader()) {
            ASSERT(request->streamLoader() == &loader);
            request->clearStreamLoader();
        }
    }

    m_streamLoaderMap.remove(&loader);

#if !LOG_DISABLED
    pdfLog(makeString("Forgot stream loader for range request ", identifier,". There are now ", m_streamLoaderMap.size()," stream loaders remaining"));
#endif
}

void PDFPlugin::cancelAndForgetLoader(NetscapePlugInStreamLoader& loader)
{
    Ref protectedLoader { loader };
    forgetLoader(loader);
    loader.cancel(loader.cancelledError());
}
#endif // HAVE(INCREMENTAL_PDF_APIS)

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

FloatSize PDFPlugin::pdfDocumentSizeForPrinting() const
{
    return FloatSize([[m_pdfDocument pageAtIndex:0] boundsForBox:kPDFDisplayBoxCropBox].size);
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

    updatePDFHUDLocation();
    updateScrollbars();

    tryRunScriptsInPDFDocument();

    if (isLocked())
        createPasswordEntryForm();

    if (m_frame && [m_pdfLayerController respondsToSelector:@selector(setURLFragment:)])
        [m_pdfLayerController setURLFragment:m_frame->url().fragmentIdentifier().createNSString().get()];
}

bool PDFPlugin::incrementalPDFStreamDidFinishLoading()
{
#if HAVE(INCREMENTAL_PDF_APIS)
#if !LOG_DISABLED
    pdfLog(makeString("PDF document finished loading with a total of ", m_streamedBytes, " bytes"));
#endif
    if (m_incrementalPDFLoadingEnabled) {
        // At this point we know all data for the document, and therefore we know how to answer any outstanding range requests.
        unconditionalCompleteOutstandingRangeRequests();
        maybeClearHighLatencyDataProviderFlag();
        return true;
    }
#endif
    return false;
}

void PDFPlugin::incrementalPDFStreamDidReceiveData(const SharedBuffer& buffer)
{
#if HAVE(INCREMENTAL_PDF_APIS)
    // Keep our ranges-lookup-table compact by continuously updating its first range
    // as the entire document streams in from the network.
    m_completedRanges.add({ 0, m_streamedBytes - 1 });
    
    if (m_incrementalPDFLoadingEnabled) {
        HashSet<uint64_t> handledRequests;
        for (auto& request : m_outstandingByteRangeRequests.values()) {
            if (request.maybeComplete(*this))
                handledRequests.add(request.identifier());
        }

        for (auto identifier : handledRequests) {
            auto request = m_outstandingByteRangeRequests.take(identifier);
            if (request.streamLoader())
                cancelAndForgetLoader(*request.streamLoader());
        }
    }
#endif
}

void PDFPlugin::incrementalPDFStreamDidFail()
{
#if HAVE(INCREMENTAL_PDF_APIS)
    if (m_incrementalPDFLoadingEnabled)
        unconditionalCompleteOutstandingRangeRequests();
#endif
}

void PDFPlugin::createPasswordEntryForm()
{
    if (!supportsForms())
        return;

    auto passwordField = PDFPluginPasswordField::create(m_pdfLayerController.get(), this);
    m_passwordField = passwordField.ptr();
    passwordField->attach(m_annotationContainer.get());
}

void PDFPlugin::attemptToUnlockPDF(const String& password)
{
    [m_pdfLayerController attemptToUnlockWithPassword:password];

    if (!isLocked()) {
        m_passwordField = nullptr;

        updatePDFHUDLocation();
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

#if HAVE(INCREMENTAL_PDF_APIS)
    // By clearing out the resource data and handling all outstanding range requests,
    // we can force the PDFThread to complete quickly
    if (m_pdfThread) {
        m_data = nullptr;
        unconditionalCompleteOutstandingRangeRequests();
        m_pdfThread->waitForCompletion();
    }
#endif

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

void PDFPlugin::geometryDidChange(const IntSize& pluginSize, const AffineTransform& pluginToRootViewTransform)
{
    if (size() == pluginSize)
        return;

    LOG_WITH_STREAM(Plugins, stream << "PDFPlugin::geometryDidChange - size " << pluginSize << " pluginToRootViewTransform " << pluginToRootViewTransform);
    PDFPluginBase::geometryDidChange(pluginSize, pluginToRootViewTransform);

    [m_pdfLayerController setFrameSize:pluginSize];

    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    CATransform3D transform = CATransform3DMakeScale(1, -1, 1);
    transform = CATransform3DTranslate(transform, 0, -pluginSize.height(), 0);
    
    updatePDFHUDLocation();
    updateScrollbars();

    if (m_activeAnnotation)
        m_activeAnnotation->updateGeometry();

    [m_contentLayer setSublayerTransform:transform];
    [CATransaction commit];
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
    m_lastMousePositionInPluginCoordinates = convertFromRootViewToPlugin(event.position());

    IntPoint positionInPDFViewCoordinates(convertFromPluginToPDFView(m_lastMousePositionInPluginCoordinates));

    NSEventType eventType;

    if (!getEventTypeFromWebEvent(event, eventType))
        return 0;

    NSUInteger modifierFlags = modifierFlagsFromWebEvent(event);

    return [NSEvent mouseEventWithType:eventType location:positionInPDFViewCoordinates modifierFlags:modifierFlags timestamp:0 windowNumber:0 context:nil eventNumber:0 clickCount:event.clickCount() pressure:0];
}

bool PDFPlugin::handleMouseEvent(const WebMouseEvent& event)
{
    PlatformMouseEvent platformEvent = platform(event);
    IntPoint mousePosition = convertFromRootViewToPlugin(event.position());

    m_lastMouseEvent = event;

    RefPtr<Scrollbar> targetScrollbar;
    RefPtr<Scrollbar> targetScrollbarForLastMousePosition;

    if (m_verticalScrollbarLayer) {
        IntRect verticalScrollbarFrame(m_verticalScrollbarLayer.get().frame);
        if (verticalScrollbarFrame.contains(mousePosition))
            targetScrollbar = verticalScrollbar();
        if (verticalScrollbarFrame.contains(m_lastMousePositionInPluginCoordinates))
            targetScrollbarForLastMousePosition = verticalScrollbar();
    }

    if (m_horizontalScrollbarLayer) {
        IntRect horizontalScrollbarFrame(m_horizontalScrollbarLayer.get().frame);
        if (horizontalScrollbarFrame.contains(mousePosition))
            targetScrollbar = horizontalScrollbar();
        if (horizontalScrollbarFrame.contains(m_lastMousePositionInPluginCoordinates))
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
    NSMenu *nsMenu = [m_pdfLayerController menuForEvent:nsEventForWebMouseEvent(event) withUserInterfaceLayoutDirection:uiLayoutDirection];

    if (!nsMenu)
        return false;
    
    std::optional<int> openInPreviewIndex;
    Vector<PDFContextMenuItem> items;
    auto itemCount = [nsMenu numberOfItems];
    for (int i = 0; i < itemCount; i++) {
        auto item = [nsMenu itemAtIndex:i];
        if ([item submenu])
            continue;
        if ([NSStringFromSelector(item.action) isEqualToString:@"openWithPreview"])
            openInPreviewIndex = i;
        PDFContextMenuItem menuItem { String([item title]), static_cast<int>([item state]), i,
            [item isEnabled] ? ContextMenuItemEnablement::Enabled : ContextMenuItemEnablement::Disabled,
            [item action] ? ContextMenuItemHasAction::Yes : ContextMenuItemHasAction::No,
            [item isSeparatorItem] ? ContextMenuItemIsSeparator::Yes : ContextMenuItemIsSeparator::No
        };
        items.append(WTFMove(menuItem));
    }
    PDFContextMenu contextMenu { point, WTFMove(items), WTFMove(openInPreviewIndex) };

    auto sendResult = webPage->sendSync(Messages::WebPageProxy::ShowPDFContextMenu(contextMenu, m_identifier));
    auto [selectedIndex] = sendResult.takeReplyOr(-1);

    if (selectedIndex && *selectedIndex >= 0 && *selectedIndex < itemCount)
        [nsMenu performActionForItemAtIndex:*selectedIndex];

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

void PDFPlugin::clickedLink(NSURL *url)
{
    URL coreURL = url;
    if (coreURL.protocolIsJavaScript())
        return;

    auto* frame = m_frame ? m_frame->coreLocalFrame() : nullptr;
    if (!frame)
        return;

    RefPtr<Event> coreEvent;
    if (m_lastMouseEvent)
        coreEvent = MouseEvent::create(eventNames().clickEvent, &frame->windowProxy(), platform(*m_lastMouseEvent), 0, 0);

    frame->loader().changeLocation(coreURL, emptyAtom(), coreEvent.get(), ReferrerPolicy::NoReferrer, ShouldOpenExternalURLsPolicy::ShouldAllow);
}

void PDFPlugin::setActiveAnnotation(PDFAnnotation *annotation)
{
    if (!supportsForms())
        return;

    if (m_activeAnnotation)
        m_activeAnnotation->commit();

    if (annotation) {
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        if ([annotation isKindOfClass:getPDFAnnotationTextWidgetClass()] && static_cast<PDFAnnotationTextWidget *>(annotation).isReadOnly) {
            m_activeAnnotation = nullptr;
            return;
        }
ALLOW_DEPRECATED_DECLARATIONS_END

        auto activeAnnotation = PDFPluginAnnotation::create(annotation, m_pdfLayerController.get(), this);
        m_activeAnnotation = activeAnnotation.get();
        activeAnnotation->attach(m_annotationContainer.get());
    } else
        m_activeAnnotation = nullptr;
}

bool PDFPlugin::supportsForms()
{
    // FIXME: We support forms for full-main-frame and <iframe> PDFs, but not <embed> or <object>, because those cases do not have their own Document into which to inject form elements.
    return isFullFramePlugin();
}

void PDFPlugin::notifyContentScaleFactorChanged(CGFloat scaleFactor)
{
    if (handlesPageScaleFactor())
        m_view->setPageScaleFactor(scaleFactor, std::nullopt);

    updatePDFHUDLocation();
    updateScrollbars();
}

void PDFPlugin::notifyDisplayModeChanged(int)
{
    updatePDFHUDLocation();
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

void PDFPlugin::save(CompletionHandler<void(const String&, const URL&, const IPC::DataReference&)>&& completionHandler)
{
    NSData *data = liveData();
    URL url;
    if (m_frame)
        url = m_frame->url();
    completionHandler(m_suggestedFilename, url, IPC:: DataReference(static_cast<const uint8_t*>(data.bytes), data.length));
}

void PDFPlugin::openWithPreview(CompletionHandler<void(const String&, FrameInfoData&&, const IPC::DataReference&, const String&)>&& completionHandler)
{
    NSData *data = liveData();
    FrameInfoData frameInfo;
    if (m_frame)
        frameInfo = m_frame->info();
    completionHandler(m_suggestedFilename, WTFMove(frameInfo), IPC:: DataReference { static_cast<const uint8_t*>(data.bytes), data.length }, createVersion4UUIDString());
}

void PDFPlugin::writeItemsToPasteboard(NSString *pasteboardName, NSArray *items, NSArray *types)
{
    auto pasteboardTypes = makeVector<String>(types);
    auto pageIdentifier = m_frame && m_frame->coreLocalFrame() ? m_frame->coreLocalFrame()->pageID() : std::nullopt;

    auto& webProcess = WebProcess::singleton();
    webProcess.parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::SetPasteboardTypes(pasteboardName, pasteboardTypes, pageIdentifier), 0);

    for (NSUInteger i = 0, count = items.count; i < count; ++i) {
        NSString *type = [types objectAtIndex:i];
        NSData *data = [items objectAtIndex:i];

        // We don't expect the data for any items to be empty, but aren't completely sure.
        // Avoid crashing in the SharedMemory constructor in release builds if we're wrong.
        ASSERT(data.length);
        if (!data.length)
            continue;

        if ([type isEqualToString:legacyStringPasteboardType()] || [type isEqualToString:NSPasteboardTypeString]) {
            auto plainTextString = adoptNS([[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding]);
            webProcess.parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::SetPasteboardStringForType(pasteboardName, type, plainTextString.get(), pageIdentifier), 0);
        } else {
            auto buffer = SharedBuffer::create(data);
            webProcess.parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::SetPasteboardBufferForType(pasteboardName, type, WTFMove(buffer), pageIdentifier), 0);
        }
    }
}

void PDFPlugin::showDefinitionForAttributedString(NSAttributedString *string, CGPoint point)
{
    DictionaryPopupInfo dictionaryPopupInfo;
    dictionaryPopupInfo.origin = convertFromPDFViewToRootView(IntPoint(point));
    dictionaryPopupInfo.platformData.attributedString = WebCore::AttributedString::fromNSAttributedString(string);
    
    
    NSRect rangeRect;
    rangeRect.origin = NSMakePoint(point.x, point.y);
    CGFloat scaleFactor = PDFPlugin::scaleFactor();

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

    NSStringCompareOptions nsOptions = options.contains(WebCore::CaseInsensitive) ? NSCaseInsensitiveSearch : 0;
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
    bool searchForward = !options.contains(WebCore::Backwards);
    bool caseSensitive = !options.contains(WebCore::CaseInsensitive);
    bool wrapSearch = options.contains(WebCore::WrapAround);

    // If the max was zero, any result means we exceeded the max, so we can skip computing the actual count.
    // FIXME: How can always returning true without searching if passed a max of 0 be right?
    // Even if it is right, why not put that special case inside countFindMatches instead of here?
    bool foundMatch = !maxMatchCount || countFindMatches(target, options, maxMatchCount);

    if (target.isEmpty()) {
        auto searchSelection = [m_pdfLayerController searchSelection];
        [m_pdfLayerController findString:target caseSensitive:caseSensitive highlightMatches:YES];
        [m_pdfLayerController setSearchSelection:searchSelection];
        m_lastFoundString = emptyString();
        return false;
    }

    if (m_lastFoundString == target) {
        auto selection = nextMatchForString(target, searchForward, caseSensitive, wrapSearch, [m_pdfLayerController searchSelection], NO);
        if (!selection)
            return false;
        [m_pdfLayerController setSearchSelection:selection];
        [m_pdfLayerController gotoSelection:selection];
    } else {
        [m_pdfLayerController findString:target caseSensitive:caseSensitive highlightMatches:YES];
        m_lastFoundString = target;
    }

    return foundMatch;
}

bool PDFPlugin::performDictionaryLookupAtLocation(const WebCore::FloatPoint& point)
{
    IntPoint localPoint = convertFromRootViewToPlugin(roundedIntPoint(point));
    PDFSelection* lookupSelection = [m_pdfLayerController getSelectionForWordAtPoint:convertFromPluginToPDFView(localPoint)];
    if ([[lookupSelection string] length])
        [m_pdfLayerController searchInDictionaryWithSelection:lookupSelection];
    return true;
}

void PDFPlugin::focusNextAnnotation()
{
    [m_pdfLayerController activateNextAnnotation:false];
}

void PDFPlugin::focusPreviousAnnotation()
{
    [m_pdfLayerController activateNextAnnotation:true];
}

void PDFPlugin::notifySelectionChanged(PDFSelection *)
{
    if (!m_frame || !m_frame->page())
        return;
    m_frame->page()->didChangeSelection(*m_frame->coreLocalFrame());
}

String PDFPlugin::getSelectionString() const
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

std::tuple<String, PDFSelection *, NSDictionary *> PDFPlugin::lookupTextAtLocation(const WebCore::FloatPoint& locationInViewCoordinates, WebHitTestResultData& data) const
{
    auto selection = [m_pdfLayerController currentSelection];
    if (existingSelectionContainsPoint(locationInViewCoordinates))
        return { selection.string, selection, nil };

    IntPoint pointInPDFView = convertFromPluginToPDFView(convertFromRootViewToPlugin(roundedIntPoint(locationInViewCoordinates)));
    selection = [m_pdfLayerController getSelectionForWordAtPoint:pointInPDFView];
    if (!selection)
        return { emptyString(), nil, nil };

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
        return { selection.string, selection, nil };
    }

    auto [lookupText, options] = DictionaryLookup::stringForPDFSelection(selection);
    if (!lookupText.length)
        return { emptyString(), selection, nil };

    [m_pdfLayerController setCurrentSelection:selection];
    return { lookupText, selection, options };
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
    
WebCore::AXObjectCache* PDFPlugin::axObjectCache() const
{
    if (!m_frame || !m_frame->coreLocalFrame() || !m_frame->coreLocalFrame()->document())
        return nullptr;
    return m_frame->coreLocalFrame()->document()->axObjectCache();
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

CGFloat PDFPlugin::scaleFactor() const
{
    return [m_pdfLayerController contentScaleFactor];
}

void PDFPlugin::performWebSearch(NSString *string)
{
    if (!m_frame || !m_frame->page())
        return;
    m_frame->page()->send(Messages::WebPageProxy::SearchTheWeb(string));
}

void PDFPlugin::performSpotlightSearch(NSString *string)
{
    if (!m_frame || !m_frame->page())
        return;
    m_frame->page()->send(Messages::WebPageProxy::SearchWithSpotlight(string));
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

    return rawData();
}

id PDFPlugin::accessibilityAssociatedPluginParentForElement(WebCore::Element* element) const
{
    if (!m_activeAnnotation)
        return nil;

    if (m_activeAnnotation->element() != element)
        return nil;

    return [m_activeAnnotation->annotation() accessibilityNode];
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
