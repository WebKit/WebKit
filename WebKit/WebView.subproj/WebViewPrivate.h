/*	
    WebViewPrivate.m
    Copyright 2001, Apple, Inc. All rights reserved.
*/
#import <WebKit/WebPolicyDelegate.h>
#import <WebKit/WebView.h>
#import <WebKit/WebFramePrivate.h>

@class NSError;
@class WebBackForwardList;
@class WebFrame;
@class WebPreferences;
@class WebCoreSettings;
@protocol WebFormDelegate;

typedef enum { Safari, MacIE, WinIE } UserAgentStringType;
enum { NumUserAgentStringTypes = WinIE + 1 };

#define NUM_LOCATION_CHANGE_DELEGATE_SELECTORS	10

typedef struct _WebResourceDelegateImplementationCache {
    uint delegateImplementsDidReceiveResponse:1;
    uint delegateImplementsDidReceiveContentLength:1;
    uint delegateImplementsDidFinishLoadingFromDataSource:1;
    uint delegateImplementsWillSendRequest:1;
    uint delegateImplementsIdentifierForRequest:1;
} WebResourceDelegateImplementationCache;

@interface WebViewPrivate : NSObject
{
@public
    WebFrame *mainFrame;
    
    id UIDelegate;
    id UIDelegateForwarder;
    id resourceProgressDelegate;
    id resourceProgressDelegateForwarder;
    id downloadDelegate;
    id policyDelegate;
    id policyDelegateForwarder;
    id frameLoadDelegate;
    id frameLoadDelegateForwarder;
    id <WebFormDelegate> formDelegate;
    
    WebBackForwardList *backForwardList;
    BOOL useBackForwardList;
    
    float textSizeMultiplier;

    NSString *applicationNameForUserAgent;
    NSString *userAgentOverride;
    NSString *userAgent[NumUserAgentStringTypes];
    
    BOOL defersCallbacks;

    NSString *controllerSetName;

    WebPreferences *preferences;
    WebCoreSettings *settings;
        
    BOOL lastElementWasNonNil;

    NSWindow *hostWindow;
    
    WebResourceDelegateImplementationCache resourceLoadDelegateImplementations;
}
@end

@interface WebView (WebPrivate)

/*!
    @method canShowFile:
    @abstract Checks if the WebKit can show the content of the file at the specified path.
    @param path The path of the file to check
    @result YES if the WebKit can show the content of the file at the specified path.
*/
+ (BOOL)canShowFile:(NSString *)path;

/*!
    @method suggestedFileExtensionForMIMEType:
    @param MIMEType The MIME type to check.
    @result The extension based on the MIME type
*/
+ (NSString *)suggestedFileExtensionForMIMEType: (NSString *)MIMEType;

/*!
    @method frameForDataSource:
    @abstract Return the frame associated with the data source.
    @discussion Traverses the frame tree to find the frame associated
    with a datasource.
    @param datasource The datasource to  match against each frame.
    @result The frame that has the associated datasource.
*/
- (WebFrame *)_frameForDataSource: (WebDataSource *)dataSource;

    /*!
            @method frameForView:
     @abstract Return the frame associated with the view.
     @discussion Traverses the frame tree to find the view.
     @param aView The view to match against each frame.
     @result The frame that has the associated view.
     */
- (WebFrame *)_frameForView: (WebFrameView *)aView;

- (WebFrame *)_createFrameNamed:(NSString *)name inParent:(WebFrame *)parent allowsScrolling:(BOOL)allowsScrolling;

- (void)_finishedLoadingResourceFromDataSource:(WebDataSource *)dataSource;
- (void)_receivedError:(NSError *)error fromDataSource:(WebDataSource *)dataSource;
- (void)_mainReceivedBytesSoFar:(unsigned)bytesSoFar fromDataSource:(WebDataSource *)dataSource complete:(BOOL)isComplete;
- (void)_mainReceivedError:(NSError *)error fromDataSource:(WebDataSource *)dataSource complete:(BOOL)isComplete;
+ (NSString *)_MIMETypeForFile:(NSString *)path;
- (void)_downloadURL:(NSURL *)URL;
- (void)_downloadURL:(NSURL *)URL toDirectory:(NSString *)directoryPath;

- (BOOL)defersCallbacks;
- (void)setDefersCallbacks:(BOOL)defers;

- (void)_setTopLevelFrameName:(NSString *)name;
- (WebFrame *)_findFrameInThisWindowNamed: (NSString *)name;
- (WebFrame *)_findFrameNamed: (NSString *)name;

- (WebView *)_openNewWindowWithRequest:(NSURLRequest *)request;

- (NSMenu *)_menuForElement:(NSDictionary *)element;

- (void)_mouseDidMoveOverElement:(NSDictionary *)dictionary modifierFlags:(unsigned)modifierFlags;

/*!
Could be worth adding to the API.
    @method loadItem:
    @abstract Loads the view with the contents described by the item, including frame content
        described by child items.
    @param item   The item to load.  It is not retained, but a copy will appear in the
        BackForwardList on this WebView.
*/
- (void)_loadItem:(WebHistoryItem *)item;
/*!
Could be worth adding to the API.
    @method loadItemsFromOtherView:
    @abstract Loads the view with the contents of the other view, including its backforward list.
    @param otherView   The WebView from which to copy contents.
*/
- (void)_loadBackForwardListFromOtherView:(WebView *)otherView;

- (void)_goToItem: (WebHistoryItem *)item withLoadType: (WebFrameLoadType)type;

// May well become public
- (void)_setFormDelegate: (id<WebFormDelegate>)delegate;
- (id<WebFormDelegate>)_formDelegate;

- (WebCoreSettings *)_settings;
- (void)_updateWebCoreSettingsFromPreferences: (WebPreferences *)prefs;

- _frameLoadDelegateForwarder;
- _resourceLoadDelegateForwarder;
- (void)_cacheResourceLoadDelegateImplementations;
- (WebResourceDelegateImplementationCache)_resourceLoadDelegateImplementations;
- _policyDelegateForwarder;
- _UIDelegateForwarder;

- (void)_registerDraggedTypes;
@end

@interface _WebSafeForwarder : NSObject
{
    id target;	// Non-retainted.  Don't retain delegates;
    id defaultTarget;
    Class templateClass;
}
- initWithTarget: t defaultTarget: dt templateClass: (Class)aClass;
+ safeForwarderWithTarget: t defaultTarget: dt templateClass: (Class)aClass;
@end

