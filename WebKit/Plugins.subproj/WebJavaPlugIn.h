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

typedef enum {
    WebJNIReturnTypeInvalid = 0,
    WebJNIReturnTypeVoid,
    WebJNIReturnTypeObject,
    WebJNIReturnTypeBoolean,
    WebJNIReturnTypeByte,
    WebJNIReturnTypeChar,
    WebJNIReturnTypeShort,
    WebJNIReturnTypeInt,
    WebJNIReturnTypeLong,
    WebJNIReturnTypeFloat,
    WebJNIReturnTypeDouble
} WebJNIReturnType;

@interface NSObject (WebJavaPlugIn)

/*!
    @method webPlugInGetApplet
    @discusssion This returns the jobject representing the java applet to the
    WebPlugInContainer.  It should always be called from the AppKit Main Thread.
    This method is only implemented by the Java plug-in.
*/
- (jobject)webPlugInGetApplet;

/*!
    @method webPlugInCallJava:isStatic:returnType:method:arguments:callingURL:exceptionDescription:
    @param object The Java instance that will receive the method call.
    @param isStatic A flag that indicated whether the method is a class method.
    @param returnType The return type of the Java method.
    @param method The ID of the Java method to call.
    @param args The arguments to use with the method invocation.
    @param callingURL The URL of the page that contains the JavaScript that is calling Java.
    @param exceptionDescription Pass in nil or the address of pointer to a string object.  If any exception
    is thrown by Java the return value will be a description of the exception, otherwise nil.
    @discussion Calls to Java from native code should not make direct
    use of JNI.  Instead they should use this method to dispatch calls to the 
    Java VM.  This is required to guarantee that the correct thread will receive
    the call.  webPlugInCallJava:isStatic:returnType:method:arguments:callingURL:exceptionDescription: must 
    always be called from the AppKit main thread.  This method is only implemented by the Java plug-in.
    @result The result of the method invocation.
*/
- (jvalue)webPlugInCallJava:(jobject)object
                   isStatic:(BOOL)isStatic
                 returnType:(WebJNIReturnType)returnType
                     method:(jmethodID)method
                  arguments:(jvalue*)args
                 callingURL:(NSURL *)url
       exceptionDescription:(NSString **)exceptionString;

@end
