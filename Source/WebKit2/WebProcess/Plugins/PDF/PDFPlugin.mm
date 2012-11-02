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

#if ENABLE(PDFKIT_PLUGIN)

#import "config.h"
#import "PDFPlugin.h"

#import "PDFKitImports.h"
#import "PDFLayerControllerDetails.h"
#import "PDFPluginAnnotation.h"
#import "PluginView.h"
#import "ShareableBitmap.h"
#import "WebEvent.h"
#import "WebEventConversion.h"
#import <PDFKit/PDFKit.h>
#import <QuartzCore/QuartzCore.h>
#import <WebCore/ArchiveResource.h>
#import <WebCore/CSSPrimitiveValue.h>
#import <WebCore/CSSPropertyNames.h>
#import <WebCore/Chrome.h>
#import <WebCore/DocumentLoader.h>
#import <WebCore/FocusController.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameView.h>
#import <WebCore/GraphicsContext.h>
#import <WebCore/HTMLElement.h>
#import <WebCore/HTTPHeaderMap.h>
#import <WebCore/LocalizedStrings.h>
#import <WebCore/Page.h>
#import <WebCore/Pasteboard.h>
#import <WebCore/PluginData.h>
#import <WebCore/RenderBoxModelObject.h>
#import <WebCore/ScrollAnimator.h>
#import <WebCore/ScrollbarTheme.h>

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
"} "
".annotation { "
"    position: absolute; "
"    pointer-events: auto; "
"} "
"textarea.annotation { "
"    resize: none; "
"}";

@interface WKPDFPluginScrollbarLayer : CALayer
{
    WebKit::PDFPlugin* _pdfPlugin;
}

@property(assign) WebKit::PDFPlugin* pdfPlugin;

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
    // FIXME: Handle types other than plain text.

    for (NSUInteger i = 0, count = items.count; i < count; ++i) {
        NSString *type = [types objectAtIndex:i];
        if ([type isEqualToString:NSStringPboardType] || [type isEqualToString:NSPasteboardTypeString]) {
            RetainPtr<NSString> plainTextString(AdoptNS, [[NSString alloc] initWithData:[items objectAtIndex:i] encoding:NSUTF8StringEncoding]);
            Pasteboard::generalPasteboard()->writePlainText(plainTextString.get(), Pasteboard::CannotSmartReplace);
        }
    }
}

- (void)showDefinitionForAttributedString:(NSAttributedString *)string atPoint:(CGPoint)point
{
    // FIXME: Implement.
}

- (void)performWebSearch:(NSString *)string
{
    // FIXME: Implement.
}

- (void)openWithPreview
{
    // FIXME: Implement.
}

- (void)saveToPDF
{
    // FIXME: Implement.
}

- (void)pdfLayerController:(PDFLayerController *)pdfLayerController didChangeActiveAnnotation:(PDFAnnotation *)annotation
{
    _pdfPlugin->setActiveAnnotation(annotation);
}

@end

namespace WebKit {

using namespace HTMLNames;

PassRefPtr<PDFPlugin> PDFPlugin::create(WebFrame* frame)
{
    return adoptRef(new PDFPlugin(frame));
}

PDFPlugin::PDFPlugin(WebFrame* frame)
    : SimplePDFPlugin(frame)
    , m_containerLayer(AdoptNS, [[CALayer alloc] init])
    , m_contentLayer(AdoptNS, [[CALayer alloc] init])
    , m_scrollCornerLayer(AdoptNS, [[WKPDFPluginScrollbarLayer alloc] initWithPDFPlugin:this])
    , m_pdfLayerController(AdoptNS, [[pdfLayerControllerClass() alloc] init])
    , m_pdfLayerControllerDelegate(AdoptNS, [[WKPDFLayerControllerDelegate alloc] initWithPDFPlugin:this])
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

    [m_containerLayer.get() addSublayer:m_contentLayer.get()];
    [m_containerLayer.get() addSublayer:m_scrollCornerLayer.get()];
}

PDFPlugin::~PDFPlugin()
{
}

