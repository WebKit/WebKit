//
//  NSViewExtras.m
//  WebKit
//
//  Created by Chris Blumenberg on Tue Jun 11 2002.
//  Copyright (c) 2002 __MyCompanyName__. All rights reserved.
//

#import "IFNSViewExtras.h"


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

@end
