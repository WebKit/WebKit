/*	
    WebAuthenticationChallenge.h
    Copyright 2003 Apple, Inc. All rights reserved.
*/

#import <WebFoundation/NSURLAuthenticationChallenge.h>

@class WebAuthenticationChallengeInternal;

@interface WebAuthenticationChallenge : NSURLAuthenticationChallenge
{
@private
    WebAuthenticationChallengeInternal *_webInternal;
}

- (void)useCredential:(NSURLCredential *)credential;
- (void)cancel;
- (void)continueWithoutCredential;

@end
