/*	
        WebPolicyDelegatePrivate.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/


#import <WebKit/WebPolicyDelegate.h>
#import <WebKit/WebFormDelegate.h>

typedef enum {
    WebPolicyUse,
    WebPolicyDownload,
    WebPolicyIgnore,
} WebPolicyAction;


@class WebPolicyDecisionListenerPrivate;

@interface WebPolicyDecisionListener : NSObject <WebPolicyDecisionListener, WebFormSubmissionListener>
{
@private
    WebPolicyDecisionListenerPrivate *_private;
}

- (id)_initWithTarget:(id)target action:(SEL)action;

- (void)_invalidate;

@end
