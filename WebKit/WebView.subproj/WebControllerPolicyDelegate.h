/*	
        WebControllerPolicyDelegate.h
	Copyright 2002, Apple, Inc. All rights reserved.

        Public header file.
*/

#import <Cocoa/Cocoa.h>

@class WebError;
@class WebFrame;
@class WebPolicyPrivate;
@class WebResponse;
@class WebRequest;


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
    @abstract Potential actions to take when loading a URL
    @constant WebPolicyNone Unitialized state.
    @constant WebPolicyUse Have WebKit use the resource.
    @constant WebPolicyRevealInFinder Reveal the file in the Finder.
    @constant WebPolicySave Save the resource to disk.
    @constant WebPolicyOpenURL Open the URL in another application.
    @constant WebPolicyOpenNewWindow Open the resource in another window.
    @constant WebPolicyOpenNewWindowBehind Open the resource in another window behind this window.
    @constant WebPolicyIgnore Do nothing with the resource.
    @constant WebPolicyShow Description forthcoming.
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
    @category WebControllerPolicyDelegate
    @discussion While loading a URL, WebKit asks the WebControllerPolicyDelegate for
    policies that determine the action of what to do with the URL or the data that
    the URL represents. Typically, the policy handler methods are called in this order:

    decideNavigationPolicyForAction:andRequest:inFrame:decisionListener:<BR>
    contentPolicyForMIMEType:andRequest:inFrame:<BR>
*/
@interface NSObject (WebControllerPolicyDelegate)

/*!
     @method decideNavigationPolicyForAction:andRequest:inFrame:decisionListener:
     @discussion Called right after the user clicks on a link.
     @param actionInformation Dictionary that describes the action that triggered this navigation.
     @param request The request for the proposed navigation
     @param frame The frame in which the navigation is taking place
     @param listener The object to call when the decision is made
*/
- (void)decideNavigationPolicyForAction:(NSDictionary *)actionInformation
                             andRequest:(WebRequest *)request
                                inFrame:(WebFrame *)frame
                       decisionListener:(WebPolicyDecisionListener *)listener;


/*!
    @method contentPolicyForMIMEType:andRequest:inFrame:
    @discussion Returns the policy for content which has been partially loaded. Sent after locationChangeStarted. 
    @param type MIME type for the resource.
    @param request A WebResourceRequest for the partially loaded content.
    @param frame The frame which is loading the URL.
*/
- (WebPolicyAction)contentPolicyForMIMEType:(NSString *)type
                                 andRequest:(WebRequest *)request
                                    inFrame:(WebFrame *)frame;

/*!
    @method unableToImplementPolicy:error:forURL:inFrame:
    @discussion Called when a WebPolicy could not be implemented. It is up to the client to display appropriate feedback.
    @param policy The policy that could not be implemented.
    @param error The error that caused the policy to not be implemented.
    @param URL The URL of the resource for which a particular action was requested but failed.
    @param frame The frame in which the policy could not be implemented.
*/
- (void)unableToImplementPolicyWithError:(WebError *)error inFrame:(WebFrame *)frame;

@end
