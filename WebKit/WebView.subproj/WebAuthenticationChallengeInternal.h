/*	
    WebAuthenticationChallengeInternal.h
    Copyright 2003 Apple, Inc. All rights reserved.
*/

#import <WebKit/WebAuthenticationChallenge.h>

@class WebBaseResourceHandleDelegate;

@interface WebAuthenticationChallenge (Internal)

- (id)_initWithAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge delegate:(WebBaseResourceHandleDelegate *)delegate;

@end
