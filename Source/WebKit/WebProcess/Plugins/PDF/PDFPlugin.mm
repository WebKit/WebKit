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
#import "WebPage.h"
#import "WebPageProxyMessages.h"
#import "WebPasteboardProxyMessages.h"
#import "WebProcess.h"
#import <JavaScriptCore/JSContextRef.h>
#import <JavaScriptCore/JSObjectRef.h>
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
#import <WebCore/ScrollAnimator.h>
#import <WebCore/ScrollbarTheme.h>
#import <WebCore/Settings.h>
#import <WebCore/WebAccessibilityObjectWrapperMac.h>
#import <WebCore/WheelEventTestTrigger.h>
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <pal/spi/mac/NSMenuSPI.h>
#import <wtf/UUID.h>

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
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300
    [_pdfLayerController setAccessibilityParent:self];
#endif
}

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (BOOL)accessibilityIsIgnored
IGNORE_WARNINGS_END
{
    return NO;
}

// This is called by PDFAccessibilityNodes from inside PDFKit to get final bounds.
- (NSRect)convertRectToScreenSpace:(NSRect)rect
{
    return _pdfPlugin->convertFromPDFViewToScreen(rect);
}

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (id)accessibilityAttributeValue:(NSString *)attribute
IGNORE_WARNINGS_END
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

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300
    if ([attribute isEqualToString:NSAccessibilityChildrenAttribute])
        return @[ _pdfLayerController ];
    if ([attribute isEqualToString:NSAccessibilityRoleAttribute])
        return NSAccessibilityGroupRole;
#else
    if ([attribute isEqualToString:NSAccessibilityValueAttribute])
        return [_pdfLayerController accessibilityValueAttribute];
    if ([attribute isEqualToString:NSAccessibilitySelectedTextAttribute])
        return [_pdfLayerController accessibilitySelectedTextAttribute];
    if ([attribute isEqualToString:NSAccessibilitySelectedTextRangeAttribute])
        return [_pdfLayerController accessibilitySelectedTextRangeAttribute];
    if ([attribute isEqualToString:NSAccessibilityNumberOfCharactersAttribute])
        return [_pdfLayerController accessibilityNumberOfCharactersAttribute];
    if ([attribute isEqualToString:NSAccessibilityVisibleCharacterRangeAttribute])
        return [_pdfLayerController accessibilityVisibleCharacterRangeAttribute];
    if ([attribute isEqualToString:NSAccessibilityRoleAttribute])
        return [_pdfLayerController accessibilityRoleAttribute];
    if ([attribute isEqualToString:NSAccessibilityRoleDescriptionAttribute])
        return [_pdfLayerController accessibilityRoleDescriptionAttribute];
    if ([attribute isEqualToString:NSAccessibilityFocusedAttribute])
        return [_parent accessibilityAttributeValue:NSAccessibilityFocusedAttribute];
#endif

    return 0;
}

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (id)accessibilityAttributeValue:(NSString *)attribute forParameter:(id)parameter
IGNORE_WARNINGS_END
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

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (NSArray *)accessibilityAttributeNames
IGNORE_WARNINGS_END
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
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300
            NSAccessibilityChildrenAttribute
#else
            NSAccessibilityRoleAttribute,
            NSAccessibilityValueAttribute,
            NSAccessibilitySelectedTextAttribute,
            NSAccessibilitySelectedTextRangeAttribute,
            NSAccessibilityNumberOfCharactersAttribute,
            NSAccessibilityVisibleCharacterRangeAttribute
#endif
            ];
        [attributeNames retain];
    }

    return attributeNames;
}

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (NSArray *)accessibilityActionNames
IGNORE_WARNINGS_END
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300
    return nil;
#else
    static NSArray *actionNames = 0;
    
    if (!actionNames)
        actionNames = [[NSArray arrayWithObject:NSAccessibilityShowMenuAction] retain];
    
    return actionNames;
