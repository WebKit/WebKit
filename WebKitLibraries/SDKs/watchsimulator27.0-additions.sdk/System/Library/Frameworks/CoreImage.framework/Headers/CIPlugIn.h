//
//  CIPlugIn.h
//
//
//  Created on Fri May 14 2004.
//  Copyright (c) 2004 Apple Computer. All rights reserved.
//

#ifndef CIPLUGIN_H
#define CIPLUGIN_H

#ifdef __OBJC__

#import <Foundation/Foundation.h>
#import <CoreImage/CIPlugInInterface.h>

#if TARGET_OS_OSX

#import <CoreImage/CoreImageDefines.h>



/**
 The CIPlugIn class is responsible for loading Image Units.
 
 The implementation of the CIPlugIn objects is private.
 An application can, however, call the 2 public class method to load plug-ins.
 
 Loading executable CIFilter plugins is deprecated starting in macOS 10.15.
 */

NS_CLASS_AVAILABLE_MAC(10_4)
@interface CIPlugIn : NSObject {
    void *_priv[8];
}

/** This call will scan for plugins with the extension .plugin in
 		/Library/Graphics/Image Units
        ~Library/Graphics/Image Units
 If called more than once, newly added plug-ins will be loaded but you cannot remove a plug-in and its filters. */
+ (void)loadAllPlugIns NS_DEPRECATED_MAC(10_4, 10_15);

/** Same as loadAllPlugIns does not load filters that contain executable code. */
+ (void)loadNonExecutablePlugIns;

/** Loads a plug-in specified by its URL. */
+ (void)loadPlugIn:(NSURL *)url allowNonExecutable:(BOOL)allowNonExecutable NS_DEPRECATED_MAC(10_4, 10_7);

/** Loads a plug-in specified by its URL.
 If allowExecutableCode is NO, filters containing executable code will not be loaded. If YES, any kind of filter will be loaded. */
+ (void)loadPlugIn:(NSURL *)url allowExecutableCode:(BOOL)allowExecutableCode NS_DEPRECATED_MAC(10_7, 10_15);

/** Loads a non-executable plug-in specified by its URL.
 If the filters containing executable code, it will not be loaded. */
+ (void)loadNonExecutablePlugIn:(NSURL *)url NS_AVAILABLE_MAC(10_15);

@end

#endif

#endif /* __OBJC__ */

#endif /* CIPLUGIN_H */
