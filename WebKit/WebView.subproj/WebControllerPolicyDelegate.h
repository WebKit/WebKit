/*	
        WebControllerPolicyDelegate.h
	Copyright 2002, Apple, Inc. All rights reserved.

        Public header file.
*/

#import <Cocoa/Cocoa.h>

@class WebError;
@class WebFrame;
@class WebPolicyPrivate;
@class WebResourceResponse;
@class WebResourceRequest;


/*!
  @enum WebNavigationType
  @abstract The type of action that triggered a possible navigation.
  @constant WebActionTypeLinkClicked A link with an href was clicked.
  @constant WebActionTypeFormSubmitted A form was submitted.
  @constant WebNavigationTypeOther Navigation is taking place for some other reason.
*/

typedef enum {
    WebNavigationTypeLinkClicked,
    WebNavigationTypeFormSubmitted,
    WebNavigationTypeOther
} WebNavigationType;


extern NSString *WebActionNavigationTypeKey; // NSNumber (WebActionType)
extern NSString *WebActionElementKey; // NSDictionary of element info
extern NSString *WebActionButtonKey;  // NSEventType
extern NSString *WebActionModifierFlagsKey; // NSNumber (unsigned)


/*!
    @enum WebPolicyAction
    @constant WebPolicyNone Unitialized state.
    @constant WebPolicyUse Have WebKit use the resource.
    @constant WebPolicyRevealInFinder Reveal the file in the Finder.
    @constant WebPolicySave Save the resource to disk.
    @constant WebPolicyOpenURL Open the URL in another application.
    @constant WebPolicyOpenNewWindow Open the resource in another window.
    @constant WebPolicyOpenNewWindowBehind Open the resource in another window behind this window.
    @constant WebPolicyIgnore Do nothing with the resource.
*/
typedef enum {
    WebPolicyNone,
    WebPolicyUse,
    WebPolicyRevealInFinder,
    WebPolicySave,
    WebPolicyOpenURL,
    WebPolicyOpenNewWindow,
    WebPolicyOpenNewWindowBehind,
    WebPolicyIgnore
} WebPolicyAction;

/*!
@enum WebClickAction
 @constant WebClickPolicyShow Have WebKit show the clicked URL.
 @constant WebClickPolicyOpenNewWindow Open the clicked URL in another window.
 @constant WebClickPolicyOpenNewWindowBehind Open the clicked URL in another window behind this window.
 @constant WebClickPolicySave Save the clicked URL to disk.
 @constant WebClickPolicyIgnore Do nothing with the clicked URL.
 */
typedef enum {
    WebClickPolicyShow = WebPolicyUse,
    WebClickPolicyOpenExternally = WebPolicyOpenURL,
    WebClickPolicyOpenNewWindow = WebPolicyOpenNewWindow,
    WebClickPolicyOpenNewWindowBehind = WebPolicyOpenNewWindowBehind,
    WebClickPolicySave = WebPolicySave,
    WebClickPolicyIgnore = WebPolicyIgnore
} WebClickAction;

/*!
    @enum WebURLAction
    @constant WebURLPolicyUseContentPolicy Continue processing URL, ask for content policy.
    @constant WebURLPolicyOpenExternally Open the URL in another application. 
    @constant WebURLPolicyIgnore Do nothing with the URL.
*/
typedef enum {
    WebURLPolicyUseContentPolicy = WebPolicyUse,
    WebURLPolicyOpenExternally = WebPolicyOpenURL,
    WebURLPolicyIgnore = WebPolicyIgnore
} WebURLAction;

/*!
    @enum WebFileAction
    @constant WebFileURLPolicyUseContentPolicy Continue processing the file, ask for content policy.
    @constant WebFileURLPolicyOpenExternally Open the file in another application.
    @constant WebFileURLPolicyRevealInFinder Reveal the file in the Finder.
    @constant WebFileURLPolicyIgnore Do nothing with the file.
*/
typedef enum {
    WebFileURLPolicyUseContentPolicy = WebPolicyUse,
    WebFileURLPolicyOpenExternally = WebPolicyOpenURL,
    WebFileURLPolicyRevealInFinder = WebPolicyRevealInFinder,
    WebFileURLPolicyIgnore = WebPolicyIgnore
} WebFileAction;

