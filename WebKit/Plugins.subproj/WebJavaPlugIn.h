/*
    WebJavaPlugIn.h
    Copyright 2004, Apple, Inc. All rights reserved.
    
    Public header file.
*/

#import <JavaVM/jni.h>

/*!
    The Java plug-in adds the following additional methods to facilitate JNI
    access to Java VM via the plug-in.
*/

@interface NSObject (WebJavaPlugIn)

/*!
    @method webPlugInGetApplet
    @discusssion This returns the jobject representing the java applet to the
    WebPlugInContainer.  It should always be called from the AppKit Main Thread.
    This method is only implemented by the Java plug-in.
*/
- (jobject)webPlugInGetApplet;

/*!
    @method webPlugInCallJava:withMethod:withArgs:
    @param object The Java instance that will receive the method call.
    @param args The arguments to use with the method invocation.
    @discussion Calls in the Java from native code should not make direct
    use of JNI.  Instead they should use this method to dispatch call to the 
    Java VM.  This is required to guarantee that the correct thread will receive
    the call.  webPlugInCallJava:withMethod:withArgs: must always be called from
    the AppKit main thread.  This method is only implemented by the Java plug-in.
    @result The result of the method invocation.
*/
- (jvalue)webPlugInCallJava:(jobject)object withMethod:(jmethodID) withArgs:(jvalue*)args;

@end
