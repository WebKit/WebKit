/*	
        WebPolicyDelegatePrivate.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/


#import <WebKit/WebPolicyDelegate.h>
#import <WebKit/WebFormDelegate.h>

@class WebHistoryItem;

typedef enum {
    WebNavigationTypePlugInRequest =  WebNavigationTypeOther + 1
} WebExtraNavigationType;

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

@interface NSObject (WebPolicyDelegatePrivate)
// Temporary SPI needed for <rdar://problem/3951283> can view pages from the back/forward cache that should be disallowed by Parental Controls
- (BOOL)webView:(WebView *)webView shouldGoToHistoryItem:(WebHistoryItem *)item;
@end
