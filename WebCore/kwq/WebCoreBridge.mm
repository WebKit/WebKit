/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "WebCoreBridge.h"

#import "csshelper.h"
#import "dom_node.h"
#import "dom_docimpl.h"
#import "dom_nodeimpl.h"
#import "html_formimpl.h"
#import "html_documentimpl.h"
#import "htmlattrs.h"
#import "htmltags.h"
#import "khtml_part.h"
#import "khtmlview.h"
#import "loader.h"
#import "render_frames.h"
#import "render_image.h"
#import "render_object.h"
#import "render_canvas.h"
#import "render_style.h"
#import "render_replaced.h"
using khtml::RenderWidget;

#import <JavaScriptCore/property_map.h>

#import "KWQAssertions.h"
#import "KWQCharsets.h"
#import "KWQDOMNode.h"
#import "KWQFont.h"
#import "KWQFrame.h"
#import "KWQLoader.h"
#import "KWQPageState.h"
#import "KWQRenderTreeDebug.h"
#import "KWQView.h"
#import "KWQPrinter.h"

#import "WebCoreDOMPrivate.h"
#import "WebCoreImageRenderer.h"
#import "WebCoreTextRendererFactory.h"
#import "WebCoreSettings.h"

#import <AppKit/NSView.h>

using DOM::DocumentImpl;
using DOM::Node;
using DOM::NodeImpl;

using khtml::Decoder;
using khtml::parseURL;
using khtml::RenderImage;
using khtml::RenderObject;
using khtml::RenderPart;
using khtml::RenderStyle;
using khtml::RenderCanvas;

using KJS::SavedProperties;

using KParts::URLArgs;

NSString *WebCoreElementFrameKey = 		@"WebElementFrame";
NSString *WebCoreElementImageAltStringKey = 	@"WebElementImageAltString";
NSString *WebCoreElementImageKey = 		@"WebElementImage";
NSString *WebCoreElementImageRectKey = 		@"WebElementImageRect";
NSString *WebCoreElementImageURLKey = 		@"WebElementImageURL";
NSString *WebCoreElementIsSelectedKey = 	@"WebElementIsSelected";
NSString *WebCoreElementLinkURLKey = 		@"WebElementLinkURL";
NSString *WebCoreElementLinkTargetFrameKey =	@"WebElementTargetFrame";
NSString *WebCoreElementLinkLabelKey = 		@"WebElementLinkLabel";
NSString *WebCoreElementLinkTitleKey = 		@"WebElementLinkTitle";
NSString *WebCoreElementNameKey = 		@"WebElementName";

NSString *WebCorePageCacheStateKey =               @"WebCorePageCacheState";

@implementation WebCoreBridge

static bool initializedObjectCacheSize = FALSE;

- init
{
    [super init];
    
    _part = new KWQKHTMLPart;
    _part->setBridge(self);

    if (!initializedObjectCacheSize){
        khtml::Cache::setSize([self getObjectCacheSize]);
        initializedObjectCacheSize = TRUE;
    }
    
    _shouldCreateRenderers = YES;
    
    return self;
}

- (void)initializeSettings: (WebCoreSettings *)settings
{
    _part->setSettings ([settings settings]);
}

- (void)dealloc
{
    [self removeFromFrame];
    
    if (_renderPart) {
        _renderPart->deref(_renderPartArena);
    }
    _part->setBridge(nil);
    _part->deref();
        
    [super dealloc];
}

- (KWQKHTMLPart *)part
{
    return _part;
}

- (void)setRenderPart:(KHTMLRenderPart *)newPart;
{
    RenderArena *arena = newPart->ref();
    if (_renderPart) {
        _renderPart->deref(_renderPartArena);
    }
    _renderPart = newPart;
    _renderPartArena = arena;
}

- (KHTMLRenderPart *)renderPart
{
    return _renderPart;
}

- (void)setParent:(WebCoreBridge *)parent
{
    _part->setParent([parent part]);
}

