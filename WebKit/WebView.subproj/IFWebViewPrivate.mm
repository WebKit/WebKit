/*	IFWebViewPrivate.mm
	Copyright 2001, Apple, Inc. All rights reserved.
        
        Private header file.  This file may reference classes (both ObjectiveC and C++)
        in WebCore.  Instances of this class are referenced by _viewPrivate in 
        NSWebPageView.
*/
#import <WebKit/WebKitDebug.h>

#import <WebKit/IFWebViewPrivate.h>
#import <WebKit/IFPluginView.h>

// Includes from KDE
#include <khtmlview.h>
#include <html/html_documentimpl.h>

@implementation IFWebViewPrivate

- init
{
    [super init];
    
    controller = nil;
    widget = 0;
    
    return self;
}


- (void)dealloc
{
    // controller is not retained!  IFWebControllers maintain
    // a reference to their view and main data source.

    [frameScrollView release];

    //if (widget)
    //    delete widget;

    [super dealloc];
}



@end


@implementation IFWebView  (IFPrivate)

- (void)_resetWidget
{
    if (((IFWebViewPrivate *)_viewPrivate)->widget)
        delete ((IFWebViewPrivate *)_viewPrivate)->widget;
    ((IFWebViewPrivate *)_viewPrivate)->widget = 0;
}

- (void)_stopPlugins 
{
    NSArray *views = [self subviews];
    int count;
    
    count = [views count];
    while (count--){
        id view;
        
        view = [views objectAtIndex: count];
        if ([view isKindOfClass: NSClassFromString (@"IFPluginView")])
            [(IFPluginView *)view stop];
    }
}


- (void)_removeSubviews
{
    // Remove all the views.  They will be be re-added if this
    // is a re-layout. 
    NSArray *views = [self subviews];
    int count;
    
    count = [views count];
    while (count--){
        id view;
        view = [views objectAtIndex: count];
        [view removeFromSuperviewWithoutNeedingDisplay]; 
    }
}



- (void)_setController: (id <IFWebController>)controller
{
    // Not retained.
    ((IFWebViewPrivate *)_viewPrivate)->controller = controller;    
}

- (KHTMLView *)_widget
{
    return ((IFWebViewPrivate *)_viewPrivate)->widget;    
}

- (DOM::DocumentImpl *)_document
{
    KHTMLPart *part = ((IFWebViewPrivate *)_viewPrivate)->widget->part();
    if (part){
        return part->xmlDocImpl();
    }
    return 0;
}

+ (NSString *)_nodeName: (DOM::NodeImpl *)node
{
    return [NSString stringWithCString:  node->nodeName().string().latin1()];
}

+ (NSString *)_nodeValue: (DOM::NodeImpl *)node
{
    return [NSString stringWithCString:  node->nodeValue().string().latin1()];
}

+ (NSString *)_nodeHTML: (DOM::NodeImpl *)node
{
    return QSTRING_TO_NSSTRING(node->recursive_toHTML(1));
}

- (khtml::RenderObject *)_renderRoot
{
    KHTMLPart *part = ((IFWebViewPrivate *)_viewPrivate)->widget->part();
    DOM::DocumentImpl *doc;
    if (part){
        doc = part->xmlDocImpl();
        if (doc)
            return doc->renderer();
    }
    return 0;
}


- (KHTMLView *)_provisionalWidget
{
    return ((IFWebViewPrivate *)_viewPrivate)->provisionalWidget;    
}


- (void)_setFrameScrollView: (NSScrollView *)sv
{
    ((IFWebViewPrivate *)_viewPrivate)->frameScrollView = [sv retain];    
    //[sv setAutoresizingMask: NSViewWidthSizable | NSViewHeightSizable];
    //[sv setHasVerticalScroller: YES];
    //[sv setHasHorizontalScroller: YES];
    [self setAutoresizingMask: NSViewWidthSizable | NSViewHeightSizable];
    [sv setDocumentView: self];
}

- (NSScrollView *)_frameScrollView
{
    return ((IFWebViewPrivate *)_viewPrivate)->frameScrollView;    
}

- (void)_setupScrollers
{
    BOOL scrollsVertically;
    BOOL scrollsHorizontally;

    if ([self _frameScrollView]){
        scrollsVertically = [self bounds].size.height > [[self _frameScrollView] frame].size.height;
        scrollsHorizontally = [self bounds].size.width > [[self _frameScrollView] frame].size.width;
    
        [[self _frameScrollView] setHasVerticalScroller: scrollsVertically];
        [[self _frameScrollView] setHasHorizontalScroller: scrollsHorizontally];
    }
}



@end
