/*
    WebPluginViewFactory.h
    Copyright 2004, Apple, Inc. All rights reserved.
    
    Public header file.
*/

#import <Cocoa/Cocoa.h>

/*!
    @constant WebPlugInBaseURLKey REQUIRED. The base URL of the document containing
    the plug-in's view.
*/
extern NSString *WebPlugInBaseURLKey;

/*!
    @constant WebPlugInAttributesKey REQUIRED. The dictionary containing the names
    and values of all attributes of the HTML element associated with the plug-in AND
    the names and values of all parameters to be passed to the plug-in (e.g. PARAM
    elements within an APPLET element). In the case of a conflict between names,
    the attributes of an element take precedence over any PARAMs.  All of the keys
    and values in this NSDictionary must be NSStrings.
*/
extern NSString *WebPlugInAttributesKey;

/*!
    @constant WebPlugInContainer OPTIONAL. An object that conforms to the
    WebPlugInContainer informal protocol. This object is used for
    callbacks from the plug-in to the app. if this argument is nil, no callbacks will
    occur.
*/
extern NSString *WebPlugInContainerKey;

/*!
	@constant WebPlugInContainingElementKey The DOMElement that was used to specify
	the plug-in.  May be nil.
*/
extern NSString *WebPlugInContainingElementKey;

/*!
    @protocol WebPlugInViewFactory
    @discussion WebPlugInViewFactory are used to create the NSView for a plug-in.
	The principal class of the plug-in bundle must implement this protocol.
*/

@protocol WebPlugInViewFactory <NSObject>

/*!
    @method plugInViewWithArguments: 
    @param arguments The arguments dictionary with the mentioned keys and objects. This method is required to implement.
    @result Returns an NSView object that conforms to the WebPlugIn informal protocol.
*/
+ (NSView *)plugInViewWithArguments:(NSDictionary *)arguments;

@end


