//
//  WebTestController.m
//  WebKit
//
//  Created by Darin Adler on Thu Aug 08 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import "WebTestController.h"

#import <WebCore/WebCoreTestController.h>

@interface WebForwardingDrawingObserver : NSObject <WebCoreDrawingObserver>
{
    id <WebDrawingObserver> _target;
}

- initWithTarget:(id <WebDrawingObserver>)target;

@end

@implementation WebForwardingDrawingObserver

- initWithTarget:(id <WebDrawingObserver>)target
{
    self = [super init];
    if (self == nil) {
        return nil;
    }
    
    _target = [target retain];
    
    return self;
}

- (void)filledRect:(NSRect)r
{
}

- (void)strokedRect:(NSRect)r
{
}

- (void)filledOval:(NSRect)r
{
}

- (void)strokedOval:(NSRect)r
{
}

- (void)strokedArcWithCenter:(NSPoint)p radius:(float)radius startAngle:(float)startAngle endAngle:(float)endAngle
{
}

- (void)drewLineFrom:(NSPoint)p1 to:(NSPoint)p2 width:(float)width
{
}


@end

@implementation WebTestController

+ (void)setDrawingObserver:(id <WebDrawingObserver>)observer
{
    WebForwardingDrawingObserver *forwardingObserver =
        [[WebForwardingDrawingObserver alloc] initWithTarget:observer];
    [WebCoreTestController setDrawingObserver:forwardingObserver];
    [forwardingObserver release];
}

@end
