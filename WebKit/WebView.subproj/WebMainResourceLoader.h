/*	
    WebMainResourceClient.h

    Private header.
    
    Copyright 2001, 2002 Apple Computer Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

#import <WebKit/WebBaseResourceHandleDelegate.h>

@class NSURLConnectionDelegateProxy;
@class WebPolicyDecisionListener;
@class WebDataSource;

@interface WebMainResourceClient : WebBaseResourceHandleDelegate
{
    int _contentLength; // for logging only
    int _bytesReceived; // for logging only
    WebPolicyDecisionListener *listener;
    NSURLResponse *policyResponse;
    NSURLConnectionDelegateProxy *proxy;
    NSURLRequest *_initialRequest;
}

- (id)initWithDataSource:(WebDataSource *)dataSource;

@end
