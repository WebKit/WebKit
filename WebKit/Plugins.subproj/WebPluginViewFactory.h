//
//  WebPluginViewFactory.h
//
//  Created by Mike Hay <mhay@apple.com> on Tue Oct 15 2002.
//  Copyright (c) 2002 Apple Computer.  All rights reserved.
//

//
//  About WebPluginViewFactory
//
//    This protocol is used to create a Cocoa view that is used as a canvas
//    by a web plugin, to display a Java Applet or QuickTime movie, for example.
//
//    The view returned will be a descendant of NSView which conforms to
//    the WebPlugin protocol.  
//
//    Because it is a descendant of NSView, a WebPluginView can be placed 
//    inside other Cocoa views or windows easily.
//
//    The application can control the web plugin via the WebPlugin protocol.
//

#import <Cocoa/Cocoa.h>

@protocol WebPlugin;


// 
//  Keys Used in the Arguments Dictionary
// 

//
//  WebPluginBaseURL ....... REQUIRED  base URL of the document containing the plugin's view.
//  (NSURL *)
//
#define WebPluginBaseURLKey @"WebPluginBaseURL"

//
//  WebPluginAttributes .... REQUIRED  dictionary containing the names and values of all attributes
//  (NSDictionary *)                   of the HTML element associated with the plugin AND the names
//                                     and values of all parameters to be passed to the plugin  
//                                     (e.g. PARAM elements within an APPLET element).
//
//                                     in the case of a conflict between names, the attributes of an
//                                     element take precedence over any PARAMs.
//
//                                     all of the keys and values in this NSDictionary must be
//                                     NSStrings.
//
#define WebPluginAttributesKey @"WebPluginAttributes"

//
//  WebPluginContainer ..... OPTIONAL  a Cocoa object that conforms to the WebPluginContainer protocol.
//  (id<WebPluginContainer> *)         this object is used for callbacks from the plugin to the app.
//
//                                     if this argument is nil, no callbacks will occur.
//
#define WebPluginContainerKey @"WebPluginContainer"

//
//  WebPluginDefaultView ... OPTIONAL  an NSView object that will be used as a visual placeholder
//  (NSView *)                         until the plugin begins drawing.
//
//                                     the view will be removed by the plugin immediately before the
//                                     plugin begins drawing into the WebPluginView.
//
//                                     if this argument is nil, the plugin's default view will be used.
//                                     (as an application, if you care about the appearance of this 
//                                     space before the plugin begins drawing or if the plugin never 
//                                     begins drawing, provide a view.)
//
#define WebPluginDefaultViewKey @"WebPluginDefaultView"



//
//  WebPluginViewFactory protocol
//

@protocol WebPluginViewFactory <NSObject>

// 
//  pluginViewWithArguments:    returns an NSView object that conforms to the WebPlugin protocol
//
+ (NSView<WebPlugin> *) pluginViewWithArguments: (NSDictionary *) args;

@end


