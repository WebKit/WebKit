/*
 *  WebKitErrors.m
 *  WebKit
 *
 *  Created by Chris Blumenberg on Wed Feb 12 2003.
 *  Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 */

#import <WebKit/WebKitErrors.h>

#import <WebKit/WebLocalizableStrings.h>
#import <Foundation/NSError_NSURLExtras.h>

#import <pthread.h>

NSString *WebKitErrorDomain = @"WebKitErrorDomain";

NSString * const WebKitErrorMIMETypeKey = 		@"WebKitErrorMIMETypeKey";
NSString * const WebKitErrorPlugInNameKey = 		@"WebKitErrorPlugInNameKey";
NSString * const WebKitErrorPlugInPageURLStringKey = 	@"WebKitErrorPlugInPageURLStringKey";

// Policy errors
#define WebKitErrorDescriptionCannotShowMIMEType UI_STRING("Cannot show content with specified mime type", "WebKitErrorCannotShowMIMEType description")
#define WebKitErrorDescriptionCannotShowURL UI_STRING("Cannot show URL", "WebKitErrorCannotShowURL description")
#define WebKitErrorDescriptionFrameLoadInterruptedByPolicyChange UI_STRING("Frame load interrupted", "WebKitErrorFrameLoadInterruptedByPolicyChange description")

// Plug-in and java errors
#define WebKitErrorDescriptionCannotFindPlugin UI_STRING("Cannot find plug-in", "WebKitErrorCannotFindPlugin description")
#define WebKitErrorDescriptionCannotLoadPlugin UI_STRING("Cannot load plug-in", "WebKitErrorCannotLoadPlugin description")
#define WebKitErrorDescriptionJavaUnavailable UI_STRING("Java is unavailable", "WebKitErrorJavaUnavailable description")


static pthread_once_t registerErrorsControl = PTHREAD_ONCE_INIT;
static void registerErrors(void);

@implementation NSError (WebKitExtras)

+ (void)_registerWebKitErrors
{
    pthread_once(&registerErrorsControl, registerErrors);
}

+ (NSError *)_webKitErrorWithCode:(int)code failingURL:(NSString *)URL
{
    [self _registerWebKitErrors];

    return [self _web_errorWithDomain:WebKitErrorDomain code:code failingURL:URL];
}

- (id)_initWithPluginErrorCode:(int)code
              contentURLString:(NSString *)contentURLString
           pluginPageURLString:(NSString *)pluginPageURLString
                    pluginName:(NSString *)pluginName
                      MIMEType:(NSString *)MIMEType
{
    [[self class] _registerWebKitErrors];
    
    NSMutableDictionary *userInfo = [[NSMutableDictionary alloc] init];
    if (contentURLString) {
        [userInfo setObject:contentURLString forKey:NSErrorFailingURLStringKey];
    }
    if (pluginPageURLString) {
        [userInfo setObject:pluginPageURLString forKey:WebKitErrorPlugInPageURLStringKey];
    }	
    if (pluginName) {
        [userInfo setObject:pluginName forKey:WebKitErrorPlugInNameKey];
    }
    if (MIMEType) {
        [userInfo setObject:MIMEType forKey:WebKitErrorMIMETypeKey];
    }

    NSDictionary *userInfoCopy = [userInfo count] > 0  ? [[NSDictionary alloc] initWithDictionary:userInfo] : nil;
    [userInfo release];
    NSError *error = [self initWithDomain:WebKitErrorDomain code:code userInfo:userInfoCopy];
    [userInfoCopy release];
    
    return error;
}


@end

static void registerErrors()
{
    NSAutoreleasePool *pool;

    pool = [[NSAutoreleasePool alloc] init];

    NSDictionary *dict = [NSDictionary dictionaryWithObjectsAndKeys:
        // Policy errors
        WebKitErrorDescriptionCannotShowMIMEType, 		[NSNumber numberWithInt: WebKitErrorCannotShowMIMEType],
        WebKitErrorDescriptionCannotShowURL, 			[NSNumber numberWithInt: WebKitErrorCannotShowURL],
        WebKitErrorDescriptionFrameLoadInterruptedByPolicyChange,[NSNumber numberWithInt: WebKitErrorFrameLoadInterruptedByPolicyChange],

        // Plug-in and java errors
        WebKitErrorDescriptionCannotFindPlugin,		[NSNumber numberWithInt: WebKitErrorCannotFindPlugin],
        WebKitErrorDescriptionCannotLoadPlugin,		[NSNumber numberWithInt: WebKitErrorCannotLoadPlugin],
        WebKitErrorDescriptionJavaUnavailable,		[NSNumber numberWithInt: WebKitErrorJavaUnavailable],
        nil];

    [NSError _web_addErrorsWithCodesAndDescriptions:dict inDomain:WebKitErrorDomain];

    [pool release];
}