- (void)openURL:(NSString *)URL reload:(BOOL)reload contentType:(NSString *)contentType refresh:(NSString *)refresh lastModified:(NSDate *)lastModified pageCache:(NSDictionary *)pageCache
{
    if (pageCache) {
        KWQPageState *state = [pageCache objectForKey:WebCorePageCacheStateKey];
        _part->openURLFromPageCache(state);
        [state invalidate];
        return;
    }
        
    // arguments
    URLArgs args(_part->browserExtension()->urlArgs());
    args.reload = reload;
    if (contentType) {
        args.serviceType = QString::fromNSString(contentType);
    }
    _part->browserExtension()->setURLArgs(args);

    // opening the URL
    if (_part->didOpenURL([URL cString])) {
        // things we have to set up after calling didOpenURL
        if (refresh) {
            _part->addMetaData("http-refresh", QString::fromNSString(refresh));
        }
        if (lastModified) {
            NSString *modifiedString = [lastModified descriptionWithCalendarFormat:@"%a %b %d %Y %H:%M:%S" timeZone:nil locale:nil];
            _part->addMetaData("modified", QString::fromNSString(modifiedString));
        }
    }
}

- (void)setEncoding:(NSString *)encoding userChosen:(BOOL)userChosen
{
    _part->setEncoding(QString::fromNSString(encoding), userChosen);
}

- (void)addData:(NSData *)data
{
    DocumentImpl *doc = _part->xmlDocImpl();
    
    // Document may be nil if the part is about to redirect
    // as a result of JS executing during load, i.e. one frame
    // changing another's location before the frame's document
    // has been created. 
    if (doc){
        doc->setShouldCreateRenderers([self shouldCreateRenderers]);
        _part->addData((const char *)[data bytes], [data length]);
    }
}

- (void)closeURL
{
    _part->closeURL();
}

- (void)didNotOpenURL:(NSString *)URL
{
    _part->didNotOpenURL(QString::fromNSString(URL));
}

- (void)saveDocumentState
{
    DocumentImpl *doc = _part->xmlDocImpl();
    if (doc != 0){
        QStringList list = doc->docState();
        NSMutableArray *documentState = [[[NSMutableArray alloc] init] autorelease];
        
        for (uint i = 0; i < list.count(); i++){
            QString s = list[i];
            [documentState addObject: [NSString stringWithCharacters: (const unichar *)s.unicode() length: s.length()]];
        }
        [self saveDocumentState: documentState];
    }
}

- (void)restoreDocumentState
{
    DocumentImpl *doc = _part->xmlDocImpl();
    
    if (doc != 0){
        NSArray *documentState = [self documentState];
        
        QStringList s;
        for (uint i = 0; i < [documentState count]; i++){
            NSString *string = [documentState objectAtIndex: i];
            s.append(QString::fromNSString(string));
        }
            
        doc->setRestoreState(s);
    }
}

- (void)scrollToAnchorWithURL:(NSString *)URL
{
    _part->scrollToAnchor([URL cString]);
}

- (BOOL)saveDocumentToPageCache
{
    DocumentImpl *doc = _part->xmlDocImpl();
    if (!doc) {
        return NO;
    }
    
    _part->clearTimers();

    SavedProperties *windowProperties = new SavedProperties;
    _part->saveWindowProperties(windowProperties);

    SavedProperties *locationProperties = new SavedProperties;
    _part->saveLocationProperties(locationProperties);
    
    KWQPageState *pageState = [[[KWQPageState alloc] initWithDocument:doc
                                                                  URL:_part->m_url
                                                     windowProperties:windowProperties
                                                   locationProperties:locationProperties] autorelease];


    [pageState setPausedActions: _part->pauseActions((const void *)pageState)];

    return [self saveDocumentToPageCache:pageState];
}

- (BOOL)canCachePage
{
    return _part->canCachePage();
}

- (void)end
{
    _part->end();
}

- (void)createKHTMLViewWithNSView:(NSView *)view marginWidth:(int)mw marginHeight:(int)mh
{
    // If we own the view, delete the old one - otherwise the render _part will take care of deleting the view.
    [self removeFromFrame];

    KHTMLView *kview = new KHTMLView(_part, 0);
    _part->setView(kview);
    kview->deref();

    kview->setView(view);
    if (mw >= 0)
        kview->setMarginWidth(mw);
    if (mh >= 0)
        kview->setMarginHeight(mh);
}

- (void)scrollToAnchor:(NSString *)a
{
    _part->gotoAnchor(QString::fromNSString(a));
}

- (NSString *)selectedString
{
    QString text = _part->selectedText();
    text.replace('\\', _part->backslashAsCurrencySymbol());
    return [[text.getNSString() copy] autorelease];
}

- (void)selectAll
{
    _part->selectAll();
}

- (void)deselectAll
{
    _part->slotClearSelection();
}

