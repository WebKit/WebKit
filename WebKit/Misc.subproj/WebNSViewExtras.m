/*
    IFNSViewExtras.mm
	Copyright (c) 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/IFNSViewExtras.h>
#import <WebKit/IFWebView.h>


@implementation NSView (IFExtensions)

- (NSView *) _IF_superviewWithName:(NSString *)viewName
{
    NSView *view;
    
    view = self;
    while(view){
        view = [view superview];
        if([[view className] isEqualToString:viewName]){
            return view;
        }
    }
    return nil;
}

- (IFWebView *)_IF_parentWebView
{
    IFWebView *view = (IFWebView *)[[[self superview] superview] superview];
    
    if ([view isKindOfClass: [IFWebView class]])
        return view;
    return nil;
}

@end
