/*
 * Copyright (C) 2002 Apple Computer, Inc.  All rights reserved.
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

#import <WebCoreBridge.h>

#import <khtml_part.h>
#import <khtmlview.h>
#import <dom_docimpl.h>
#import <render_image.h>
#import <render_object.h>
#import <render_root.h>
#import <render_frames.h>
#import <kwqdebug.h>
#include <html/html_documentimpl.h>
#include <xml/dom_nodeimpl.h>
#include <htmlattrs.h>
#include <htmltags.h>
#include <csshelper.h>
#include <KWQDOMNode.h>
#include <WebCoreImageRenderer.h>

#include <WebFoundation/WebNSURLExtras.h>

using namespace DOM;
using namespace khtml;

@implementation WebCoreBridge

- init
{
    [super init];
    
    part = new KHTMLPart;
    part->impl->setBridge(self);
    
    return self;
}

- (void)dealloc
{
    [self removeFromFrame];
    
    if (renderPart) {
        renderPart->deref();
    }
    part->deref();
    
    [super dealloc];
}

- (KHTMLPart *)part
{
    return part;
}

- (void)setRenderPart:(KHTMLRenderPart *)newPart;
{
    newPart->ref();
    if (renderPart) {
        renderPart->deref();
    }
    renderPart = newPart;
}

- (KHTMLRenderPart *)renderPart
{
    return renderPart;
}


- (void)openURL:(NSURL *)URL
{
    part->openURL([[URL absoluteString] cString]);
}

- (void)addData:(NSData *)data withEncoding:(NSString *)encoding
{
    part->impl->slotData(encoding, (const char *)[data bytes], [data length], NO);
}

- (void)closeURL
{
    part->closeURL();
}

- (void)end
{
    part->end();
}

- (void)setURL:(NSURL *)URL
{
    part->impl->setBaseURL([[URL absoluteString] cString]);
}

- (void)createKHTMLViewWithNSView:(NSView *)view
    width:(int)width height:(int)height
    marginWidth:(int)mw marginHeight:(int)mh
{
    KHTMLView *kview = new KHTMLView(part, 0);
    part->impl->setView(kview);

    kview->setView(view);
    if (mw >= 0)
        kview->setMarginWidth(mw);
    if (mh >= 0)
        kview->setMarginHeight(mh);
    kview->resize(width, height);
    
    bridgeOwnsKHTMLView = YES;
}

- (NSString *)documentTextFromDOM
{
    NSString *string = nil;
    DOM::DocumentImpl *doc = part->xmlDocImpl();
    if (doc) {
        string = [[doc->recursive_toHTML(1).getNSString() copy] autorelease];
    }
    if (string == nil) {
        string = @"";
    }
    return string;
}

- (void)scrollToBaseAnchor
{
    part->impl->gotoBaseAnchor();
}

- (NSString *)selectedText
{
    return [[part->selectedText().getNSString() copy] autorelease];
}

- (void)selectAll
{
    part->selectAll();
}

- (BOOL)isFrameSet
{
    return part->impl->isFrameSet();
}

- (void)reapplyStyles
{
    DOM::DocumentImpl *doc = part->xmlDocImpl();
    if (doc && doc->renderer()) {
        doc->updateStyleSelector();
    }
}

- (void)forceLayout
{
    DOM::DocumentImpl *doc = part->xmlDocImpl();
    if (doc) {
        khtml::RenderObject *renderer = doc->renderer();
        if (renderer) {
            renderer->setLayouted(false);
        }
    }
    KHTMLView *view = part->impl->getView();
    if (view) {
        view->layout();
    }
}

- (void)drawRect:(NSRect)rect withPainter:(QPainter *)p
{
    part->impl->paint(*p, (int)rect.origin.x, (int)rect.origin.y, (int)rect.size.width, (int)rect.size.height);
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

- (NSObject *)copyDOMNode:(DOM::NodeImpl *)node copier:(id <WebCoreDOMTreeCopier>)copier
{
    NSMutableArray *children = [[NSMutableArray alloc] init];
    for (DOM::NodeImpl *child = node->firstChild(); child; child = child->nextSibling()) {
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
    DOM::DocumentImpl *doc = part->xmlDocImpl();
    if (!doc) {
        return nil;
    }
    return [self copyDOMNode:doc copier:copier];
}

- (NSObject *)copyRenderNode:(khtml::RenderObject *)node copier:(id <WebCoreRenderTreeCopier>)copier
{
    NSMutableArray *children = [[NSMutableArray alloc] init];
    for (khtml::RenderObject *child = node->firstChild(); child; child = child->nextSibling()) {
        [children addObject:[self copyRenderNode:child copier:copier]];
    }
    
    NSString *name = [[NSString alloc] initWithUTF8String:node->renderName()];
    
    khtml::RenderPart *nodeRenderPart = dynamic_cast<khtml::RenderPart *>(node);
    QWidget *widget = nodeRenderPart ? nodeRenderPart->widget() : 0;
    NSView *view = widget ? widget->getView() : nil;
    
    NSObject *copiedNode = [copier nodeWithName:name
                                           rect:NSMakeRect(node->xPos(), node->yPos(), node->width(), node->height())
                                           view:view
                                       children:children];
    
    [name release];
    [children release];
    
    return copiedNode;
}

- (NSObject *)copyRenderTree:(id <WebCoreRenderTreeCopier>)copier
{
    DOM::DocumentImpl *doc = part->xmlDocImpl();
    if (!doc) {
        return nil;
    }
    khtml::RenderObject *renderer = doc->renderer();
    if (!renderer) {
        return nil;
    }
    return [self copyRenderNode:renderer copier:copier];
}

- (void)removeFromFrame
{
    if (bridgeOwnsKHTMLView) {
        delete part->impl->getView();
    }
    bridgeOwnsKHTMLView = NO;
}

- (void)installInFrame:(NSView *)view
{
    part->impl->getView()->setView(view);

    // If this isn't the main frame, it must have a render part set, or it
    // won't ever get installed in the view hierarchy.
    KWQ_ASSERT(self == [self mainFrame] || renderPart != nil);

    if (renderPart) {
        renderPart->setWidget(part->impl->getView());
        // Now that the render part is holding the widget, we don't own it any more.
        bridgeOwnsKHTMLView = NO;
    }
}

- (int)stateForEvent:(NSEvent *)event
{
    unsigned modifiers = [event modifierFlags];
    int state = 0;
    
    if (modifiers & NSControlKeyMask)
        state |= Qt::ControlButton;
    if (modifiers & NSShiftKeyMask)
        state |= Qt::ShiftButton;
    if (modifiers & NSAlternateKeyMask)
        state |= Qt::AltButton;
    
    // Mapping command to meta is slightly questionable, but it works for now.
    if (modifiers & NSCommandKeyMask)
        state |= Qt::MetaButton;
    
    return state;
}

- (void)mouseUp:(NSEvent *)event
{
    NSPoint p = [event locationInWindow];

    int button, state;
    switch ([event type]) {
    case NSRightMouseUp:
        button = Qt::RightButton;
        state = Qt::RightButton;
        break;
    case NSOtherMouseUp:
        button = Qt::MidButton;
        state = Qt::MidButton;
        break;
    default:
        button = Qt::LeftButton;
        state = Qt::LeftButton;
        break;
    }
    state |= [self stateForEvent:event];
    
    QMouseEvent kEvent(QEvent::MouseButtonPress, QPoint((int)p.x, (int)p.y), button, state);
    if (part->impl->getView()) {
        part->impl->getView()->viewportMouseReleaseEvent(&kEvent);
    }
}

- (void)mouseDown:(NSEvent *)event
{
    NSPoint p = [event locationInWindow];

    int button, state;     
    switch ([event type]) {
    case NSRightMouseUp:
        button = Qt::RightButton;
        state = Qt::RightButton;
        break;
    case NSOtherMouseUp:
        button = Qt::MidButton;
        state = Qt::MidButton;
        break;
    default:
        button = Qt::LeftButton;
        state = Qt::LeftButton;
        break;
    }
    state |= [self stateForEvent:event];
    
    QMouseEvent kEvent(QEvent::MouseButtonPress, QPoint((int)p.x, (int)p.y), button, state);
    if (part->impl->getView()) {
        part->impl->getView()->viewportMousePressEvent(&kEvent);
    }
}

- (void)mouseMoved:(NSEvent *)event
{
    NSPoint p = [event locationInWindow];
    
    QMouseEvent kEvent(QEvent::MouseMove, QPoint((int)p.x, (int)p.y), 0, [self stateForEvent:event]);
    if (part->impl->getView()) {
        part->impl->getView()->viewportMouseMoveEvent(&kEvent);
    }
}

- (void)mouseDragged:(NSEvent *)event
{
    NSPoint p = [event locationInWindow];
    
    QMouseEvent kEvent(QEvent::MouseMove, QPoint((int)p.x, (int)p.y), Qt::LeftButton, Qt::LeftButton);
    if (part->impl->getView()) {
        part->impl->getView()->viewportMouseMoveEvent(&kEvent);
    }
}

- (NSURL *)completeURLForDOMString:(DOMString &)s
{
    NSString *URLString = part->xmlDocImpl()->completeURL(s.string()).getNSString();
    return [NSURL _web_URLWithString:URLString];
}

- (NSDictionary *)elementInfoAtPoint:(NSPoint)point
{
    NSMutableDictionary *elementInfo = [NSMutableDictionary dictionary];
    RenderObject::NodeInfo nodeInfo(true, true);
    NodeImpl *node, *URLNode;
    DOMString domURL;
    NSURL *URL;

    part->xmlDocImpl()->renderer()->nodeAtPoint(nodeInfo, (int)point.x, (int)point.y, 0, 0);
    
    node = nodeInfo.innerNode();
    URLNode = nodeInfo.URLElement();
    
    if(URLNode){
        ElementImpl* e =  static_cast<ElementImpl*>(URLNode);
        domURL = khtml::parseURL(e->getAttribute(ATTR_HREF));
        URL = [self completeURLForDOMString:domURL];
        if(URL){
            [elementInfo setObject: URL forKey: WebCoreContextLinkURL];
        }
    }

    if(isImage(node)){
        ElementImpl* i =  static_cast<ElementImpl*>(node);
        domURL = khtml::parseURL(i->getAttribute(ATTR_SRC));
        URL = [self completeURLForDOMString:domURL];
        if(URL){
            [elementInfo setObject: URL forKey: WebCoreContextImageURL];
            RenderImage *r = (RenderImage *)node->renderer();
            id <WebCoreImageRenderer> image = r->pixmap().image();
            if(image){
                [elementInfo setObject: image forKey: WebCoreContextImage];
            }
        }
    }

    if(part->hasSelection()){
        [elementInfo setObject: [self selectedText] forKey: WebCoreContextString];
    }
    
    return elementInfo;
}


- (BOOL)searchFor: (NSString *)string direction: (BOOL)forward caseSensitive: (BOOL)caseFlag
{
    return part->findTextNext (QString::fromCFString((CFStringRef)string), forward, caseFlag, FALSE);
}

@end
