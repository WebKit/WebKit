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
  @constant WebNavigationTypeLinkClicked A link with an href was clicked.
  @constant WebNavigationTypeFormSubmitted A form was submitted.
  @constant WebNavigationTypeBackForward The user chose back or forward.
  @constant WebNavigationTypeReload The User hit the reload button.
  @constant WebNavigationTypeFormResubmitted A form was resubmitted (by virtue of doing back, forward or reload).
  @constant WebNavigationTypeOther Navigation is taking place for some other reason.
*/

typedef enum {
    WebNavigationTypeLinkClicked,
    WebNavigationTypeFormSubmitted,
    WebNavigationTypeBackForward,
    WebNavigationTypeReload,
    WebNavigationTypeFormResubmitted,
    WebNavigationTypeOther
} WebNavigationType;


extern NSString *WebActionNavigationTypeKey; // NSNumber (WebActionType)
extern NSString *WebActionElementKey; // NSDictionary of element info
extern NSString *WebActionButtonKey;  // NSEventType
extern NSString *WebActionModifierFlagsKey; // NSNumber (unsigned)
extern NSString *WebActionOriginalURLKey; // NSURL


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
    WebPolicyIgnore,
    WebPolicyShow
} WebPolicyAction;


@class WebPolicyDecisionListenerPrivate;

@interface WebPolicyDecisionListener : NSObject
{
@private
    WebPolicyDecisionListenerPrivate *_private;
}

-(void)usePolicy:(WebPolicyAction) policy;

@end


/*!
    @protocol WebControllerPolicyDelegate
    @discussion While loading a URL, WebKit asks the WebControllerPolicyDelegate for
    policies that determine the action of what to do with the URL or the data that
    the URL represents. Typically, the policy handler methods are called in this order:

    navigationPolicyForAction:andRequest:inFrame:<BR>
    contentPolicyForMIMEType:andRequest:inFrame:<BR>
*/
@protocol WebControllerPolicyDelegate <NSObject>

/*!
     @method decideNavigationPolicyForAction:andRequest:inFrame:
     @discussion Called right after the user clicks on a link.
     @param actionInformation Dictionary that describes the action that triggered this navigation.
     @param andRequest The request for the proposed navigation
     @param frame The frame in which the navigation is taking place
     @param listener The object to call when the decision is made
*/
- (void)decideNavigationPolicyForAction:(NSDictionary *)actionInformation
                                        andRequest:(WebResourceRequest *)request
                                           inFrame:(WebFrame *)frame
                                  decisionListener:(WebPolicyDecisionListener *)listener;


/*!
    @method contentPolicyForResponse:andRequest:inFrame:withContentPolicy:
    @discussion Returns the policy for content which has been partially loaded. Sent after locationChangeStarted. 
    @param type MIME type for the file.
    @param request A WebResourceRequest for the partially loaded content.
    @param frame The frame which is loading the URL.
*/
- (WebPolicyAction)contentPolicyForMIMEType:(NSString *)type
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
