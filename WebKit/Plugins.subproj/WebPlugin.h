/*
    WebPlugin.h
    Copyright 2004, Apple, Inc. All rights reserved. 
*/

#import <Cocoa/Cocoa.h>

/*!
    @protocol WebPlugin
    @discussion Protocol that enables the application that loads the web plugin
    to control it (e.g. init, start, stop, destroy). This protocol is typically
    implemented by an NSView subclass.
*/

@protocol WebPlugin <NSObject>

/*!
    @method pluginInitialize
    @abstract Tell the plugin to perform one-time initialization.
    @discussion This method must be only called once per instance of the plugin
    object and must be called before any other methods in this protocol.
*/
- (void)pluginInitialize;

/*!
    @method pluginStart
    @abstract Tell the plugin to start normal operation.
    @discussion The plug-in usually begins drawing, playing sounds and/or
    animation in this method.  This method must be called before calling pluginStop.
    This method may called more than once, provided that the application has
    already called pluginInitialize and that each call to pluginStart is followed
    by a call to pluginStop.
*/
- (void)pluginStart;

/*!
    @method pluginStop
    @abstract Tell the plugin stop normal operation.
    @discussion pluginStart must be called before this method.  This method may be
    called more than once, provided that the application has already called
    pluginInitialize and that each call to pluginStop is preceded by a call to
    pluginStart.
*/
- (void)pluginStop;

/*!
    @method pluginDestroy
    @abstract Tell the plugin perform cleanup and prepare to be deallocated.
    @discussion The plug-in typically releases memory and other resources in this
    method.  If the plug-in has retained the WebPluginContainer, it must release
    it in this mehthod.  This method must be only called once per instance of the
    plugin object.  No other methods in this interface may be called after the
    application has called pluginDestroy.
*/
- (void)pluginDestroy;
                
@end

@protocol WebPluginSelection <NSObject>

- (void)setIsSelected:(BOOL)isSelected;
- (BOOL)isSelected;

@end

/*!
    WebPlugIn is an informal protocol that enables interaction between an application
    and web related plug-ins it may contain.
*/

@interface NSObject (WebPlugIn)

/*!
    @method webPlugInInitialize
    @abstract Tell the plug-in to perform one-time initialization.
    @discussion This method must be only called once per instance of the plug-in
    object and must be called before any other methods in this protocol.
*/
- (void)webPlugInInitialize;

/*!
    @method webPlugInStart
    @abstract Tell the plug-in to start normal operation.
    @discussion The plug-in usually begins drawing, playing sounds and/or
    animation in this method.  This method must be called before calling webPlugInStop.
    This method may called more than once, provided that the application has
    already called webPlugInInitialize and that each call to webPlugInStart is followed
    by a call to webPlugInStop.
*/
- (void)webPlugInStart;

/*!
    @method webPlugInStop
    @abstract Tell the plug-in to stop normal operation.
    @discussion webPlugInStop must be called before this method.  This method may be
    called more than once, provided that the application has already called
    webPlugInInitialize and that each call to webPlugInStop is preceded by a call to
    webPlugInStart.
*/
- (void)webPlugInStop;

/*!
    @method webPlugInDestroy
    @abstract Tell the plug-in perform cleanup and prepare to be deallocated.
    @discussion The plug-in typically releases memory and other resources in this
    method.  If the plug-in has retained the WebPlugInContainer, it must release
    it in this mehthod.  This method must be only called once per instance of the
    plug-in object.  No other methods in this interface may be called after the
    application has called webPlugInDestroy.
*/
- (void)webPlugInDestroy;

/*!
	@method webPlugInSetIsSelected:
	@discusssion Informs the plug-in whether or not it is selected.  This is typically
	used to allow the plug-in to alter it's appearance when selected.
*/
- (void)webPlugInSetIsSelected:(BOOL)isSelected;

/*!
    @method objectForWebScript
    @discussion objectForWebScript is used to expose a plug-in's scripting interface.  The 
    methods of the object are exposed to the script environment.  See the WebScriptMethods
    informal protocol for more details.
    @result Returns the object that exposes the plug-in's interface.  The class of this
    object can implement methods from the WebScriptMethods informal protocol.
*/
- (id)objectForWebScript;

@end

