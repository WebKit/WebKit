//
//  NSViewExtras.h
//  WebKit
//
//  Created by Chris Blumenberg on Tue Jun 11 2002.
//  Copyright (c) 2002 __MyCompanyName__. All rights reserved.
//

#import <AppKit/AppKit.h>


@interface NSView (IFExtensions)

- (NSView *) _IF_superviewWithName:(NSString *)viewName;

@end
