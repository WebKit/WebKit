/*	IFWebViewPrivate.mm
	Copyright 2001, Apple, Inc. All rights reserved.
        
        Private header file.  This file may reference classes (both ObjectiveC and C++)
        in WebCore.  Instances of this class are referenced by _private in 
        NSWebPageView.
*/
#import <WebKit/WebKitDebug.h>

#import <WebKit/IFWebViewPrivate.h>
#import <WebKit/IFPluginView.h>

// Includes from KDE
#import <khtmlview.h>
#import <html/html_documentimpl.h>

@implementation IFWebViewPrivate

- (void)dealloc
{
    [controller release];
    [frameScrollView release];

    //if (widget)
    //    delete widget;

    [super dealloc];
}

@end

@implementation IFWebView  (IFPrivate)

- (void)_resetWidget
{
    delete _private->widget;
    _private->widget = 0;
}

- (void)_stopPlugins 
{
    NSArray *views = [self subviews];
    int count = [views count];
    while (count--) {
        id view = [views objectAtIndex: count];
        if ([view isKindOfClass: NSClassFromString (@"IFPluginView")])
            [(IFPluginView *)view stop];
    }
}

- (void)_removeSubviews
{
    // Remove all the views.  They will be be re-added if this is a re-layout. 
    [[self subviews] makeObjectsPerformSelector:@selector(removeFromSuperviewWithoutNeedingDisplay)];
}

- (void)_setController: (id <IFWebController>)controller
{
    // Not retained; the controller owns the view.
    _private->controller = controller;    
}

- (KHTMLView *)_widget
{
    return _private->widget;    
}

- (DOM::DocumentImpl *)_document
{
    KHTMLPart *part = _private->widget->part();
    if (part) {
        return part->xmlDocImpl();
    }
    return 0;
}

+ (NSString *)_nodeName: (DOM::NodeImpl *)node
{
    // FIXME: good for debugging only since it's Latin 1 only
    return [NSString stringWithCString:node->nodeName().string().latin1()];
}

+ (NSString *)_nodeValue: (DOM::NodeImpl *)node
{
    // FIXME: good for debugging only since it's Latin 1 only
    return [NSString stringWithCString:node->nodeValue().string().latin1()];
}

+ (NSString *)_nodeHTML: (DOM::NodeImpl *)node
{
    return QSTRING_TO_NSSTRING(node->recursive_toHTML(1));
}

- (khtml::RenderObject *)_renderRoot
{
    KHTMLPart *part = _private->widget->part();
    DOM::DocumentImpl *doc;
    if (part) {
        doc = part->xmlDocImpl();
        if (doc)
            return doc->renderer();
    }
    return 0;
}

- (KHTMLView *)_provisionalWidget
{
    return _private->provisionalWidget;    
}

- (void)_setFrameScrollView: (NSScrollView *)sv
{
    [sv retain];
    [_private->frameScrollView release];
    _private->frameScrollView = sv;    
    //[sv setAutoresizingMask: NSViewWidthSizable | NSViewHeightSizable];
    //[sv setHasVerticalScroller: YES];
    //[sv setHasHorizontalScroller: YES];
    [self setAutoresizingMask: NSViewWidthSizable | NSViewHeightSizable];
    [sv setDocumentView: self];
}

- (NSScrollView *)_frameScrollView
{
    return _private->frameScrollView;    
}

- (void)_setupScrollers
{
    BOOL scrollsVertically;
    BOOL scrollsHorizontally;

    if ([self _frameScrollView]) {
        scrollsVertically = [self bounds].size.height > [[self _frameScrollView] frame].size.height;
        scrollsHorizontally = [self bounds].size.width > [[self _frameScrollView] frame].size.width;
    
        [[self _frameScrollView] setHasVerticalScroller: scrollsVertically];
        [[self _frameScrollView] setHasHorizontalScroller: scrollsHorizontally];
    }
}

@end
