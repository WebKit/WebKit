//
//  WebPlugin.h
//
//  Created by Mike Hay <mhay@apple.com> on Tue Oct 15 2002.
//  Copyright (c) 2002 Apple Computer.  All rights reserved.
//

//
//  About WebPlugin
//
//    This protocol enables the application that loads the web plugin 
//    to control the plugin (e.g. init, start, stop, destroy).
//

#import <Cocoa/Cocoa.h>


@protocol WebPlugin <NSObject>

//
//  pluginInitialize    request that the plugin perform one-time initialization.
//
//                      must be only called once per instance of the plugin object.
//
//                      must be called before any other methods in this protocol.
//
- (void)pluginInitialize;

//
//  pluginStart         request that the plugin start normal operation.
//                      (e.g. drawing, playing sounds, animation)
//
//                      application must call pluginStart before calling pluginStop.
//
//                      application may call pluginStart more than once, provided
//                      that the application has already called pluginInitialize
//                      and that each call to pluginStart is followed by a call
//                      to pluginStop.
//
- (void)pluginStart;

//
//  pluginStop          request that the plugin stop normal operation.
//                      (e.g. drawing, playing sounds, animation)
//
//                      application must call pluginStart before calling pluginStop.
//
//                      application may call pluginStop more than once, provided
//                      that the application has already called pluginInitialize
//                      and that each call to pluginStop is preceded by a call
//                      to pluginStart.
//
- (void)pluginStop;

//
//  pluginDestroy       request that the plugin perform cleanup and prepare to be 
//                      destroyed.  (e.g. release memory and other resources)
//
//                      must be only called once per instance of the plugin object.
//
//                      no other methods in this interface may be called after 
//                      the application has called pluginDestroy.
- (void)pluginDestroy;
                
@end