#endif
}

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (void)accessibilityPerformAction:(NSString *)action
IGNORE_WARNINGS_END
{
    if ([action isEqualToString:NSAccessibilityShowMenuAction])
        _pdfPlugin->showContextMenuAtPoint(WebCore::IntRect(WebCore::IntPoint(), _pdfPlugin->size()).center());
}

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (BOOL)accessibilityIsAttributeSettable:(NSString *)attribute
IGNORE_WARNINGS_END
{
    return [_pdfLayerController accessibilityIsAttributeSettable:attribute];
}

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (void)accessibilitySetValue:(id)value forAttribute:(NSString *)attribute
IGNORE_WARNINGS_END
{
    return [_pdfLayerController accessibilitySetValue:value forAttribute:attribute];
}

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (NSArray *)accessibilityParameterizedAttributeNames
IGNORE_WARNINGS_END
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300
    return nil;
#else
    return [_pdfLayerController accessibilityParameterizedAttributeNames];
#endif
}

- (id)accessibilityFocusedUIElement
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300
    if (WebKit::PDFPluginAnnotation* activeAnnotation = _pdfPlugin->activeAnnotation()) {
        if (WebCore::AXObjectCache* existingCache = _pdfPlugin->axObjectCache()) {
            if (WebCore::AccessibilityObject* object = existingCache->getOrCreate(activeAnnotation->element()))
                ALLOW_DEPRECATED_DECLARATIONS_BEGIN
                return [object->wrapper() accessibilityAttributeValue:@"_AXAssociatedPluginParent"];
                ALLOW_DEPRECATED_DECLARATIONS_END
        }
    }
    return nil;
#else
    return self;
#endif
}

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300
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
#endif

- (id)accessibilityHitTest:(NSPoint)point
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300
    point = _pdfPlugin->convertFromRootViewToPDFView(WebCore::IntPoint(point));
    return [_pdfLayerController accessibilityHitTest:point];
#else
    return self;
#endif
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

        const UInt8* bytes = 0;
        CFIndex length;
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
        if (!bytes)
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
    , m_frame(&frame)
    , m_containerLayer(adoptNS([[CALayer alloc] init]))
    , m_contentLayer(adoptNS([[CALayer alloc] init]))
    , m_scrollCornerLayer(adoptNS([[WKPDFPluginScrollbarLayer alloc] initWithPDFPlugin:this]))
    , m_pdfLayerController(adoptNS([[pdfLayerControllerClass() alloc] init]))
    , m_pdfLayerControllerDelegate(adoptNS([[WKPDFLayerControllerDelegate alloc] initWithPDFPlugin:this]))
{
    m_pdfLayerController.get().delegate = m_pdfLayerControllerDelegate.get();
    m_pdfLayerController.get().parentLayer = m_contentLayer.get();

    if (supportsForms()) {
        Document* document = webFrame()->coreFrame()->document();
        m_annotationContainer = document->createElement(divTag, false);
        m_annotationContainer->setAttributeWithoutSynchronization(idAttr, AtomicString("annotationContainer", AtomicString::ConstructFromLiteral));

        auto annotationStyleElement = document->createElement(styleTag, false);
        annotationStyleElement->setTextContent(annotationStyle);

        m_annotationContainer->appendChild(annotationStyleElement);
        document->bodyOrFrameset()->appendChild(*m_annotationContainer);
    }

    m_accessibilityObject = adoptNS([[WKPDFPluginAccessibilityObject alloc] initWithPDFPlugin:this]);
    [m_accessibilityObject setPdfLayerController:m_pdfLayerController.get()];
    [m_accessibilityObject setParent:webFrame()->page()->accessibilityRemoteObject()];

    [m_containerLayer addSublayer:m_contentLayer.get()];
    [m_containerLayer addSublayer:m_scrollCornerLayer.get()];
#if ENABLE(WEBPROCESS_WINDOWSERVER_BLOCKING)
    if ([m_pdfLayerController respondsToSelector:@selector(setDeviceColorSpace:)]) {
        auto view = webFrame()->coreFrame()->view();
        [m_pdfLayerController setDeviceColorSpace:screenColorSpace(view)];
    }
#endif
}

PDFPlugin::~PDFPlugin()
{
}

