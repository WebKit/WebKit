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

#import <dom_node.h>
#import <dom_docimpl.h>
#import <dom_nodeimpl.h>
#import <khtml_part.h>
#import <khtmlview.h>
#import <render_frames.h>
#import <render_image.h>
#import <render_object.h>
#import <render_root.h>
#import <render_style.h>

#import <qfont.h>

#import <KWQAssertions.h>
#import <html/html_documentimpl.h>
#import <xml/dom_nodeimpl.h>
#import <htmlattrs.h>
#import <htmltags.h>
#import <csshelper.h>
#import <KWQDOMNode.h>
#import <WebCoreImageRenderer.h>
#import <WebCoreTextRendererFactory.h>
#import <KWQCharsets.h>

#import <WebCoreDOMPrivate.h>

using DOM::DocumentImpl;

using khtml::parseURL;
using khtml::RenderImage;
using khtml::RenderObject;
using khtml::RenderPart;

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
    part->impl->setBridge(nil);
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

- (void)setParent:(WebCoreBridge *)parent
{
    part->setParent([parent part]);
}

- (void)openURL:(NSURL *)URL
{
    part->openURL([[URL absoluteString] cString]);
}

- (void)addData:(NSData *)data withEncoding:(NSString *)encoding
{
    part->impl->slotData(encoding, NO, (const char *)[data bytes], [data length], NO);
}

- (void)addData:(NSData *)data withOverrideEncoding:(NSString *)encoding
{
    part->impl->slotData(encoding, YES, (const char *)[data bytes], [data length], NO);
}

- (void)closeURL
{
    part->closeURL();
}

