/*	
    WebDebugDOMNode.h
    Copyright (c) 2002, Apple, Inc. All rights reserved.
*/


#import <Foundation/Foundation.h>

@class WebFrameView;

@interface WebDebugDOMNode : NSObject
{
    NSArray *children;
    NSString *name;
    NSString *value;
    NSString *source;
}

- (id)initWithWebFrameView:(WebFrameView *)view;

- (NSArray *)children;
- (NSString *)name;
- (NSString *)value;
- (NSString *)source;

@end