- (BOOL)isFrameSet
{
    return _part->isFrameSet();
}

- (void)reapplyStylesForDeviceType:(WebCoreDeviceType)deviceType
{
    _part->setMediaType(deviceType == WebCoreDeviceScreen ? "screen" : "print");
    DocumentImpl *doc = _part->xmlDocImpl();
    if (doc) {
        static QPaintDevice screen;
        static QPrinter printer;
    	doc->setPaintDevice(deviceType == WebCoreDeviceScreen ? &screen : &printer);
    }
    return _part->reparseConfiguration();
}

static BOOL nowPrinting(WebCoreBridge *self)
{
    DocumentImpl *doc = self->_part->xmlDocImpl();
    return doc && doc->paintDevice() && doc->paintDevice()->devType() == QInternal::Printer;
}

// Set or unset the printing mode in the view.  We only toy with this if we're printing.
- (void)_setupRootForPrinting:(BOOL)onOrOff
{
    if (nowPrinting(self)) {
        RenderCanvas *root = static_cast<khtml::RenderCanvas *>(_part->xmlDocImpl()->renderer());
        if (root) {
            root->setPrintingMode(onOrOff);
        }
    }
}

- (void)forceLayout
{
    [self _setupRootForPrinting:YES];
    _part->forceLayout();
    [self _setupRootForPrinting:NO];
}

- (void)forceLayoutForPageWidth:(float)pageWidth
{
    [self _setupRootForPrinting:YES];
    _part->forceLayoutForPageWidth(pageWidth);
    [self _setupRootForPrinting:NO];
}

- (void)sendResizeEvent
{
    _part->sendResizeEvent();
}

- (void)drawRect:(NSRect)rect withPainter:(QPainter *)p
{
    [self _setupRootForPrinting:YES];
    if (_drawSelectionOnly) {
        _part->paintSelectionOnly(p, QRect(rect));
    } else {
        _part->paint(p, QRect(rect));
    }
    [self _setupRootForPrinting:NO];
}

- (void)drawRect:(NSRect)rect
{
    QPainter painter(nowPrinting(self));
    painter.setUsesInactiveTextBackgroundColor(_part->usesInactiveTextBackgroundColor());
    [self drawRect:rect withPainter:&painter];
}

- (void)adjustFrames:(NSRect)rect
{
    // Ick!  khtml sets the frame size during layout and
    // the frame origins during drawing!  So we have to 
    // layout and do a draw with rendering disabled to
    // correctly adjust the frames.
    [self forceLayout];
    QPainter painter(nowPrinting(self));
    painter.setPaintingDisabled(YES);
    [self drawRect:rect withPainter:&painter];
}

// Vertical pagination hook from AppKit
- (void)adjustPageHeightNew:(float *)newBottom top:(float)oldTop bottom:(float)oldBottom limit:(float)bottomLimit
{
    [self _setupRootForPrinting:YES];
    _part->adjustPageHeight(newBottom, oldTop, oldBottom, bottomLimit);
    [self _setupRootForPrinting:NO];
}

- (NSObject *)copyDOMNode:(NodeImpl *)node copier:(id <WebCoreDOMTreeCopier>)copier
{
    NSMutableArray *children = [[NSMutableArray alloc] init];
    for (NodeImpl *child = node->firstChild(); child; child = child->nextSibling()) {
        [children addObject:[self copyDOMNode:child copier:copier]];
    }
    NSObject *copiedNode = [copier nodeWithName:node->nodeName().string().getNSString()
                                          value:node->nodeValue().string().getNSString()
                                         source:node->recursive_toHTML(1).getNSString()
                                       children:children];
    [children release];
    return copiedNode;
}

- (NSObject *)copyDOMTree:(id <WebCoreDOMTreeCopier>)copier
{
    DocumentImpl *doc = _part->xmlDocImpl();
    if (!doc) {
        return nil;
    }
    return [self copyDOMNode:doc copier:copier];
}

