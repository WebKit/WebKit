/*	
    WebController.m
	Copyright 2001, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebController.h>
#import <WebKit/WebControllerPolicyDelegate.h>

@class WebError;
@class WebFrame;
@class WebPreferences;
@class WebCoreSettings;
@protocol WebFormDelegate;

typedef enum { Safari, MacIE, WinIE } UserAgentStringType;
enum { NumUserAgentStringTypes = WinIE + 1 };

#define NUM_LOCATION_CHANGE_DELEGATE_SELECTORS	10

@interface WebControllerPrivate : NSObject
{
@public
    WebFrame *mainFrame;
    
    id windowContext;
    id resourceProgressDelegate;
    id downloadProgressDelegate;
    id contextMenuDelegate;
    id policyDelegate;
    id locationChangeDelegate;
    id <WebFormDelegate> formDelegate;
    
    id defaultContextMenuDelegate;

    WebBackForwardList *backForwardList;
    BOOL useBackForwardList;
    
    float textSizeMultiplier;

    NSString *applicationNameForUserAgent;
    NSString *userAgentOverride;
    NSString *userAgent[NumUserAgentStringTypes];
    
    BOOL defersCallbacks;

    NSString *controllerSetName;
    NSString *topLevelFrameName;

    WebPreferences *preferences;
    WebCoreSettings *settings;
        
    BOOL lastElementWasNonNil;
}
@end

@interface WebController (WebPrivate)

- (WebFrame *)_createFrameNamed:(NSString *)name inParent:(WebFrame *)parent allowsScrolling:(BOOL)allowsScrolling;

- (void)_finishedLoadingResourceFromDataSource:(WebDataSource *)dataSource;
- (void)_receivedError:(WebError *)error fromDataSource:(WebDataSource *)dataSource;
- (void)_mainReceivedBytesSoFar:(unsigned)bytesSoFar fromDataSource:(WebDataSource *)dataSource complete:(BOOL)isComplete;
- (void)_mainReceivedError:(WebError *)error fromDataSource:(WebDataSource *)dataSource complete:(BOOL)isComplete;
+ (NSString *)_MIMETypeForFile:(NSString *)path;
+ (NSArray *)_supportedImageMIMETypes;
- (void)_downloadURL:(NSURL *)URL;
- (void)_downloadURL:(NSURL *)URL toDirectory:(NSString *)directoryPath;

- (BOOL)defersCallbacks;
- (void)setDefersCallbacks:(BOOL)defers;

- (void)_setTopLevelFrameName:(NSString *)name;
- (WebFrame *)_findFrameInThisWindowNamed: (NSString *)name;
- (WebFrame *)_findFrameNamed: (NSString *)name;

- (WebController *)_openNewWindowWithRequest:(WebRequest *)request behind:(BOOL)behind;

- (NSMenu *)_menuForElement:(NSDictionary *)element;

- (void)_mouseDidMoveOverElement:(NSDictionary *)dictionary modifierFlags:(unsigned)modifierFlags;

// May well become public
- (void)_setFormDelegate: (id<WebFormDelegate>)delegate;
- (id<WebFormDelegate>)_formDelegate;

- (WebCoreSettings *)_settings;
- (void)_updateWebCoreSettingsFromPreferences: (WebPreferences *)prefs;

- _locationChangeDelegateForwarder;
- _resourceLoadDelegateForwarder;
- _policyDelegateForwarder;
- _contextMenuDelegateForwarder;
- _windowOperationsDelegateForwarder;
@end

@interface _WebSafeForwarder : NSObject
{
    id target;	// Non-retainted.  Don't retain delegates;
    id defaultTarget;
    Class templateClass;
}
+ safeForwarderWithTarget: t defaultTarget: dt templateClass: (Class)aClass;
@end

