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
#import "render_root.h"
#import "render_style.h"
#import "render_replaced.h"
using khtml::RenderWidget;

#import <JavaScriptCore/property_map.h>

#import "KWQAssertions.h"
#import "KWQCharsets.h"
#import "KWQDOMNode.h"
#import "KWQFont.h"
#import "KWQFrame.h"
#import "KWQPageState.h"
#import "KWQRenderTreeDebug.h"
#import "KWQView.h"

#import "WebCoreDOMPrivate.h"
#import "WebCoreImageRenderer.h"
#import "WebCoreTextRendererFactory.h"
#import "WebCoreSettings.h"

#import <AppKit/NSView.h>

using DOM::DocumentImpl;
using DOM::Node;
using DOM::NodeImpl;

using khtml::parseURL;
using khtml::RenderImage;
using khtml::RenderObject;
using khtml::RenderPart;
using khtml::RenderStyle;

using KJS::SavedProperties;

using KParts::URLArgs;

NSString *WebCoreElementFrameKey = 		@"WebElementFrame";
NSString *WebCoreElementImageAltStringKey = 	@"WebElementImageAltString";
NSString *WebCoreElementImageKey = 		@"WebElementImage";
NSString *WebCoreElementImageRectKey = 		@"WebElementImageRect";
NSString *WebCoreElementImageURLKey = 		@"WebElementImageURL";
NSString *WebCoreElementIsSelectedTextKey = 	@"WebElementIsSelectedText";
NSString *WebCoreElementLinkURLKey = 		@"WebElementLinkURL";
NSString *WebCoreElementLinkTargetFrameKey =	@"WebElementTargetFrame";
NSString *WebCoreElementLinkLabelKey = 		@"WebElementLinkLabel";
NSString *WebCoreElementLinkTitleKey = 		@"WebElementLinkTitle";
NSString *WebCoreElementNameKey = 		@"WebElementName";

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
        KWQPageState *state = [pageCache objectForKey:@"WebCorePageState"];
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
    _part->didOpenURL([URL cString]);

    // things we have to set up after calling didOpenURL
    if (refresh) {
        _part->addMetaData("http-refresh", QString::fromNSString(refresh));
    }
    if (lastModified) {
	_part->addMetaData("modified", QString::fromNSString([lastModified description]));
    }
}

- (void)addData:(NSData *)data withEncoding:(NSString *)encoding
{
    _part->slotData(encoding, NO, (const char *)[data bytes], [data length], NO);
}

- (void)addData:(NSData *)data withOverrideEncoding:(NSString *)encoding
{
    _part->slotData(encoding, YES, (const char *)[data bytes], [data length], NO);
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
    _part->setView(kview, true);

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
    return [[_part->selectedText().getNSString() copy] autorelease];
}

- (void)selectAll
{
    _part->selectAll();
}

- (void)deselectAll
{
    _part->xmlDocImpl()->clearSelection();
}

- (BOOL)isFrameSet
{
    return _part->isFrameSet();
}

- (void)reapplyStyles
{
    return _part->reparseConfiguration();
}

- (void)forceLayout
{
    _part->forceLayout();
}

- (void)drawRect:(NSRect)rect withPainter:(QPainter *)p
{
    if (_drawSelectionOnly) {
        _part->paintSelectionOnly(p, QRect(rect));
    } else {
        _part->paint(p, QRect(rect));
    }
}

- (void)drawRect:(NSRect)rect
{
    QPainter p;
    [self drawRect:rect withPainter:&p];
}

- (void)adjustFrames:(NSRect)rect
{
    // Ick!  khtml sets the frame size during layout and
    // the frame origins during drawing!  So we have to 
    // layout and do a draw with rendering disabled to
    // correctly adjust the frames.
    [self forceLayout];
    QPainter p;
    p.setPaintingDisabled(YES);
    [self drawRect:rect withPainter:&p];
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
    _part->setView(0, false);
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
        _part->setOwnsView(false);
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

- (BOOL)formIsLoginForm:(id <WebDOMElement>)element
{
    HTMLFormElementImpl *formElement = formElementFromDOMElement(element);
    return formElement->isLoginForm();
}

- (BOOL)elementDoesAutoComplete:(id <WebDOMElement>)element
{
    HTMLInputElementImpl *inputElement = inputElementFromDOMElement(element);
    return inputElement != nil
        && inputElement->inputType() == HTMLInputElementImpl::TEXT
        && inputElement->autoComplete();
}

- (id <WebDOMElement>)formForElement:(id <WebDOMElement>)element;
{
    HTMLInputElementImpl *inputElement = inputElementFromDOMElement(element);
    return inputElement ? [WebCoreDOMElement elementWithImpl:inputElement->form()] : nil;
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
        unsigned int i;
        for (i = 0; i < elements.count(); i++) {
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
                forKey:WebCoreElementIsSelectedTextKey];
    
    NodeImpl *URLNode = nodeInfo.URLElement();
    if (URLNode) {
        ElementImpl *e = static_cast<ElementImpl *>(URLNode);
        
        DOMString title = e->getAttribute(ATTR_TITLE);
        if (!title.isEmpty()) {
            [element setObject:title.string().getNSString() forKey:WebCoreElementLinkTitleKey];
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
            [element setObject:alt.string().getNSString() forKey:WebCoreElementImageAltStringKey];
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

- (BOOL)searchFor:(NSString *)string direction:(BOOL)forward caseSensitive:(BOOL)caseFlag
{
    return _part->findTextNext(QString::fromNSString(string), forward, caseFlag, FALSE);
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

- (NSView *)nextKeyViewInsideWebViews
{
    return _part->nextKeyViewInFrameHierarchy(0, KWQSelectingNext);
}

- (NSView *)previousKeyViewInsideWebViews
{
    return _part->nextKeyViewInFrameHierarchy(0, KWQSelectingPrevious);
}

- (NSString *)stringByEvaluatingJavaScriptFromString:(NSString *)string
{
    _part->createEmptyDocument();
    return _part->executeScript(QString::fromNSString(string)).asString().getNSString();
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
    NSView *view = _part->view()->getDocumentView();
    if (!view) {
        return NSZeroRect;
    }

    return NSIntersectionRect(NSRect(_part->selectionRect()), [view visibleRect]); 
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
    return renderer ? !renderer->layouted() : false;
}

- (BOOL)interceptKeyEvent:(NSEvent *)event toView:(NSView *)view
{
    return _part->keyEvent(event);
}

- (NSString *)renderTreeAsExternalRepresentation
{
    return externalRepresentation(_part->renderer()).getNSString();
}

@end