- (NSObject *)copyRenderNode:(RenderObject *)node copier:(id <WebCoreRenderTreeCopier>)copier
{
    NSMutableArray *children = [[NSMutableArray alloc] init];
    for (RenderObject *child = node->firstChild(); child; child = child->nextSibling()) {
        [children addObject:[self copyRenderNode:child copier:copier]];
    }
          
    NSString *name = [[NSString alloc] initWithUTF8String:node->renderName()];
    
    RenderPart *nodeRenderPart = dynamic_cast<RenderPart *>(node);
    QWidget *widget = nodeRenderPart ? nodeRenderPart->widget() : 0;
    NSView *view = widget ? widget->getView() : nil;
    
    int nx, ny;
    node->absolutePosition(nx,ny);
    NSObject *copiedNode = [copier nodeWithName:name
                                       position:NSMakePoint(nx,ny)
                                           rect:NSMakeRect(node->xPos(), node->yPos(), node->width(), node->height())
                                           view:view
                                       children:children];
    
    [name release];
    [children release];
    
    return copiedNode;
}

- (NSObject *)copyRenderTree:(id <WebCoreRenderTreeCopier>)copier
{
    RenderObject *renderer = _part->renderer();
    if (!renderer) {
        return nil;
    }
    return [self copyRenderNode:renderer copier:copier];
}

- (void)removeFromFrame
{
    _part->setView(0);
}

- (void)installInFrame:(NSView *)view
{
    // If this isn't the main frame, it must have a render _part set, or it
    // won't ever get installed in the view hierarchy.
    ASSERT(self == [self mainFrame] || _renderPart != nil);

    _part->view()->setView(view);
    if (_renderPart) {
        _renderPart->setWidget(_part->view());
        // Now the render part owns the view, so we don't any more.
    }
}

- (void)mouseDown:(NSEvent *)event
{
    _part->mouseDown(event);
}

- (void)mouseDragged:(NSEvent *)event
{
    _part->mouseDragged(event);
}

- (void)mouseUp:(NSEvent *)event
{
    _part->mouseUp(event);
}

- (void)mouseMoved:(NSEvent *)event
{
    _part->mouseMoved(event);
}

- (id <WebDOMElement>)elementForView:(NSView *)view
{
    // FIXME: implemetented currently for only a subset of the KWQ widgets
    if ([view conformsToProtocol:@protocol(KWQWidgetHolder)]) {
        QWidget *widget = [(NSView <KWQWidgetHolder> *)view widget];
        NodeImpl *node = static_cast<const RenderWidget *>(widget->eventFilterObject())->element();
        return [WebCoreDOMElement elementWithImpl:static_cast<ElementImpl *>(node)];
    } else {
        return nil;
    }
}

static NSView *viewForElement(DOM::ElementImpl *elementImpl)
{
    RenderObject *renderer = elementImpl->renderer();
    if (renderer && renderer->isWidget()) {
        QWidget *widget = static_cast<const RenderWidget *>(renderer)->widget();
        if (widget) {
            return widget->getView();
        }
    }
    return nil;
}

static HTMLInputElementImpl *inputElementFromDOMElement(id <WebDOMElement>element)
{
    ASSERT([(NSObject *)element isKindOfClass:[WebCoreDOMElement class]]);
    DOM::ElementImpl *domElement = [(WebCoreDOMElement *)element elementImpl];
    if (idFromNode(domElement) == ID_INPUT) {
        return static_cast<HTMLInputElementImpl *>(domElement);
    }
    return nil;
}

static HTMLFormElementImpl *formElementFromDOMElement(id <WebDOMElement>element)
{
    ASSERT([(NSObject *)element isKindOfClass:[WebCoreDOMElement class]]);
    DOM::ElementImpl *domElement = [(WebCoreDOMElement *)element elementImpl];
    if (idFromNode(domElement) == ID_FORM) {
        return static_cast<HTMLFormElementImpl *>(domElement);
    }
    return nil;
}

- (id <WebDOMElement>)elementWithName:(NSString *)name inForm:(id <WebDOMElement>)form
{
    HTMLFormElementImpl *formElement = formElementFromDOMElement(form);
    if (formElement) {
        QPtrList<HTMLGenericFormElementImpl> elements = formElement->formElements;
        QString targetName = QString::fromNSString(name);
        for (unsigned int i = 0; i < elements.count(); i++) {
            HTMLGenericFormElementImpl *elt = elements.at(i);
            // Skip option elements, other duds
            if (elt->name() == targetName) {
                return [WebCoreDOMElement elementWithImpl:elt];
            }
        }
    }
    return nil;
}

- (BOOL)elementDoesAutoComplete:(id <WebDOMElement>)element
{
    HTMLInputElementImpl *inputElement = inputElementFromDOMElement(element);
    return inputElement != nil
        && inputElement->inputType() == HTMLInputElementImpl::TEXT
        && inputElement->autoComplete();
}

