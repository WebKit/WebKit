//
//  WebNSWindowExtras.h
//  WebKit
//
//  Created by Darin Adler on Tue Oct 22 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@interface NSWindow (WebExtras)
- (void)centerOverMainWindow; // centers "visually", putting 1/3 of the remaining space above, and 2/3 below
@end