PluginInfo PDFPlugin::pluginInfo()
{
    PluginInfo info;
    info.name = builtInPDFPluginName();
    info.isApplicationPlugin = true;
    info.clientLoadPolicy = PluginLoadClientPolicyUndefined;
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

    FrameView* frameView = m_frame->coreFrame()->view();
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
    if (Frame* frame = webFrame()->coreFrame()) {
        if (Page* page = frame->page()) {
            if (page->expectsWheelEventTriggers())
                scrollAnimator().setWheelEventTestTrigger(page->testTrigger());
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

int PDFPlugin::scrollSize(ScrollbarOrientation orientation) const
{
    Scrollbar* scrollbar = ((orientation == HorizontalScrollbar) ? m_horizontalScrollbar : m_verticalScrollbar).get();
    return scrollbar ? (scrollbar->totalSize() - scrollbar->visibleSize()) : 0;
}

bool PDFPlugin::isActive() const
{
    if (Frame* coreFrame = m_frame->coreFrame()) {
        if (Page* page = coreFrame->page())
            return page->focusController().isActive();
    }

    return false;
}

bool PDFPlugin::forceUpdateScrollbarsOnMainThreadForPerformanceTesting() const
{
    if (Frame* coreFrame = m_frame->coreFrame()) {
        if (Page* page = coreFrame->page())
            return page->settings().forceUpdateScrollbarsOnMainThreadForPerformanceTesting();
    }

    return false;
}

int PDFPlugin::scrollOffset(ScrollbarOrientation orientation) const
{
    if (orientation == HorizontalScrollbar)
        return m_scrollOffset.width();

    if (orientation == VerticalScrollbar)
        return m_scrollOffset.height();

    ASSERT_NOT_REACHED();
    return 0;
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
    PDFPlugin* pdfView = static_cast<PDFPlugin*>(JSObjectGetPrivate(thisObject));

    WebFrame* frame = pdfView->m_frame;
    if (!frame)
        return JSValueMakeUndefined(ctx);

    Frame* coreFrame = frame->coreFrame();
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

    m_suggestedFilename = String(m_suggestedFilename + ".pdf");
    m_data = PDFDocumentImage::convertPostScriptDataToPDF(WTFMove(m_data));
}

void PDFPlugin::pdfDocumentDidLoad()
{
    addArchiveResource();
    
    RetainPtr<PDFDocument> document = adoptNS([[pdfDocumentClass() alloc] initWithData:rawData()]);

    setPDFDocument(document);

    updatePageAndDeviceScaleFactors();

    [m_pdfLayerController setFrameSize:size()];
    m_pdfLayerController.get().document = document.get();

    if (handlesPageScaleFactor())
        pluginView()->setPageScaleFactor([m_pdfLayerController contentScaleFactor], IntPoint());

    notifyScrollPositionChanged(IntPoint([m_pdfLayerController scrollPosition]));

    calculateSizes();
    updateScrollbars();

    runScriptsInPDFDocument();

    if ([document isLocked])
        createPasswordEntryForm();

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300
    if ([m_pdfLayerController respondsToSelector:@selector(setURLFragment:)]) {
        String pdfURLFragment = webFrame()->url().fragmentIdentifier();
        [m_pdfLayerController setURLFragment:pdfURLFragment];
    }
#endif
}
    
void PDFPlugin::streamDidReceiveResponse(uint64_t streamID, const URL&, uint32_t, uint32_t, const String& mimeType, const String&, const String& suggestedFilename)
{
    ASSERT_UNUSED(streamID, streamID == pdfDocumentRequestID);

    m_suggestedFilename = suggestedFilename;

    if (equalIgnoringASCIICase(mimeType, postScriptMIMEType))
        m_isPostScript = true;
}

void PDFPlugin::streamDidReceiveData(uint64_t streamID, const char* bytes, int length)
{
    ASSERT_UNUSED(streamID, streamID == pdfDocumentRequestID);

    if (!m_data)
        m_data = adoptCF(CFDataCreateMutable(0, 0));

    CFDataAppendBytes(m_data.get(), reinterpret_cast<const UInt8*>(bytes), length);
}

void PDFPlugin::streamDidFinishLoading(uint64_t streamID)
{
    ASSERT_UNUSED(streamID, streamID == pdfDocumentRequestID);

    convertPostScriptDataIfNeeded();
    pdfDocumentDidLoad();
}

void PDFPlugin::streamDidFail(uint64_t streamID, bool wasCancelled)
{
    ASSERT_UNUSED(streamID, streamID == pdfDocumentRequestID);

    m_data.clear();
}

void PDFPlugin::manualStreamDidReceiveResponse(const URL& responseURL, uint32_t streamLength,  uint32_t lastModifiedTime, const String& mimeType, const String& headers, const String& suggestedFilename)
{
    m_suggestedFilename = suggestedFilename;

    if (equalIgnoringASCIICase(mimeType, postScriptMIMEType))
        m_isPostScript = true;
}

void PDFPlugin::manualStreamDidReceiveData(const char* bytes, int length)
{
    if (!m_data)
        m_data = adoptCF(CFDataCreateMutable(0, 0));

    CFDataAppendBytes(m_data.get(), reinterpret_cast<const UInt8*>(bytes), length);
}

void PDFPlugin::manualStreamDidFinishLoading()
{
    convertPostScriptDataIfNeeded();
    pdfDocumentDidLoad();
}

void PDFPlugin::manualStreamDidFail(bool)
{
    m_data.clear();
}

void PDFPlugin::runScriptsInPDFDocument()
{
    Vector<RetainPtr<CFStringRef>> scripts;
    getAllScriptsInPDFDocument([m_pdfDocument documentRef], scripts);

    if (scripts.isEmpty())
        return;

    JSGlobalContextRef ctx = JSGlobalContextCreate(0);
    JSObjectRef jsPDFDoc = makeJSPDFDoc(ctx);
    for (auto& script : scripts)
        JSEvaluateScript(ctx, OpaqueJSString::tryCreate(script.get()).get(), jsPDFDoc, nullptr, 0, nullptr);
    JSGlobalContextRelease(ctx);
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

    if (![pdfDocument() isLocked]) {
        m_passwordField = nullptr;

        calculateSizes();
        updateScrollbars();
    }
}

void PDFPlugin::updatePageAndDeviceScaleFactors()
{
    double newScaleFactor = controller()->contentsScaleFactor();
    if (!handlesPageScaleFactor())
        newScaleFactor *= webFrame()->page()->pageScaleFactor();

    if (newScaleFactor != [m_pdfLayerController deviceScaleFactor])
        [m_pdfLayerController setDeviceScaleFactor:newScaleFactor];
}

void PDFPlugin::contentsScaleFactorChanged(float)
{
    updatePageAndDeviceScaleFactors();
}

void PDFPlugin::calculateSizes()
{
    if ([pdfDocument() isLocked]) {
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
    if (webFrame()) {
        if (FrameView* frameView = webFrame()->coreFrame()->view())
            frameView->removeScrollableArea(this);
    }
}

void PDFPlugin::destroy()
{
    m_pdfLayerController.get().delegate = 0;

    if (webFrame()) {
        if (FrameView* frameView = webFrame()->coreFrame()->view())
            frameView->removeScrollableArea(this);
    }

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
    auto* page = webFrame()->coreFrame()->page();
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
    FrameView* frameView = webFrame()->coreFrame()->view();

    if (!frameView)
        return FloatRect();

    FloatRect updatedRect = rect;
    updatedRect.setLocation(convertFromPDFViewToRootView(IntPoint(updatedRect.location())));
    return webFrame()->coreFrame()->page()->chrome().rootViewToScreen(enclosingIntRect(updatedRect));
}

IntRect PDFPlugin::boundsOnScreen() const
{
    FrameView* frameView = webFrame()->coreFrame()->view();

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

#if __MAC_OS_X_VERSION_MIN_REQUIRED < 101300
void PDFPlugin::updateCursor(const WebMouseEvent& event, UpdateCursorMode mode)
{
    HitTestResult hitTestResult = None;

    PDFSelection *selectionUnderMouse = [m_pdfLayerController getSelectionForWordAtPoint:convertFromPluginToPDFView(event.position())];
    if (selectionUnderMouse && [[selectionUnderMouse string] length])
        hitTestResult = Text;

    if (hitTestResult == m_lastHitTestResult && mode == UpdateIfNeeded)
        return;

    webFrame()->page()->send(Messages::WebPageProxy::SetCursor(hitTestResult == Text ? WebCore::iBeamCursor() : WebCore::pointerCursor()));
    m_lastHitTestResult = hitTestResult;
}
#endif

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

    if ([pdfDocument() isLocked])
        return false;

    // Right-clicks and Control-clicks always call handleContextMenuEvent as well.
    if (event.button() == WebMouseEvent::RightButton || (event.button() == WebMouseEvent::LeftButton && event.controlKey()))
        return true;

    NSEvent *nsEvent = nsEventForWebMouseEvent(event);

    switch (event.type()) {
    case WebEvent::MouseMove:
        mouseMovedInContentArea();
#if __MAC_OS_X_VERSION_MIN_REQUIRED < 101300
        updateCursor(event);
#endif

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
#if __MAC_OS_X_VERSION_MIN_REQUIRED < 101300
    updateCursor(event, ForceUpdate);
#endif
    return false;
}

bool PDFPlugin::handleMouseLeaveEvent(const WebMouseEvent&)
{
    mouseExitedContentArea();
    return false;
}
    
bool PDFPlugin::showContextMenuAtPoint(const IntPoint& point)
{
    FrameView* frameView = webFrame()->coreFrame()->view();
    IntPoint contentsPoint = frameView->contentsToRootView(point);
    WebMouseEvent event(WebEvent::MouseDown, WebMouseEvent::RightButton, 0, contentsPoint, contentsPoint, 0, 0, 0, 1, OptionSet<WebEvent::Modifier> { }, WallTime::now(), WebCore::ForceAtClick);
    return handleContextMenuEvent(event);
}

bool PDFPlugin::handleContextMenuEvent(const WebMouseEvent& event)
{
    if (!webFrame()->page())
        return false;

    WebPage* webPage = webFrame()->page();
    FrameView* frameView = webFrame()->coreFrame()->view();
    IntPoint point = frameView->contentsToScreen(IntRect(frameView->windowToContents(event.position()), IntSize())).location();

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300
    NSUserInterfaceLayoutDirection uiLayoutDirection = webPage->userInterfaceLayoutDirection() == UserInterfaceLayoutDirection::LTR ? NSUserInterfaceLayoutDirectionLeftToRight : NSUserInterfaceLayoutDirectionRightToLeft;
    NSMenu *nsMenu = [m_pdfLayerController menuForEvent:nsEventForWebMouseEvent(event) withUserInterfaceLayoutDirection:uiLayoutDirection];
#else
    NSMenu *nsMenu = [m_pdfLayerController menuForEvent:nsEventForWebMouseEvent(event)];
#endif

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
    Document* document = webFrame()->coreFrame()->document();
    return document->isPluginDocument() && static_cast<PluginDocument*>(document)->pluginWidget() == pluginView();
}

bool PDFPlugin::handlesPageScaleFactor() const
{
    return webFrame()->isMainFrame() && isFullFramePlugin();
}

void PDFPlugin::clickedLink(NSURL *url)
{
    URL coreURL = url;
    if (WTF::protocolIsJavaScript(coreURL))
        return;

    Frame* frame = webFrame()->coreFrame();

    RefPtr<Event> coreEvent;
    if (m_lastMouseEvent.type() != WebEvent::NoType)
        coreEvent = MouseEvent::create(eventNames().clickEvent, &frame->windowProxy(), platform(m_lastMouseEvent), 0, 0);

    frame->loader().urlSelected(coreURL, emptyString(), coreEvent.get(), LockHistory::No, LockBackForwardList::No, MaybeSendReferrer, ShouldOpenExternalURLsPolicy::ShouldAllow);
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
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300
    return true;
#else
    return false;
#endif
}

    
void PDFPlugin::saveToPDF()
{
    // FIXME: We should probably notify the user that they can't save before the document is finished loading.
    // PDFViewController does an NSBeep(), but that seems insufficient.
    if (!pdfDocument())
        return;

    NSData *data = liveData();
    webFrame()->page()->savePDFToFileInDownloadsFolder(m_suggestedFilename, webFrame()->url(), static_cast<const unsigned char *>([data bytes]), [data length]);
}

void PDFPlugin::openWithNativeApplication()
{
    if (!m_temporaryPDFUUID) {
        // FIXME: We should probably notify the user that they can't save before the document is finished loading.
        // PDFViewController does an NSBeep(), but that seems insufficient.
        if (!pdfDocument())
            return;

        NSData *data = liveData();

        m_temporaryPDFUUID = createCanonicalUUIDString();
        ASSERT(m_temporaryPDFUUID);

        webFrame()->page()->savePDFToTemporaryFolderAndOpenWithNativeApplication(m_suggestedFilename, webFrame()->url(), static_cast<const unsigned char *>([data bytes]), [data length], m_temporaryPDFUUID);
        return;
    }

    webFrame()->page()->send(Messages::WebPageProxy::OpenPDFFromTemporaryFolderWithNativeApplication(m_temporaryPDFUUID));
}

void PDFPlugin::writeItemsToPasteboard(NSString *pasteboardName, NSArray *items, NSArray *types)
{
    Vector<String> pasteboardTypes;

    for (NSString *type in types)
        pasteboardTypes.append(type);

    uint64_t newChangeCount;
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
    
    webFrame()->page()->send(Messages::WebPageProxy::DidPerformDictionaryLookup(dictionaryPopupInfo));
}

unsigned PDFPlugin::countFindMatches(const String& target, WebCore::FindOptions options, unsigned /*maxMatchCount*/)
{
    // FIXME: Why is it OK to ignore the passed-in maximum match count?

    if (!target.length())
        return 0;

    NSStringCompareOptions nsOptions = options.contains(WebCore::CaseInsensitive) ? NSCaseInsensitiveSearch : 0;
    return [[pdfDocument() findString:target withOptions:nsOptions] count];
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

    PDFDocument *document = pdfDocument().get();

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

    PDFSelection *foundSelection = [document findString:target fromSelection:selectionForInitialSearch.get() withOptions:options];

    // If we first searched in the selection, and we found the selection, search again from just past the selection.
    if (startInSelection && [foundSelection isEqual:initialSelection])
        foundSelection = [document findString:target fromSelection:initialSelection withOptions:options];

    if (!foundSelection && wrapSearch) {
        auto emptySelection = adoptNS([[pdfSelectionClass() alloc] initWithDocument:document]);
        foundSelection = [document findString:target fromSelection:emptySelection.get() withOptions:options];
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
    webFrame()->page()->didChangeSelection();
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
    webFrame()->page()->send(Messages::WebPageProxy::SetCursor(coreCursor(static_cast<PDFLayerControllerCursorType>(type))));
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

    NSString *lookupText;
    NSDictionary *options;
    std::tie(lookupText, options) = DictionaryLookup::stringForPDFSelection(selection);
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
    return webFrame()->coreFrame()->document()->axObjectCache();
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
    webFrame()->page()->send(Messages::WebPageProxy::SearchTheWeb(string));
}

void PDFPlugin::performSpotlightSearch(NSString *string)
{
    webFrame()->page()->send(Messages::WebPageProxy::SearchWithSpotlight(string));
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
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300
    if (!m_activeAnnotation)
        return nil;

    if (m_activeAnnotation->element() != element)
        return nil;

    return [m_activeAnnotation->annotation() accessibilityNode];
#else
    return nil;
#endif
}

NSObject *PDFPlugin::accessibilityObject() const
{
    return m_accessibilityObject.get();
}

} // namespace WebKit

#endif // ENABLE(PDFKIT_PLUGIN)
