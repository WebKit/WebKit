/*	
    WebController.m
	Copyright 2001, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebController.h>
#import <WebKit/WebControllerPolicyDelegate.h>

@class WebError;
@class WebFrame;

@interface WebControllerPrivate : NSObject
{
@public
    WebFrame *mainFrame;
    
    id <WebWindowOperationsDelegate> windowContext;
    id <WebResourceLoadDelegate> resourceProgressDelegate;
    id <WebResourceLoadDelegate> downloadProgressDelegate;
    id <WebContextMenuDelegate> contextMenuDelegate;
    id <WebControllerPolicyDelegate> policyDelegate;
    id <WebLocationChangeDelegate> locationChangeDelegate;
    
    id <WebContextMenuDelegate> defaultContextMenuDelegate;

    WebBackForwardList *backForwardList;
    BOOL useBackForwardList;
    
    float textSizeMultiplier;

    NSString *applicationNameForUserAgent;
    NSString *userAgentOverride;
    NSLock *userAgentLock;
    
    BOOL defersCallbacks;

    NSString *controllerSetName;
    NSString *topLevelFrameName;
}
@end

@interface WebController (WebPrivate)

- (WebFrame *)_createFrameNamed:(NSString *)name inParent:(WebFrame *)parent allowsScrolling:(BOOL)allowsScrolling;

- (id <WebContextMenuDelegate>)_defaultContextMenuDelegate;
- (void)_receivedProgressForResourceHandle:(WebResourceHandle *)resourceHandle fromDataSource:(WebDataSource *)dataSource complete:(BOOL)isComplete;
- (void)_receivedError:(WebError *)error forResourceHandle:(WebResourceHandle *)resourceHandle fromDataSource:(WebDataSource *)dataSource;
- (void)_mainReceivedProgressForResourceHandle:(WebResourceHandle *)resourceHandle bytesSoFar:(unsigned)bytesSoFar fromDataSource:(WebDataSource *)dataSource complete:(BOOL)isComplete;
- (void)_mainReceivedError:(WebError *)error forResourceHandle:(WebResourceHandle *)resourceHandle fromDataSource:(WebDataSource *)dataSource;
- (void)_didStartLoading:(NSURL *)URL;
- (void)_didStopLoading:(NSURL *)URL;
+ (NSString *)_MIMETypeForFile:(NSString *)path;
- (void)_downloadURL:(NSURL *)URL withContentPolicy:(WebContentPolicy *)contentPolicy;

- (BOOL)_defersCallbacks;
- (void)_setDefersCallbacks:(BOOL)defers;

- (void)_setTopLevelFrameName:(NSString *)name;
- (WebFrame *)_frameInThisWindowNamed:(NSString *)name;

- (WebController *)_openNewWindowWithURL:(NSURL *)URL referrer:(NSString *)referrer behind:(BOOL)behind;

- (NSMenu *)_menuForElement:(NSDictionary *)element;
@end