- (BOOL)elementIsPassword:(id <WebDOMElement>)element
{
    HTMLInputElementImpl *inputElement = inputElementFromDOMElement(element);
    return inputElement != nil
        && inputElement->inputType() == HTMLInputElementImpl::PASSWORD;
}

- (id <WebDOMElement>)formForElement:(id <WebDOMElement>)element;
{
    HTMLInputElementImpl *inputElement = inputElementFromDOMElement(element);
    if (inputElement) {
        HTMLFormElementImpl *formElement = inputElement->form();
        if (formElement) {
            return [WebCoreDOMElement elementWithImpl:formElement];
        }
    }
    return nil;
}

- (id <WebDOMElement>)currentForm
{
    HTMLFormElementImpl *formElement = _part->currentForm();
    return formElement ? [WebCoreDOMElement elementWithImpl:formElement] : nil;
}

- (NSArray *)controlsInForm:(id <WebDOMElement>)form
{
    NSMutableArray *results = nil;
    HTMLFormElementImpl *formElement = formElementFromDOMElement(form);
    if (formElement) {
        QPtrList<HTMLGenericFormElementImpl> elements = formElement->formElements;
        for (unsigned int i = 0; i < elements.count(); i++) {
            if (elements.at(i)->isEnumeratable()) {		// Skip option elements, other duds
                NSView *view = viewForElement(elements.at(i));
                if (view) {
                    if (!results) {
                        results = [NSMutableArray arrayWithObject:view];
                    } else {
                        [results addObject:view];
                    }
                }
            }
        }
    }
    return results;
}

- (NSString *)searchForLabels:(NSArray *)labels beforeElement:(id <WebDOMElement>)element
{
    ASSERT([(NSObject *)element isKindOfClass:[WebCoreDOMElement class]]);
    return _part->searchForLabelsBeforeElement(labels, [(WebCoreDOMElement *)element elementImpl]);
}

- (NSString *)matchLabels:(NSArray *)labels againstElement:(id <WebDOMElement>)element
{
    ASSERT([(NSObject *)element isKindOfClass:[WebCoreDOMElement class]]);
    return _part->matchLabelsAgainstElement(labels, [(WebCoreDOMElement *)element elementImpl]);
}

- (NSDictionary *)elementAtPoint:(NSPoint)point
{
    RenderObject *renderer = _part->renderer();
    if (!renderer) {
        return nil;
    }
    RenderObject::NodeInfo nodeInfo(true, true);
    renderer->layer()->nodeAtPoint(nodeInfo, (int)point.x, (int)point.y);
    
    NSMutableDictionary *element = [NSMutableDictionary dictionary];
    [element setObject:[NSNumber numberWithBool:_part->isPointInsideSelection((int)point.x, (int)point.y)]
                forKey:WebCoreElementIsSelectedKey];
    
    NodeImpl *URLNode = nodeInfo.URLElement();
    if (URLNode) {
        ElementImpl *e = static_cast<ElementImpl *>(URLNode);
        
        DOMString title = e->getAttribute(ATTR_TITLE);
        if (!title.isEmpty()) {
            QString titleText = title.string();
            titleText.replace('\\', _part->backslashAsCurrencySymbol());
            [element setObject:titleText.getNSString() forKey:WebCoreElementLinkTitleKey];
        }
        
        DOMString link = e->getAttribute(ATTR_HREF);
        if (!link.isNull()) {
            // Look for the first #text node to use as a label.
            NodeImpl *labelParent = e;
            while (labelParent->hasChildNodes()){
                NodeImpl *childNode = labelParent->firstChild();
                unsigned short type = childNode->nodeType();
                if (type == Node::TEXT_NODE){
                    DOMStringImpl *dv = childNode->nodeValue().implementation();
                    if (dv){
                        NSString *value = [NSString stringWithCharacters: (const unichar *)dv->s length: dv->l];
                        [element setObject:value forKey:WebCoreElementLinkLabelKey];
                        break;
                    }
                }
                labelParent = childNode;
            }
            [element setObject:_part->xmlDocImpl()->completeURL(link.string()).getNSString() forKey:WebCoreElementLinkURLKey];
        }
        
        DOMString target = e->getAttribute(ATTR_TARGET);
        if (target.isEmpty() && _part->xmlDocImpl()) {
            target = _part->xmlDocImpl()->baseTarget();
        }
        if (!target.isEmpty()) {
            [element setObject:target.string().getNSString() forKey:WebCoreElementLinkTargetFrameKey];
        }
    }

    NodeImpl *node = nodeInfo.innerNonSharedNode();
    if (node && isImage(node)){
        ElementImpl *i = static_cast<ElementImpl*>(node);
        DOMString attr = i->getAttribute(ATTR_SRC);
        if (attr.isEmpty()) {
            // Look for the URL in the DATA attribute of the OBJECT tag.
            attr = i->getAttribute(ATTR_DATA);
        }

        if (!attr.isEmpty()) {
            [element setObject:_part->xmlDocImpl()->completeURL(attr.string()).getNSString() forKey:WebCoreElementImageURLKey];
        }
        
        DOMString alt = i->getAttribute(ATTR_ALT);
        if (!alt.isNull()) {
            QString altText = alt.string();
            altText.replace('\\', _part->backslashAsCurrencySymbol());
            [element setObject:altText.getNSString() forKey:WebCoreElementImageAltStringKey];
        }
        
        RenderImage *r = (RenderImage *)node->renderer();
        if (r) {
            int x, y;
            if (r->absolutePosition(x, y)) {
                NSValue *rect = [NSValue valueWithRect:NSMakeRect(x, y, r->contentWidth(), r->contentHeight())];
                [element setObject:rect forKey:WebCoreElementImageRectKey];
            }
            
            NSImage *image = r->pixmap().image();
            if (image) {
                [element setObject:image forKey:WebCoreElementImageKey];
            }
        }
    }
    
    return element;
}