/*!
    @enum WebContentAction
    @constant WebContentPolicyNone Unitialized state.
    @constant WebContentPolicyShow Show the content in WebKit.
    @constant WebContentPolicySave Save the content to disk.
    @constant WebContentPolicyIgnore Do nothing with the content.
*/
typedef enum {
    WebContentPolicyNone = WebPolicyNone,
    WebContentPolicyShow = WebPolicyUse,
    WebContentPolicySave = WebPolicySave,
    WebContentPolicyIgnore = WebPolicyIgnore
} WebContentAction;



/*!
    @protocol WebControllerPolicyDelegate
    @discussion While loading a URL, WebKit asks the WebControllerPolicyDelegate for
    policies that determine the action of what to do with the URL or the data that
    the URL represents. Typically, the policy handler methods are called in this order:

    clickPolicyForElement:button:modifierFlags:<BR>
    URLPolicyForRequest:inFrame:<BR>
    fileURLPolicyForMIMEType:andRequest:inFrame:<BR>
    contentPolicyForMIMEType:andRequest:inFrame:<BR>
*/
@protocol WebControllerPolicyDelegate <NSObject>

/*!
     @method clickPolicyForElement:button:modifierFlags:
     @discussion Called right after the user clicks on a link.
     @param elementInformation Dictionary that describes the clicked element.
     @param eventType The type of event.
     @param modifierFlags The modifier flags as described in NSEvent.h.
     @result The WebClickPolicy for WebKit to implement
*/
- (WebClickAction)clickPolicyForAction:(NSDictionary *)actionInformation
                              andRequest:(WebResourceRequest *)request
                                 inFrame:(WebFrame *)frame;

/*!
    @method URLPolicyForURL:inFrame:
    @discussion URLPolicyForURL: is used to determine what to do BEFORE a URL is loaded, i.e.
    before it is clicked or loaded via a URL bar.  Clients can choose to handle the
    URL normally, hand the URL off to launch services, or
    ignore the URL.  The default implementation could return +defaultURLPolicyForURL:.
    @param URL The URL that WebKit has been asked to load.
    @param frame The frame which will load the URL.
*/
- (WebURLAction)URLPolicyForRequest:(WebResourceRequest *)request inFrame:(WebFrame *)frame;

/*!
     @method fileURLPolicyForMIMEType:inFrame:isDirectory:
     @discussion Called when the response to URLPolicyForURL is WebURLPolicyUseContentPolicy and the URL is
     a file URL. This allows clients to special-case WebKit's behavior for file URLs.
     @param type MIME type for the file.
     @param request WebResourceRequest to be used to load the item
     @param frame The frame which will load the file.
*/
- (WebFileAction)fileURLPolicyForMIMEType:(NSString *)type
                               andRequest:(WebResourceRequest *)request
                                  inFrame:(WebFrame *)frame;

/*!
    @method contentPolicyForResponse:andRequest:inFrame:withContentPolicy:
    @discussion Returns the policy for content which has been partially loaded. Sent after locationChangeStarted. 
    @param response The response for the partially loaded content.
    @param request A WebResourceRequest for the partially loaded content.
    @param frame The frame which is loading the URL.
*/
- (WebContentAction)contentPolicyForResponse:(WebResourceResponse *)response
                                    andRequest:(WebResourceRequest *)request
                                       inFrame:(WebFrame *)frame;


/*!
    @method saveFilenameForResponse:andRequest:
    @discussion Returns the filename to use to for a load that's being saved.
    @param response The response for the partially loaded content.
    @param request A WebResourceRequest for the partially loaded content.
*/
- (NSString *)saveFilenameForResponse:(WebResourceResponse *)response
                           andRequest:(WebResourceRequest *)request;


/*!
    @method unableToImplementPolicy:error:forURL:inFrame:
    @discussion Called when a WebPolicy could not be implemented. It is up to the client to display appropriate feedback.
    @param policy The policy that could not be implemented.
    @param error The error that caused the policy to not be implemented.
    @param frame The frame in the which the policy could not be implemented.
*/
- (void)unableToImplementPolicy:(WebPolicyAction)policy error:(WebError *)error forURL:(NSURL *)URL inFrame:(WebFrame *)frame;

@end


