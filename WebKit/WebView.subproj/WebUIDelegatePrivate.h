/*
    WebUIDelegatePrivate.h
    Copyright (C) 2003 Apple Computer, Inc. All rights reserved.    
*/

#import <WebKit/WebUIDelegate.h>

/*!
    @enum WebDragDestinationAction
    @abstract Actions that the destination of a drag can perform.
    @constant WebDragDestinationActionNone No action
    @constant WebDragDestinationActionDHTML Allows DHTML (such as JavaScript) to handle the drag
    @constant WebDragDestinationActionEdit Allows editable documents to be edited from the drag
    @constant WebDragDestinationActionLoad Allows a location change from the drag
    @constant WebDragDestinationActionAny Allows any of the above to occur
*/
typedef enum {
    WebDragDestinationActionNone    = 0,
    WebDragDestinationActionDHTML   = 1,
    WebDragDestinationActionEdit    = 2,
    WebDragDestinationActionLoad    = 4,
    WebDragDestinationActionAny     = UINT_MAX
} WebDragDestinationAction;

/*!
    @enum WebDragSourceAction
    @abstract Actions that the source of a drag can perform.
    @constant WebDragSourceActionNone No action
    @constant WebDragSourceActionDHTML Allows DHTML (such as JavaScript) to start a drag
    @constant WebDragSourceActionImage Allows an image drag to occur
    @constant WebDragSourceActionLink Allows a link drag to occur
    @constant WebDragSourceActionSelection Allows a selection drag to occur
    @constant WebDragSourceActionAny Allows any of the above to occur
*/
typedef enum {
    WebDragSourceActionNone         = 0,
    WebDragSourceActionDHTML        = 1,
    WebDragSourceActionImage        = 2,
    WebDragSourceActionLink         = 4,
    WebDragSourceActionSelection    = 8,
    WebDragSourceActionAny          = UINT_MAX
} WebDragSourceAction;

@interface NSObject (WebUIDelegatePrivate)

- (void)webViewPrint:(WebView *)sender;

- (float)webViewHeaderHeight:(WebView *)sender;
- (float)webViewFooterHeight:(WebView *)sender;
- (void)webView:(WebView *)sender drawHeaderInRect:(NSRect)rect;
- (void)webView:(WebView *)sender drawFooterInRect:(NSRect)rect;
- (void)webView:(WebView *)webView addMessageToConsole:(NSDictionary *)message;

/*!
    @method webView:dragDestinationActionMaskForDraggingInfo:
    @abstract Controls behavior when dragging to a WebView
    @param webView The WebView sending the delegate method
    @param draggingInfo The dragging info of the drag
    @discussion This method is called periodically as something is dragged over a WebView. The UI delegate can return a mask
    indicating which drag destination actions can occur, WebDragDestinationActionAny to allow any kind of action or
    WebDragDestinationActionNone to not accept the drag.
*/
- (unsigned)webView:(WebView *)webView dragDestinationActionMaskForDraggingInfo:(id <NSDraggingInfo>)draggingInfo;

/*!
    @method webView:willPerformDragDestinationAction:forDraggingInfo:
    @abstract Informs that WebView will perform a drag destination action
    @param webView The WebView sending the delegate method
    @param action The drag destination action
    @param draggingInfo The dragging info of the drag
    @discussion This method is called after the last call to webView:dragDestinationActionMaskForDraggingInfo: after something is dropped on a WebView.
    This method informs the UI delegate of the drag destination that WebView will perform.
*/
- (void)webView:(WebView *)webView willPerformDragDestinationAction:(WebDragDestinationAction)action forDraggingInfo:(id <NSDraggingInfo>)draggingInfo;

/*!
    @method webView:dragSourceActionMaskForPoint:
    @abstract Controls behavior when dragging from a WebView
    @param webView The WebView sending the delegate method
    @param point The point where the drag started in the coordinates of the WebView
    @discussion This method is called after the user has begun a drag from a WebView. The UI delegate can return a mask indicating
    which drag source actions can occur, WebDragSourceActionAny to allow any kind of action or WebDragSourceActionNone to not begin a drag.
*/
- (unsigned)webView:(WebView *)webView dragSourceActionMaskForPoint:(NSPoint)point;

/*!
    @method webView:willPerformDragSourceAction:fromPoint:withPasteboard:
    @abstract Informs that a drag a has begun from a WebView
    @param webView The WebView sending the delegate method
    @param action The drag source action
    @param point The point where the drag started in the coordinates of the WebView
    @param pasteboard The drag pasteboard
    @discussion This method is called after webView:dragSourceActionMaskForPoint: is called after the user has begun a drag from a WebView.
    This method informs the UI delegate of the drag source action that will be performed and gives the delegate an opportunity to modify
    the contents of the dragging pasteboard.
*/
- (void)webView:(WebView *)webView willPerformDragSourceAction:(WebDragSourceAction)action fromPoint:(NSPoint)point withPasteboard:(NSPasteboard *)pasteboard;

@end
