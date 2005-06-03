/*
 *  WebKitErrors.m
 *  WebKit
 *
 *  Created by Chris Blumenberg on Wed Feb 12 2003.
 *  Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 */

#import <WebKit/WebKitErrors.h>

#import <WebKit/WebKitErrorsPrivate.h>
#import <WebKit/WebLocalizableStrings.h>
#import <WebKit/WebNSURLExtras.h>

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
#define WebKitErrorDescriptionPlugInCancelledConnection UI_STRING("Plug-in cancelled", "WebKitErrorPlugInCancelledConnection description")

static pthread_once_t registerErrorsControl = PTHREAD_ONCE_INIT;
static void registerErrors(void);

@implementation NSError (WebKitExtras)

static NSMutableDictionary *descriptions = nil;

+ (void)_registerWebKitErrors
{
    pthread_once(&registerErrorsControl, registerErrors);
}

-(id)_webkit_initWithDomain:(NSString *)domain code:(int)code URL:(NSURL *)URL
{
    NSDictionary *descriptionsDict;
    NSString *localizedDesc;
    NSDictionary *dict;
	// insert a localized string here for those folks not savvy to our category methods
    descriptionsDict = [descriptions objectForKey:domain];
    localizedDesc = descriptionsDict ? [descriptionsDict objectForKey:[NSNumber numberWithInt:code]] : nil;
    dict = [NSDictionary dictionaryWithObjectsAndKeys:
        URL, @"NSErrorFailingURLKey",
        [URL absoluteString], @"NSErrorFailingURLStringKey",
        localizedDesc, NSLocalizedDescriptionKey,
        nil];
    return [self initWithDomain:domain code:code userInfo:dict];
}

+(id)_webkit_errorWithDomain:(NSString *)domain code:(int)code URL:(NSURL *)URL
{
    return [[[self alloc] _webkit_initWithDomain:domain code:code URL:URL] autorelease];
}

+ (NSError *)_webKitErrorWithDomain:(NSString *)domain code:(int)code URL:(NSURL *)URL
{
    [self _registerWebKitErrors];
    return [self _webkit_errorWithDomain:domain code:code URL:URL];
}

+ (NSError *)_webKitErrorWithCode:(int)code failingURL:(NSString *)URLString
{
    return [self _webKitErrorWithDomain:WebKitErrorDomain code:code URL:[NSURL _web_URLWithUserTypedString:URLString]];
}

- (id)_initWithPluginErrorCode:(int)code
                    contentURL:(NSURL *)contentURL
                 pluginPageURL:(NSURL *)pluginPageURL
                    pluginName:(NSString *)pluginName
                      MIMEType:(NSString *)MIMEType
{
    [[self class] _registerWebKitErrors];
    
    NSMutableDictionary *userInfo = [[NSMutableDictionary alloc] init];
    if (contentURL) {
        [userInfo setObject:contentURL forKey:@"NSErrorFailingURLKey"];
        [userInfo setObject:[contentURL _web_userVisibleString] forKey:NSErrorFailingURLStringKey];
    }
    if (pluginPageURL) {
        [userInfo setObject:[pluginPageURL _web_userVisibleString] forKey:WebKitErrorPlugInPageURLStringKey];
    }	
    if (pluginName) {
        [userInfo setObject:pluginName forKey:WebKitErrorPlugInNameKey];
    }
    if (MIMEType) {
        [userInfo setObject:MIMEType forKey:WebKitErrorMIMETypeKey];
    }

    NSDictionary *userInfoCopy = [userInfo count] > 0 ? [[NSDictionary alloc] initWithDictionary:userInfo] : nil;
    [userInfo release];
    NSError *error = [self initWithDomain:WebKitErrorDomain code:code userInfo:userInfoCopy];
    [userInfoCopy release];
    
    return error;
}

+ (void)_webkit_addErrorsWithCodesAndDescriptions:(NSDictionary *)dictionary inDomain:(NSString *)domain
{
	if (!descriptions)
		descriptions = [[NSMutableDictionary alloc] init];

    [descriptions setObject:dictionary forKey:domain];
}

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
        WebKitErrorDescriptionCannotFindPlugin,		[NSNumber numberWithInt: WebKitErrorCannotFindPlugIn],
        WebKitErrorDescriptionCannotLoadPlugin,		[NSNumber numberWithInt: WebKitErrorCannotLoadPlugIn],
        WebKitErrorDescriptionJavaUnavailable,		[NSNumber numberWithInt: WebKitErrorJavaUnavailable],
        WebKitErrorDescriptionPlugInCancelledConnection,[NSNumber numberWithInt: WebKitErrorPlugInCancelledConnection],
        nil];

    [NSError _webkit_addErrorsWithCodesAndDescriptions:dict inDomain:WebKitErrorDomain];

#if BUILDING_ON_PANTHER
    [pool release];
#else
    [pool drain];
#endif
}



@end

