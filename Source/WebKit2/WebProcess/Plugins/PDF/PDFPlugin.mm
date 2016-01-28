/*
 * Copyright (C) 2009, 2011, 2012, 2015 Apple Inc. All rights reserved.
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

#if ENABLE(PDFKIT_PLUGIN) && !USE(DEPRECATED_PDF_PLUGIN)

#import "ArgumentCoders.h"
#import "DataReference.h"
#import "PDFAnnotationTextWidgetDetails.h"
#import "PDFLayerControllerSPI.h"
#import "PDFPluginAnnotation.h"
#import "PDFPluginPasswordField.h"
#import "PluginView.h"
#import "WKAccessibilityWebPageObjectMac.h"
#import "WKPageFindMatchesClient.h"
#import "WebCoreArgumentCoders.h"
#import "WebEvent.h"
#import "WebEventConversion.h"
#import "WebPage.h"
#import "WebPageProxyMessages.h"
#import "WebPasteboardProxyMessages.h"
#import "WebProcess.h"
#import <JavaScriptCore/JSContextRef.h>
#import <JavaScriptCore/JSObjectRef.h>
#import <JavaScriptCore/JSStringRef.h>
#import <JavaScriptCore/JSStringRefCF.h>
#import <PDFKit/PDFKit.h>
#import <QuartzCore/QuartzCore.h>
#import <WebCore/ArchiveResource.h>
#import <WebCore/Chrome.h>
#import <WebCore/Cursor.h>
#import <WebCore/DictionaryLookup.h>
#import <WebCore/DocumentLoader.h>
#import <WebCore/FocusController.h>
#import <WebCore/FormState.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameLoader.h>
#import <WebCore/FrameView.h>
#import <WebCore/GraphicsContext.h>
#import <WebCore/GraphicsLayerCA.h>
#import <WebCore/HTMLElement.h>
#import <WebCore/HTMLFormElement.h>
#import <WebCore/HTMLPlugInElement.h>
#import <WebCore/LocalizedStrings.h>
#import <WebCore/MainFrame.h>
#import <WebCore/MouseEvent.h>
#import <WebCore/Page.h>
#import <WebCore/PageOverlayController.h>
#import <WebCore/Pasteboard.h>
#import <WebCore/PlatformCAAnimationCocoa.h>
#import <WebCore/PluginData.h>
#import <WebCore/PluginDocument.h>
#import <WebCore/RenderBoxModelObject.h>
#import <WebCore/Settings.h>
#import <WebCore/UUID.h>
#import <WebKitSystemInterface.h>
#import <wtf/CurrentTime.h>

using namespace WebCore;

static const char* annotationStyle =
"body { "
"    background-color: rgb(146, 146, 146) !important;"
"} "
"#passwordContainer {"
"    display: -webkit-box; "
"    -webkit-box-align: center; "
"    -webkit-box-pack: center; "
"    position: fixed; "
"    top: 0; "
"    left: 0; "
"    right: 0; "
"    bottom: 0; "
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

const double zoomButtonScaleMultiplier = 1.18920;

@interface WKPDFPluginAccessibilityObject : NSObject {
    PDFLayerController *_pdfLayerController;
    NSObject *_parent;
    WebKit::PDFPlugin* _pdfPlugin;
}

@property (assign) PDFLayerController *pdfLayerController;
@property (assign) NSObject *parent;
@property (assign) WebKit::PDFPlugin* pdfPlugin;

- (id)initWithPDFPlugin:(WebKit::PDFPlugin *)plugin;

@end

@implementation WKPDFPluginAccessibilityObject

@synthesize pdfLayerController=_pdfLayerController;
@synthesize parent=_parent;
@synthesize pdfPlugin=_pdfPlugin;

- (id)initWithPDFPlugin:(WebKit::PDFPlugin *)plugin
{
    if (!(self = [super init]))
        return nil;

    _pdfPlugin = plugin;

    return self;
}

- (BOOL)accessibilityIsIgnored
{
    return NO;
}

- (id)accessibilityAttributeValue:(NSString *)attribute
{
    if ([attribute isEqualToString:NSAccessibilityParentAttribute])
        return _parent;
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
    if ([attribute isEqualToString:NSAccessibilityTopLevelUIElementAttribute])
        return [_parent accessibilityAttributeValue:NSAccessibilityTopLevelUIElementAttribute];
    if ([attribute isEqualToString:NSAccessibilityRoleAttribute])
        return [_pdfLayerController accessibilityRoleAttribute];
    if ([attribute isEqualToString:NSAccessibilityRoleDescriptionAttribute])
        return [_pdfLayerController accessibilityRoleDescriptionAttribute];
    if ([attribute isEqualToString:NSAccessibilityWindowAttribute])
        return [_parent accessibilityAttributeValue:NSAccessibilityWindowAttribute];
    if ([attribute isEqualToString:NSAccessibilitySizeAttribute])
        return [NSValue valueWithSize:_pdfPlugin->boundsOnScreen().size()];
    if ([attribute isEqualToString:NSAccessibilityFocusedAttribute])
        return [_parent accessibilityAttributeValue:NSAccessibilityFocusedAttribute];
    if ([attribute isEqualToString:NSAccessibilityEnabledAttribute])
        return [_parent accessibilityAttributeValue:NSAccessibilityEnabledAttribute];
    if ([attribute isEqualToString:NSAccessibilityPositionAttribute])
        return [NSValue valueWithPoint:_pdfPlugin->boundsOnScreen().location()];

    return 0;
}

- (id)accessibilityAttributeValue:(NSString *)attribute forParameter:(id)parameter
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

- (NSArray *)accessibilityAttributeNames
{
    static NSArray *attributeNames = 0;

    if (!attributeNames) {
        attributeNames = @[NSAccessibilityValueAttribute,
            NSAccessibilitySelectedTextAttribute,
            NSAccessibilitySelectedTextRangeAttribute,
            NSAccessibilityNumberOfCharactersAttribute,
            NSAccessibilityVisibleCharacterRangeAttribute,
            NSAccessibilityParentAttribute,
            NSAccessibilityRoleAttribute,
            NSAccessibilityWindowAttribute,
            NSAccessibilityTopLevelUIElementAttribute,
            NSAccessibilityRoleDescriptionAttribute,
            NSAccessibilitySizeAttribute,
            NSAccessibilityFocusedAttribute,
            NSAccessibilityEnabledAttribute,
            NSAccessibilityPositionAttribute];
        [attributeNames retain];
    }

    return attributeNames;
}

- (NSArray *)accessibilityActionNames
{
    static NSArray *actionNames = 0;
    
    if (!actionNames)
        actionNames = [[NSArray arrayWithObject:NSAccessibilityShowMenuAction] retain];
    
    return actionNames;
}

- (void)accessibilityPerformAction:(NSString *)action
{
    if ([action isEqualToString:NSAccessibilityShowMenuAction])
        _pdfPlugin->showContextMenuAtPoint(IntRect(IntPoint(), _pdfPlugin->size()).center());
}

- (BOOL)accessibilityIsAttributeSettable:(NSString *)attribute
{
    return [_pdfLayerController accessibilityIsAttributeSettable:attribute];
}

- (void)accessibilitySetValue:(id)value forAttribute:(NSString *)attribute
{
    return [_pdfLayerController accessibilitySetValue:value forAttribute:attribute];
}

- (NSArray *)accessibilityParameterizedAttributeNames
{
    return [_pdfLayerController accessibilityParameterizedAttributeNames];
}

- (id)accessibilityFocusedUIElement
{
    return self;
}

- (id)accessibilityHitTest:(NSPoint)point
{
    return self;
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

- (void)pdfLayerController:(PDFLayerController *)pdfLayerController scrollToPoint:(CGPoint)newPosition
{
    _pdfPlugin->scrollToPoint(IntPoint(newPosition));
}

- (void)pdfLayerController:(PDFLayerController *)pdfLayerController copyItems:(NSArray *)items withTypes:(NSArray *)types
{
    _pdfPlugin->writeItemsToPasteboard(NSGeneralPboard, items, types);
}

- (void)pdfLayerController:(PDFLayerController *)pdfLayerController showDefinitionForAttributedString:(NSAttributedString *)string atPoint:(CGPoint)point
{
    _pdfPlugin->showDefinitionForAttributedString(string, point);
}

- (void)pdfLayerController:(PDFLayerController *)pdfLayerController performWebSearchForString:(NSString *)string
{
    _pdfPlugin->performWebSearch(string);
}

- (void)pdfLayerController:(PDFLayerController *)pdfLayerController performSpotlightSearchForString:(NSString *)string
{
    _pdfPlugin->performSpotlightSearch(string);
}

- (void)pdfLayerControllerOpenWithNativeApplication:(PDFLayerController *)pdfLayerController
{
    _pdfPlugin->openWithNativeApplication();
}

- (void)pdfLayerControllerSaveToPDF:(PDFLayerController *)pdfLayerController
{
    _pdfPlugin->saveToPDF();
}

- (void)pdfLayerController:(PDFLayerController *)pdfLayerController didClickLinkWithURL:(NSURL *)url
{
    _pdfPlugin->clickedLink(url);
}

- (void)pdfLayerController:(PDFLayerController *)pdfLayerController didChangeActiveAnnotation:(PDFAnnotation *)annotation
{
    _pdfPlugin->setActiveAnnotation(annotation);
}

- (void)pdfLayerController:(PDFLayerController *)pdfLayerController didChangeDisplayMode:(int)mode
{
    _pdfPlugin->notifyDisplayModeChanged(mode);
}

- (void)pdfLayerController:(PDFLayerController *)pdfLayerController didChangeSelection:(PDFSelection *)selection
{
    _pdfPlugin->notifySelectionChanged(selection);
}

- (void)pdfLayerController:(PDFLayerController *)pdfLayerController invalidateRect:(CGRect)rect
{
    _pdfPlugin->invalidatePDFRect(enclosingIntRect(rect));
}

- (void)pdfLayerControllerInvalidateHUD:(PDFLayerController *)pdfLayerController
{
    _pdfPlugin->invalidateHUD();
}

- (void)pdfLayerControllerZoomIn:(PDFLayerController *)pdfLayerController
{
    _pdfPlugin->zoomIn();
}

- (void)pdfLayerControllerZoomOut:(PDFLayerController *)pdfLayerController
{
    _pdfPlugin->zoomOut();
}

@end

@interface WKPDFHUDAnimationDelegate : NSObject {
    std::function<void (bool)> _completionHandler;
}
@end

@implementation WKPDFHUDAnimationDelegate

- (id)initWithAnimationCompletionHandler:(std::function<void (bool)>)completionHandler
{
    if (!(self = [super init]))
        return nil;

    _completionHandler = WTFMove(completionHandler);

    return self;
}

- (void)animationDidStop:(CAAnimation *)animation finished:(BOOL)flag
{
    _completionHandler(flag);
}

@end

@interface PDFViewLayout
- (NSPoint)convertPoint:(NSPoint)point toPage:(PDFPage *)page forScaleFactor:(CGFloat)scaleFactor;
- (NSRect)convertRect:(NSRect)rect fromPage:(PDFPage *) page forScaleFactor:(CGFloat) scaleFactor;
- (PDFPage *)pageNearestPoint:(NSPoint)point currentPage:(PDFPage *)currentPage;
@end

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

namespace WebKit {

using namespace HTMLNames;

Ref<PDFPlugin> PDFPlugin::create(WebFrame* frame)
{
    return adoptRef(*new PDFPlugin(frame));
}

PDFPlugin::PDFPlugin(WebFrame* frame)
    : Plugin(PDFPluginType)
    , m_frame(frame)
    , m_pdfLayerController(adoptNS([[pdfLayerControllerClass() alloc] init]))
    , m_pdfLayerControllerDelegate(adoptNS([[WKPDFLayerControllerDelegate alloc] initWithPDFPlugin:this]))
    , m_HUD(*this)
{
    [m_pdfLayerController setDelegate:m_pdfLayerControllerDelegate.get()];

    if (supportsForms()) {
        Document* document = webFrame()->coreFrame()->document();

        Ref<Element> annotationStyleElement = document->createElement(styleTag, false);
        annotationStyleElement->setTextContent(annotationStyle, ASSERT_NO_EXCEPTION);

        document->bodyOrFrameset()->appendChild(annotationStyleElement.get());
    }

    m_accessibilityObject = adoptNS([[WKPDFPluginAccessibilityObject alloc] initWithPDFPlugin:this]);
    [m_accessibilityObject setPdfLayerController:m_pdfLayerController.get()];
    [m_accessibilityObject setParent:webFrame()->page()->accessibilityRemoteObject()];
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

PluginView* PDFPlugin::pluginView()
{
    return static_cast<PluginView*>(controller());
}

const PluginView* PDFPlugin::pluginView() const
{
    return static_cast<const PluginView*>(controller());
}

void PDFPlugin::addArchiveResource()
{
    // FIXME: It's a hack to force add a resource to DocumentLoader. PDF documents should just be fetched as CachedResources.

    // Add just enough data for context menu handling and web archives to work.
    NSDictionary* headers = @{ @"Content-Disposition": (NSString *)m_suggestedFilename, @"Content-Type" : @"application/pdf" };
    RetainPtr<NSURLResponse> response = adoptNS([[NSHTTPURLResponse alloc] initWithURL:m_sourceURL statusCode:200 HTTPVersion:(NSString*)kCFHTTPVersion1_1 headerFields:headers]);
    ResourceResponse synthesizedResponse(response.get());

    RefPtr<ArchiveResource> resource = ArchiveResource::create(SharedBuffer::wrapCFData(m_data.get()), m_sourceURL, "application/pdf", String(), String(), synthesizedResponse);
    pluginView()->frame()->document()->loader()->addArchiveResource(resource.release());
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

    page->chrome().print(coreFrame);

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

static RetainPtr<CFMutableDataRef> convertPostScriptDataToPDF(RetainPtr<CFDataRef> postScriptData)
{
    // Convert PostScript to PDF using the Quartz 2D API.
    // http://developer.apple.com/documentation/GraphicsImaging/Conceptual/drawingwithquartz2d/dq_ps_convert/chapter_16_section_1.html

    CGPSConverterCallbacks callbacks = { 0, 0, 0, 0, 0, 0, 0, 0 };
    RetainPtr<CGPSConverterRef> converter = adoptCF(CGPSConverterCreate(0, &callbacks, 0));
    RetainPtr<CGDataProviderRef> provider = adoptCF(CGDataProviderCreateWithCFData(postScriptData.get()));
    RetainPtr<CFMutableDataRef> pdfData = adoptCF(CFDataCreateMutable(kCFAllocatorDefault, 0));
    RetainPtr<CGDataConsumerRef> consumer = adoptCF(CGDataConsumerCreateWithCFData(pdfData.get()));
    
    CGPSConverterConvert(converter.get(), provider.get(), consumer.get(), 0);
    
    return pdfData;
}

void PDFPlugin::convertPostScriptDataIfNeeded()
{
    if (!m_isPostScript)
        return;

    m_suggestedFilename = String(m_suggestedFilename + ".pdf");
    m_data = convertPostScriptDataToPDF(m_data);
}

void PDFPlugin::pdfDocumentDidLoad()
{
    addArchiveResource();
    
    RetainPtr<PDFDocument> document = adoptNS([[pdfDocumentClass() alloc] initWithData:rawData()]);

    setPDFDocument(document);

    [m_pdfLayerController setFrameSize:webFrame()->coreFrame()->view()->visibleContentRect().size()]; /// ???
    [m_pdfLayerController setDocument:document.get()];

    calculateSizes();

    runScriptsInPDFDocument();

    if (isLocked())
        createPasswordEntryForm();

    m_HUD.setVisible(!isLocked(), HUD::AnimateVisibilityTransition::No);
}

bool PDFPlugin::isLocked() const
{
    return [m_pdfDocument isLocked];
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

    size_t scriptCount = scripts.size();
    if (!scriptCount)
        return;

    JSGlobalContextRef ctx = JSGlobalContextCreate(0);
    JSObjectRef jsPDFDoc = makeJSPDFDoc(ctx);

    for (size_t i = 0; i < scriptCount; ++i) {
        JSStringRef script = JSStringCreateWithCFString(scripts[i].get());
        JSEvaluateScript(ctx, script, jsPDFDoc, 0, 0, 0);
        JSStringRelease(script);
    }
    
    JSGlobalContextRelease(ctx);
}

void PDFPlugin::invalidatePDFRect(IntRect rect)
{
    Widget* widget = pluginView();

    // FIXME: One of the conversion functions should do this (with the flipping).
    IntRect invalidRect = rect;
    invalidRect.setY(m_pdfDocumentSize.height() - invalidRect.y() - invalidRect.height());

    widget->invalidateRect(invalidRect);
}

void PDFPlugin::invalidateHUD()
{
    m_HUD.invalidate();
}

void PDFPlugin::createPasswordEntryForm()
{
    if (!supportsForms())
        return;

    Document* document = webFrame()->coreFrame()->document();
    m_passwordContainer = document->createElement(divTag, false);
    m_passwordContainer->setAttribute(idAttr, "passwordContainer");

    m_passwordField = PDFPluginPasswordField::create(m_pdfLayerController.get(), this);
    m_passwordField->attach(m_passwordContainer.get());
    document->bodyOrFrameset()->appendChild(*m_passwordContainer);
}

void PDFPlugin::attemptToUnlockPDF(const String& password)
{
    [m_pdfLayerController attemptToUnlockWithPassword:password];

    if (!isLocked()) {
        m_passwordContainer = nullptr;
        m_passwordField = nullptr;

        calculateSizes();

        m_HUD.setVisible(true, HUD::AnimateVisibilityTransition::Yes);
    }
}

void PDFPlugin::calculateSizes()
{
    setPDFDocumentSize(isLocked() ? IntSize() : IntSize([m_pdfLayerController contentSizeRespectingZoom]));

    // We have to asynchronously update styles because we could be inside layout.
    RefPtr<PDFPlugin> reffedThis = this;
    dispatch_async(dispatch_get_main_queue(), [reffedThis] {
        reffedThis->didCalculateSizes();
    });
}

void PDFPlugin::didCalculateSizes()
{
    HTMLPlugInElement* pluginElement = downcast<PluginDocument>(*webFrame()->coreFrame()->document()).pluginElement();

    if (isLocked()) {
        pluginElement->setInlineStyleProperty(CSSPropertyWidth, 100, CSSPrimitiveValue::CSS_PERCENTAGE);
        pluginElement->setInlineStyleProperty(CSSPropertyHeight, 100, CSSPrimitiveValue::CSS_PERCENTAGE);
        return;
    }

    pluginElement->setInlineStyleProperty(CSSPropertyWidth, m_pdfDocumentSize.width(), CSSPrimitiveValue::CSS_PX);
    pluginElement->setInlineStyleProperty(CSSPropertyHeight, m_pdfDocumentSize.height(), CSSPrimitiveValue::CSS_PX);

    // FIXME: Can't do this in the overflow/subframe case. Where does scroll-snap-type go?
    if (m_usingContinuousMode || !m_frame->isMainFrame() || !isFullFramePlugin())
        return;

    RetainPtr<NSArray> pageRects = [m_pdfLayerController pageRects];

    StringBuilder coordinates;
    for (NSValue *rect in pageRects.get()) {
        // FIXME: Why 4?
        coordinates.appendNumber((long)[rect rectValue].origin.y / 4);
        coordinates.appendLiteral("px ");
    }

    pluginElement->setInlineStyleProperty(CSSPropertyWebkitScrollSnapCoordinate, coordinates.toString());

    Document* document = webFrame()->coreFrame()->document();
    document->bodyOrFrameset()->setInlineStyleProperty(CSSPropertyWebkitScrollSnapType, "mandatory");
}

bool PDFPlugin::initialize(const Parameters& parameters)
{
    m_sourceURL = parameters.url;
    if (!parameters.shouldUseManualLoader && !parameters.url.isEmpty())
        controller()->loadURL(pdfDocumentRequestID, "GET", parameters.url.string(), String(), HTTPHeaderMap(), Vector<uint8_t>(), false);

    controller()->didInitializePlugin();
    return true;
}

void PDFPlugin::destroy()
{
    [m_pdfLayerController setDelegate:nil];

    m_activeAnnotation = nullptr;
}

void PDFPlugin::paint(GraphicsContext& context, const IntRect& dirtyRectInWindowCoordinates)
{
    context.scale(FloatSize(1, -1));
    context.translate(0, -m_pdfDocumentSize.height());
    [m_pdfLayerController drawInContext:context.platformContext()];
}

RefPtr<ShareableBitmap> PDFPlugin::snapshot()
{
    return nullptr;
}

IntPoint PDFPlugin::convertFromPluginToPDFView(const IntPoint& point) const
{
    return IntPoint(point.x(), m_size.height() - point.y());
}

IntPoint PDFPlugin::convertFromRootViewToPlugin(const IntPoint& point) const
{
    return m_rootViewToPluginTransform.mapPoint(point);
}

IntPoint PDFPlugin::convertFromPDFViewToRootView(const IntPoint& point) const
{
    IntPoint pointInPluginCoordinates(point.x(), m_size.height() - point.y());
    return m_pluginToRootViewTransform.mapPoint(pointInPluginCoordinates);
}

FloatRect PDFPlugin::convertFromPDFViewToScreen(const FloatRect& rect) const
{
    FrameView* frameView = webFrame()->coreFrame()->view();

    if (!frameView)
        return FloatRect();

    FloatPoint originInPluginCoordinates(rect.x(), m_size.height() - rect.y() - rect.height());
    FloatRect rectInRootViewCoordinates = m_pluginToRootViewTransform.mapRect(FloatRect(originInPluginCoordinates, rect.size()));

    return frameView->contentsToScreen(enclosingIntRect(rectInRootViewCoordinates));
}

IntRect PDFPlugin::boundsOnScreen() const
{
    FrameView* frameView = webFrame()->coreFrame()->view();

    if (!frameView)
        return IntRect();

    FloatRect bounds = FloatRect(FloatPoint(), size());
    FloatRect rectInRootViewCoordinates = m_pluginToRootViewTransform.mapRect(bounds);
    return frameView->contentsToScreen(enclosingIntRect(rectInRootViewCoordinates));
}

void PDFPlugin::geometryDidChange(const IntSize& pluginSize, const IntRect&, const AffineTransform& pluginToRootViewTransform)
{
    m_pluginToRootViewTransform = pluginToRootViewTransform;
    m_rootViewToPluginTransform = pluginToRootViewTransform.inverse().valueOr(AffineTransform());
    m_size = pluginSize;

    FrameView* frameView = webFrame()->coreFrame()->view();

    [m_pdfLayerController setFrameSize:frameView->frameRect().size()];
    [m_pdfLayerController setVisibleRect:frameView->visibleContentRect()];

    calculateSizes();

    if (m_activeAnnotation)
        m_activeAnnotation->updateGeometry();
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
    return (event.shiftKey() ? NSShiftKeyMask : 0)
        | (event.controlKey() ? NSControlKeyMask : 0)
        | (event.altKey() ? NSAlternateKeyMask : 0)
        | (event.metaKey() ? NSCommandKeyMask : 0);
}
    
static bool getEventTypeFromWebEvent(const WebEvent& event, NSEventType& eventType)
{
    switch (event.type()) {
    case WebEvent::MouseDown:
        switch (static_cast<const WebMouseEvent&>(event).button()) {
        case WebMouseEvent::LeftButton:
            eventType = NSLeftMouseDown;
            return true;
        case WebMouseEvent::RightButton:
            eventType = NSRightMouseDown;
            return true;
        default:
            return false;
        }
    case WebEvent::MouseUp:
        switch (static_cast<const WebMouseEvent&>(event).button()) {
        case WebMouseEvent::LeftButton:
            eventType = NSLeftMouseUp;
            return true;
        case WebMouseEvent::RightButton:
            eventType = NSRightMouseUp;
            return true;
        default:
            return false;
        }
    case WebEvent::MouseMove:
        switch (static_cast<const WebMouseEvent&>(event).button()) {
        case WebMouseEvent::LeftButton:
            eventType = NSLeftMouseDragged;
            return true;
        case WebMouseEvent::RightButton:
            eventType = NSRightMouseDragged;
            return true;
        case WebMouseEvent::NoButton:
            eventType = NSMouseMoved;
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
        return nullptr;

    NSUInteger modifierFlags = modifierFlagsFromWebEvent(event);

    return [NSEvent mouseEventWithType:eventType location:positionInPDFViewCoordinates modifierFlags:modifierFlags timestamp:0 windowNumber:0 context:nil eventNumber:0 clickCount:event.clickCount() pressure:0];
}

void PDFPlugin::updateCursor(const WebMouseEvent& event, UpdateCursor mode)
{
    // FIXME: Should have a hand for links, and something special for annotations.

    HitTestResult hitTestResult = HitTestResult::None;

    IntPoint positionInPDFViewCoordinates(convertFromPluginToPDFView(m_lastMousePositionInPluginCoordinates));
    if (m_HUD.containsPointInRootView(event.position()))
        hitTestResult = HitTestResult::HUD;
    else {
        PDFSelection *selectionUnderMouse = [m_pdfLayerController getSelectionForWordAtPoint:positionInPDFViewCoordinates];
        if (selectionUnderMouse && [[selectionUnderMouse string] length])
            hitTestResult = HitTestResult::Text;
    }

    if (hitTestResult == m_lastHitTestResult && mode == UpdateCursor::IfNeeded)
        return;

    const Cursor& cursor = [hitTestResult] {
        switch (hitTestResult) {
        case HitTestResult::None:
            return pointerCursor();
        case HitTestResult::Text:
            return iBeamCursor();
        case HitTestResult::HUD:
            return pointerCursor();
        };
    }();

    webFrame()->page()->send(Messages::WebPageProxy::SetCursor(cursor));
    m_lastHitTestResult = hitTestResult;
}

bool PDFPlugin::handleMouseEvent(const WebMouseEvent& event)
{
    PlatformMouseEvent platformEvent = platform(event);

    m_lastMouseEvent = event;

    if (isLocked())
        return false;

    // Right-clicks and Control-clicks always call handleContextMenuEvent as well.
    if (event.button() == WebMouseEvent::RightButton || (event.button() == WebMouseEvent::LeftButton && event.controlKey()))
        return true;

    if (event.button() != WebMouseEvent::LeftButton)
        return false;

    NSEvent *nsEvent = nsEventForWebMouseEvent(event);

    switch (event.type()) {
    case WebEvent::MouseMove:
        updateCursor(event);
        [m_pdfLayerController mouseDragged:nsEvent];
        return true;
    case WebEvent::MouseDown:
        [m_pdfLayerController mouseDown:nsEvent];
        return true;
    case WebEvent::MouseUp:
        [m_pdfLayerController mouseUp:nsEvent];
        return true;
    default:
        break;
    }

    return false;
}

bool PDFPlugin::handleMouseEnterEvent(const WebMouseEvent& event)
{
    updateCursor(event, UpdateCursor::Force);
    return false;
}

bool PDFPlugin::handleMouseLeaveEvent(const WebMouseEvent&)
{
    return false;
}
    
bool PDFPlugin::showContextMenuAtPoint(const IntPoint& point)
{
    FrameView* frameView = webFrame()->coreFrame()->view();
    IntPoint contentsPoint = frameView->contentsToRootView(point);
    WebMouseEvent event(WebEvent::MouseDown, WebMouseEvent::RightButton, contentsPoint, contentsPoint, 0, 0, 0, 1, static_cast<WebEvent::Modifiers>(0), monotonicallyIncreasingTime(), ForceAtClick);
    return handleContextMenuEvent(event);
}

bool PDFPlugin::handleContextMenuEvent(const WebMouseEvent& event)
{
    FrameView* frameView = webFrame()->coreFrame()->view();
    IntPoint point = frameView->contentsToScreen(IntRect(frameView->windowToContents(event.position()), IntSize())).location();
    
    if (NSMenu *nsMenu = [m_pdfLayerController menuForEvent:nsEventForWebMouseEvent(event)]) {
        WKPopupContextMenu(nsMenu, point);
        return true;
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
        if (string.length)
            writeItemsToPasteboard(NSFindPboard, @[ [string dataUsingEncoding:NSUTF8StringEncoding] ], @[ NSPasteboardTypeString ]);
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

bool PDFPlugin::isFullFramePlugin() const
{
    // <object> or <embed> plugins will appear to be in their parent frame, so we have to
    // check whether our frame's widget is exactly our PluginView.
    Document* document = webFrame()->coreFrame()->document();
    return document->isPluginDocument() && static_cast<PluginDocument*>(document)->pluginWidget() == pluginView();
}

void PDFPlugin::clickedLink(NSURL *url)
{
    URL coreURL = url;
    if (protocolIsJavaScript(coreURL))
        return;

    Frame* frame = webFrame()->coreFrame();

    RefPtr<Event> coreEvent;
    if (m_lastMouseEvent.type() != WebEvent::NoType)
        coreEvent = MouseEvent::create(eventNames().clickEvent, frame->document()->defaultView(), platform(m_lastMouseEvent), 0, 0);

    frame->loader().urlSelected(coreURL, emptyString(), coreEvent.get(), LockHistory::No, LockBackForwardList::No, MaybeSendReferrer, ShouldOpenExternalURLsPolicy::ShouldAllow);
}

void PDFPlugin::setActiveAnnotation(PDFAnnotation *annotation)
{
    if (!supportsForms())
        return;

    if (m_activeAnnotation)
        m_activeAnnotation->commit();

    if (annotation) {
        if ([annotation isKindOfClass:pdfAnnotationTextWidgetClass()] && static_cast<PDFAnnotationTextWidget *>(annotation).isReadOnly) {
            m_activeAnnotation = nullptr;
            return;
        }

        m_activeAnnotation = PDFPluginAnnotation::create(annotation, m_pdfLayerController.get(), this);
        Document* document = webFrame()->coreFrame()->document();
        m_activeAnnotation->attach(document->bodyOrFrameset());
    } else
        m_activeAnnotation = nullptr;
}

bool PDFPlugin::supportsForms()
{
    // FIXME: We support forms for full-main-frame and <iframe> PDFs, but not <embed> or <object>, because those cases do not have their own Document into which to inject form elements.
    return isFullFramePlugin();
}

void PDFPlugin::notifyDisplayModeChanged(int mode)
{
    m_usingContinuousMode = (mode == kPDFDisplaySinglePageContinuous || mode == kPDFDisplayTwoUpContinuous);
    calculateSizes();
}

RefPtr<SharedBuffer> PDFPlugin::liveResourceData() const
{
    NSData *pdfData = liveData();

    if (!pdfData)
        return nullptr;

    return SharedBuffer::wrapNSData(pdfData);
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

        if ([type isEqualToString:NSStringPboardType] || [type isEqualToString:NSPasteboardTypeString]) {
            RetainPtr<NSString> plainTextString = adoptNS([[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding]);
            webProcess.parentProcessConnection()->sendSync(Messages::WebPasteboardProxy::SetPasteboardStringForType(pasteboardName, type, plainTextString.get()), Messages::WebPasteboardProxy::SetPasteboardStringForType::Reply(newChangeCount), 0);
        } else {
            RefPtr<SharedBuffer> buffer = SharedBuffer::wrapNSData(data);

            if (!buffer)
                continue;

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

    webFrame()->page()->send(Messages::WebPageProxy::DidPerformDictionaryLookup(dictionaryPopupInfo));
}

unsigned PDFPlugin::countFindMatches(const String& target, WebCore::FindOptions options, unsigned maxMatchCount)
{
    if (!target.length())
        return 0;

    int nsOptions = (options & FindOptionsCaseInsensitive) ? NSCaseInsensitiveSearch : 0;

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

    PDFSelection *selectionForInitialSearch = [initialSelection copy];
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

    PDFSelection *foundSelection = [document findString:target fromSelection:selectionForInitialSearch withOptions:options];
    [selectionForInitialSearch release];

    // If we first searched in the selection, and we found the selection, search again from just past the selection.
    if (startInSelection && [foundSelection isEqual:initialSelection])
        foundSelection = [document findString:target fromSelection:initialSelection withOptions:options];

    if (!foundSelection && wrapSearch)
        foundSelection = [document findString:target fromSelection:nil withOptions:options];

    return foundSelection;
}

bool PDFPlugin::findString(const String& target, WebCore::FindOptions options, unsigned maxMatchCount)
{
    BOOL searchForward = !(options & FindOptionsBackwards);
    BOOL caseSensitive = !(options & FindOptionsCaseInsensitive);
    BOOL wrapSearch = options & FindOptionsWrapAround;

    unsigned matchCount;
    if (!maxMatchCount) {
        // If the max was zero, any result means we exceeded the max. We can skip computing the actual count.
        matchCount = static_cast<unsigned>(kWKMoreThanMaximumMatchCount);
    } else {
        matchCount = countFindMatches(target, options, maxMatchCount);
        if (matchCount > maxMatchCount)
            matchCount = static_cast<unsigned>(kWKMoreThanMaximumMatchCount);
    }

    if (target.isEmpty()) {
        PDFSelection* searchSelection = [m_pdfLayerController searchSelection];
        [m_pdfLayerController findString:target caseSensitive:caseSensitive highlightMatches:YES];
        [m_pdfLayerController setSearchSelection:searchSelection];
        m_lastFoundString = emptyString();
        return false;
    }

    if (m_lastFoundString == target) {
        PDFSelection *selection = nextMatchForString(target, searchForward, caseSensitive, wrapSearch, [m_pdfLayerController searchSelection], NO);
        if (!selection)
            return false;

        [m_pdfLayerController setSearchSelection:selection];
        [m_pdfLayerController gotoSelection:selection];
    } else {
        [m_pdfLayerController findString:target caseSensitive:caseSensitive highlightMatches:YES];
        m_lastFoundString = target;
    }

    return matchCount > 0;
}

bool PDFPlugin::performDictionaryLookupAtLocation(const FloatPoint& point)
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

void PDFPlugin::notifySelectionChanged(PDFSelection *selection)
{
    webFrame()->page()->didChangeSelection();
}

String PDFPlugin::getSelectionString() const
{
    return [[m_pdfLayerController currentSelection] string];
}

String PDFPlugin::getSelectionForWordAtPoint(const FloatPoint& point) const
{
    IntPoint pointInView = convertFromPluginToPDFView(convertFromRootViewToPlugin(roundedIntPoint(point)));
    PDFSelection *selectionForWord = [m_pdfLayerController getSelectionForWordAtPoint:pointInView];
    [m_pdfLayerController setCurrentSelection:selectionForWord];
    
    return [selectionForWord string];
}

bool PDFPlugin::existingSelectionContainsPoint(const FloatPoint& locationInViewCoordinates) const
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

    CGPoint scrollOffset = CGPointZero;
    CGFloat scaleFactor = [pdfLayerController contentScaleFactor];

    scrollOffset.y = [pdfLayerController contentSizeRespectingZoom].height - NSRectToCGRect([pdfLayerController frame]).size.height - scrollOffset.y;
    
    CGPoint newPoint = CGPointMake(scrollOffset.x + point.x, scrollOffset.y + point.y);
    newPoint.x /= scaleFactor;
    newPoint.y /= scaleFactor;
    return NSPointFromCGPoint(newPoint);
}

String PDFPlugin::lookupTextAtLocation(const FloatPoint& locationInViewCoordinates, WebHitTestResultData& data, PDFSelection **selectionPtr, NSDictionary **options) const
{
    PDFSelection*& selection = *selectionPtr;

    selection = [m_pdfLayerController currentSelection];
    if (existingSelectionContainsPoint(locationInViewCoordinates))
        return selection.string;
        
    IntPoint pointInPDFView = convertFromPluginToPDFView(convertFromRootViewToPlugin(roundedIntPoint(locationInViewCoordinates)));
    selection = [m_pdfLayerController getSelectionForWordAtPoint:pointInPDFView];
    if (!selection)
        return "";

    NSPoint pointInLayoutSpace = pointInLayoutSpaceForPointInWindowSpace(m_pdfLayerController.get(), pointInPDFView);

    PDFPage *currentPage = [[m_pdfLayerController layout] pageNearestPoint:pointInLayoutSpace currentPage:[m_pdfLayerController currentPage]];
    
    NSPoint pointInPageSpace = [[m_pdfLayerController layout] convertPoint:pointInLayoutSpace toPage:currentPage forScaleFactor:1.0];
    
    for (PDFAnnotation *annotation in currentPage.annotations) {
        if (![annotation isKindOfClass:pdfAnnotationLinkClass()])
            continue;
    
        NSRect bounds = annotation.bounds;
        if (!NSPointInRect(pointInPageSpace, bounds))
            continue;
        
        PDFAnnotationLink *linkAnnotation = (PDFAnnotationLink *)annotation;
        NSURL *url = linkAnnotation.URL;
        if (!url)
            continue;
        
        data.absoluteLinkURL = url.absoluteString;
        data.linkLabel = selection.string;
        return selection.string;
    }
    
    NSString *lookupText = DictionaryLookup::stringForPDFSelection(selection, options);
    if (!lookupText || !lookupText.length)
        return "";

    [m_pdfLayerController setCurrentSelection:selection];
    return lookupText;
}

static NSRect rectInViewSpaceForRectInLayoutSpace(PDFLayerController* pdfLayerController, NSRect layoutSpaceRect)
{
    CGRect newRect = NSRectToCGRect(layoutSpaceRect);
    CGFloat scaleFactor = pdfLayerController.contentScaleFactor;
    CGPoint scrollOffset = CGPointZero;

    scrollOffset.y = pdfLayerController.contentSizeRespectingZoom.height - NSRectToCGRect(pdfLayerController.frame).size.height - scrollOffset.y;

    newRect.origin.x *= scaleFactor;
    newRect.origin.y *= scaleFactor;
    newRect.size.width *= scaleFactor;
    newRect.size.height *= scaleFactor;

    newRect.origin.x -= scrollOffset.x;
    newRect.origin.y -= scrollOffset.y;

    return NSRectFromCGRect(newRect);
}

FloatRect PDFPlugin::rectForSelectionInRootView(PDFSelection *selection) const
{
    PDFPage *currentPage = nil;
    NSArray* pages = selection.pages;
    if (pages.count)
        currentPage = (PDFPage *)[pages objectAtIndex:0];

    if (!currentPage)
        currentPage = [m_pdfLayerController currentPage];
    
    NSRect rectInPageSpace = [selection boundsForPage:currentPage];
    NSRect rectInLayoutSpace = [[m_pdfLayerController layout] convertRect:rectInPageSpace fromPage:currentPage forScaleFactor:1.0];
    NSRect rectInView = rectInViewSpaceForRectInLayoutSpace(m_pdfLayerController.get(), rectInLayoutSpace);

    rectInView.origin = convertFromPDFViewToRootView(IntPoint(rectInView.origin));

    return FloatRect(rectInView);
}

void PDFPlugin::performWebSearch(NSString *string)
{
    webFrame()->page()->send(Messages::WebPageProxy::SearchTheWeb(string));
}

void PDFPlugin::performSpotlightSearch(NSString *string)
{
    webFrame()->page()->send(Messages::WebPageProxy::SearchWithSpotlight(string));
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

NSObject *PDFPlugin::accessibilityObject() const
{
    return m_accessibilityObject.get();
}

void PDFPlugin::scrollToPoint(IntPoint scrollPoint)
{
    Frame* frame = pluginView()->frame();
    float scale = frame->page()->pageScaleFactor();
    scrollPoint.scale(scale, scale);
    frame->view()->scrollToOffsetWithoutAnimation(scrollPoint);
}

float PDFPlugin::scaleFactor() const
{
    Frame* frame = pluginView()->frame();
    return frame->page()->pageScaleFactor();
}

void PDFPlugin::zoomIn()
{
    if (webFrame()->isMainFrame() && isFullFramePlugin()) {
        WebPage* page = webFrame()->page();
        page->scalePage(page->pageScaleFactor() * zoomButtonScaleMultiplier, IntPoint());
    } else {
        [m_pdfLayerController setContentScaleFactor:[m_pdfLayerController contentScaleFactor] * zoomButtonScaleMultiplier];
        calculateSizes();
    }
}

void PDFPlugin::zoomOut()
{
    if (webFrame()->isMainFrame() && isFullFramePlugin()) {
        WebPage* page = webFrame()->page();
        page->scalePage(page->pageScaleFactor() / zoomButtonScaleMultiplier, IntPoint());
    } else {
        [m_pdfLayerController setContentScaleFactor:[m_pdfLayerController contentScaleFactor] / zoomButtonScaleMultiplier];
        calculateSizes();
    }
}

PDFPlugin::HUD::HUD(PDFPlugin& plugin)
    : m_overlay(PageOverlay::create(*this))
    , m_plugin(plugin)
{
    WebFrame* webFrame = plugin.webFrame();

    // In order to avoid the overlay lagging behind main-frame scrolling, we need
    // to force synchronous scrolling for the whole page if we have a HUD in a subframe PDF.
    m_overlay->setNeedsSynchronousScrolling(!webFrame->isMainFrame());

    MainFrame& mainFrame = webFrame->coreFrame()->mainFrame();
    mainFrame.pageOverlayController().installPageOverlay(m_overlay.ptr(), PageOverlay::FadeMode::DoNotFade);
    m_overlay->setNeedsDisplay();

    RefPtr<PlatformCALayer> platformCALayer = downcast<GraphicsLayerCA>(m_overlay->layer()).platformCALayer();
    platformCALayer->setOpacity(0);
}

PDFPlugin::HUD::~HUD()
{
    MainFrame& mainFrame = m_plugin.webFrame()->coreFrame()->mainFrame();
    mainFrame.pageOverlayController().uninstallPageOverlay(m_overlay.ptr(), PageOverlay::FadeMode::DoNotFade);
}

void PDFPlugin::HUD::pageOverlayDestroyed(PageOverlay&)
{
}

void PDFPlugin::HUD::willMoveToPage(PageOverlay&, Page* page)
{
}

void PDFPlugin::HUD::didMoveToPage(PageOverlay&, Page*)
{
}

IntRect PDFPlugin::HUD::frameInRootView() const
{
    // FIXME: These should come from PDFKit.
    const int HUDWidth = 236;
    const int HUDHeight = 72;
    const int HUDYOffset = 50;

    FrameView* frameView = m_plugin.webFrame()->coreFrame()->view();
    IntRect frameRectInRootView = frameView->convertToRootView(frameView->boundsRect());
    FloatRect HUDRect(frameRectInRootView.x() + (frameRectInRootView.width() / 2 - (HUDWidth / 2)), frameRectInRootView.y() + (frameRectInRootView.height() - HUDYOffset - HUDHeight), HUDWidth, HUDHeight);
    return enclosingIntRect(HUDRect);
}

bool PDFPlugin::HUD::containsPointInRootView(IntPoint point)
{
    if (m_plugin.isLocked())
        return false;

    return frameInRootView().contains(point);
}

void PDFPlugin::HUD::drawRect(PageOverlay&, GraphicsContext& context, const IntRect& dirtyRect)
{
    IntRect frameInRootView = this->frameInRootView();

    context.translate(IntSize(frameInRootView.x(), frameInRootView.y() + frameInRootView.height()));
    context.scale(FloatSize(1, -1));

    IntRect localRect(IntPoint(), frameInRootView.size());
    context.clip(localRect);

    [m_plugin.pdfLayerController() drawHUDInContext:context.platformContext()];
}

void PDFPlugin::HUD::invalidate()
{
    m_overlay->setNeedsDisplay();
}

void PDFPlugin::HUD::setVisible(bool visible, AnimateVisibilityTransition animateTransition)
{
    if (visible == m_visible)
        return;

    m_visible = visible;

    float toValue = m_visible ? 0.75 : 0;
    RefPtr<PlatformCALayer> platformCALayer = downcast<GraphicsLayerCA>(m_overlay->layer()).platformCALayer();

    if (animateTransition == AnimateVisibilityTransition::No) {
        platformCALayer->removeAnimationForKey("HUDFade");
        platformCALayer->setOpacity(toValue);
        return;
    }

    float duration = m_visible ? 0.25 : 0.5;

    RetainPtr<CABasicAnimation> fadeAnimation = [CABasicAnimation animationWithKeyPath:@"opacity"];
    [fadeAnimation setDuration:duration];
    [fadeAnimation setTimingFunction:[CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseInEaseOut]];
    [fadeAnimation setDelegate:[[[WKPDFHUDAnimationDelegate alloc] initWithAnimationCompletionHandler:[platformCALayer, toValue] (bool completed) {
        if (completed)
            platformCALayer->setOpacity(toValue);

    }] autorelease]];
    [fadeAnimation setFromValue:@(((CALayer *)platformCALayer->platformLayer().presentationLayer).opacity)];
    [fadeAnimation setToValue:@(toValue)];

    RefPtr<PlatformCAAnimation> animation = PlatformCAAnimationCocoa::create(fadeAnimation.get());
    platformCALayer->removeAnimationForKey("HUDFade");
    platformCALayer->addAnimationForKey("HUDFade", *animation);
}

bool PDFPlugin::HUD::mouseEvent(PageOverlay&, const PlatformMouseEvent& event)
{
    if (m_plugin.isLocked())
        return false;

    // We don't want the HUD to appear when dragging past it, or disappear when dragging from a button,
    // so don't update visibility while any mouse buttons are pressed.
    if (event.button() == MouseButton::NoButton)
        setVisible(containsPointInRootView(event.position()), AnimateVisibilityTransition::Yes);

    if (!m_visible)
        return false;

    if (event.button() != MouseButton::LeftButton)
        return false;

    IntPoint positionInRootViewCoordinates(event.position());
    IntRect HUDRectInRootViewCoordinates = frameInRootView();

    auto eventWithType = [&](NSEventType eventType) -> NSEvent * {
        return [NSEvent mouseEventWithType:eventType location:positionInRootViewCoordinates modifierFlags:0 timestamp:0 windowNumber:0 context:nil eventNumber:0 clickCount:event.clickCount() pressure:0];
    };

    auto pdfLayerController = m_plugin.pdfLayerController();

    switch (event.type()) {
    case PlatformEvent::MouseMoved:
        return [pdfLayerController mouseDragged:eventWithType(NSLeftMouseDragged) inHUDWithBounds:HUDRectInRootViewCoordinates];
    case PlatformEvent::MousePressed:
        return [pdfLayerController mouseDown:eventWithType(NSLeftMouseDown) inHUDWithBounds:HUDRectInRootViewCoordinates];
    case PlatformEvent::MouseReleased:
        return [pdfLayerController mouseUp:eventWithType(NSLeftMouseUp) inHUDWithBounds:HUDRectInRootViewCoordinates];
    default:
        break;
    }

    return false;
}

} // namespace WebKit

#endif // ENABLE(PDFKIT_PLUGIN)
