/*	IFHTMLViewPrivate.mm
	Copyright 2002, Apple, Inc. All rights reserved.
        
        Private header file.  This file may reference classes (both ObjectiveC and C++)
        in WebCore.  Instances of this class are referenced by _private in 
        NSWebPageView.
*/
#import <WebKit/WebKitDebug.h>

#import <WebKit/IFImageRenderer.h>
#import <WebKit/IFHTMLViewPrivate.h>
#import <WebKit/IFPluginView.h>

// Includes from KDE
#import <khtmlview.h>

@implementation IFHTMLViewPrivate

- (void)dealloc
{
    [cursor release];

    [super dealloc];
}

@end

@implementation IFHTMLView (IFPrivate)

- (void)_reset
{
    NSArray *subviews = [[self subviews] copy];

    [IFImageRenderer stopAnimationsInView: self];
    
    int count = [subviews count];
    while (count--) {
        id view = [subviews objectAtIndex:count];
        if ([view isKindOfClass:[IFPluginView class]]) {
            IFPluginView *pluginView = (IFPluginView *)view;
            [pluginView stop];
        }
    }
    [subviews release];

    delete _private->provisionalWidget;
    _private->provisionalWidget = 0;
    if (_private->widgetOwned)
        delete _private->widget;
    _private->widget = 0;
    _private->widgetOwned = NO;
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

- (KHTMLView *)_provisionalWidget
{
    return _private->provisionalWidget;    
}

- (void)_takeOwnershipOfWidget
{
    _private->widgetOwned = NO;
}

@end
