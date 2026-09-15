/*
 *  CIPlugInInterface.h
 *
 *
 *  Copyright (c) 2004 Apple Computer. All rights reserved.
 *
 */

#ifndef CIPLUGININTERFACE_H
#define CIPLUGININTERFACE_H

#ifdef __OBJC__

#import <objc/objc.h>

#if TARGET_OS_OSX

/*!
    @header CIPlugInInterface
    @abstract   Definition of the plug-in registration protocol
    @discussion The principal class of a CIPlugIn must support the CIPlugInRegistration protocol
*/

/*!
    @protocol   CIPlugInRegistration
    @abstract   This protocol defines the calls made by the host to the CIPlugIn when initializing it
    @discussion The principal class of a CIPlugIn must support the CIPlugInRegistration protocol
*/
@protocol CIPlugInRegistration

/*!
    @method     load
    @abstract   the plugin gets a chance to do custom initialization (like registration check ) here
    @discussion Load gets called once by the host when the first filter from the plug-in gets instantiated. Return of true means that the plugIn successfully initialized
    @param      host    for future use only
*/
- (BOOL)load:(void *)host;

@end

#endif

#endif /* __OBJC__ */

#endif /* CIPLUGININTERFACE_H */
