/*	
    WebMainResourceClient.h

    Private header.
    
    Copyright 2001, 2002 Apple Computer Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

#import <WebKit/WebBaseResourceHandleDelegate.h>

@class WebDataSource;

@interface WebResourceDelegateProxy : NSObject <WebResourceDelegate>
{
    id <WebResourceDelegate> delegate;
}
- (void)setDelegate:(id <WebResourceDelegate>)theDelegate;
@end

@interface WebMainResourceClient : WebBaseResourceHandleDelegate
{
    NSMutableData *resourceData;
    int _contentLength; // for logging only
    int _bytesReceived; // for logging only
    WebResourceDelegateProxy *proxy;
}

- initWithDataSource:(WebDataSource *)dataSource;
- (NSData *)resourceData;

@end
