/*
        WebPlugin.h
        Copyright 2002, Apple, Inc. All rights reserved. 
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
    @abstract Tell the plugin perform cleanup and prepare to be dealloced.
    @discussion The plug-in typically releases memory and other resources in this
    method.  If the plug-in has retained the WebPluginContainer, it must release
    it in this mehthod.  This method must be only called once per instance of the
    plugin object.  No other methods in this interface may be called after the
    application has called pluginDestroy.
*/
- (void)pluginDestroy;
                
@end