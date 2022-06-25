/*
 * Copyright (C) 2005-2016 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import <WebKitLegacy/WebKitErrors.h>

#import "WebLocalizableStringsInternal.h"
#import <Foundation/NSURLError.h>
#import <WebKitLegacy/WebKitErrorsPrivate.h>
#import <WebKitLegacy/WebNSURLExtras.h>

#import <dispatch/dispatch.h>

NSString *WebKitErrorDomain = @"WebKitErrorDomain";

NSString * const WebKitErrorMIMETypeKey =               @"WebKitErrorMIMETypeKey";
NSString * const WebKitErrorPlugInNameKey =             @"WebKitErrorPlugInNameKey";
NSString * const WebKitErrorPlugInPageURLStringKey =    @"WebKitErrorPlugInPageURLStringKey";

// Policy errors
#define WebKitErrorDescriptionCannotShowMIMEType UI_STRING_INTERNAL("Content with specified MIME type can’t be shown", "WebKitErrorCannotShowMIMEType description")
#define WebKitErrorDescriptionCannotShowURL UI_STRING_INTERNAL("The URL can’t be shown", "WebKitErrorCannotShowURL description")
#define WebKitErrorDescriptionFrameLoadInterruptedByPolicyChange UI_STRING_INTERNAL("Frame load interrupted", "WebKitErrorFrameLoadInterruptedByPolicyChange description")
#define WebKitErrorDescriptionCannotUseRestrictedPort UI_STRING_INTERNAL("Not allowed to use restricted network port", "WebKitErrorCannotUseRestrictedPort description")
#define WebKitErrorDescriptionFrameLoadBlockedByContentFilter UI_STRING_INTERNAL("The URL was blocked by a content filter", "WebKitErrorFrameLoadBlockedByContentFilter description")

// Plug-in and java errors
#define WebKitErrorDescriptionCannotFindPlugin UI_STRING_INTERNAL("The plug-in can’t be found", "WebKitErrorCannotFindPlugin description")
#define WebKitErrorDescriptionCannotLoadPlugin UI_STRING_INTERNAL("The plug-in can’t be loaded", "WebKitErrorCannotLoadPlugin description")
#define WebKitErrorDescriptionJavaUnavailable UI_STRING_INTERNAL("Java is unavailable", "WebKitErrorJavaUnavailable description")
#define WebKitErrorDescriptionPlugInCancelledConnection UI_STRING_INTERNAL("Plug-in cancelled", "WebKitErrorPlugInCancelledConnection description")
#define WebKitErrorDescriptionPlugInWillHandleLoad UI_STRING_INTERNAL("Plug-in handled load", "WebKitErrorPlugInWillHandleLoad description")

// Geolocations errors

#define WebKitErrorDescriptionGeolocationLocationUnknown UI_STRING_INTERNAL("The current location cannot be found.", "WebKitErrorGeolocationLocationUnknown description")

static NSMutableDictionary *descriptions = nil;

@interface NSError (WebKitInternal)
- (instancetype)_webkit_initWithDomain:(NSString *)domain code:(int)code URL:(NSURL *)URL __attribute__((objc_method_family(init)));
@end

@implementation NSError (WebKitInternal)

- (instancetype)_webkit_initWithDomain:(NSString *)domain code:(int)code URL:(NSURL *)URL
{
    // Insert a localized string here for those folks not savvy to our category methods.
    NSString *localizedDescription = [[descriptions objectForKey:domain] objectForKey:@(code)];
    NSDictionary *userInfo = [NSDictionary dictionaryWithObjectsAndKeys:
        URL, @"NSErrorFailingURLKey",
        [URL absoluteString], NSURLErrorFailingURLStringErrorKey,
        localizedDescription, NSLocalizedDescriptionKey,
        nil];
    return [self initWithDomain:domain code:code userInfo:userInfo];
}

@end

@implementation NSError (WebKitExtras)

+ (void)_registerWebKitErrors
{
    static dispatch_once_t flag;
    dispatch_once(&flag, ^{
        @autoreleasepool {
            NSDictionary *dict = [NSDictionary dictionaryWithObjectsAndKeys:
                // Policy errors
                WebKitErrorDescriptionCannotShowMIMEType,                   @(WebKitErrorCannotShowMIMEType),
                WebKitErrorDescriptionCannotShowURL,                        @(WebKitErrorCannotShowURL),
                WebKitErrorDescriptionFrameLoadInterruptedByPolicyChange,   @(WebKitErrorFrameLoadInterruptedByPolicyChange),
                WebKitErrorDescriptionCannotUseRestrictedPort,              @(WebKitErrorCannotUseRestrictedPort),
                WebKitErrorDescriptionFrameLoadBlockedByContentFilter,      @(WebKitErrorFrameLoadBlockedByContentFilter),

                // Plug-in and java errors
                WebKitErrorDescriptionCannotFindPlugin,                     @(WebKitErrorCannotFindPlugIn),
                WebKitErrorDescriptionCannotLoadPlugin,                     @(WebKitErrorCannotLoadPlugIn),
                WebKitErrorDescriptionJavaUnavailable,                      @(WebKitErrorJavaUnavailable),
                WebKitErrorDescriptionPlugInCancelledConnection,            @(WebKitErrorPlugInCancelledConnection),
                WebKitErrorDescriptionPlugInWillHandleLoad,                 @(WebKitErrorPlugInWillHandleLoad),

                // Geolocation errors
                WebKitErrorDescriptionGeolocationLocationUnknown,           @(WebKitErrorGeolocationLocationUnknown),
                nil];

            [NSError _webkit_addErrorsWithCodesAndDescriptions:dict inDomain:WebKitErrorDomain];
        }
    });
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
    return [self _webKitErrorWithDomain:WebKitErrorDomain code:code URL:[NSURL _webkit_URLWithUserTypedString:URLString]];
}

- (id)_initWithPluginErrorCode:(int)code
                    contentURL:(NSURL *)contentURL
                 pluginPageURL:(NSURL *)pluginPageURL
                    pluginName:(NSString *)pluginName
                      MIMEType:(NSString *)MIMEType
{
    [[self class] _registerWebKitErrors];
    
    NSMutableDictionary *userInfo = [[NSMutableDictionary alloc] init];
    NSDictionary *descriptionsForWebKitErrorDomain = [descriptions objectForKey:WebKitErrorDomain];
    NSString *localizedDescription = [descriptionsForWebKitErrorDomain objectForKey:@(code)];
    if (localizedDescription)
        [userInfo setObject:localizedDescription forKey:NSLocalizedDescriptionKey];
    if (contentURL) {
        [userInfo setObject:contentURL forKey:@"NSErrorFailingURLKey"];
        [userInfo setObject:[contentURL _web_userVisibleString] forKey:NSURLErrorFailingURLStringErrorKey];
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

@end