void PDFPlugin::updateScrollbars()
{
    SimplePDFPlugin::updateScrollbars();
    
    if (m_verticalScrollbarLayer) {
        m_verticalScrollbarLayer.get().frame = verticalScrollbar()->frameRect();
        [m_verticalScrollbarLayer.get() setNeedsDisplay];
    }
    
    if (m_horizontalScrollbarLayer) {
        m_horizontalScrollbarLayer.get().frame = horizontalScrollbar()->frameRect();
        [m_horizontalScrollbarLayer.get() setNeedsDisplay];
    }
    
    if (m_scrollCornerLayer) {
        m_scrollCornerLayer.get().frame = scrollCornerRect();
        [m_scrollCornerLayer.get() setNeedsDisplay];
    }
}

PassRefPtr<Scrollbar> PDFPlugin::createScrollbar(ScrollbarOrientation orientation)
{
    RefPtr<Scrollbar> widget = Scrollbar::createNativeScrollbar(this, orientation, RegularScrollbar);
    if (orientation == HorizontalScrollbar) {
        m_horizontalScrollbarLayer.adoptNS([[WKPDFPluginScrollbarLayer alloc] initWithPDFPlugin:this]);
        [m_containerLayer.get() addSublayer:m_horizontalScrollbarLayer.get()];
        
        didAddHorizontalScrollbar(widget.get());
    } else {
        m_verticalScrollbarLayer.adoptNS([[WKPDFPluginScrollbarLayer alloc] initWithPDFPlugin:this]);
        [m_containerLayer.get() addSublayer:m_verticalScrollbarLayer.get()];
        
        didAddVerticalScrollbar(widget.get());
    }
    pluginView()->frame()->view()->addChild(widget.get());
    return widget.release();
}

void PDFPlugin::destroyScrollbar(ScrollbarOrientation orientation)
{
    SimplePDFPlugin::destroyScrollbar(orientation);

    if (orientation == HorizontalScrollbar) {
        [m_horizontalScrollbarLayer.get() removeFromSuperlayer];
        m_horizontalScrollbarLayer = 0;
    } else {
        [m_verticalScrollbarLayer.get() removeFromSuperlayer];
        m_verticalScrollbarLayer = 0;
    }
}

void PDFPlugin::pdfDocumentDidLoad()
{
    addArchiveResource();
    
    RetainPtr<PDFDocument> document(AdoptNS, [[pdfDocumentClass() alloc] initWithData:(NSData *)data().get()]);

    setPDFDocument(document);

    [m_pdfLayerController.get() setFrameSize:size()];
    m_pdfLayerController.get().document = document.get();
    
    if (handlesPageScaleFactor())
        pluginView()->setPageScaleFactor([m_pdfLayerController.get() tileScaleFactor], IntPoint());

    notifyScrollPositionChanged(IntPoint([m_pdfLayerController.get() scrollPosition]));

    calculateSizes();
    updateScrollbars();

    controller()->invalidate(IntRect(IntPoint(), size()));
    
    runScriptsInPDFDocument();
}

void PDFPlugin::calculateSizes()
{
    // FIXME: This should come straight from PDFKit.
    computePageBoxes();

    setPDFDocumentSize(IntSize([m_pdfLayerController.get() contentSizeRespectingZoom]));
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
    
    [m_scrollCornerLayer.get() removeFromSuperlayer];
    [m_contentLayer.get() removeFromSuperlayer];
}

void PDFPlugin::paint(GraphicsContext* graphicsContext, const IntRect& dirtyRect)
{
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
    
    // FIXME: Support non-1 page/deviceScaleFactor.
    IntSize backingStoreSize = size();

    RefPtr<ShareableBitmap> bitmap = ShareableBitmap::createShareable(backingStoreSize, ShareableBitmap::SupportsAlpha);
    OwnPtr<GraphicsContext> context = bitmap->createGraphicsContext();

    context->scale(FloatSize(1, -1));
    context->translate(0, -size().height());

    [m_pdfLayerController.get() snapshotInContext:context->platformContext()];

    return bitmap.release();
}

