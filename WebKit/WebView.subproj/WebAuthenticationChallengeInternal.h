/*	
    WebAuthenticationChallenge.h
    Copyright (C) 2003 Apple Computer, Inc. All rights reserved.    
*/

#import <WebKit/WebAuthenticationChallenge.h>

@class WebBaseResourceHandleDelegate;

@interface WebAuthenticationChallenge (Internal)

- (id)_initWithAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge delegate:(WebBaseResourceHandleDelegate *)delegate;

@end
