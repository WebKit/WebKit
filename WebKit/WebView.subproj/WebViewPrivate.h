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
    BOOL useBackForwardList;
    
    float textSizeMultiplier;

    NSString *applicationNameForUserAgent;
    NSString *userAgentOverride;
    NSLock *userAgentLock;
    
    BOOL defersCallbacks;
}
@end

@interface WebController (WebPrivate);

- (id<WebContextMenuHandler>)_defaultContextMenuHandler;
- (void)_receivedProgress: (WebLoadProgress *)progress forResourceHandle: (WebResourceHandle *)resourceHandle fromDataSource: (WebDataSource *)dataSource complete:(BOOL)isComplete;
- (void)_receivedError: (WebError *)error forResourceHandle: (WebResourceHandle *)resourceHandle partialProgress: (WebLoadProgress *)progress fromDataSource: (WebDataSource *)dataSource;
- (void)_mainReceivedProgress: (WebLoadProgress *)progress forResourceHandle: (WebResourceHandle *)resourceHandle fromDataSource: (WebDataSource *)dataSource complete:(BOOL)isComplete;
- (void)_mainReceivedError: (WebError *)error forResourceHandle: (WebResourceHandle *)resourceHandle partialProgress: (WebLoadProgress *)progress fromDataSource: (WebDataSource *)dataSource;
- (void)_didStartLoading: (NSURL *)URL;
- (void)_didStopLoading: (NSURL *)URL;
+ (NSString *)_MIMETypeForFile: (NSString *)path;
- (void)_downloadURL:(NSURL *)URL toPath:(NSString *)path;

- (BOOL)_defersCallbacks;
- (void)_setDefersCallbacks:(BOOL)defers;

@end
