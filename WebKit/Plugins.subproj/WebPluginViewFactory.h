/*
        WebPluginViewFactory.h
        Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Cocoa/Cocoa.h>

@protocol WebPlugin;


/*!
    @constant WebPluginBaseURLKey REQUIRED. The base URL of the document containing
    the plugin's view.
*/
#define WebPluginBaseURLKey @"WebPluginBaseURL"

/*!
    @constant WebPluginAttributesKey REQUIRED. The dictionary containing the names
    and values of all attributes of the HTML element associated with the plugin AND
    the names and values of all parameters to be passed to the plugin (e.g. PARAM
    elements within an APPLET element). In the case of a conflict between names,
    the attributes of an element take precedence over any PARAMs.  All of the keys
    and values in this NSDictionary must be NSStrings.
*/
#define WebPluginAttributesKey @"WebPluginAttributes"

/*!
    @constant WebPluginContainer OPTIONAL. An object that conforms to the
    WebPluginContainer protocol (id<WebPluginContainer> *). This object is used for
    callbacks from the plugin to the app. if this argument is nil, no callbacks will
    occur.
*/
#define WebPluginContainerKey @"WebPluginContainer"

/*!
    @constant WebPluginDefaultView OPTIONAL. An NSView object that will be used as a
    visual placeholder until the plugin begins drawing.  The view will be removed by
    the plugin immediately before the plugin begins drawing into the view.
    If this argument is nil, the plugin's default view will be used.
*/
#define WebPluginDefaultViewKey @"WebPluginDefaultView"

/*!
    @protocol WebPluginViewFactory
    @discussion Protocol is used to create a Cocoa view that is used as a canvas
    by a web plugin, to display a Java Applet or QuickTime movie, for example.
    The view returned will be a descendant of NSView which conforms to the WebPlugin
    protocol.  Because it is a descendant of NSView, a WebPluginView can be placed
    inside other Cocoa views or windows easily.  The application can control the web
    plugin via the WebPlugin protocol.
*/

@protocol WebPluginViewFactory <NSObject>

/*!
    @method pluginViewWithArguments: 
    @abstract Returns an NSView object that conforms to the WebPlugin protocol.
    @param arguments The arguments dictionary with the mentioned keys and objects.
*/
+ (NSView<WebPlugin> *) pluginViewWithArguments:(NSDictionary *)arguments;

@end


