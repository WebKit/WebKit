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
    
    BOOL lastElementWasNonNil;
}
@end

@interface WebController (WebPrivate)

- (WebFrame *)_createFrameNamed:(NSString *)name inParent:(WebFrame *)parent allowsScrolling:(BOOL)allowsScrolling;

- (id <WebContextMenuDelegate>)_defaultContextMenuDelegate;
- (void)_finishedLoadingResourceFromDataSource:(WebDataSource *)dataSource;
- (void)_receivedError:(WebError *)error fromDataSource:(WebDataSource *)dataSource;
- (void)_mainReceivedBytesSoFar:(unsigned)bytesSoFar fromDataSource:(WebDataSource *)dataSource complete:(BOOL)isComplete;
- (void)_mainReceivedError:(WebError *)error fromDataSource:(WebDataSource *)dataSource;
+ (NSString *)_MIMETypeForFile:(NSString *)path;
+ (NSArray *)_supportedImageMIMETypes;
- (void)_downloadURL:(NSURL *)URL;
- (void)_downloadURL:(NSURL *)URL toPath:(NSString *)path;

- (BOOL)_defersCallbacks;
- (void)_setDefersCallbacks:(BOOL)defers;

- (void)_setTopLevelFrameName:(NSString *)name;
- (WebFrame *)_findFrameInThisWindowNamed: (NSString *)name;
- (WebFrame *)_findFrameNamed: (NSString *)name;

- (WebController *)_openNewWindowWithRequest:(WebResourceRequest *)request behind:(BOOL)behind;

- (NSMenu *)_menuForElement:(NSDictionary *)element;

- (void)_mouseDidMoveOverElement:(NSDictionary *)dictionary modifierFlags:(unsigned)modifierFlags;

@end