- (BOOL)searchFor:(NSString *)string direction:(BOOL)forward caseSensitive:(BOOL)caseFlag wrap:(BOOL)wrapFlag
{
    return _part->findString(string, forward, caseFlag, wrapFlag);
}

- (void)jumpToSelection
{
    _part->jumpToSelection();
}

- (void)setTextSizeMultiplier:(float)multiplier
{
    int newZoomFactor = (int)rint(multiplier * 100);
    if (_part->zoomFactor() == newZoomFactor) {
        return;
    }
    _part->setZoomFactor(newZoomFactor);
    // setZoomFactor will trigger a timed layout, but we want to do the layout before
    // we do any drawing. This takes care of that. Without this we redraw twice.
    [self setNeedsLayout];
}

- (CFStringEncoding)textEncoding
{
    return KWQCFStringEncodingFromIANACharsetName(_part->encoding().latin1());
}

- (NSView *)nextKeyView
{
    return _part->nextKeyView(0, KWQSelectingNext);
}

- (NSView *)previousKeyView
{
    return _part->nextKeyView(0, KWQSelectingPrevious);
}

- (NSView *)nextKeyViewInsideWebFrameViews
{
    return _part->nextKeyViewInFrameHierarchy(0, KWQSelectingNext);
}

- (NSView *)previousKeyViewInsideWebFrameViews
{
    return _part->nextKeyViewInFrameHierarchy(0, KWQSelectingPrevious);
}

- (NSString *)stringByEvaluatingJavaScriptFromString:(NSString *)string
{
    _part->createEmptyDocument();
    return _part->executeScript(QString::fromNSString(string), true).asString().getNSString();
}

- (id<WebDOMDocument>)DOMDocument
{
    return [WebCoreDOMDocument documentWithImpl:_part->xmlDocImpl()];
}

- (void)setSelectionFrom:(id<WebDOMNode>)start startOffset:(int)startOffset to:(id<WebDOMNode>)end endOffset:(int) endOffset
{
    _part->xmlDocImpl()->setSelection([(WebCoreDOMNode *)start impl], startOffset, [(WebCoreDOMNode *)end impl], endOffset);
}

- (NSAttributedString *)selectedAttributedString
{
    return KWQKHTMLPart::attributedString(_part->selectionStart(), _part->selectionStartOffset(), _part->selectionEnd(), _part->selectionEndOffset());
}

- (NSAttributedString *)attributedStringFrom:(id<WebDOMNode>)startNode startOffset:(int)startOffset to:(id<WebDOMNode>)endNode endOffset:(int)endOffset
{
    return KWQKHTMLPart::attributedString([(WebCoreDOMNode *)startNode impl], startOffset, [(WebCoreDOMNode *)endNode impl], endOffset);
}

- (id<WebDOMNode>)selectionStart
{
    return [WebCoreDOMNode nodeWithImpl:_part->selectionStart()];
}

