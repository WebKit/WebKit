//
//  WebDOMNode.h
//  WebKit
//
//  Created by Darin Adler on Tue Jun 11 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>

@class WebView;

@interface WebDOMNode : NSObject
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