PlatformLayer* PDFPlugin::pluginLayer()
{
    return m_containerLayer.get();
}

void PDFPlugin::geometryDidChange(const IntSize& pluginSize, const IntRect&, const AffineTransform& pluginToRootViewTransform)
{
    if (size() == pluginSize && pluginView()->pageScaleFactor() == [m_pdfLayerController.get() tileScaleFactor])
        return;

    setSize(pluginSize);
    m_rootViewToPluginTransform = pluginToRootViewTransform.inverse();
    [m_pdfLayerController.get() setFrameSize:pluginSize];

    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    CATransform3D transform = CATransform3DMakeScale(1, -1, 1);
    transform = CATransform3DTranslate(transform, 0, -pluginSize.height(), 0);
    
    if (handlesPageScaleFactor()) {
        CGFloat magnification = pluginView()->pageScaleFactor() - [m_pdfLayerController.get() tileScaleFactor];

        // FIXME: Instead of m_lastMousePoint, we should use the zoom origin from PluginView::setPageScaleFactor.
        if (magnification)
            [m_pdfLayerController.get() magnifyWithMagnification:magnification atPoint:m_lastMousePoint immediately:YES];
    }

    calculateSizes();
    updateScrollbars();

    if (m_activeAnnotation)
        m_activeAnnotation->updateGeometry();

    [m_contentLayer.get() setSublayerTransform:transform];
    [CATransaction commit];
}
    
static NSUInteger modifierFlagsFromWebEvent(const WebEvent& event)
{
    return (event.shiftKey() ? NSShiftKeyMask : 0)
        | (event.controlKey() ? NSControlKeyMask : 0)
        | (event.altKey() ? NSAlternateKeyMask : 0)
        | (event.metaKey() ? NSCommandKeyMask : 0);
}
    
static NSEventType eventTypeFromWebEvent(const WebEvent& event, bool mouseButtonIsDown)
{
    switch (event.type()) {
    case WebEvent::KeyDown:
        return NSKeyDown;
    case WebEvent::KeyUp:
        return NSKeyUp;

    case WebEvent::MouseDown:
        switch (static_cast<const WebMouseEvent&>(event).button()) {
        case WebMouseEvent::LeftButton:
            return NSLeftMouseDown;
        case WebMouseEvent::RightButton:
            return NSRightMouseDown;
        default:
            return 0;
        }
        break;
    case WebEvent::MouseUp:
        switch (static_cast<const WebMouseEvent&>(event).button()) {
        case WebMouseEvent::LeftButton:
            return NSLeftMouseUp;
        case WebMouseEvent::RightButton:
            return NSRightMouseUp;
        default:
            return 0;
        }
        break;
    case WebEvent::MouseMove:
        if (mouseButtonIsDown) {
            switch (static_cast<const WebMouseEvent&>(event).button()) {
            case WebMouseEvent::LeftButton:
                return NSLeftMouseDragged;
            case WebMouseEvent::RightButton:
                return NSRightMouseDragged;
            default:
                return 0;
            }
        } else
            return NSMouseMoved;
        break;

    default:
        return 0;
    }
}