- (void)saveDocumentState
{
    DocumentImpl *doc = part->impl->document();
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
    DocumentImpl *doc = part->impl->document();
    
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

- (void)end
{
    part->end();
}

- (void)createKHTMLViewWithNSView:(NSView *)view marginWidth:(int)mw marginHeight:(int)mh
{
    // If we own the view, delete the old one - otherwise the render part will take care of deleting the view.
    [self removeFromFrame];

    KHTMLView *kview = new KHTMLView(part, 0);
    part->impl->setView(kview);

    kview->setView(view);
    if (mw >= 0)
        kview->setMarginWidth(mw);
    if (mh >= 0)
        kview->setMarginHeight(mh);
    
    bridgeOwnsKHTMLView = YES;
}

- (void)scrollToAnchor:(NSString *)a
{
    part->gotoAnchor(QString::fromNSString(a));
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
    return part->reparseConfiguration();
}

- (void)forceLayout
{
    part->impl->forceLayout();
}

- (void)drawRect:(NSRect)rect withPainter:(QPainter *)p
{
    part->impl->paint(p, QRect(rect));
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
    DocumentImpl *doc = part->impl->document();
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
    RenderObject *renderer = part->impl->renderer();
    if (!renderer) {
        return nil;
    }
    return [self copyRenderNode:renderer copier:copier];
}

- (void)removeFromFrame
{
    if (bridgeOwnsKHTMLView) {
        delete part->impl->view();
    }
    bridgeOwnsKHTMLView = NO;
}

- (void)installInFrame:(NSView *)view
{
    part->impl->view()->setView(view);

    // If this isn't the main frame, it must have a render part set, or it
    // won't ever get installed in the view hierarchy.
    ASSERT(self == [self mainFrame] || renderPart != nil);

    if (renderPart) {
        renderPart->setWidget(part->impl->view());
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
    
    if (part->impl->view()) {
        int clickCount = [event clickCount];

        // Our behavior here is a little different that Qt.  Qt always sends
        // a mouse release event, even for a double click.  To correct problems
        // in khtml's DOM click event handling we do not send a release here
        // for a double click.  Instead we send that event from khtmlview's
        // viewportMouseDoubleClickEvent.
        if (clickCount > 0 && clickCount % 2 == 0) {
            QMouseEvent doubleClickEvent(QEvent::MouseButtonDblClick, QPoint(p), button, state, clickCount);
            part->impl->view()->viewportMouseDoubleClickEvent(&doubleClickEvent);
        }
        else {
            QMouseEvent releaseEvent(QEvent::MouseButtonRelease, QPoint(p), button, state, clickCount);
            part->impl->view()->viewportMouseReleaseEvent(&releaseEvent);
        }
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
    
    if (part->impl->view()) {
        QMouseEvent kEvent(QEvent::MouseButtonPress, QPoint(p), button, state, [event clickCount]);
        part->impl->view()->viewportMousePressEvent(&kEvent);
    }
}

- (void)mouseMoved:(NSEvent *)event
{
    NSPoint p = [event locationInWindow];
    
    QMouseEvent kEvent(QEvent::MouseMove, QPoint(p), 0, [self stateForEvent:event]);
    if (part->impl->view()) {
        part->impl->view()->viewportMouseMoveEvent(&kEvent);
    }
}

- (void)mouseDragged:(NSEvent *)event
{
    NSPoint p = [event locationInWindow];
    
    QMouseEvent kEvent(QEvent::MouseMove, QPoint(p), Qt::LeftButton, Qt::LeftButton);
    if (part->impl->view()) {
        part->impl->view()->viewportMouseMoveEvent(&kEvent);
    }
}

- (NSURL *)completeURLForDOMString:(const DOMString &)s
{
    return KURL(part->impl->document()->completeURL(s.string())).getNSURL();
}

- (NSDictionary *)elementAtPoint:(NSPoint)point
{
    RenderObject::NodeInfo nodeInfo(true, true);
    part->impl->renderer()->layer()->nodeAtPoint(nodeInfo, (int)point.x, (int)point.y);
    
    NSMutableDictionary *elementInfo = [NSMutableDictionary dictionary];

    NodeImpl *URLNode = nodeInfo.URLElement();
    if (URLNode) {
        ElementImpl* e = static_cast<ElementImpl*>(URLNode);
        
        NSURL *URL = [self completeURLForDOMString:parseURL(e->getAttribute(ATTR_HREF))];
        if (URL) {
            // Look for the first #text node to use as a label.
            NodeImpl *labelParent = e;
            while (labelParent->hasChildNodes()){
                NodeImpl *childNode = labelParent->firstChild();
                unsigned short type = childNode->nodeType();
                if (type == Node::TEXT_NODE){
                    DOMStringImpl *dv = childNode->nodeValue().implementation();
                    if (dv){
                        NSString *value = [NSString stringWithCharacters: (const unichar *)dv->s length: dv->l];
                        [elementInfo setObject:value forKey:WebCoreElementLinkLabel];
                        break;
                    }
                }
                labelParent = childNode;
            }
            [elementInfo setObject:URL forKey:WebCoreElementLinkURL];
        }
        
        DOMString target = e->getAttribute(ATTR_TARGET);
        if (target.isEmpty() && part->impl->document()) {
            target = part->impl->document()->baseTarget();
        }
        if (!target.isEmpty()) {
            [elementInfo setObject:target.string().getNSString() forKey:WebCoreElementLinkTarget];
        }
    }

    NodeImpl *node = nodeInfo.innerNode();
    if (isImage(node)) {
        ElementImpl* i =  static_cast<ElementImpl*>(node);
        NSURL *URL = [self completeURLForDOMString:parseURL(i->getAttribute(ATTR_SRC))];
        if (URL) {
            [elementInfo setObject:URL forKey:WebCoreElementImageURL];
            RenderImage *r = (RenderImage *)node->renderer();
            id <WebCoreImageRenderer> image = r->pixmap().image();
            if (image) {
                [elementInfo setObject:image forKey:WebCoreElementImage];
                int x, y;
                if(r->absolutePosition(x, y)){
                    [elementInfo setObject:[NSValue valueWithPoint:NSMakePoint(x,y)] forKey:WebCoreElementImageLocation];
                }
            }
        }
    }

    if (part->hasSelection()) {
        [elementInfo setObject:[self selectedText] forKey:WebCoreElementString];
    }
    
    return elementInfo;
}

- (BOOL)searchFor:(NSString *)string direction:(BOOL)forward caseSensitive:(BOOL)caseFlag
{
    return part->findTextNext(QString::fromNSString(string), forward, caseFlag, FALSE);
}

- (void)jumpToSelection
{
    part->impl->jumpToSelection();
}

- (void)setTextSizeMultiplier:(float)multiplier
{
    int newZoomFactor = (int)rint(multiplier * 100);
    if (part->zoomFactor() == newZoomFactor) {
        return;
    }
    part->setZoomFactor(newZoomFactor);
    // setZoomFactor will trigger a timed layout, but we want to do the layout before
    // we do any drawing. This takes care of that. Without this we redraw twice.
    [self setNeedsLayout];
}

- (CFStringEncoding)textEncoding
{
    return KWQCFStringEncodingFromIANACharsetName(part->encoding().getCFString());
}

- (NSView *)nextKeyView
{
    return part->impl->nextKeyView(0, KWQSelectingNext);
}

- (NSView *)previousKeyView
{
    return part->impl->nextKeyView(0, KWQSelectingPrevious);
}

- (NSView *)nextKeyViewInsideWebViews
{
    return part->impl->nextKeyViewInFrameHierarchy(0, KWQSelectingNext);
}

- (NSView *)previousKeyViewInsideWebViews
{
    return part->impl->nextKeyViewInFrameHierarchy(0, KWQSelectingPrevious);
}

- (void)executeJavaScriptFromString:(NSString *)string
{
    // FIXME: We want to execute it even without a document, no?
    if (part->impl->document()) {
        part->executeScript(QString::fromNSString(string));
    }
}

- (id<WebDOMDocument>)DOMDocument
{
    DocumentImpl *doc = part->impl->document();
    printf ("(id<WebDOMDocument>)DOMDocument doc = %p, lastChild = %p\n", doc, doc->lastChild());
    return [WebCoreDOMDocument documentWithImpl:doc];
}

- (void)setSelectionFrom:(id<WebDOMNode>)start startOffset:(int)startOffset to:(id<WebDOMNode>)end endOffset:(int) endOffset
{
    DocumentImpl *doc = part->impl->document();
    
    doc->setSelection ([(WebCoreDOMNode *)start impl], startOffset, [(WebCoreDOMNode *)end impl], endOffset);
}


static NSAttributedString *attributedString(DOM::NodeImpl *_startNode, int startOffset, DOM::NodeImpl *endNode, int endOffset)
{
    bool hasNewLine = true;
    DOM::Node n = _startNode;
    khtml::RenderObject *renderer;
    khtml::RenderStyle *style;
    NSFont *font;
    NSMutableAttributedString *result = [[[NSMutableAttributedString alloc] init] autorelease];
    NSAttributedString *partialString;
    NSColor *color;
    
    while(!n.isNull()) {
        if(n.nodeType() == DOM::Node::TEXT_NODE) {
            QString text;
            QString str = n.nodeValue().string();
            
            renderer = n.handle()->renderer();
            font = nil;
            if (renderer){
                style = renderer->style();
                if (style) {
                    font = style->font().getNSFont();
                    color = style->color().getNSColor();
                }
            }
            
            hasNewLine = false;            
            if(n == _startNode && n == endNode && startOffset >=0 && endOffset >= 0)
                text = str.mid(startOffset, endOffset - startOffset);
            else if(n == _startNode && startOffset >= 0)
                text = str.mid(startOffset);
            else if(n == endNode && endOffset >= 0)
                text = str.left(endOffset);
            else
                text = str;
                
            if (font)
                partialString = [[NSAttributedString alloc] initWithString: text.getNSString() attributes: [NSDictionary dictionaryWithObjectsAndKeys: font, NSFontAttributeName, color, NSForegroundColorAttributeName, 0]];
            else
                partialString = [[NSAttributedString alloc] initWithString: text.getNSString() attributes: nil];
                
            [result appendAttributedString: partialString];
            [partialString release];
        }
        else {
            // This is our simple HTML -> ASCII transformation:
            QString text;
            unsigned short _id = n.elementId();
            switch(_id) {
                case ID_BR:
                    text += "\n";
                    hasNewLine = true;
                break;
        
                case ID_TD:
                case ID_TH:
                case ID_HR:
                case ID_OL:
                case ID_UL:
                case ID_LI:
                case ID_DD:
                case ID_DL:
                case ID_DT:
                case ID_PRE:
                case ID_BLOCKQUOTE:
                case ID_DIV:
                    if (!hasNewLine)
                        text += "\n";
                    hasNewLine = true;
                break;
                case ID_P:
                case ID_TR:
                case ID_H1:
                case ID_H2:
                case ID_H3:
                case ID_H4:
                case ID_H5:
                case ID_H6:
                    if (!hasNewLine)
                        text += "\n";
                    text += "\n";
                    hasNewLine = true;
                break;
            }
            partialString = [[NSAttributedString alloc] initWithString: text.getNSString() attributes: nil];
            [result appendAttributedString: partialString];
            [partialString release];
        }
        
        if(n == endNode)
            break;
        
        DOM::Node next = n.firstChild();
        if(next.isNull())
            next = n.nextSibling();
        while( next.isNull() && !n.parentNode().isNull() ) {
            QString text;
            n = n.parentNode();
            next = n.nextSibling();
            
            unsigned short _id = n.elementId();
            switch(_id) {
                case ID_TD:
                case ID_TH:
                case ID_HR:
                case ID_OL:
                case ID_UL:
                case ID_LI:
                case ID_DD:
                case ID_DL:
                case ID_DT:
                case ID_PRE:
                case ID_BLOCKQUOTE:
                case ID_DIV:
                if (!hasNewLine)
                    text += "\n";
                hasNewLine = true;
                break;
                case ID_P:
                case ID_TR:
                case ID_H1:
                case ID_H2:
                case ID_H3:
                case ID_H4:
                case ID_H5:
                case ID_H6:
                if (!hasNewLine)
                    text += "\n";
                // An extra newline is needed at the start, not the end, of these types of tags,
                // so don't add another here.
                hasNewLine = true;
                break;
            }
            partialString = [[NSAttributedString alloc] initWithString: text.getNSString() attributes: nil];
            [result appendAttributedString: partialString];
            [partialString release];
        }
    
        n = next;
    }
/*    
    int start = 0;
    int end = text.length();
    
    // Strip leading LFs
    while ((start < end) && (text[start] == '\n'))
        start++;
    
    // Strip excessive trailing LFs
    while ((start < (end-1)) && (text[end-1] == '\n') && (text[end-2] == '\n'))
        end--;
        
    text.mid(start, end-start);
*/    
    return result;
}

- (NSAttributedString *)selectedAttributedString
{
    return attributedString (part->impl->selectionStart(), part->impl->selectionStartOffset(), part->impl->selectionEnd(), part->impl->selectionEndOffset());
}

- (NSAttributedString *)attributedStringFrom: (id<WebDOMNode>)startNode startOffset: (int)startOffset to: (id<WebDOMNode>)endNode endOffset: (int)endOffset
{
    return attributedString ([(WebCoreDOMNode *)startNode impl], startOffset, [(WebCoreDOMNode *)endNode impl], endOffset);
}


- (id<WebDOMNode>)selectionStart
{
    return [WebCoreDOMNode nodeWithImpl: part->impl->selectionStart()];
}

- (int)selectionStartOffset
{
    return part->impl->selectionStartOffset();
}

- (id<WebDOMNode>)selectionEnd
{
    return [WebCoreDOMNode nodeWithImpl: part->impl->selectionEnd()];
}

- (int)selectionEndOffset
{
    return part->impl->selectionEndOffset();
}

- (void)setName:(NSString *)name
{
    part->setName(QString::fromNSString(name));
}

- (NSString *)name
{
    return part->name().getNSString();
}

@end
