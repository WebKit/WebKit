//
//  WebRenderNode.h
//  WebKit
//
//  Created by Darin Adler on Tue Jun 11 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>

@class WebFrameView;

@interface WebRenderNode : NSObject
{
    NSArray *children;
    NSString *name;
    NSRect rect;
    NSPoint absolutePosition;
}

- (id)initWithWebFrameView:(WebFrameView *)view;

- (NSArray *)children;

- (NSString *)name;
- (NSString *)positionString;
- (NSString *)absolutePositionString;
- (NSString *)widthString;
- (NSString *)heightString;

@end
