//
//  NSHIViewAdapter.h
//  Synergy
//
//  Created by Ed Voas on Mon Jan 20 2003.
//  Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
//

#import <WebKit/WebKit.h>
#include <HIToolbox/HIView.h>

@interface HIViewAdapter : NSView {
//	HIViewRef		_hiView;
}

+ (void)bindHIViewToNSView:(HIViewRef)hiView nsView:(NSView*)nsView;
+ (void)unbindNSView:(NSView*)nsView;
+ (HIViewRef)getHIViewForNSView:(NSView*)inView;
//- (void)mouseIsDown;
//- (void)mouseIsUp;

//- (id)initWithFrame:(NSRect)frame view:(HIViewRef) inView;
//- (HIViewRef)hiView;

@end
