/*	
    WebMainResourceClient.h

    Private header.
    
    Copyright 2001, 2002 Apple Computer Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

#import <WebKit/WebBaseResourceHandleDelegate.h>

@class WebPolicyDecisionListener;
@class WebDataSource;

@interface WebResourceDelegateProxy : NSObject
{
    id delegate;
}
- (void)setDelegate:(id)theDelegate;
@end

@interface WebMainResourceClient : WebBaseResourceHandleDelegate
{
    int _contentLength; // for logging only
    int _bytesReceived; // for logging only
    WebPolicyDecisionListener *listener;
    NSURLResponse *policyResponse;
    WebResourceDelegateProxy *proxy;
}

- initWithDataSource:(WebDataSource *)dataSource;

@end
