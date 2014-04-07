/*
 * Copyright (C) 2009, 2011, 2012 Apple Inc. All rights reserved.
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
#import "AttributedString.h"
#import "DataReference.h"
#import "DictionaryPopupInfo.h"
#import "PDFAnnotationTextWidgetDetails.h"
#import "PDFKitImports.h"
#import "PDFLayerControllerDetails.h"
#import "PDFPluginAnnotation.h"
#import "PDFPluginPasswordField.h"
#import "PluginView.h"
#import "WKAccessibilityWebPageObjectMac.h"
#import "WKPageFindMatchesClient.h"
#import "WebContextMessages.h"
#import "WebCoreArgumentCoders.h"
#import "WebEvent.h"
#import "WebEventConversion.h"
#import "WebPage.h"
#import "WebPageProxyMessages.h"
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
#import <WebCore/DocumentLoader.h>
#import <WebCore/FocusController.h>
#import <WebCore/FormState.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameLoader.h>
#import <WebCore/FrameView.h>
#import <WebCore/GraphicsContext.h>
#import <WebCore/HTMLElement.h>
#import <WebCore/HTMLFormElement.h>
#import <WebCore/LocalizedStrings.h>
#import <WebCore/MouseEvent.h>
#import <WebCore/Page.h>
#import <WebCore/Pasteboard.h>
#import <WebCore/PluginData.h>
#import <WebCore/PluginDocument.h>
#import <WebCore/RenderBoxModelObject.h>
#import <WebCore/ScrollbarTheme.h>
#import <WebCore/UUID.h>
#import <WebKitSystemInterface.h>
#import <wtf/CurrentTime.h>

using namespace WebCore;

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

@interface WKPDFPluginAccessibilityObject : NSObject
{
    PDFLayerController *_pdfLayerController;
    NSObject *_parent;
    WebKit::PDFPlugin* _pdfPlugin;
}

@property(assign) PDFLayerController *pdfLayerController;
@property(assign) NSObject *parent;
@property(assign) WebKit::PDFPlugin* pdfPlugin;

- (id)initWithPDFPlugin:(WebKit::PDFPlugin *)plugin;

@end

@implementation WKPDFPluginAccessibilityObject

@synthesize pdfLayerController = _pdfLayerController;
@synthesize parent = _parent;
@synthesize pdfPlugin = _pdfPlugin;

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
    else if ([attribute isEqualToString:NSAccessibilityValueAttribute])
        return [_pdfLayerController accessibilityValueAttribute];
    else if ([attribute isEqualToString:NSAccessibilitySelectedTextAttribute])
        return [_pdfLayerController accessibilitySelectedTextAttribute];
    else if ([attribute isEqualToString:NSAccessibilitySelectedTextRangeAttribute])
        return [_pdfLayerController accessibilitySelectedTextRangeAttribute];
    else if ([attribute isEqualToString:NSAccessibilityNumberOfCharactersAttribute])
        return [_pdfLayerController accessibilityNumberOfCharactersAttribute];
    else if ([attribute isEqualToString:NSAccessibilityVisibleCharacterRangeAttribute])
        return [_pdfLayerController accessibilityVisibleCharacterRangeAttribute];
    else if ([attribute isEqualToString:NSAccessibilityTopLevelUIElementAttribute])
        return [_parent accessibilityAttributeValue:NSAccessibilityTopLevelUIElementAttribute];
    else if ([attribute isEqualToString:NSAccessibilityRoleAttribute])
        return [_pdfLayerController accessibilityRoleAttribute];
    else if ([attribute isEqualToString:NSAccessibilityRoleDescriptionAttribute])
        return [_pdfLayerController accessibilityRoleDescriptionAttribute];
    else if ([attribute isEqualToString:NSAccessibilityWindowAttribute])
        return [_parent accessibilityAttributeValue:NSAccessibilityWindowAttribute];
    else if ([attribute isEqualToString:NSAccessibilitySizeAttribute])
        return [NSValue valueWithSize:_pdfPlugin->boundsOnScreen().size()];
    else if ([attribute isEqualToString:NSAccessibilityFocusedAttribute])
        return [_parent accessibilityAttributeValue:NSAccessibilityFocusedAttribute];
    else if ([attribute isEqualToString:NSAccessibilityEnabledAttribute])
        return [_parent accessibilityAttributeValue:NSAccessibilityEnabledAttribute];
    else if ([attribute isEqualToString:NSAccessibilityPositionAttribute])
        return [NSValue valueWithPoint:_pdfPlugin->boundsOnScreen().location()];

    return 0;
}

- (id)accessibilityAttributeValue:(NSString *)attribute forParameter:(id)parameter
{
    if ([attribute isEqualToString:NSAccessibilityBoundsForRangeParameterizedAttribute]) {
        NSRect boundsInPDFViewCoordinates = [[_pdfLayerController accessibilityBoundsForRangeAttributeForParameter:parameter] rectValue];
        NSRect boundsInScreenCoordinates = _pdfPlugin->convertFromPDFViewToScreen(boundsInPDFViewCoordinates);
        return [NSValue valueWithRect:boundsInScreenCoordinates];;
    } else if ([attribute isEqualToString:NSAccessibilityLineForIndexParameterizedAttribute])
        return [_pdfLayerController accessibilityLineForIndexAttributeForParameter:parameter];
    else if ([attribute isEqualToString:NSAccessibilityRangeForLineParameterizedAttribute])
        return [_pdfLayerController accessibilityRangeForLineAttributeForParameter:parameter];
    else if ([attribute isEqualToString:NSAccessibilityStringForRangeParameterizedAttribute])
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

    if (!attributeNames)
        attributeNames = [[NSArray arrayWithObjects:NSAccessibilityValueAttribute,
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
            NSAccessibilityPositionAttribute,
            nil] retain];

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


@interface WKPDFPluginScrollbarLayer : CALayer
{
    WebKit::PDFPlugin* _pdfPlugin;
}

@property (assign) WebKit::PDFPlugin* pdfPlugin;

@end

@implementation WKPDFPluginScrollbarLayer

@synthesize pdfPlugin = _pdfPlugin;

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

@interface WKPDFLayerControllerDelegate : NSObject<PDFLayerControllerDelegate>
{
    WebKit::PDFPlugin* _pdfPlugin;
}

@property(assign) WebKit::PDFPlugin* pdfPlugin;

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
    _pdfPlugin->notifyScrollPositionChanged(IntPoint(newPosition));
}

- (void)writeItemsToPasteboard:(NSArray *)items withTypes:(NSArray *)types
{
    _pdfPlugin->writeItemsToPasteboard(NSGeneralPboard, items, types);
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

PassRefPtr<PDFPlugin> PDFPlugin::create(WebFrame* frame)
{
    return adoptRef(new PDFPlugin(frame));
}

PDFPlugin::PDFPlugin(WebFrame* frame)
    : m_frame(frame)
    , m_isPostScript(false)
    , m_pdfDocumentWasMutated(false)
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
        m_annotationContainer->setAttribute(idAttr, "annotationContainer");

        RefPtr<Element> m_annotationStyle = document->createElement(styleTag, false);
        m_annotationStyle->setTextContent(annotationStyle, ASSERT_NO_EXCEPTION);

        m_annotationContainer->appendChild(m_annotationStyle.get());
        document->body()->appendChild(m_annotationContainer.get());
    }

    m_accessibilityObject = adoptNS([[WKPDFPluginAccessibilityObject alloc] initWithPDFPlugin:this]);
    m_accessibilityObject.get().pdfLayerController = m_pdfLayerController.get();
    m_accessibilityObject.get().parent = webFrame()->page()->accessibilityRemoteObject();

    [m_containerLayer addSublayer:m_contentLayer.get()];
    [m_containerLayer addSublayer:m_scrollCornerLayer.get()];
}

PDFPlugin::~PDFPlugin()
{
}

PluginInfo PDFPlugin::pluginInfo()
{
    PluginInfo info;
    info.name = builtInPDFPluginName();
    info.isApplicationPlugin = true;

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

    int horizontalScrollbarHeight = (m_horizontalScrollbar && !m_horizontalScrollbar->isOverlayScrollbar()) ? m_horizontalScrollbar->height() : 0;
    int verticalScrollbarWidth = (m_verticalScrollbar && !m_verticalScrollbar->isOverlayScrollbar()) ? m_verticalScrollbar->width() : 0;

    int pageStep = m_pageBoxes.isEmpty() ? 0 : m_pageBoxes[0].height();

    if (m_horizontalScrollbar) {
        m_horizontalScrollbar->setSteps(Scrollbar::pixelsPerLineStep(), pageStep);
        m_horizontalScrollbar->setProportion(m_size.width() - verticalScrollbarWidth, m_pdfDocumentSize.width());
        IntRect scrollbarRect(pluginView()->x(), pluginView()->y() + m_size.height() - m_horizontalScrollbar->height(), m_size.width(), m_horizontalScrollbar->height());
        if (m_verticalScrollbar)
            scrollbarRect.contract(m_verticalScrollbar->width(), 0);
        m_horizontalScrollbar->setFrameRect(scrollbarRect);
    }
    if (m_verticalScrollbar) {
        m_verticalScrollbar->setSteps(Scrollbar::pixelsPerLineStep(), pageStep);
        m_verticalScrollbar->setProportion(m_size.height() - horizontalScrollbarHeight, m_pdfDocumentSize.height());
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

        frameView->setNeedsLayout();
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

PassRefPtr<Scrollbar> PDFPlugin::createScrollbar(ScrollbarOrientation orientation)
{
    RefPtr<Scrollbar> widget = Scrollbar::createNativeScrollbar(this, orientation, RegularScrollbar);
    if (orientation == HorizontalScrollbar) {
        m_horizontalScrollbarLayer = adoptNS([[WKPDFPluginScrollbarLayer alloc] initWithPDFPlugin:this]);
        [m_containerLayer addSublayer:m_horizontalScrollbarLayer.get()];
    } else {
        m_verticalScrollbarLayer = adoptNS([[WKPDFPluginScrollbarLayer alloc] initWithPDFPlugin:this]);
        [m_containerLayer addSublayer:m_verticalScrollbarLayer.get()];
    }
    didAddScrollbar(widget.get(), orientation);
    pluginView()->frame()->view()->addChild(widget.get());
    return widget.release();
}

void PDFPlugin::destroyScrollbar(ScrollbarOrientation orientation)
{
    RefPtr<Scrollbar>& scrollbar = orientation == HorizontalScrollbar ? m_horizontalScrollbar : m_verticalScrollbar;
    if (!scrollbar)
        return;

    willRemoveScrollbar(scrollbar.get(), orientation);
    scrollbar->removeFromParent();
    scrollbar->disconnectFromScrollableArea();
    scrollbar = 0;

    if (orientation == HorizontalScrollbar) {
        [m_horizontalScrollbarLayer removeFromSuperlayer];
        m_horizontalScrollbarLayer = 0;
    } else {
        [m_verticalScrollbarLayer removeFromSuperlayer];
        m_verticalScrollbarLayer = 0;
    }
}

IntRect PDFPlugin::convertFromScrollbarToContainingView(const Scrollbar* scrollbar, const IntRect& scrollbarRect) const
{
    IntRect rect = scrollbarRect;
    rect.move(scrollbar->location() - pluginView()->location());

    return pluginView()->frame()->view()->convertFromRenderer(pluginView()->renderer(), rect);
}

IntRect PDFPlugin::convertFromContainingViewToScrollbar(const Scrollbar* scrollbar, const IntRect& parentRect) const
{
    IntRect rect = pluginView()->frame()->view()->convertToRenderer(pluginView()->renderer(), parentRect);
    rect.move(pluginView()->location() - scrollbar->location());

    return rect;
}

IntPoint PDFPlugin::convertFromScrollbarToContainingView(const Scrollbar* scrollbar, const IntPoint& scrollbarPoint) const
{
    IntPoint point = scrollbarPoint;
    point.move(scrollbar->location() - pluginView()->location());

    return pluginView()->frame()->view()->convertFromRenderer(pluginView()->renderer(), point);
}

IntPoint PDFPlugin::convertFromContainingViewToScrollbar(const Scrollbar* scrollbar, const IntPoint& parentPoint) const
{
    IntPoint point = pluginView()->frame()->view()->convertToRenderer(pluginView()->renderer(), parentPoint);
    point.move(pluginView()->location() - scrollbar->location());
    
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
    return 0;
}

IntRect PDFPlugin::scrollableAreaBoundingBox() const
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

int PDFPlugin::scrollPosition(Scrollbar* scrollbar) const
{
    if (scrollbar->orientation() == HorizontalScrollbar)
        return m_scrollOffset.width();
    if (scrollbar->orientation() == VerticalScrollbar)
        return m_scrollOffset.height();
    ASSERT_NOT_REACHED();
    return 0;
}

IntPoint PDFPlugin::scrollPosition() const
{
    return IntPoint(m_scrollOffset.width(), m_scrollOffset.height());
}

IntPoint PDFPlugin::minimumScrollPosition() const
{
    return IntPoint();
}

IntPoint PDFPlugin::maximumScrollPosition() const
{
    int horizontalScrollbarHeight = (m_horizontalScrollbar && !m_horizontalScrollbar->isOverlayScrollbar()) ? m_horizontalScrollbar->height() : 0;
    int verticalScrollbarWidth = (m_verticalScrollbar && !m_verticalScrollbar->isOverlayScrollbar()) ? m_verticalScrollbar->width() : 0;

    IntPoint maximumOffset(m_pdfDocumentSize.width() - m_size.width() + verticalScrollbarWidth, m_pdfDocumentSize.height() - m_size.height() + horizontalScrollbarHeight);
    maximumOffset.clampNegativeToZero();
    return maximumOffset;
}

void PDFPlugin::scrollbarStyleChanged(int, bool forceUpdate)
{
    if (!forceUpdate)
        return;

    // If the PDF was scrolled all the way to bottom right and scrollbars change to overlay style, we don't want to display white rectangles where scrollbars were.
    IntPoint newScrollOffset = IntPoint(m_scrollOffset).shrunkTo(maximumScrollPosition());
    setScrollOffset(newScrollOffset);
    
    // As size of the content area changes, scrollbars may need to appear or to disappear.
    updateScrollbars();
    
    ScrollableArea::contentsResized();
}

void PDFPlugin::addArchiveResource()
{
    // FIXME: It's a hack to force add a resource to DocumentLoader. PDF documents should just be fetched as CachedResources.

    // Add just enough data for context menu handling and web archives to work.
    ResourceResponse synthesizedResponse;
    synthesizedResponse.setSuggestedFilename(m_suggestedFilename);
    synthesizedResponse.setURL(m_sourceURL); // Needs to match the HitTestResult::absolutePDFURL.
    synthesizedResponse.setMimeType("application/pdf");

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
}

void PDFPlugin::streamDidReceiveResponse(uint64_t streamID, const URL&, uint32_t, uint32_t, const String& mimeType, const String&, const String& suggestedFilename)
{
    ASSERT_UNUSED(streamID, streamID == pdfDocumentRequestID);

    m_suggestedFilename = suggestedFilename;

    if (equalIgnoringCase(mimeType, postScriptMIMEType))
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

    if (equalIgnoringCase(mimeType, postScriptMIMEType))
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

void PDFPlugin::createPasswordEntryForm()
{
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

    [m_pdfLayerController setDeviceScaleFactor:newScaleFactor];
}

void PDFPlugin::contentsScaleFactorChanged(float)
{
    updatePageAndDeviceScaleFactors();
}

void PDFPlugin::computePageBoxes()
{
    size_t pageCount = CGPDFDocumentGetNumberOfPages([m_pdfDocument documentRef]);
    for (size_t i = 0; i < pageCount; ++i) {
        CGPDFPageRef pdfPage = CGPDFDocumentGetPage([m_pdfDocument documentRef], i + 1);
        ASSERT(pdfPage);

        CGRect box = CGPDFPageGetBoxRect(pdfPage, kCGPDFCropBox);
        if (CGRectIsEmpty(box))
            box = CGPDFPageGetBoxRect(pdfPage, kCGPDFMediaBox);
        m_pageBoxes.append(IntRect(box));
    }
}

void PDFPlugin::calculateSizes()
{
    if ([pdfDocument() isLocked]) {
        setPDFDocumentSize(IntSize(0, 0));
        return;
    }

    // FIXME: This should come straight from PDFKit.
    computePageBoxes();

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

void PDFPlugin::destroy()
{
    m_pdfLayerController.get().delegate = 0;

    if (webFrame()) {
        if (FrameView* frameView = webFrame()->coreFrame()->view())
            frameView->removeScrollableArea(this);
    }

    m_activeAnnotation = 0;
    m_annotationContainer = 0;

    destroyScrollbar(HorizontalScrollbar);
    destroyScrollbar(VerticalScrollbar);
    
    [m_scrollCornerLayer removeFromSuperlayer];
    [m_contentLayer removeFromSuperlayer];
}

void PDFPlugin::updateControlTints(GraphicsContext* graphicsContext)
{
    ASSERT(graphicsContext->updatingControlTints());

    if (m_horizontalScrollbar)
        m_horizontalScrollbar->invalidate();
    if (m_verticalScrollbar)
        m_verticalScrollbar->invalidate();
    invalidateScrollCorner(scrollCornerRect());
}

void PDFPlugin::paintControlForLayerInContext(CALayer *layer, CGContextRef context)
{
    GraphicsContext graphicsContext(context);
    GraphicsContextStateSaver stateSaver(graphicsContext);
    
    graphicsContext.setIsCALayerContext(true);
    
    if (layer == m_scrollCornerLayer) {
        IntRect scrollCornerRect = this->scrollCornerRect();
        graphicsContext.translate(-scrollCornerRect.x(), -scrollCornerRect.y());
        ScrollbarTheme::theme()->paintScrollCorner(0, &graphicsContext, scrollCornerRect);
        return;
    }
    
    Scrollbar* scrollbar = 0;
    
    if (layer == m_verticalScrollbarLayer)
        scrollbar = verticalScrollbar();
    else if (layer == m_horizontalScrollbarLayer)
        scrollbar = horizontalScrollbar();

    if (!scrollbar)
        return;
    
    graphicsContext.translate(-scrollbar->x(), -scrollbar->y());
    scrollbar->paint(&graphicsContext, scrollbar->frameRect());
}

PassRefPtr<ShareableBitmap> PDFPlugin::snapshot()
{
    if (size().isEmpty())
        return 0;

    float contentsScaleFactor = controller()->contentsScaleFactor();
    IntSize backingStoreSize = size();
    backingStoreSize.scale(contentsScaleFactor);

    RefPtr<ShareableBitmap> bitmap = ShareableBitmap::createShareable(backingStoreSize, ShareableBitmap::SupportsAlpha);
    auto context = bitmap->createGraphicsContext();

    context->scale(FloatSize(contentsScaleFactor, -contentsScaleFactor));
    context->translate(0, -size().height());

    [m_pdfLayerController snapshotInContext:context->platformContext()];

    return bitmap.release();
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
    return m_rootViewToPluginTransform.inverse().mapPoint(pointInPluginCoordinates);
}

FloatRect PDFPlugin::convertFromPDFViewToScreen(const FloatRect& rect) const
{
    FrameView* frameView = webFrame()->coreFrame()->view();

    if (!frameView)
        return FloatRect();

    FloatPoint originInPluginCoordinates(rect.x(), size().height() - rect.y() - rect.height());
    FloatRect rectInRootViewCoordinates = m_rootViewToPluginTransform.inverse().mapRect(FloatRect(originInPluginCoordinates, rect.size()));

    return frameView->contentsToScreen(enclosingIntRect(rectInRootViewCoordinates));
}

IntRect PDFPlugin::boundsOnScreen() const
{
    FrameView* frameView = webFrame()->coreFrame()->view();

    if (!frameView)
        return IntRect();

    FloatRect bounds = FloatRect(FloatPoint(), size());
    FloatRect rectInRootViewCoordinates = m_rootViewToPluginTransform.inverse().mapRect(bounds);
    return frameView->contentsToScreen(enclosingIntRect(rectInRootViewCoordinates));
}

void PDFPlugin::geometryDidChange(const IntSize& pluginSize, const IntRect&, const AffineTransform& pluginToRootViewTransform)
{
    if (size() == pluginSize && pluginView()->pageScaleFactor() == [m_pdfLayerController contentScaleFactor])
        return;

    m_size = pluginSize;
    m_rootViewToPluginTransform = pluginToRootViewTransform.inverse();
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
    return (event.shiftKey() ? NSShiftKeyMask : 0)
        | (event.controlKey() ? NSControlKeyMask : 0)
        | (event.altKey() ? NSAlternateKeyMask : 0)
        | (event.metaKey() ? NSCommandKeyMask : 0);
}
    
static bool getEventTypeFromWebEvent(const WebEvent& event, NSEventType& eventType)
{
    switch (event.type()) {
    case WebEvent::KeyDown:
        eventType = NSKeyDown;
        return true;
    case WebEvent::KeyUp:
        eventType = NSKeyUp;
        return true;
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
        return 0;

    NSUInteger modifierFlags = modifierFlagsFromWebEvent(event);

    return [NSEvent mouseEventWithType:eventType location:positionInPDFViewCoordinates modifierFlags:modifierFlags timestamp:0 windowNumber:0 context:nil eventNumber:0 clickCount:event.clickCount() pressure:0];
}

void PDFPlugin::updateCursor(const WebMouseEvent& event, UpdateCursorMode mode)
{
    HitTestResult hitTestResult = None;

    PDFSelection *selectionUnderMouse = [m_pdfLayerController getSelectionForWordAtPoint:convertFromPluginToPDFView(event.position())];
    if (selectionUnderMouse && [[selectionUnderMouse string] length])
        hitTestResult = Text;

    if (hitTestResult == m_lastHitTestResult && mode == UpdateIfNeeded)
        return;

    webFrame()->page()->send(Messages::WebPageProxy::SetCursor(hitTestResult == Text ? iBeamCursor() : pointerCursor()));
    m_lastHitTestResult = hitTestResult;
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

    if ([pdfDocument() isLocked])
        return false;

    // Right-clicks and Control-clicks always call handleContextMenuEvent as well.
    if (event.button() == WebMouseEvent::RightButton || (event.button() == WebMouseEvent::LeftButton && event.controlKey()))
        return true;

    NSEvent *nsEvent = nsEventForWebMouseEvent(event);

    switch (event.type()) {
    case WebEvent::MouseMove:
        mouseMovedInContentArea();
        updateCursor(event);

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
    default:
        break;
    }

    return false;
}

bool PDFPlugin::handleMouseEnterEvent(const WebMouseEvent& event)
{
    mouseEnteredContentArea();
    updateCursor(event, ForceUpdate);
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
    WebMouseEvent event(WebEvent::MouseDown, WebMouseEvent::RightButton, contentsPoint, contentsPoint, 0, 0, 0, 1, static_cast<WebEvent::Modifiers>(0), monotonicallyIncreasingTime());
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

void PDFPlugin::setScrollOffset(const IntPoint& offset)
{
    m_scrollOffset = IntSize(offset.x(), offset.y());

    [CATransaction begin];
    [m_pdfLayerController setScrollPosition:offset];

    if (m_activeAnnotation)
        m_activeAnnotation->updateGeometry();

    [CATransaction commit];
}

void PDFPlugin::invalidateScrollbarRect(Scrollbar* scrollbar, const IntRect& rect)
{
    if (scrollbar == horizontalScrollbar())
        [m_horizontalScrollbarLayer setNeedsDisplay];
    else if (scrollbar == verticalScrollbar())
        [m_verticalScrollbarLayer setNeedsDisplay];
}

void PDFPlugin::invalidateScrollCornerRect(const IntRect& rect)
{
    [m_scrollCornerLayer setNeedsDisplay];
}

bool PDFPlugin::isFullFramePlugin()
{
    // <object> or <embed> plugins will appear to be in their parent frame, so we have to
    // check whether our frame's widget is exactly our PluginView.
    Document* document = webFrame()->coreFrame()->document();
    return document->isPluginDocument() && static_cast<PluginDocument*>(document)->pluginWidget() == pluginView();
}

bool PDFPlugin::handlesPageScaleFactor()
{
    return webFrame()->isMainFrame() && isFullFramePlugin();
}

void PDFPlugin::clickedLink(NSURL *url)
{
    Frame* frame = webFrame()->coreFrame();

    RefPtr<Event> coreEvent;
    if (m_lastMouseEvent.type() != WebEvent::NoType)
        coreEvent = MouseEvent::create(eventNames().clickEvent, frame->document()->defaultView(), platform(m_lastMouseEvent), 0, 0);

    frame->loader().urlSelected(url, emptyString(), coreEvent.get(), LockHistory::No, LockBackForwardList::No, MaybeSendReferrer);
}

void PDFPlugin::setActiveAnnotation(PDFAnnotation *annotation)
{
    if (!supportsForms())
        return;

    if (m_activeAnnotation)
        m_activeAnnotation->commit();

    if (annotation) {
        if ([annotation isKindOfClass:pdfAnnotationTextWidgetClass()] && static_cast<PDFAnnotationTextWidget *>(annotation).isReadOnly) {
            m_activeAnnotation = 0;
            return;
        }

        m_activeAnnotation = PDFPluginAnnotation::create(annotation, m_pdfLayerController.get(), this);
        m_activeAnnotation->attach(m_annotationContainer.get());
    } else
        m_activeAnnotation = 0;
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

PassRefPtr<SharedBuffer> PDFPlugin::liveResourceData() const
{
    NSData *pdfData = liveData();

    if (!pdfData)
        return 0;

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

        m_temporaryPDFUUID = WebCore::createCanonicalUUIDString();
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
    WebProcess::shared().parentProcessConnection()->sendSync(Messages::WebContext::SetPasteboardTypes(pasteboardName, pasteboardTypes),
        Messages::WebContext::SetPasteboardTypes::Reply(newChangeCount), 0);

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
            WebProcess::shared().parentProcessConnection()->sendSync(Messages::WebContext::SetPasteboardStringForType(pasteboardName, type, plainTextString.get()),
                Messages::WebContext::SetPasteboardStringForType::Reply(newChangeCount), 0);
        } else {
            RefPtr<SharedBuffer> buffer = SharedBuffer::wrapNSData(data);

            if (!buffer)
                continue;

            SharedMemory::Handle handle;
            RefPtr<SharedMemory> sharedMemory = SharedMemory::create(buffer->size());
            memcpy(sharedMemory->data(), buffer->data(), buffer->size());
            sharedMemory->createHandle(handle, SharedMemory::ReadOnly);
            WebProcess::shared().parentProcessConnection()->sendSync(Messages::WebContext::SetPasteboardBufferForType(pasteboardName, type, handle, buffer->size()),
                Messages::WebContext::SetPasteboardBufferForType::Reply(newChangeCount), 0);
        }
    }
}

void PDFPlugin::showDefinitionForAttributedString(NSAttributedString *string, CGPoint point)
{
    DictionaryPopupInfo dictionaryPopupInfo;
    dictionaryPopupInfo.origin = convertFromPDFViewToRootView(IntPoint(point));

    AttributedString attributedString;
    attributedString.string = string;

    webFrame()->page()->send(Messages::WebPageProxy::DidPerformDictionaryLookup(attributedString, dictionaryPopupInfo));
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
        // Initially we want to include the selected text in the search.  So we must modify the starting search
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

bool PDFPlugin::performDictionaryLookupAtLocation(const WebCore::FloatPoint& point)
{
    PDFSelection* lookupSelection = [m_pdfLayerController getSelectionForWordAtPoint:convertFromPluginToPDFView(roundedIntPoint(point))];

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

String PDFPlugin::getSelectionString() const
{
    return [[m_pdfLayerController currentSelection] string];
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
    bool inFirstPage = currentPageIndex == 0;
    bool inLastPage = [m_pdfLayerController lastPageIndex] == currentPageIndex;

    bool atScrollTop = scrollPosition().y() == 0;
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
    } else if (atScrollTop && !inFirstPage && event.delta().height() > 0) {
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
    else
        return rawData();
}

NSObject *PDFPlugin::accessibilityObject() const
{
    return m_accessibilityObject.get();
}

} // namespace WebKit

#endif // ENABLE(PDFKIT_PLUGIN)
