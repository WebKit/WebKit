/*	
    WebController.mm
	Copyright 2001, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebController.h>

@class WebError;
@class WebLoadProgress;
@class WebFrame;

@interface WebControllerPrivate : NSObject
{
@public
    WebFrame *mainFrame;
    id<WebWindowContext> windowContext;
    id<WebResourceProgressHandler> resourceProgressHandler;
    id<WebResourceProgressHandler> downloadProgressHandler;
    id<WebContextMenuHandler> contextMenuHandler;
    id<WebContextMenuHandler> defaultContextMenuHandler;
    id<WebControllerPolicyHandler> policyHandler;
    id<WebLocationChangeHandler> locationChangeHandler;
    WebBackForwardList *backForwardList;
    BOOL openedByScript;
}
@end

@interface WebController (WebPrivate);
- (id<WebContextMenuHandler>)_defaultContextMenuHandler;
- (void)_receivedProgress: (WebLoadProgress *)progress forResourceHandle: (WebResourceHandle *)resourceHandle fromDataSource: (WebDataSource *)dataSource complete:(BOOL)isComplete;
- (void)_receivedError: (WebError *)error forResourceHandle: (WebResourceHandle *)resourceHandle partialProgress: (WebLoadProgress *)progress fromDataSource: (WebDataSource *)dataSource;
- (void)_mainReceivedProgress: (WebLoadProgress *)progress forResourceHandle: (WebResourceHandle *)resourceHandle fromDataSource: (WebDataSource *)dataSource complete:(BOOL)isComplete;
- (void)_mainReceivedError: (WebError *)error forResourceHandle: (WebResourceHandle *)resourceHandle partialProgress: (WebLoadProgress *)progress fromDataSource: (WebDataSource *)dataSource;
- (void)_didStartLoading: (NSURL *)url;
- (void)_didStopLoading: (NSURL *)url;
+ (NSString *)_MIMETypeForFile: (NSString *)path;
- (BOOL)_openedByScript;
- (void)_setOpenedByScript:(BOOL)openedByScript;
@end
