/*	
    WebDebugDOMNode.h
    Copyright (c) 2002, Apple, Inc. All rights reserved.
*/


#import <Foundation/Foundation.h>

@class WebView;

@interface WebDebugDOMNode : NSObject
{
    NSArray *children;
    NSString *name;
    NSString *value;
    NSString *source;
}

- initWithWebView:(WebView *)view;

- (NSArray *)children;
- (NSString *)name;
- (NSString *)value;
- (NSString *)source;

@end
