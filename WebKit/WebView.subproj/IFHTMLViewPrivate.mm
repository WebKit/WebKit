/*	IFHTMLViewPrivate.mm
	Copyright 2002, Apple, Inc. All rights reserved.
        
        Private header file.  This file may reference classes (both ObjectiveC and C++)
        in WebCore.  Instances of this class are referenced by _private in 
        NSWebPageView.
*/
#import <WebKit/WebKitDebug.h>

#import <WebKit/IFHTMLViewPrivate.h>
#import <WebKit/IFPluginView.h>

// Includes from KDE
#import <khtmlview.h>
#import <khtml_part.h>
#import <html/html_documentimpl.h>
#import "IFWebController.h"

@implementation IFHTMLViewPrivate

- (void)dealloc
{
    [controller release];
    [cursor release];

    [super dealloc];
}

@end

@implementation IFHTMLView (IFPrivate)

- (void)_resetWidget
{
    delete _private->widget;
    _private->widget = 0;
}

- (void)_stopPlugins 
{
    NSArray *subviews = [[self subviews] copy];
    int count = [subviews count];
    while (count--) {
        id view = [subviews objectAtIndex:count];
        if ([view isKindOfClass:[IFPluginView class]]) {
            IFPluginView *pluginView = (IFPluginView *)view;
            [pluginView stop];
        }
    }
    [subviews release];
}

- (void)_removeSubviews
{
    // Remove all the views.  They will be be re-added if this is a re-layout. 
    NSArray *subviews = [[self subviews] copy];
    [subviews makeObjectsPerformSelector:@selector(removeFromSuperviewWithoutNeedingDisplay)];
    [subviews release];
}

- (void)_setController: (IFWebController *)controller
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
    NSString *string =  QSTRING_TO_NSSTRING(node->recursive_toHTML(1));
    return string;
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

@end
