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

#define WebErrorDescriptionCannotCreateFile UI_STRING("Cannot create file", "WebErrorCannotCreateFile description")
#define WebErrorDescriptionCannotOpenFile UI_STRING("Cannot open file", "WebErrorCannotOpenFile description")
#define WebErrorDescriptionCannotCloseFile UI_STRING("Failure occurred while closing file", "WebKitErrorCannotCloseFile description")
#define WebErrorDescriptionCannotWriteToFile UI_STRING("Cannot write file", "WebErrorCannotWriteToFile description")
#define WebErrorDescriptionCannotRemoveFile UI_STRING("Cannot remove file", "WebErrorCannotRemoveFile description")
#define WebErrorDescriptionCannotMoveFile UI_STRING("Cannot move file", "WebKitErrorCannotMoveFile description")
#define WebErrorDescriptionCannotFindApplicationForFile UI_STRING("Cannot find application for file", "WebErrorCannotFindApplicationForFile description")
#define WebErrorDescriptionFinderCannotOpenDirectory UI_STRING("Finder cannot open directory", "WebErrorFinderCannotOpenDirectory description")
#define WebErrorDescriptionCannotShowDirectory UI_STRING("Cannot show a file directory", "WebErrorCannotShowDirectory description")
#define WebErrorDescriptionCannotShowMIMEType UI_STRING("Cannot show content with specified mime type", "WebErrorCannotShowMIMEType description")
#define WebErrorDescriptionCannotShowURL UI_STRING("Cannot show URL", "WebErrorCannotShowURL description")
#define WebErrorDescriptionCannotFindApplicationForURL UI_STRING("Cannot find application for URL", "WebErrorCannotNotFindApplicationForURL description")
#define WebErrorDescriptionWebErrorCannotFindPlugin UI_STRING("Cannot find plug-in", "WebErrorCannotFindPlugin description")
#define WebErrorDescriptionWebErrorCannotLoadPlugin UI_STRING("Cannot load plug-in", "WebErrorCannotLoadPlugin description")
#define WebErrorDescriptionWebErrorJavaUnavailable UI_STRING("Java is unavailable", "WebErrorJavaUnavailable description")
#define WebErrorDescriptionDownloadDecodingFailedToComplete UI_STRING_KEY("Download decoding failed", "Download decoding failed (at end)", "WebErrorDownloadDecodingFailedToComplete description")
#define WebErrorDescriptionDownloadDecodingFailedMidStream UI_STRING_KEY("Download decoding failed", "Download decoding failed (midstream)", "WebErrorDownloadDecodingFailedMidStream description")

static pthread_once_t categoryInitializeControl = PTHREAD_ONCE_INIT;
static void categoryInitialize(void);

@interface WebError (WebExtras)
@end

@implementation WebError (WebExtras)

+ (void)initialize
{
    pthread_once(&categoryInitializeControl, categoryInitialize);
}

@end

static void categoryInitialize()
{
    NSAutoreleasePool *pool;

    pool = [[NSAutoreleasePool alloc] init];

    NSDictionary *dict = [NSDictionary dictionaryWithObjectsAndKeys:
        WebErrorDescriptionCannotCreateFile, 			[NSNumber numberWithInt: WebKitErrorCannotCreateFile],
        WebErrorDescriptionCannotOpenFile, 			[NSNumber numberWithInt: WebKitErrorCannotOpenFile],
        WebErrorDescriptionCannotCloseFile, 			[NSNumber numberWithInt: WebKitErrorCannotCloseFile],
        WebErrorDescriptionCannotWriteToFile, 			[NSNumber numberWithInt: WebKitErrorCannotWriteToFile],
        WebErrorDescriptionCannotRemoveFile, 			[NSNumber numberWithInt: WebKitErrorCannotRemoveFile],
        WebErrorDescriptionCannotMoveFile, 			[NSNumber numberWithInt: WebKitErrorCannotMoveFile],
        WebErrorDescriptionCannotFindApplicationForFile, 	[NSNumber numberWithInt: WebKitErrorCannotFindApplicationForFile],
        WebErrorDescriptionFinderCannotOpenDirectory, 		[NSNumber numberWithInt: WebKitErrorFinderCannotOpenDirectory],
        WebErrorDescriptionCannotShowDirectory, 		[NSNumber numberWithInt: WebKitErrorCannotShowDirectory],
        WebErrorDescriptionCannotShowMIMEType, 			[NSNumber numberWithInt: WebKitErrorCannotShowMIMEType],
        WebErrorDescriptionCannotShowURL, 			[NSNumber numberWithInt: WebKitErrorCannotShowURL],
        WebErrorDescriptionCannotFindApplicationForURL, 	[NSNumber numberWithInt: WebKitErrorCannotNotFindApplicationForURL],
        WebErrorDescriptionWebErrorCannotFindPlugin,		[NSNumber numberWithInt: WebKitErrorCannotFindPlugin],
        WebErrorDescriptionWebErrorCannotLoadPlugin,		[NSNumber numberWithInt: WebKitErrorCannotLoadPlugin],
        WebErrorDescriptionWebErrorJavaUnavailable,		[NSNumber numberWithInt: WebKitErrorJavaUnavailable],
        WebErrorDescriptionDownloadDecodingFailedToComplete,	[NSNumber numberWithInt: WebKitErrorDownloadDecodingFailedToComplete],
        WebErrorDescriptionDownloadDecodingFailedMidStream, 	[NSNumber numberWithInt: WebKitErrorDownloadDecodingFailedMidStream],
        nil];

    [WebError addErrorsWithCodesAndDescriptions:dict inDomain:WebErrorDomainWebKit];

    [pool release];
}