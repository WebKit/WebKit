//
//  IFDOMNode.h
//  WebKit
//
//  Created by Darin Adler on Tue Jun 11 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>

@class IFWebView;

@interface IFDOMNode : NSObject
{
    NSArray *children;
    NSString *name;
    NSString *value;
    NSString *source;
}

- initWithWebView:(IFWebView *)view;

- (NSArray *)children;
- (NSString *)name;
- (NSString *)value;
- (NSString *)source;

@end
