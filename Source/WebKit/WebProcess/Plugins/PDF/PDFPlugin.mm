/*
 * Copyright (C) 2009-2017 Apple Inc. All rights reserved.
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

#if ENABLE(PDFKIT_PLUGIN)

#import "ArgumentCoders.h"
#import "DataReference.h"
#import "FrameInfoData.h"
#import "Logging.h"
#import "PDFAnnotationTextWidgetDetails.h"
#import "PDFContextMenu.h"
#import "PDFLayerControllerSPI.h"
#import "PDFPluginAnnotation.h"
#import "PDFPluginPasswordField.h"
#import "PluginView.h"
#import "WKAccessibilityWebPageObjectMac.h"
#import "WKPageFindMatchesClient.h"
#import "WebCoreArgumentCoders.h"
#import "WebEvent.h"
#import "WebEventConversion.h"
#import "WebFindOptions.h"
#import "WebLoaderStrategy.h"
#import "WebPage.h"
#import "WebPageProxyMessages.h"
#import "WebPasteboardProxyMessages.h"
#import "WebProcess.h"
#import <JavaScriptCore/JSContextRef.h>
#import <JavaScriptCore/JSObjectRef.h>
#import <JavaScriptCore/OpaqueJSString.h>
#import <Quartz/Quartz.h>
#import <QuartzCore/QuartzCore.h>
#import <WebCore/AXObjectCache.h>
#import <WebCore/ArchiveResource.h>
#import <WebCore/Chrome.h>
#import <WebCore/Cursor.h>
#import <WebCore/DictionaryLookup.h>
#import <WebCore/DocumentLoader.h>
#import <WebCore/EventNames.h>
#import <WebCore/FocusController.h>
#import <WebCore/FormState.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameLoader.h>
#import <WebCore/FrameView.h>
#import <WebCore/GraphicsContext.h>
#import <WebCore/HTMLElement.h>
#import <WebCore/HTMLFormElement.h>
#import <WebCore/HTMLPlugInElement.h>
#import <WebCore/LegacyNSPasteboardTypes.h>
#import <WebCore/LoaderNSURLExtras.h>
#import <WebCore/LocalDefaultSystemAppearance.h>
#import <WebCore/LocalizedStrings.h>
#import <WebCore/MouseEvent.h>
#import <WebCore/PDFDocumentImage.h>
#import <WebCore/Page.h>
#import <WebCore/Pasteboard.h>
#import <WebCore/PlatformScreen.h>
#import <WebCore/PluginData.h>
#import <WebCore/PluginDocument.h>
#import <WebCore/RenderBoxModelObject.h>
#import <WebCore/RuntimeEnabledFeatures.h>
#import <WebCore/ScrollAnimator.h>
#import <WebCore/ScrollbarTheme.h>
#import <WebCore/Settings.h>
#import <WebCore/WebAccessibilityObjectWrapperMac.h>
#import <WebCore/WheelEventTestMonitor.h>
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <pal/spi/mac/NSMenuSPI.h>
#import <wtf/HexNumber.h>
#import <wtf/Scope.h>
#import <wtf/UUID.h>
#import <wtf/WTFSemaphore.h>
#import <wtf/WorkQueue.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/TextStream.h>

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
static const char* annotationStyle =
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
"    width: 200px; "
"    margin-top: 100px; "
"} ";

// In non-continuous modes, a single scroll event with a magnitude of >= 20px
// will jump to the next or previous page, to match PDFKit behavior.
static const int defaultScrollMagnitudeThresholdForPageFlip = 20;

// <rdar://problem/61473378> - PDFKit asks for a "way too large" range when the PDF it is loading
// incrementally turns out to be non-linearized.
// We'll assume any size over 4gb is PDFKit noticing non-linearized data.
static const uint32_t nonLinearizedPDFSentinel = std::numeric_limits<uint32_t>::max();

@interface WKPDFPluginAccessibilityObject : NSObject {
    PDFLayerController *_pdfLayerController;
    __unsafe_unretained NSObject *_parent;
    WebKit::PDFPlugin* _pdfPlugin;
}

@property (assign) PDFLayerController *pdfLayerController;
@property (assign) NSObject *parent;
@property (assign) WebKit::PDFPlugin* pdfPlugin;

- (id)initWithPDFPlugin:(WebKit::PDFPlugin *)plugin;

@end

@implementation WKPDFPluginAccessibilityObject

@synthesize parent=_parent;
@synthesize pdfPlugin=_pdfPlugin;

- (id)initWithPDFPlugin:(WebKit::PDFPlugin *)plugin
{
    if (!(self = [super init]))
        return nil;

    _pdfPlugin = plugin;

    return self;
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
        return _parent;
    if ([attribute isEqualToString:NSAccessibilityTopLevelUIElementAttribute])
        return [_parent accessibilityAttributeValue:NSAccessibilityTopLevelUIElementAttribute];
    if ([attribute isEqualToString:NSAccessibilityWindowAttribute])
        return [_parent accessibilityAttributeValue:NSAccessibilityWindowAttribute];
    if ([attribute isEqualToString:NSAccessibilitySizeAttribute])
        return [NSValue valueWithSize:_pdfPlugin->boundsOnScreen().size()];
    if ([attribute isEqualToString:NSAccessibilityEnabledAttribute])
        return [_parent accessibilityAttributeValue:NSAccessibilityEnabledAttribute];
    if ([attribute isEqualToString:NSAccessibilityPositionAttribute])
        return [NSValue valueWithPoint:_pdfPlugin->boundsOnScreen().location()];

    if ([attribute isEqualToString:NSAccessibilityChildrenAttribute])
        return @[ _pdfLayerController ];
    if ([attribute isEqualToString:NSAccessibilityRoleAttribute])
        return NSAccessibilityGroupRole;

    return 0;
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (id)accessibilityAttributeValue:(NSString *)attribute forParameter:(id)parameter
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    if ([attribute isEqualToString:NSAccessibilityBoundsForRangeParameterizedAttribute]) {
        NSRect boundsInPDFViewCoordinates = [[_pdfLayerController accessibilityBoundsForRangeAttributeForParameter:parameter] rectValue];
        NSRect boundsInScreenCoordinates = _pdfPlugin->convertFromPDFViewToScreen(boundsInPDFViewCoordinates);
        return [NSValue valueWithRect:boundsInScreenCoordinates];
    }

    if ([attribute isEqualToString:NSAccessibilityLineForIndexParameterizedAttribute])
        return [_pdfLayerController accessibilityLineForIndexAttributeForParameter:parameter];
    if ([attribute isEqualToString:NSAccessibilityRangeForLineParameterizedAttribute])
        return [_pdfLayerController accessibilityRangeForLineAttributeForParameter:parameter];
    if ([attribute isEqualToString:NSAccessibilityStringForRangeParameterizedAttribute])
        return [_pdfLayerController accessibilityStringForRangeAttributeForParameter:parameter];

    return 0;
}

- (CPReadingModel *)readingModel
{
    return [_pdfLayerController readingModel];
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (NSArray *)accessibilityAttributeNames
ALLOW_DEPRECATED_IMPLEMENTATIONS_END
{
    static NSArray *attributeNames = 0;

    if (!attributeNames) {
        attributeNames = @[
            NSAccessibilityParentAttribute,
            NSAccessibilityWindowAttribute,
            NSAccessibilityTopLevelUIElementAttribute,
            NSAccessibilityRoleDescriptionAttribute,
            NSAccessibilitySizeAttribute,
            NSAccessibilityEnabledAttribute,
            NSAccessibilityPositionAttribute,
            NSAccessibilityFocusedAttribute,
            // PDFLayerController has its own accessibilityChildren.
            NSAccessibilityChildrenAttribute
            ];
        [attributeNames retain];
    }

    return attributeNames;
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
    if (WebKit::PDFPluginAnnotation* activeAnnotation = _pdfPlugin->activeAnnotation()) {
        if (WebCore::AXObjectCache* existingCache = _pdfPlugin->axObjectCache()) {
            if (WebCore::AccessibilityObject* object = existingCache->getOrCreate(activeAnnotation->element()))
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
    WebKit::PDFPluginAnnotation* activeAnnotation = _pdfPlugin->activeAnnotation();
    if (!activeAnnotation || ![activeAnnotation->annotation() isEqual:annotation])
        return nil;
    
    WebCore::AXObjectCache* cache = _pdfPlugin->axObjectCache();
    if (!cache)
        return nil;
    
    WebCore::AccessibilityObject* object = cache->getOrCreate(activeAnnotation->element());
    if (!object)
        return nil;

    return object->wrapper();
}

- (id)accessibilityHitTest:(NSPoint)point
{
    point = _pdfPlugin->convertFromRootViewToPDFView(WebCore::IntPoint(point));
    return [_pdfLayerController accessibilityHitTest:point];
}

@end


@interface WKPDFPluginScrollbarLayer : CALayer {
    WebKit::PDFPlugin* _pdfPlugin;
}

@property (assign) WebKit::PDFPlugin* pdfPlugin;

@end

@implementation WKPDFPluginScrollbarLayer

@synthesize pdfPlugin=_pdfPlugin;

- (id)initWithPDFPlugin:(WebKit::PDFPlugin *)plugin
{
    if (!(self = [super init]))
        return nil;
    
    _pdfPlugin = plugin;
    
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

@end

@implementation WKPDFLayerControllerDelegate

@synthesize pdfPlugin=_pdfPlugin;

- (id)initWithPDFPlugin:(WebKit::PDFPlugin *)plugin
{
    if (!(self = [super init]))
        return nil;
    
    _pdfPlugin = plugin;
    
    return self;
}

- (void)updateScrollPosition:(CGPoint)newPosition
{
    _pdfPlugin->notifyScrollPositionChanged(WebCore::IntPoint(newPosition));
}

- (void)writeItemsToPasteboard:(NSArray *)items withTypes:(NSArray *)types
{
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    _pdfPlugin->writeItemsToPasteboard(NSGeneralPboard, items, types);
    ALLOW_DEPRECATED_DECLARATIONS_END
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
    _pdfPlugin->openWithNativeApplication();
}

- (void)saveToPDF
{
    _pdfPlugin->saveToPDF();
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

- (void)setMouseCursor:(PDFLayerControllerCursorType)cursorType
{
    _pdfPlugin->notifyCursorChanged(cursorType);
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

static const char* postScriptMIMEType = "application/postscript";
const uint64_t pdfDocumentRequestID = 1; // PluginController supports loading multiple streams, but we only need one for PDF.

static void appendValuesInPDFNameSubtreeToVector(CGPDFDictionaryRef subtree, Vector<CGPDFObjectRef>& values)
{
    CGPDFArrayRef names;
    if (CGPDFDictionaryGetArray(subtree, "Names", &names)) {
        size_t nameCount = CGPDFArrayGetCount(names) / 2;
        for (size_t i = 0; i < nameCount; ++i) {
            CGPDFObjectRef object;
            CGPDFArrayGetObject(names, 2 * i + 1, &object);
            values.append(object);
        }
        return;
    }

    CGPDFArrayRef kids;
    if (!CGPDFDictionaryGetArray(subtree, "Kids", &kids))
        return;

    size_t kidCount = CGPDFArrayGetCount(kids);
    for (size_t i = 0; i < kidCount; ++i) {
        CGPDFDictionaryRef kid;
        if (!CGPDFArrayGetDictionary(kids, i, &kid))
            continue;
        appendValuesInPDFNameSubtreeToVector(kid, values);
    }
}

static void getAllValuesInPDFNameTree(CGPDFDictionaryRef tree, Vector<CGPDFObjectRef>& allValues)
{
    appendValuesInPDFNameSubtreeToVector(tree, allValues);
}

static void getAllScriptsInPDFDocument(CGPDFDocumentRef pdfDocument, Vector<RetainPtr<CFStringRef>>& scripts)
{
    if (!pdfDocument)
        return;

    CGPDFDictionaryRef pdfCatalog = CGPDFDocumentGetCatalog(pdfDocument);
    if (!pdfCatalog)
        return;

    // Get the dictionary of all document-level name trees.
    CGPDFDictionaryRef namesDictionary;
    if (!CGPDFDictionaryGetDictionary(pdfCatalog, "Names", &namesDictionary))
        return;

    // Get the document-level "JavaScript" name tree.
    CGPDFDictionaryRef javaScriptNameTree;
    if (!CGPDFDictionaryGetDictionary(namesDictionary, "JavaScript", &javaScriptNameTree))
        return;

    // The names are arbitrary. We are only interested in the values.
    Vector<CGPDFObjectRef> objects;
    getAllValuesInPDFNameTree(javaScriptNameTree, objects);
    size_t objectCount = objects.size();

    for (size_t i = 0; i < objectCount; ++i) {
        CGPDFDictionaryRef javaScriptAction;
        if (!CGPDFObjectGetValue(reinterpret_cast<CGPDFObjectRef>(objects[i]), kCGPDFObjectTypeDictionary, &javaScriptAction))
            continue;

        // A JavaScript action must have an action type of "JavaScript".
        const char* actionType;
        if (!CGPDFDictionaryGetName(javaScriptAction, "S", &actionType) || strcmp(actionType, "JavaScript"))
            continue;

        const UInt8* bytes = nullptr;
        CFIndex length = 0;
        CGPDFStreamRef stream;
        CGPDFStringRef string;
        RetainPtr<CFDataRef> data;
        if (CGPDFDictionaryGetStream(javaScriptAction, "JS", &stream)) {
            CGPDFDataFormat format;
            data = adoptCF(CGPDFStreamCopyData(stream, &format));
            if (!data)
                continue;
            bytes = CFDataGetBytePtr(data.get());
            length = CFDataGetLength(data.get());
        } else if (CGPDFDictionaryGetString(javaScriptAction, "JS", &string)) {
            bytes = CGPDFStringGetBytePtr(string);
            length = CGPDFStringGetLength(string);
        }
        if (!bytes || !length)
            continue;

        CFStringEncoding encoding = (length > 1 && bytes[0] == 0xFE && bytes[1] == 0xFF) ? kCFStringEncodingUnicode : kCFStringEncodingUTF8;
        RetainPtr<CFStringRef> script = adoptCF(CFStringCreateWithBytes(kCFAllocatorDefault, bytes, length, encoding, true));
        if (!script)
            continue;
        
        scripts.append(script);
    }
}

Ref<PDFPlugin> PDFPlugin::create(WebFrame& frame)
{
    return adoptRef(*new PDFPlugin(frame));
}

inline PDFPlugin::PDFPlugin(WebFrame& frame)
    : Plugin(PDFPluginType)
    , m_frame(frame)
    , m_containerLayer(adoptNS([[CALayer alloc] init]))
    , m_contentLayer(adoptNS([[CALayer alloc] init]))
    , m_scrollCornerLayer(adoptNS([[WKPDFPluginScrollbarLayer alloc] initWithPDFPlugin:this]))
    , m_pdfLayerController(adoptNS([[pdfLayerControllerClass() alloc] init]))
    , m_pdfLayerControllerDelegate(adoptNS([[WKPDFLayerControllerDelegate alloc] initWithPDFPlugin:this]))
#if HAVE(INCREMENTAL_PDF_APIS)
    , m_incrementalPDFLoadingEnabled(WebCore::RuntimeEnabledFeatures::sharedFeatures().incrementalPDFLoadingEnabled())
#endif
{
    m_pdfLayerController.get().delegate = m_pdfLayerControllerDelegate.get();
    m_pdfLayerController.get().parentLayer = m_contentLayer.get();

    if (supportsForms()) {
        Document* document = m_frame.coreFrame()->document();
        m_annotationContainer = document->createElement(divTag, false);
        m_annotationContainer->setAttributeWithoutSynchronization(idAttr, AtomString("annotationContainer", AtomString::ConstructFromLiteral));

        auto annotationStyleElement = document->createElement(styleTag, false);
        annotationStyleElement->setTextContent(annotationStyle);

        m_annotationContainer->appendChild(annotationStyleElement);
        document->bodyOrFrameset()->appendChild(*m_annotationContainer);
    }

    m_accessibilityObject = adoptNS([[WKPDFPluginAccessibilityObject alloc] initWithPDFPlugin:this]);
    [m_accessibilityObject setPdfLayerController:m_pdfLayerController.get()];
    [m_accessibilityObject setParent:m_frame.page()->accessibilityRemoteObject()];

    [m_containerLayer addSublayer:m_contentLayer.get()];
    [m_containerLayer addSublayer:m_scrollCornerLayer.get()];
#if ENABLE(WEBPROCESS_WINDOWSERVER_BLOCKING)
    if ([m_pdfLayerController respondsToSelector:@selector(setDeviceColorSpace:)]) {
        auto view = m_frame.coreFrame()->view();
        [m_pdfLayerController setDeviceColorSpace:screenColorSpace(view)];
    }
#endif

#if HAVE(INCREMENTAL_PDF_APIS)
    if (m_incrementalPDFLoadingEnabled) {
        m_pdfThread = Thread::create("PDF document thread", [protectedThis = makeRef(*this), this] () mutable {
            threadEntry(WTFMove(protectedThis));
        });
    }
#endif
}

PDFPlugin::~PDFPlugin()
{
}

#if HAVE(INCREMENTAL_PDF_APIS)
#if !LOG_DISABLED
void PDFPlugin::pdfLog(const String& message)
{
    if (!isMainThread()) {
        callOnMainThread([this, protectedThis = makeRef(*this), message = message.isolatedCopy()] {
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
    ASSERT(isMainThread());

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
    ASSERT(isMainThread());

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

    if (!isMainThread()) {
#if !LOG_DISABLED
        pdfLog("Disabling incremental PDF loading on background thread");
#endif
        callOnMainThread([this, protectedThis = makeRef(*this)] {
            receivedNonLinearizedPDFSentinel();
        });
        return;
    }

#if !LOG_DISABLED
    pdfLog(makeString("Cancelling all ", m_streamLoaderMap.size(), " range request loaders"));
#endif

    for (auto iterator = m_streamLoaderMap.begin(); iterator != m_streamLoaderMap.end(); iterator = m_streamLoaderMap.begin()) {
        m_outstandingByteRangeRequests.remove(iterator->value);
        cancelAndForgetLoader(*iterator->key);
    }

    if (!m_documentFinishedLoading || m_pdfDocument)
        return;

    m_pdfDocument = adoptNS([[pdfDocumentClass() alloc] initWithData:rawData()]);
    installPDFDocument();
    tryRunScriptsInPDFDocument();
}

static size_t dataProviderGetBytesAtPositionCallback(void* info, void* buffer, off_t position, size_t count)
{
    Ref<PDFPlugin> plugin = *((PDFPlugin*)info);

    if (isMainThread()) {
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
            ASSERT_UNUSED(count, bytesCount <= count);
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
    ASSERT(!isMainThread());

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
    ASSERT(!isMainThread());
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
        callOnMainThread([protectedPlugin = WTFMove(protectedPlugin)] { });
    });

    // Balanced by a deref inside of the dataProviderReleaseInfoCallback
    ref();

    RetainPtr<CGDataProviderRef> dataProvider = adoptCF(CGDataProviderCreateMultiRangeDirectAccess(this, kCGDataProviderIndeterminateSize, &dataProviderCallbacks));
    CGDataProviderSetProperty(dataProvider.get(), kCGDataProviderHasHighLatency, kCFBooleanTrue);
    m_backgroundThreadDocument = adoptNS([[pdfDocumentClass() alloc] initWithProvider:dataProvider.get()]);

    // [PDFDocument initWithProvider:] will return nil in cases where the PDF is non-linearized.
    // In those cases we'll just keep buffering the entire PDF on the main thread.
    if (!m_backgroundThreadDocument)
        return;

    if (!m_incrementalPDFLoadingEnabled) {
        m_backgroundThreadDocument = nil;
        return;
    }

    WTF::Semaphore firstPageSemaphore { 0 };
    auto firstPageQueue = WorkQueue::create("PDF first page work queue");

    [m_backgroundThreadDocument preloadDataOfPagesInRange:NSMakeRange(0, 1) onQueue:firstPageQueue->dispatchQueue() completion:[&firstPageSemaphore, this] (NSIndexSet *) mutable {
        if (m_incrementalPDFLoadingEnabled) {
            callOnMainThread([this] {
                adoptBackgroundThreadDocument();
            });
        } else
            m_backgroundThreadDocument = nil;

        firstPageSemaphore.signal();
    }];

    firstPageSemaphore.wait();

#if !LOG_DISABLED
    pdfLog("Finished preloading first page");
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
    ASSERT(isMainThread());
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
    ASSERT(isMainThread()); 
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

    auto* coreFrame = m_frame.coreFrame();
    if (!coreFrame)
        return;

    auto* documentLoader = coreFrame->loader().documentLoader();
    if (!documentLoader)
        return;

    auto resourceRequest = documentLoader->request();
    resourceRequest.setHTTPHeaderField(HTTPHeaderName::Range, makeString("bytes="_s, position, "-"_s, position + count - 1));
    resourceRequest.setCachePolicy(ResourceRequestCachePolicy::DoNotUseAnyCache);

#if !LOG_DISABLED
    pdfLog(makeString("Scheduling a stream loader for request ", identifier, " (", count, " bytes at ", position, ")"));
#endif

    WebProcess::singleton().webLoaderStrategy().schedulePluginStreamLoad(*coreFrame, *this, WTFMove(resourceRequest), [this, protectedThis = makeRef(*this), identifier] (RefPtr<WebCore::NetscapePlugInStreamLoader>&& loader) {
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
    ASSERT(!m_pdfDocument);
    ASSERT(m_backgroundThreadDocument);
    ASSERT(isMainThread());

#if !LOG_DISABLED
    pdfLog("Adopting PDFDocument from background thread");
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

    m_completionHandler(m_accumulatedData.data(), m_accumulatedData.size());

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

void PDFPlugin::willSendRequest(NetscapePlugInStreamLoader* loader, ResourceRequest&&, const ResourceResponse&, CompletionHandler<void(ResourceRequest&&)>&&)
{
    // Redirections for range requests are unexpected.
    cancelAndForgetLoader(*loader);
}

void PDFPlugin::didReceiveResponse(NetscapePlugInStreamLoader* loader, const ResourceResponse& response)
{
    auto* request = byteRangeRequestForLoader(*loader);
    if (!request) {
        cancelAndForgetLoader(*loader);
        return;
    }

    ASSERT(request->streamLoader() == loader);

    // Range success! We'll expect to receive the data in future didReceiveData callbacks.
    if (response.httpStatusCode() == 206)
        return;

    // If the response wasn't a successful range response, we don't need this stream loader anymore.
    // This can happen, for example, if the server doesn't support range requests.
    // We'll still resolve the ByteRangeRequest later once enough of the full resource has loaded.
    cancelAndForgetLoader(*loader);

    // The server might support range requests and explicitly told us this range was not satisfiable.
    // In this case, we can reject the ByteRangeRequest right away.
    if (response.httpStatusCode() == 416 && request) {
        request->completeWithAccumulatedData(*this);
        m_outstandingByteRangeRequests.remove(request->identifier());
    }
}

void PDFPlugin::didReceiveData(NetscapePlugInStreamLoader* loader, const char* data, int count)
{
    auto* request = byteRangeRequestForLoader(*loader);
    if (!request)
        return;

    request->addData(reinterpret_cast<const uint8_t*>(data), count);
}

void PDFPlugin::didFail(NetscapePlugInStreamLoader* loader, const ResourceError&)
{
    if (m_documentFinishedLoading) {
        auto identifier = m_streamLoaderMap.get(loader);
        if (identifier)
            m_outstandingByteRangeRequests.remove(identifier);
    }

    forgetLoader(*loader);
}

void PDFPlugin::didFinishLoading(NetscapePlugInStreamLoader* loader)
{
    auto* request = byteRangeRequestForLoader(*loader);
    if (!request)
        return;

    request->completeWithAccumulatedData(*this);
    m_outstandingByteRangeRequests.remove(request->identifier());
}

PDFPlugin::ByteRangeRequest* PDFPlugin::byteRangeRequestForLoader(NetscapePlugInStreamLoader& loader)
{
    uint64_t identifier = m_streamLoaderMap.get(&loader);
    if (!identifier)
        return nullptr;

    auto request = m_outstandingByteRangeRequests.find(identifier);
    if (request == m_outstandingByteRangeRequests.end())
        return nullptr;

    return &(request->value);
}

void PDFPlugin::forgetLoader(NetscapePlugInStreamLoader& loader)
{
    uint64_t identifier = m_streamLoaderMap.get(&loader);
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
    auto protectedLoader = makeRef(loader);
    forgetLoader(loader);
    loader.cancel(loader.cancelledError());
}
#endif // HAVE(INCREMENTAL_PDF_APIS)

PluginInfo PDFPlugin::pluginInfo()
{
    PluginInfo info;
    info.name = builtInPDFPluginName();
    info.isApplicationPlugin = true;
    info.clientLoadPolicy = PluginLoadClientPolicy::Undefined;
    info.bundleIdentifier = "com.apple.webkit.builtinpdfplugin"_s;

    MimeClassInfo pdfMimeClassInfo;
    pdfMimeClassInfo.type = "application/pdf";
    pdfMimeClassInfo.desc = pdfDocumentTypeDescription();
    pdfMimeClassInfo.extensions.append("pdf");
    info.mimes.append(pdfMimeClassInfo);

    MimeClassInfo textPDFMimeClassInfo;
    textPDFMimeClassInfo.type = "text/pdf";
    textPDFMimeClassInfo.desc = pdfDocumentTypeDescription();
    textPDFMimeClassInfo.extensions.append("pdf");
    info.mimes.append(textPDFMimeClassInfo);

    MimeClassInfo postScriptMimeClassInfo;
    postScriptMimeClassInfo.type = postScriptMIMEType;
    postScriptMimeClassInfo.desc = postScriptDocumentTypeDescription();
    postScriptMimeClassInfo.extensions.append("ps");
    info.mimes.append(postScriptMimeClassInfo);
    
    return info;
}

void PDFPlugin::updateScrollbars()
{
    bool hadScrollbars = m_horizontalScrollbar || m_verticalScrollbar;

    if (m_horizontalScrollbar) {
        if (m_size.width() >= m_pdfDocumentSize.width())
            destroyScrollbar(HorizontalScrollbar);
    } else if (m_size.width() < m_pdfDocumentSize.width())
        m_horizontalScrollbar = createScrollbar(HorizontalScrollbar);

    if (m_verticalScrollbar) {
        if (m_size.height() >= m_pdfDocumentSize.height())
            destroyScrollbar(VerticalScrollbar);
    } else if (m_size.height() < m_pdfDocumentSize.height())
        m_verticalScrollbar = createScrollbar(VerticalScrollbar);

    IntSize scrollbarSpace = scrollbarIntrusion();

    if (m_horizontalScrollbar) {
        m_horizontalScrollbar->setSteps(Scrollbar::pixelsPerLineStep(), m_firstPageHeight);
        m_horizontalScrollbar->setProportion(m_size.width() - scrollbarSpace.width(), m_pdfDocumentSize.width());
        IntRect scrollbarRect(pluginView()->x(), pluginView()->y() + m_size.height() - m_horizontalScrollbar->height(), m_size.width(), m_horizontalScrollbar->height());
        if (m_verticalScrollbar)
            scrollbarRect.contract(m_verticalScrollbar->width(), 0);
        m_horizontalScrollbar->setFrameRect(scrollbarRect);
    }

    if (m_verticalScrollbar) {
        m_verticalScrollbar->setSteps(Scrollbar::pixelsPerLineStep(), m_firstPageHeight);
        m_verticalScrollbar->setProportion(m_size.height() - scrollbarSpace.height(), m_pdfDocumentSize.height());
        IntRect scrollbarRect(IntRect(pluginView()->x() + m_size.width() - m_verticalScrollbar->width(), pluginView()->y(), m_verticalScrollbar->width(), m_size.height()));
        if (m_horizontalScrollbar)
            scrollbarRect.contract(0, m_horizontalScrollbar->height());
        m_verticalScrollbar->setFrameRect(scrollbarRect);
    }

    FrameView* frameView = m_frame.coreFrame()->view();
    if (!frameView)
        return;

    bool hasScrollbars = m_horizontalScrollbar || m_verticalScrollbar;
    if (hadScrollbars != hasScrollbars) {
        if (hasScrollbars)
            frameView->addScrollableArea(this);
        else
            frameView->removeScrollableArea(this);

        frameView->setNeedsLayoutAfterViewConfigurationChange();
    }
    
    if (m_verticalScrollbarLayer) {
        m_verticalScrollbarLayer.get().frame = verticalScrollbar()->frameRect();
        [m_verticalScrollbarLayer setNeedsDisplay];
    }
    
    if (m_horizontalScrollbarLayer) {
        m_horizontalScrollbarLayer.get().frame = horizontalScrollbar()->frameRect();
        [m_horizontalScrollbarLayer setNeedsDisplay];
    }
    
    if (m_scrollCornerLayer) {
        m_scrollCornerLayer.get().frame = scrollCornerRect();
        [m_scrollCornerLayer setNeedsDisplay];
    }
}

PluginView* PDFPlugin::pluginView()
{
    return static_cast<PluginView*>(controller());
}

const PluginView* PDFPlugin::pluginView() const
{
    return static_cast<const PluginView*>(controller());
}

Ref<Scrollbar> PDFPlugin::createScrollbar(ScrollbarOrientation orientation)
{
    auto widget = Scrollbar::createNativeScrollbar(*this, orientation, RegularScrollbar);
    if (orientation == HorizontalScrollbar) {
        m_horizontalScrollbarLayer = adoptNS([[WKPDFPluginScrollbarLayer alloc] initWithPDFPlugin:this]);
        [m_containerLayer addSublayer:m_horizontalScrollbarLayer.get()];
    } else {
        m_verticalScrollbarLayer = adoptNS([[WKPDFPluginScrollbarLayer alloc] initWithPDFPlugin:this]);
        [m_containerLayer addSublayer:m_verticalScrollbarLayer.get()];
    }
    didAddScrollbar(widget.ptr(), orientation);
    if (auto* frame = m_frame.coreFrame()) {
        if (Page* page = frame->page()) {
            if (page->isMonitoringWheelEvents())
                scrollAnimator().setWheelEventTestMonitor(page->wheelEventTestMonitor());
        }
    }
    pluginView()->frame()->view()->addChild(widget);
    return widget;
}

void PDFPlugin::destroyScrollbar(ScrollbarOrientation orientation)
{
    RefPtr<Scrollbar>& scrollbar = orientation == HorizontalScrollbar ? m_horizontalScrollbar : m_verticalScrollbar;
    if (!scrollbar)
        return;

    willRemoveScrollbar(scrollbar.get(), orientation);
    scrollbar->removeFromParent();
    scrollbar = nullptr;

    if (orientation == HorizontalScrollbar) {
        [m_horizontalScrollbarLayer removeFromSuperlayer];
        m_horizontalScrollbarLayer = 0;
    } else {
        [m_verticalScrollbarLayer removeFromSuperlayer];
        m_verticalScrollbarLayer = 0;
    }
}

IntRect PDFPlugin::convertFromScrollbarToContainingView(const Scrollbar& scrollbar, const IntRect& scrollbarRect) const
{
    IntRect rect = scrollbarRect;
    rect.move(scrollbar.location() - pluginView()->location());

    return pluginView()->frame()->view()->convertFromRendererToContainingView(pluginView()->pluginElement()->renderer(), rect);
}

IntRect PDFPlugin::convertFromContainingViewToScrollbar(const Scrollbar& scrollbar, const IntRect& parentRect) const
{
    IntRect rect = pluginView()->frame()->view()->convertFromContainingViewToRenderer(pluginView()->pluginElement()->renderer(), parentRect);
    rect.move(pluginView()->location() - scrollbar.location());

    return rect;
}

IntPoint PDFPlugin::convertFromScrollbarToContainingView(const Scrollbar& scrollbar, const IntPoint& scrollbarPoint) const
{
    IntPoint point = scrollbarPoint;
    point.move(scrollbar.location() - pluginView()->location());

    return pluginView()->frame()->view()->convertFromRendererToContainingView(pluginView()->pluginElement()->renderer(), point);
}

IntPoint PDFPlugin::convertFromContainingViewToScrollbar(const Scrollbar& scrollbar, const IntPoint& parentPoint) const
{
    IntPoint point = pluginView()->frame()->view()->convertFromContainingViewToRenderer(pluginView()->pluginElement()->renderer(), parentPoint);
    point.move(pluginView()->location() - scrollbar.location());
    
    return point;
}

String PDFPlugin::debugDescription() const
{
    return makeString("PDFPlugin 0x", hex(reinterpret_cast<uintptr_t>(this), Lowercase));
}

bool PDFPlugin::handleScroll(ScrollDirection direction, ScrollGranularity granularity)
{
    return scroll(direction, granularity);
}

IntRect PDFPlugin::scrollCornerRect() const
{
    if (!m_horizontalScrollbar || !m_verticalScrollbar)
        return IntRect();
    if (m_horizontalScrollbar->isOverlayScrollbar()) {
        ASSERT(m_verticalScrollbar->isOverlayScrollbar());
        return IntRect();
    }
    return IntRect(pluginView()->width() - m_verticalScrollbar->width(), pluginView()->height() - m_horizontalScrollbar->height(), m_verticalScrollbar->width(), m_horizontalScrollbar->height());
}

ScrollableArea* PDFPlugin::enclosingScrollableArea() const
{
    // FIXME: Walk up the frame tree and look for a scrollable parent frame or RenderLayer.
    return nullptr;
}

IntRect PDFPlugin::scrollableAreaBoundingBox(bool*) const
{
    return pluginView()->frameRect();
}

bool PDFPlugin::isActive() const
{
    if (Frame* coreFrame = m_frame.coreFrame()) {
        if (Page* page = coreFrame->page())
            return page->focusController().isActive();
    }

    return false;
}

bool PDFPlugin::forceUpdateScrollbarsOnMainThreadForPerformanceTesting() const
{
    if (Frame* coreFrame = m_frame.coreFrame()) {
        if (Page* page = coreFrame->page())
            return page->settings().forceUpdateScrollbarsOnMainThreadForPerformanceTesting();
    }

    return false;
}

ScrollPosition PDFPlugin::scrollPosition() const
{
    return IntPoint(m_scrollOffset.width(), m_scrollOffset.height());
}

ScrollPosition PDFPlugin::minimumScrollPosition() const
{
    return IntPoint();
}

ScrollPosition PDFPlugin::maximumScrollPosition() const
{
    IntSize scrollbarSpace = scrollbarIntrusion();

    IntPoint maximumOffset(m_pdfDocumentSize.width() - m_size.width() + scrollbarSpace.width(), m_pdfDocumentSize.height() - m_size.height() + scrollbarSpace.height());
    maximumOffset.clampNegativeToZero();
    return maximumOffset;
}

void PDFPlugin::scrollbarStyleChanged(ScrollbarStyle style, bool forceUpdate)
{
    if (!forceUpdate)
        return;

    // If the PDF was scrolled all the way to bottom right and scrollbars change to overlay style, we don't want to display white rectangles where scrollbars were.
    IntPoint newScrollOffset = IntPoint(m_scrollOffset).shrunkTo(maximumScrollPosition());
    setScrollOffset(newScrollOffset);
    
    ScrollableArea::scrollbarStyleChanged(style, forceUpdate);
    // As size of the content area changes, scrollbars may need to appear or to disappear.
    updateScrollbars();
}

void PDFPlugin::addArchiveResource()
{
    // FIXME: It's a hack to force add a resource to DocumentLoader. PDF documents should just be fetched as CachedResources.

    // Add just enough data for context menu handling and web archives to work.
    NSDictionary* headers = @{ @"Content-Disposition": (NSString *)m_suggestedFilename, @"Content-Type" : @"application/pdf" };
    RetainPtr<NSURLResponse> response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:m_sourceURL statusCode:200 HTTPVersion:(NSString*)kCFHTTPVersion1_1 headerFields:headers]);
    ResourceResponse synthesizedResponse(response.get());

    auto resource = ArchiveResource::create(SharedBuffer::create(m_data.get()), m_sourceURL, "application/pdf", String(), String(), synthesizedResponse);
    pluginView()->frame()->document()->loader()->addArchiveResource(resource.releaseNonNull());
}

static void jsPDFDocInitialize(JSContextRef ctx, JSObjectRef object)
{
    PDFPlugin* pdfView = static_cast<PDFPlugin*>(JSObjectGetPrivate(object));
    pdfView->ref();
}

static void jsPDFDocFinalize(JSObjectRef object)
{
    PDFPlugin* pdfView = static_cast<PDFPlugin*>(JSObjectGetPrivate(object));
    pdfView->deref();
}

JSValueRef PDFPlugin::jsPDFDocPrint(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    PDFPlugin* pdfPlugin = static_cast<PDFPlugin*>(JSObjectGetPrivate(thisObject));

    Frame* coreFrame = pdfPlugin->m_frame.coreFrame();
    if (!coreFrame)
        return JSValueMakeUndefined(ctx);

    Page* page = coreFrame->page();
    if (!page)
        return JSValueMakeUndefined(ctx);

    page->chrome().print(*coreFrame);

    return JSValueMakeUndefined(ctx);
}

JSObjectRef PDFPlugin::makeJSPDFDoc(JSContextRef ctx)
{
    static JSStaticFunction jsPDFDocStaticFunctions[] = {
        { "print", jsPDFDocPrint, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { 0, 0, 0 },
    };

    static JSClassDefinition jsPDFDocClassDefinition = {
        0,
        kJSClassAttributeNone,
        "Doc",
        0,
        0,
        jsPDFDocStaticFunctions,
        jsPDFDocInitialize, jsPDFDocFinalize, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

    static JSClassRef jsPDFDocClass = JSClassCreate(&jsPDFDocClassDefinition);

    return JSObjectMake(ctx, jsPDFDocClass, this);
}

void PDFPlugin::convertPostScriptDataIfNeeded()
{
    if (!m_isPostScript)
        return;

    m_data = PDFDocumentImage::convertPostScriptDataToPDF(WTFMove(m_data));
}

void PDFPlugin::documentDataDidFinishLoading()
{
    addArchiveResource();

    m_documentFinishedLoading = true;

#if HAVE(INCREMENTAL_PDF_APIS)
#if !LOG_DISABLED
    pdfLog(makeString("PDF document finished loading with a total of ", m_streamedBytes, " bytes"));
#endif
    if (m_incrementalPDFLoadingEnabled) {
        // At this point we know all data for the document, and therefore we know how to answer any outstanding range requests.
        unconditionalCompleteOutstandingRangeRequests();
        maybeClearHighLatencyDataProviderFlag();
    } else
#endif
    {
        m_pdfDocument = adoptNS([[pdfDocumentClass() alloc] initWithData:rawData()]);
        installPDFDocument();
    }

    tryRunScriptsInPDFDocument();
}

void PDFPlugin::installPDFDocument()
{
    ASSERT(m_pdfDocument);
    ASSERT(isMainThread());
    LOG(IncrementalPDF, "Installing PDF document");

#if HAVE(INCREMENTAL_PDF_APIS)
    maybeClearHighLatencyDataProviderFlag();
#endif

    updatePageAndDeviceScaleFactors();

    [m_pdfLayerController setFrameSize:size()];
    m_pdfLayerController.get().document = m_pdfDocument.get();

    if (handlesPageScaleFactor())
        pluginView()->setPageScaleFactor([m_pdfLayerController contentScaleFactor], IntPoint());

    notifyScrollPositionChanged(IntPoint([m_pdfLayerController scrollPosition]));

    calculateSizes();
    updateScrollbars();

    tryRunScriptsInPDFDocument();

    if ([m_pdfDocument isLocked])
        createPasswordEntryForm();

    if ([m_pdfLayerController respondsToSelector:@selector(setURLFragment:)])
        [m_pdfLayerController setURLFragment:m_frame.url().fragmentIdentifier().createNSString().get()];
}

void PDFPlugin::setSuggestedFilename(const String& suggestedFilename)
{
    m_suggestedFilename = suggestedFilename;

    if (m_suggestedFilename.isEmpty())
        m_suggestedFilename = suggestedFilenameWithMIMEType(nil, "application/pdf");

    if (!m_suggestedFilename.endsWithIgnoringASCIICase(".pdf"))
        m_suggestedFilename.append(".pdf");
}
    
void PDFPlugin::streamDidReceiveResponse(uint64_t streamID, const URL&, uint32_t, uint32_t, const String& mimeType, const String&, const String& suggestedFilename)
{
    ASSERT_UNUSED(streamID, streamID == pdfDocumentRequestID);

    setSuggestedFilename(suggestedFilename);

    if (equalIgnoringASCIICase(mimeType, postScriptMIMEType))
        m_isPostScript = true;
}

void PDFPlugin::streamDidReceiveData(uint64_t streamID, const char* bytes, int length)
{
    ASSERT_UNUSED(streamID, streamID == pdfDocumentRequestID);

    if (!m_data)
        m_data = adoptCF(CFDataCreateMutable(0, 0));

    CFDataAppendBytes(m_data.get(), reinterpret_cast<const UInt8*>(bytes), length);
    m_streamedBytes += length;
}

void PDFPlugin::streamDidFinishLoading(uint64_t streamID)
{
    ASSERT_UNUSED(streamID, streamID == pdfDocumentRequestID);

    convertPostScriptDataIfNeeded();
    documentDataDidFinishLoading();
}

void PDFPlugin::streamDidFail(uint64_t streamID, bool wasCancelled)
{
    ASSERT_UNUSED(streamID, streamID == pdfDocumentRequestID);

    m_data.clear();
}

void PDFPlugin::manualStreamDidReceiveResponse(const URL& responseURL, uint32_t streamLength,  uint32_t lastModifiedTime, const String& mimeType, const String& headers, const String& suggestedFilename)
{
    setSuggestedFilename(suggestedFilename);

    if (equalIgnoringASCIICase(mimeType, postScriptMIMEType))
        m_isPostScript = true;
}

void PDFPlugin::ensureDataBufferLength(uint64_t targetLength)
{
    if (!m_data)
        m_data = adoptCF(CFDataCreateMutable(0, 0));

    auto currentLength = CFDataGetLength(m_data.get());
    ASSERT(currentLength >= 0);
    if (targetLength > (uint64_t)currentLength)
        CFDataIncreaseLength(m_data.get(), targetLength - currentLength);
}

void PDFPlugin::manualStreamDidReceiveData(const char* bytes, int length)
{
    if (!m_data)
        m_data = adoptCF(CFDataCreateMutable(0, 0));

    ensureDataBufferLength(m_streamedBytes + length);
    memcpy(CFDataGetMutableBytePtr(m_data.get()) + m_streamedBytes, bytes, length);
    m_streamedBytes += length;

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

void PDFPlugin::manualStreamDidFinishLoading()
{
    convertPostScriptDataIfNeeded();
    documentDataDidFinishLoading();
}

void PDFPlugin::manualStreamDidFail(bool)
{
    m_data = nullptr;
#if HAVE(INCREMENTAL_PDF_APIS)
    if (m_incrementalPDFLoadingEnabled)
        unconditionalCompleteOutstandingRangeRequests();
#endif
}

void PDFPlugin::tryRunScriptsInPDFDocument()
{
    ASSERT(isMainThread());

    if (!m_pdfDocument || !m_documentFinishedLoading)
        return;

    auto completionHandler = [this, protectedThis = makeRef(*this)] (Vector<RetainPtr<CFStringRef>>&& scripts) mutable {
        if (scripts.isEmpty())
            return;

        JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);
        JSObjectRef jsPDFDoc = makeJSPDFDoc(ctx);
        for (auto& script : scripts)
            JSEvaluateScript(ctx, OpaqueJSString::tryCreate(script.get()).get(), jsPDFDoc, nullptr, 1, nullptr);
        JSGlobalContextRelease(ctx);
    };

#if HAVE(INCREMENTAL_PDF_APIS)
    auto scriptUtilityQueue = WorkQueue::create("PDF script utility");
    auto& rawQueue = scriptUtilityQueue.get();
    RetainPtr<CGPDFDocumentRef> document = [m_pdfDocument documentRef];
    rawQueue.dispatch([scriptUtilityQueue = WTFMove(scriptUtilityQueue), completionHandler = WTFMove(completionHandler), document = WTFMove(document)] () mutable {
        ASSERT(!isMainThread());

        Vector<RetainPtr<CFStringRef>> scripts;
        getAllScriptsInPDFDocument(document.get(), scripts);

        callOnMainThread([completionHandler = WTFMove(completionHandler), scripts = WTFMove(scripts)] () mutable {
            completionHandler(WTFMove(scripts));
        });
    });
#else
    Vector<RetainPtr<CFStringRef>> scripts;
    getAllScriptsInPDFDocument([m_pdfDocument documentRef], scripts);
    completionHandler(WTFMove(scripts));
#endif
}

void PDFPlugin::createPasswordEntryForm()
{
    if (!supportsForms())
        return;

    m_passwordField = PDFPluginPasswordField::create(m_pdfLayerController.get(), this);
    m_passwordField->attach(m_annotationContainer.get());
}

void PDFPlugin::attemptToUnlockPDF(const String& password)
{
    [m_pdfLayerController attemptToUnlockWithPassword:password];

    if (![m_pdfDocument isLocked]) {
        m_passwordField = nullptr;

        calculateSizes();
        updateScrollbars();
    }
}

void PDFPlugin::updatePageAndDeviceScaleFactors()
{
    if (!controller())
        return;

    double newScaleFactor = controller()->contentsScaleFactor();
    if (!handlesPageScaleFactor()) {
        if (auto* page = m_frame.page())
            newScaleFactor *= page->pageScaleFactor();
    }

    if (newScaleFactor != [m_pdfLayerController deviceScaleFactor])
        [m_pdfLayerController setDeviceScaleFactor:newScaleFactor];
}

void PDFPlugin::contentsScaleFactorChanged(float)
{
    updatePageAndDeviceScaleFactors();
}

void PDFPlugin::calculateSizes()
{
    if ([m_pdfDocument isLocked]) {
        m_firstPageHeight = 0;
        setPDFDocumentSize(IntSize(0, 0));
        return;
    }

    m_firstPageHeight = [m_pdfDocument pageCount] ? static_cast<unsigned>(CGCeiling([[m_pdfDocument pageAtIndex:0] boundsForBox:kPDFDisplayBoxCropBox].size.height)) : 0;
    setPDFDocumentSize(IntSize([m_pdfLayerController contentSizeRespectingZoom]));
}

bool PDFPlugin::initialize(const Parameters& parameters)
{
    m_sourceURL = parameters.url;
    if (!parameters.shouldUseManualLoader && !parameters.url.isEmpty())
        controller()->loadURL(pdfDocumentRequestID, "GET", parameters.url.string(), String(), HTTPHeaderMap(), Vector<uint8_t>(), false);

    controller()->didInitializePlugin();
    return true;
}

void PDFPlugin::willDetachRenderer()
{
    if (auto* frameView = m_frame.coreFrame()->view())
        frameView->removeScrollableArea(this);
}

void PDFPlugin::destroy()
{
    m_documentFinishedLoading = true;

#if HAVE(INCREMENTAL_PDF_APIS)
    // By clearing out the resource data and handling all outstanding range requests,
    // we can force the PDFThread to complete quickly
    if (m_pdfThread) {
        m_data = nullptr;
        unconditionalCompleteOutstandingRangeRequests();
        m_pdfThread->waitForCompletion();
    }
#endif

    m_pdfLayerController.get().delegate = 0;

    if (auto* frameView = m_frame.coreFrame()->view())
        frameView->removeScrollableArea(this);

    m_activeAnnotation = nullptr;
    m_annotationContainer = nullptr;

    destroyScrollbar(HorizontalScrollbar);
    destroyScrollbar(VerticalScrollbar);
    
    [m_scrollCornerLayer removeFromSuperlayer];
    [m_contentLayer removeFromSuperlayer];
}

void PDFPlugin::updateControlTints(GraphicsContext& graphicsContext)
{
    ASSERT(graphicsContext.invalidatingControlTints());

    if (m_horizontalScrollbar)
        m_horizontalScrollbar->invalidate();
    if (m_verticalScrollbar)
        m_verticalScrollbar->invalidate();
    invalidateScrollCorner(scrollCornerRect());
}

void PDFPlugin::paintControlForLayerInContext(CALayer *layer, CGContextRef context)
{
#if PLATFORM(MAC)
    auto* page = m_frame.coreFrame()->page();
    LocalDefaultSystemAppearance localAppearance(page->useDarkAppearance());
#endif

    GraphicsContext graphicsContext(context);
    GraphicsContextStateSaver stateSaver(graphicsContext);

    graphicsContext.setIsCALayerContext(true);

    if (layer == m_scrollCornerLayer) {
        IntRect scrollCornerRect = this->scrollCornerRect();
        graphicsContext.translate(-scrollCornerRect.x(), -scrollCornerRect.y());
        ScrollbarTheme::theme().paintScrollCorner(graphicsContext, scrollCornerRect);
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

    float contentsScaleFactor = controller()->contentsScaleFactor();
    IntSize backingStoreSize = size();
    backingStoreSize.scale(contentsScaleFactor);

    auto bitmap = ShareableBitmap::createShareable(backingStoreSize, { });
    auto context = bitmap->createGraphicsContext();
    if (!context)
        return nullptr;

    context->scale(FloatSize(contentsScaleFactor, -contentsScaleFactor));
    context->translate(-m_scrollOffset.width(), -m_pdfDocumentSize.height() + m_scrollOffset.height());

    [m_pdfLayerController snapshotInContext:context->platformContext()];

    return bitmap;
}

PlatformLayer* PDFPlugin::pluginLayer()
{
    return m_containerLayer.get();
}

IntPoint PDFPlugin::convertFromPluginToPDFView(const IntPoint& point) const
{
    return IntPoint(point.x(), size().height() - point.y());
}

IntPoint PDFPlugin::convertFromRootViewToPlugin(const IntPoint& point) const
{
    return m_rootViewToPluginTransform.mapPoint(point);
}

IntPoint PDFPlugin::convertFromPDFViewToRootView(const IntPoint& point) const
{
    IntPoint pointInPluginCoordinates(point.x(), size().height() - point.y());
    return m_rootViewToPluginTransform.inverse().valueOr(AffineTransform()).mapPoint(pointInPluginCoordinates);
}
    
IntPoint PDFPlugin::convertFromRootViewToPDFView(const IntPoint& point) const
{
    IntPoint pointInPluginCoordinates = m_rootViewToPluginTransform.mapPoint(point);
    return IntPoint(pointInPluginCoordinates.x(), size().height() - pointInPluginCoordinates.y());
}

FloatRect PDFPlugin::convertFromPDFViewToScreen(const FloatRect& rect) const
{
    FrameView* frameView = m_frame.coreFrame()->view();

    if (!frameView)
        return FloatRect();

    FloatRect updatedRect = rect;
    updatedRect.setLocation(convertFromPDFViewToRootView(IntPoint(updatedRect.location())));
    return m_frame.coreFrame()->page()->chrome().rootViewToScreen(enclosingIntRect(updatedRect));
}

IntRect PDFPlugin::boundsOnScreen() const
{
    FrameView* frameView = m_frame.coreFrame()->view();

    if (!frameView)
        return IntRect();

    FloatRect bounds = FloatRect(FloatPoint(), size());
    FloatRect rectInRootViewCoordinates = m_rootViewToPluginTransform.inverse().valueOr(AffineTransform()).mapRect(bounds);
    return frameView->contentsToScreen(enclosingIntRect(rectInRootViewCoordinates));
}

void PDFPlugin::geometryDidChange(const IntSize& pluginSize, const IntRect&, const AffineTransform& pluginToRootViewTransform)
{
    if (size() == pluginSize && pluginView()->pageScaleFactor() == [m_pdfLayerController contentScaleFactor])
        return;

    m_size = pluginSize;
    m_rootViewToPluginTransform = pluginToRootViewTransform.inverse().valueOr(AffineTransform());
    [m_pdfLayerController setFrameSize:pluginSize];

    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    CATransform3D transform = CATransform3DMakeScale(1, -1, 1);
    transform = CATransform3DTranslate(transform, 0, -pluginSize.height(), 0);
    
    if (handlesPageScaleFactor()) {
        CGFloat magnification = pluginView()->pageScaleFactor() - [m_pdfLayerController contentScaleFactor];

        // FIXME: Instead of m_lastMousePositionInPluginCoordinates, we should use the zoom origin from PluginView::setPageScaleFactor.
        if (magnification)
            [m_pdfLayerController magnifyWithMagnification:magnification atPoint:convertFromPluginToPDFView(m_lastMousePositionInPluginCoordinates) immediately:NO];
    } else {
        // If we don't handle page scale ourselves, we need to respect our parent page's
        // scale, which may have changed.
        updatePageAndDeviceScaleFactors();
    } 

    calculateSizes();
    updateScrollbars();

    if (m_activeAnnotation)
        m_activeAnnotation->updateGeometry();

    [m_contentLayer setSublayerTransform:transform];
    [CATransaction commit];
}

void PDFPlugin::frameDidFinishLoading(uint64_t)
{
    ASSERT_NOT_REACHED();
}

void PDFPlugin::frameDidFail(uint64_t, bool)
{
    ASSERT_NOT_REACHED();
}

void PDFPlugin::didEvaluateJavaScript(uint64_t, const WTF::String&)
{
    ASSERT_NOT_REACHED();
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
    case WebEvent::KeyDown:
        eventType = NSEventTypeKeyDown;
        return true;
    case WebEvent::KeyUp:
        eventType = NSEventTypeKeyUp;
        return true;
    case WebEvent::MouseDown:
        switch (static_cast<const WebMouseEvent&>(event).button()) {
        case WebMouseEvent::LeftButton:
            eventType = NSEventTypeLeftMouseDown;
            return true;
        case WebMouseEvent::RightButton:
            eventType = NSEventTypeRightMouseDown;
            return true;
        default:
            return false;
        }
    case WebEvent::MouseUp:
        switch (static_cast<const WebMouseEvent&>(event).button()) {
        case WebMouseEvent::LeftButton:
            eventType = NSEventTypeLeftMouseUp;
            return true;
        case WebMouseEvent::RightButton:
            eventType = NSEventTypeRightMouseUp;
            return true;
        default:
            return false;
        }
    case WebEvent::MouseMove:
        switch (static_cast<const WebMouseEvent&>(event).button()) {
        case WebMouseEvent::LeftButton:
            eventType = NSEventTypeLeftMouseDragged;
            return true;
        case WebMouseEvent::RightButton:
            eventType = NSEventTypeRightMouseDragged;
            return true;
        case WebMouseEvent::NoButton:
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

    if ([m_pdfDocument isLocked])
        return false;

    // Right-clicks and Control-clicks always call handleContextMenuEvent as well.
    if (event.button() == WebMouseEvent::RightButton || (event.button() == WebMouseEvent::LeftButton && event.controlKey()))
        return true;

    NSEvent *nsEvent = nsEventForWebMouseEvent(event);

    switch (event.type()) {
    case WebEvent::MouseMove:
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
        case WebMouseEvent::LeftButton:
            [m_pdfLayerController mouseDragged:nsEvent];
            return true;
        case WebMouseEvent::RightButton:
        case WebMouseEvent::MiddleButton:
            return false;
        case WebMouseEvent::NoButton:
            [m_pdfLayerController mouseMoved:nsEvent];
            return true;
        }
        break;
    case WebEvent::MouseDown:
        switch (event.button()) {
        case WebMouseEvent::LeftButton:
            if (targetScrollbar)
                return targetScrollbar->mouseDown(platformEvent);

            [m_pdfLayerController mouseDown:nsEvent];
            return true;
        case WebMouseEvent::RightButton:
            [m_pdfLayerController rightMouseDown:nsEvent];
            return true;
        case WebMouseEvent::MiddleButton:
        case WebMouseEvent::NoButton:
            return false;
        }
        break;
    case WebEvent::MouseUp:
        switch (event.button()) {
        case WebMouseEvent::LeftButton:
            if (targetScrollbar)
                return targetScrollbar->mouseUp(platformEvent);

            [m_pdfLayerController mouseUp:nsEvent];
            return true;
        case WebMouseEvent::RightButton:
        case WebMouseEvent::MiddleButton:
        case WebMouseEvent::NoButton:
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
    FrameView* frameView = m_frame.coreFrame()->view();
    IntPoint contentsPoint = frameView->contentsToRootView(point);
    WebMouseEvent event(WebEvent::MouseDown, WebMouseEvent::RightButton, 0, contentsPoint, contentsPoint, 0, 0, 0, 1, OptionSet<WebEvent::Modifier> { }, WallTime::now(), WebCore::ForceAtClick);
    return handleContextMenuEvent(event);
}

bool PDFPlugin::handleContextMenuEvent(const WebMouseEvent& event)
{
    if (!m_frame.page())
        return false;

    WebPage* webPage = m_frame.page();
    FrameView* frameView = m_frame.coreFrame()->view();
    IntPoint point = frameView->contentsToScreen(IntRect(frameView->windowToContents(event.position()), IntSize())).location();

    NSUserInterfaceLayoutDirection uiLayoutDirection = webPage->userInterfaceLayoutDirection() == UserInterfaceLayoutDirection::LTR ? NSUserInterfaceLayoutDirectionLeftToRight : NSUserInterfaceLayoutDirectionRightToLeft;
    NSMenu *nsMenu = [m_pdfLayerController menuForEvent:nsEventForWebMouseEvent(event) withUserInterfaceLayoutDirection:uiLayoutDirection];

    if (!nsMenu)
        return false;
    
    Vector<PDFContextMenuItem> items;
    auto itemCount = [nsMenu numberOfItems];
    for (int i = 0; i < itemCount; i++) {
        auto item = [nsMenu itemAtIndex:i];
        if ([item submenu])
            continue;
        PDFContextMenuItem menuItem { String([item title]), !![item isEnabled], !![item isSeparatorItem], static_cast<int>([item state]), [item action], i };
        items.append(WTFMove(menuItem));
    }
    PDFContextMenu contextMenu { point, WTFMove(items) };

    Optional<int> selectedIndex = -1;
    webPage->sendSync(Messages::WebPageProxy::ShowPDFContextMenu(contextMenu), Messages::WebPageProxy::ShowPDFContextMenu::Reply(selectedIndex));

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
    
    switch (event.type()) {
    case WebEvent::KeyDown:
        return [m_pdfLayerController keyDown:fakeEvent];
    default:
        return false;
    }
    
    return false;
}
    
bool PDFPlugin::handleEditingCommand(const String& commandName, const String& argument)
{
    if (commandName == "copy")
        [m_pdfLayerController copySelection];
    else if (commandName == "selectAll")
        [m_pdfLayerController selectAll];
    else if (commandName == "takeFindStringFromSelection") {
        NSString *string = [m_pdfLayerController currentSelection].string;
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        if (string.length)
            writeItemsToPasteboard(NSFindPboard, @[ [string dataUsingEncoding:NSUTF8StringEncoding] ], @[ NSPasteboardTypeString ]);
        ALLOW_DEPRECATED_DECLARATIONS_END
    }

    return true;
}

bool PDFPlugin::isEditingCommandEnabled(const String& commandName)
{
    if (commandName == "copy" || commandName == "takeFindStringFromSelection")
        return [m_pdfLayerController currentSelection];

    if (commandName == "selectAll")
        return true;

    return false;
}

void PDFPlugin::setScrollOffset(const ScrollOffset& offset)
{
    m_scrollOffset = IntSize(offset.x(), offset.y());

    [CATransaction begin];
    [m_pdfLayerController setScrollPosition:offset];

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

bool PDFPlugin::isFullFramePlugin() const
{
    // <object> or <embed> plugins will appear to be in their parent frame, so we have to
    // check whether our frame's widget is exactly our PluginView.
    Document* document = m_frame.coreFrame()->document();
    return document->isPluginDocument() && static_cast<PluginDocument*>(document)->pluginWidget() == pluginView();
}

bool PDFPlugin::handlesPageScaleFactor() const
{
    return m_frame.isMainFrame() && isFullFramePlugin();
}

void PDFPlugin::clickedLink(NSURL *url)
{
    URL coreURL = url;
    if (coreURL.protocolIsJavaScript())
        return;

    auto* frame = m_frame.coreFrame();

    RefPtr<Event> coreEvent;
    if (m_lastMouseEvent.type() != WebEvent::NoType)
        coreEvent = MouseEvent::create(eventNames().clickEvent, &frame->windowProxy(), platform(m_lastMouseEvent), 0, 0);

    frame->loader().changeLocation(coreURL, emptyString(), coreEvent.get(), LockHistory::No, LockBackForwardList::No, ReferrerPolicy::NoReferrer, ShouldOpenExternalURLsPolicy::ShouldAllow);
}

void PDFPlugin::setActiveAnnotation(PDFAnnotation *annotation)
{
    if (!supportsForms())
        return;

    if (m_activeAnnotation)
        m_activeAnnotation->commit();

    if (annotation) {
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        if ([annotation isKindOfClass:pdfAnnotationTextWidgetClass()] && static_cast<PDFAnnotationTextWidget *>(annotation).isReadOnly) {
            m_activeAnnotation = nullptr;
            return;
        }
        ALLOW_DEPRECATED_DECLARATIONS_END

        m_activeAnnotation = PDFPluginAnnotation::create(annotation, m_pdfLayerController.get(), this);
        m_activeAnnotation->attach(m_annotationContainer.get());
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
        pluginView()->setPageScaleFactor(scaleFactor, IntPoint());

    calculateSizes();
    updateScrollbars();
}

void PDFPlugin::notifyDisplayModeChanged(int)
{
    calculateSizes();
    updateScrollbars();
}

RefPtr<SharedBuffer> PDFPlugin::liveResourceData() const
{
    NSData *pdfData = liveData();

    if (!pdfData)
        return nullptr;

    return SharedBuffer::create(pdfData);
}

bool PDFPlugin::pluginHandlesContentOffsetForAccessibilityHitTest() const
{
    // The PDF plugin handles the scroll view offset natively as part of the layer conversions.
    return true;
}

    
void PDFPlugin::saveToPDF()
{
    // FIXME: We should probably notify the user that they can't save before the document is finished loading.
    // PDFViewController does an NSBeep(), but that seems insufficient.
    if (!m_documentFinishedLoading)
        return;

    NSData *data = liveData();
    m_frame.page()->savePDFToFileInDownloadsFolder(m_suggestedFilename, m_frame.url(), static_cast<const unsigned char *>([data bytes]), [data length]);
}

void PDFPlugin::openWithNativeApplication()
{
    if (!m_temporaryPDFUUID) {
        // FIXME: We should probably notify the user that they can't save before the document is finished loading.
        // PDFViewController does an NSBeep(), but that seems insufficient.
        if (!m_documentFinishedLoading)
            return;

        NSData *data = liveData();

        m_temporaryPDFUUID = createCanonicalUUIDString();
        ASSERT(m_temporaryPDFUUID);

        m_frame.page()->savePDFToTemporaryFolderAndOpenWithNativeApplication(m_suggestedFilename, m_frame.info(), static_cast<const unsigned char *>([data bytes]), [data length], m_temporaryPDFUUID);
        return;
    }

    m_frame.page()->send(Messages::WebPageProxy::OpenPDFFromTemporaryFolderWithNativeApplication(m_frame.info(), m_temporaryPDFUUID));
}

void PDFPlugin::writeItemsToPasteboard(NSString *pasteboardName, NSArray *items, NSArray *types)
{
    auto pasteboardTypes = makeVector<String>(types);

    int64_t newChangeCount;
    auto& webProcess = WebProcess::singleton();
    webProcess.parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::SetPasteboardTypes(pasteboardName, pasteboardTypes),
        Messages::WebPasteboardProxy::SetPasteboardTypes::Reply(newChangeCount), 0);

    for (NSUInteger i = 0, count = items.count; i < count; ++i) {
        NSString *type = [types objectAtIndex:i];
        NSData *data = [items objectAtIndex:i];

        // We don't expect the data for any items to be empty, but aren't completely sure.
        // Avoid crashing in the SharedMemory constructor in release builds if we're wrong.
        ASSERT(data.length);
        if (!data.length)
            continue;

        if ([type isEqualToString:legacyStringPasteboardType()] || [type isEqualToString:NSPasteboardTypeString]) {
            RetainPtr<NSString> plainTextString = adoptNS([[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding]);
            webProcess.parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::SetPasteboardStringForType(pasteboardName, type, plainTextString.get()), Messages::WebPasteboardProxy::SetPasteboardStringForType::Reply(newChangeCount), 0);
        } else {
            auto buffer = SharedBuffer::create(data);
            SharedMemory::Handle handle;
            RefPtr<SharedMemory> sharedMemory = SharedMemory::allocate(buffer->size());
            memcpy(sharedMemory->data(), buffer->data(), buffer->size());
            sharedMemory->createHandle(handle, SharedMemory::Protection::ReadOnly);
            webProcess.parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::SetPasteboardBufferForType(pasteboardName, type, handle, buffer->size()), Messages::WebPasteboardProxy::SetPasteboardBufferForType::Reply(newChangeCount), 0);
        }
    }
}

void PDFPlugin::showDefinitionForAttributedString(NSAttributedString *string, CGPoint point)
{
    DictionaryPopupInfo dictionaryPopupInfo;
    dictionaryPopupInfo.origin = convertFromPDFViewToRootView(IntPoint(point));
    dictionaryPopupInfo.attributedString = string;
    
    
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
    
    m_frame.page()->send(Messages::WebPageProxy::DidPerformDictionaryLookup(dictionaryPopupInfo));
}

unsigned PDFPlugin::countFindMatches(const String& target, WebCore::FindOptions options, unsigned /*maxMatchCount*/)
{
    // FIXME: Why is it OK to ignore the passed-in maximum match count?

    if (!target.length())
        return 0;

    NSStringCompareOptions nsOptions = options.contains(WebCore::CaseInsensitive) ? NSCaseInsensitiveSearch : 0;
    return [[m_pdfDocument findString:target withOptions:nsOptions] count];
}