bool PDFPlugin::handleMouseEvent(const WebMouseEvent& event)
{
    static bool mouseButtonIsDown;

    IntPoint mousePosition = event.position();

    // FIXME: Forward mouse events to the appropriate scrollbar.
    if (IntRect(m_verticalScrollbarLayer.get().frame).contains(mousePosition)
        || IntRect(m_horizontalScrollbarLayer.get().frame).contains(mousePosition)
        || IntRect(m_scrollCornerLayer.get().frame).contains(mousePosition))
        return false;

    IntPoint positionInPDFView(mousePosition);
    positionInPDFView = m_rootViewToPluginTransform.mapPoint(positionInPDFView);
    positionInPDFView.setY(size().height() - positionInPDFView.y());
    
    m_lastMousePoint = positionInPDFView;

    NSEventType eventType = eventTypeFromWebEvent(event, mouseButtonIsDown);

    if (!eventType)
        return false;

    NSUInteger modifierFlags = modifierFlagsFromWebEvent(event);

    NSEvent *fakeEvent = [NSEvent mouseEventWithType:eventType location:positionInPDFView modifierFlags:modifierFlags timestamp:0 windowNumber:0 context:nil eventNumber:0 clickCount:event.clickCount() pressure:0];

    switch (event.type()) {
    case WebEvent::MouseMove:
        if (mouseButtonIsDown)
            [m_pdfLayerController.get() mouseDragged:fakeEvent];
        else
            [m_pdfLayerController.get() mouseMoved:fakeEvent];
        mouseMovedInContentArea();
        return true;
    case WebEvent::MouseDown: {
        mouseButtonIsDown = true;
        [m_pdfLayerController.get() mouseDown:fakeEvent];
        return true;
    }
    case WebEvent::MouseUp: {
        [m_pdfLayerController.get() mouseUp:fakeEvent];
        mouseButtonIsDown = false;
        PlatformMouseEvent platformEvent = platform(event);
        return true;
    }
    default:
        break;
    }
        
    return false;
}

bool PDFPlugin::handleKeyboardEvent(const WebKeyboardEvent& event)
{
    NSEventType eventType = eventTypeFromWebEvent(event, false);
    NSUInteger modifierFlags = modifierFlagsFromWebEvent(event);
    
    NSEvent *fakeEvent = [NSEvent keyEventWithType:eventType location:NSZeroPoint modifierFlags:modifierFlags timestamp:0 windowNumber:0 context:0 characters:event.text() charactersIgnoringModifiers:event.unmodifiedText() isARepeat:event.isAutoRepeat() keyCode:event.nativeVirtualKeyCode()];
    
    switch (event.type()) {
    case WebEvent::KeyDown:
        return [m_pdfLayerController.get() keyDown:fakeEvent];
    default:
        return false;
    }
    
    return false;
}
    
bool PDFPlugin::handleEditingCommand(const String& commandName, const String& argument)
{
    if (commandName == "copy")
        [m_pdfLayerController.get() copySelection];
    else if (commandName == "selectAll")
        [m_pdfLayerController.get() selectAll];
    
    return true;
}

bool PDFPlugin::isEditingCommandEnabled(const String& commandName)
{
    if (commandName == "copy")
        return [m_pdfLayerController.get() currentSelection];
        
    if (commandName == "selectAll")
        return true;

    return false;
}

void PDFPlugin::setScrollOffset(const IntPoint& offset)
{
    SimplePDFPlugin::setScrollOffset(offset);

    [CATransaction begin];
    [m_pdfLayerController.get() setScrollPosition:offset];

    if (m_activeAnnotation)
        m_activeAnnotation->updateGeometry();

    [CATransaction commit];
}

void PDFPlugin::invalidateScrollbarRect(Scrollbar* scrollbar, const IntRect& rect)
{
    if (scrollbar == horizontalScrollbar())
        [m_horizontalScrollbarLayer.get() setNeedsDisplay];
    else if (scrollbar == verticalScrollbar())
        [m_verticalScrollbarLayer.get() setNeedsDisplay];
}

void PDFPlugin::invalidateScrollCornerRect(const IntRect& rect)
{
    [m_scrollCornerLayer.get() setNeedsDisplay];
}

bool PDFPlugin::handlesPageScaleFactor()
{
    return webFrame()->isMainFrame();
}

void PDFPlugin::setActiveAnnotation(PDFAnnotation *annotation)
{
    if (!supportsForms())
        return;

    if (m_activeAnnotation)
        m_activeAnnotation->commit();

    if (annotation) {
        m_activeAnnotation = PDFPluginAnnotation::create(annotation, m_pdfLayerController.get(), this);
        m_activeAnnotation->attach(m_annotationContainer.get());
    } else
        m_activeAnnotation = 0;
}

bool PDFPlugin::supportsForms()
{
    // FIXME: Should we support forms for inline PDFs? Since we touch the document, this might be difficult.
    return webFrame()->isMainFrame();
}

} // namespace WebKit

#endif // ENABLE(PDFKIT_PLUGIN)