- (int)selectionStartOffset
{
    return _part->selectionStartOffset();
}

- (id<WebDOMNode>)selectionEnd
{
    return [WebCoreDOMNode nodeWithImpl:_part->selectionEnd()];
}

- (int)selectionEndOffset
{
    return _part->selectionEndOffset();
}

- (NSRect)selectionRect
{
    KHTMLView *view = _part->view();
    if (!view) {
        return NSZeroRect;
    }
    NSView *documentView = view->getDocumentView();
    if (!documentView) {
        return NSZeroRect;
    }
    return NSIntersectionRect(_part->selectionRect(), [documentView visibleRect]); 
}

- (NSImage *)selectionImage
{
    NSView *view = _part->view()->getDocumentView();
    if (!view) {
        return nil;
    }

    NSRect rect = [self selectionRect];
    NSRect bounds = [view bounds];
    NSImage *selectionImage = [[[NSImage alloc] initWithSize:rect.size] autorelease];
    [selectionImage setFlipped:YES];
    [selectionImage lockFocus];

    [NSGraphicsContext saveGraphicsState];
    CGContextTranslateCTM((CGContext *)[[NSGraphicsContext currentContext] graphicsPort],
                          -(NSMinX(rect) - NSMinX(bounds)), -(NSMinY(rect) - NSMinY(bounds)));

    _drawSelectionOnly = YES;
    [view drawRect:rect];
    _drawSelectionOnly = NO;

    [NSGraphicsContext restoreGraphicsState];
        
    [selectionImage unlockFocus];
    [selectionImage setFlipped:NO];

    return selectionImage;
}

- (void)setName:(NSString *)name
{
    _part->setName(QString::fromNSString(name));
}

- (NSString *)name
{
    return _part->name().getNSString();
}

- (NSString *)URL
{
    return _part->url().url().getNSString();
}

- (NSString *)referrer
{
    // Do not allow file URLs to be used as referrers as that is potentially a security issue
    NSString *referrer = _part->referrer().getNSString();
    BOOL isFileURL = [referrer rangeOfString:@"file:" options:(NSCaseInsensitiveSearch | NSAnchoredSearch)].location != NSNotFound;
    return isFileURL ? nil : referrer;
}

- (int)frameBorderStyle
{
    KHTMLView *view = _part->view();
    if (view) {
        if (view->frameStyle() & QFrame::Sunken)
            return SunkenFrameBorder;
        if (view->frameStyle() & QFrame::Plain)
            return PlainFrameBorder;
    }
    return NoFrameBorder;
}

+ (NSString *)stringWithData:(NSData *)data textEncoding:(CFStringEncoding)textEncoding
{
    if (textEncoding == kCFStringEncodingInvalidId || textEncoding == kCFStringEncodingISOLatin1) {
        textEncoding = kCFStringEncodingWindowsLatin1;
    }
    return QTextCodec(textEncoding).toUnicode((const char*)[data bytes], [data length]).getNSString();
}

+ (NSString *)stringWithData:(NSData *)data textEncodingName:(NSString *)textEncodingName
{
    CFStringEncoding textEncoding = KWQCFStringEncodingFromIANACharsetName([textEncodingName lossyCString]);
    return [WebCoreBridge stringWithData:data textEncoding:textEncoding];
}

- (BOOL)needsLayout
{
    RenderObject *renderer = _part->renderer();
    return renderer ? renderer->needsLayout() : false;
}

- (BOOL)interceptKeyEvent:(NSEvent *)event toView:(NSView *)view
{
    return _part->keyEvent(event);
}

- (NSString *)renderTreeAsExternalRepresentation
{
    return externalRepresentation(_part->renderer()).getNSString();
}

- (void)setUsesInactiveTextBackgroundColor:(BOOL)uses
{
    _part->setUsesInactiveTextBackgroundColor(uses);
}

- (BOOL)usesInactiveTextBackgroundColor
{
    return _part->usesInactiveTextBackgroundColor();
}

- (void)setShouldCreateRenderers:(BOOL)f
{
    _shouldCreateRenderers = f;
}

- (BOOL)shouldCreateRenderers
{
    return _shouldCreateRenderers;
}

- (int)numPendingOrLoadingRequests
{
    DocumentImpl *doc = _part->xmlDocImpl();
    
    if (doc)
        return KWQNumberOfPendingOrLoadingRequests (doc->docLoader());
    return 0;
}
    

@end