PDFSelection *PDFPlugin::nextMatchForString(const String& target, BOOL searchForward, BOOL caseSensitive, BOOL wrapSearch, PDFSelection *initialSelection, BOOL startInSelection)
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
        auto emptySelection = adoptNS([[pdfSelectionClass() alloc] initWithDocument:m_pdfDocument.get()]);
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
    m_frame.page()->didChangeSelection();
}

static const WebCore::Cursor& coreCursor(PDFLayerControllerCursorType type)
{
    switch (type) {
    case kPDFLayerControllerCursorTypeHand:
        return WebCore::handCursor();
    case kPDFLayerControllerCursorTypeIBeam:
        return WebCore::iBeamCursor();
    case kPDFLayerControllerCursorTypePointer:
    default:
        return WebCore::pointerCursor();
    }
}

void PDFPlugin::notifyCursorChanged(uint64_t type)
{
    m_frame.page()->send(Messages::WebPageProxy::SetCursor(coreCursor(static_cast<PDFLayerControllerCursorType>(type))));
}

String PDFPlugin::getSelectionString() const
{
    return [[m_pdfLayerController currentSelection] string];
}

String PDFPlugin::getSelectionForWordAtPoint(const WebCore::FloatPoint& point) const
{
    IntPoint pointInView = convertFromPluginToPDFView(convertFromRootViewToPlugin(roundedIntPoint(point)));
    PDFSelection *selectionForWord = [m_pdfLayerController getSelectionForWordAtPoint:pointInView];
    [m_pdfLayerController setCurrentSelection:selectionForWord];
    return [selectionForWord string];
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
        if (![annotation isKindOfClass:pdfAnnotationLinkClass()])
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
    return m_frame.coreFrame()->document()->axObjectCache();
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
    m_frame.page()->send(Messages::WebPageProxy::SearchTheWeb(string));
}

void PDFPlugin::performSpotlightSearch(NSString *string)
{
    m_frame.page()->send(Messages::WebPageProxy::SearchWithSpotlight(string));
}

bool PDFPlugin::handleWheelEvent(const WebWheelEvent& event)
{
    PDFDisplayMode displayMode = [m_pdfLayerController displayMode];

    if (displayMode == kPDFDisplaySinglePageContinuous || displayMode == kPDFDisplayTwoUpContinuous)
        return ScrollableArea::handleWheelEvent(platform(event));

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

    return ScrollableArea::handleWheelEvent(platform(event));
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

NSObject *PDFPlugin::accessibilityObject() const
{
    return m_accessibilityObject.get();
}

} // namespace WebKit

#endif // ENABLE(PDFKIT_PLUGIN)
