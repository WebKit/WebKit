/*
 *  WebKitErrors.m
 *  WebKit
 *
 *  Created by Chris Blumenberg on Wed Feb 12 2003.
 *  Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 */

#import <WebKit/WebKitErrors.h>

#import <WebFoundation/WebError.h>
#import <WebFoundation/WebLocalizableStrings.h>

#import <pthread.h>

NSString *WebErrorDomainWebKit = @"WebErrorDomainWebKit";

// Download and file I/O errors
#define WebKitErrorDescriptionCannotCreateFile UI_STRING("Cannot create file", "WebKitErrorCannotCreateFile description")
#define WebKitErrorDescriptionCannotOpenFile UI_STRING("Cannot open file", "WebKitErrorCannotOpenFile description")
#define WebKitErrorDescriptionCannotCloseFile UI_STRING("Failure occurred while closing file", "WebKitErrorCannotCloseFile description")
#define WebKitErrorDescriptionCannotWriteToFile UI_STRING("Cannot write file", "WebKitErrorCannotWriteToFile description")
#define WebKitErrorDescriptionCannotRemoveFile UI_STRING("Cannot remove file", "WebKitErrorCannotRemoveFile description")
#define WebKitErrorDescriptionCannotMoveFile UI_STRING("Cannot move file", "WebKitErrorCannotMoveFile description")
#define WebKitErrorDescriptionDownloadDecodingFailedToComplete UI_STRING_KEY("Download decoding failed", "Download decoding failed (at end)", "WebKitErrorDownloadDecodingFailedToComplete description")
#define WebKitErrorDescriptionDownloadDecodingFailedMidStream UI_STRING_KEY("Download decoding failed", "Download decoding failed (midstream)", "WebKitErrorDownloadDecodingFailedMidStream description")

// Policy errors
#define WebKitErrorDescriptionCannotShowMIMEType UI_STRING("Cannot show content with specified mime type", "WebKitErrorCannotShowMIMEType description")
#define WebKitErrorDescriptionCannotShowURL UI_STRING("Cannot show URL", "WebKitErrorCannotShowURL description")
#define WebKitErrorDescriptionLocationChangeInterruptedByPolicyChange UI_STRING("Location change interrupted", "WebKitErrorLocationChangeInterruptedByPolicyChange description")
#define WebKitErrorDescriptionResourceLoadInterruptedByPolicyChange UI_STRING("Resource load interrupted", "WebKitErrorResourceLoadInterruptedByPolicyChange description")

// Plug-in and java errors
#define WebKitErrorDescriptionWebErrorCannotFindPlugin UI_STRING("Cannot find plug-in", "WebKitErrorCannotFindPlugin description")
#define WebKitErrorDescriptionWebErrorCannotLoadPlugin UI_STRING("Cannot load plug-in", "WebKitErrorCannotLoadPlugin description")
#define WebKitErrorDescriptionWebErrorJavaUnavailable UI_STRING("Java is unavailable", "WebKitErrorJavaUnavailable description")


static pthread_once_t registerErrorsControl = PTHREAD_ONCE_INIT;
static void registerErrors(void);

@implementation WebError (WebExtras)

+ (void)_registerWebKitErrors
{
    pthread_once(&registerErrorsControl, registerErrors);
}

@end

static void registerErrors()
{
    NSAutoreleasePool *pool;

    pool = [[NSAutoreleasePool alloc] init];

    NSDictionary *dict = [NSDictionary dictionaryWithObjectsAndKeys:
        // Download and file I/O errors
        WebKitErrorDescriptionCannotCreateFile, 		[NSNumber numberWithInt: WebKitErrorCannotCreateFile],
        WebKitErrorDescriptionCannotOpenFile, 			[NSNumber numberWithInt: WebKitErrorCannotOpenFile],
        WebKitErrorDescriptionCannotCloseFile, 			[NSNumber numberWithInt: WebKitErrorCannotCloseFile],
        WebKitErrorDescriptionCannotWriteToFile, 		[NSNumber numberWithInt: WebKitErrorCannotWriteToFile],
        WebKitErrorDescriptionCannotRemoveFile, 		[NSNumber numberWithInt: WebKitErrorCannotRemoveFile],
        WebKitErrorDescriptionCannotMoveFile, 			[NSNumber numberWithInt: WebKitErrorCannotMoveFile],
        WebKitErrorDescriptionDownloadDecodingFailedToComplete,	[NSNumber numberWithInt: WebKitErrorDownloadDecodingFailedToComplete],
        WebKitErrorDescriptionDownloadDecodingFailedMidStream, 	[NSNumber numberWithInt: WebKitErrorDownloadDecodingFailedMidStream],

        // Policy errors
        WebKitErrorDescriptionCannotShowMIMEType, 		[NSNumber numberWithInt: WebKitErrorCannotShowMIMEType],
        WebKitErrorDescriptionCannotShowURL, 			[NSNumber numberWithInt: WebKitErrorCannotShowURL],
        WebKitErrorDescriptionLocationChangeInterruptedByPolicyChange, 	[NSNumber numberWithInt: WebKitErrorLocationChangeInterruptedByPolicyChange],
        WebKitErrorDescriptionResourceLoadInterruptedByPolicyChange,  	[NSNumber numberWithInt: WebKitErrorResourceLoadInterruptedByPolicyChange],

        // Plug-in and java errors
        WebKitErrorDescriptionWebErrorCannotFindPlugin,		[NSNumber numberWithInt: WebKitErrorCannotFindPlugin],
        WebKitErrorDescriptionWebErrorCannotLoadPlugin,		[NSNumber numberWithInt: WebKitErrorCannotLoadPlugin],
        WebKitErrorDescriptionWebErrorJavaUnavailable,		[NSNumber numberWithInt: WebKitErrorJavaUnavailable],
        nil];

    [WebError addErrorsWithCodesAndDescriptions:dict inDomain:WebErrorDomainWebKit];

    [pool release];
}