/*
 *  WebPluginJava.h
 *  JavaDeploy
 *
 *  Created by Greg Bolsinga on Wed Apr 21 2004.
 *  Copyright (c) 2004 Apple Computer. All rights reserved.
 *
 */

#import <JavaVM/JavaVM.h>

// The WebPluginContainer will call these methods for LiveConnect support. The
//  WebPluginContainer should see if the WebPlugin supports these methods before
//  calling them.

@protocol WebPluginJava <NSObject>

// This returns the jobject representing the java applet to the WebPluginContainer.
//  It should always be called from the AppKit Main Thread.
- (jobject) getApplet;

// This will call the given method on the given jobject with the given arguments
//  from the proper thread for this WebPlugin. This way, no java applet code will
//  be run on the AppKit Main Thread, it will be called on a thread belonging to the
//  java applet.
//  It should always be called from the AppKit Main Thread.
- (jvalue) callJavaOn:(jobject)object withMethod:(jmethodID) withArgs:(jvalue*)args;

@end
