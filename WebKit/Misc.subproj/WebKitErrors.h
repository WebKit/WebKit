/*	WebKitErrors.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

// WebFoundation error codes < 10000
// WebKit error codes >= 10000

typedef enum {
    WebErrorNoError = 0,
    WebErrorCannotShowMIMEType = 10000,
    WebErrorFileDoesNotExist = 10001,
    WebErrorCouldNotFindApplicationForFile = 10002,
    WebErrorCouldNotFindApplicationForURL = 10003,
    WebErrorFileNotReadable = 10004,
    WebErrorFinderCouldNotOpenDirectory = 10005,
    WebErrorCannotShowDirectory = 10006,
    WebErrorCannotShowURL = 10007,
} WebErrorCode;

#define WebErrorDescriptionCannotShowMIMEType NSLocalizedStringFromTable (@"Can't show content with specified mime type", @"WebError", @"WebErrorCannotShowMIMEType description")

#define WebErrorDescriptionCouldNotFindApplicationForFile NSLocalizedStringFromTable (@"Could not find application for specified file", @"WebError", @"WebErrorCouldNotFindApplicationForFile description")

#define WebErrorDescriptionCouldNotFindApplicationForURL NSLocalizedStringFromTable (@"Could not find application for specified URL", @"WebError", @"WebErrorCouldNotFindApplicationForURL description")

#define WebErrorDescriptionFileDoesNotExist NSLocalizedStringFromTable (@"Could not find file", @"WebError", @"WebErrorFileDoesNotExist description")
    
#define WebErrorDescriptionFileNotReadable NSLocalizedStringFromTable (@"Could not read file", @"WebError", @"WebErrorFileNotReadable description")

#define WebErrorDescriptionFinderCouldNotOpenDirectory NSLocalizedStringFromTable (@"Finder could not open directory", @"WebError", @"WebErrorFinderCouldNotOpenDirectory description")
    
#define WebErrorDescriptionCannotShowDirectory NSLocalizedStringFromTable (@"Can't show a file directory", @"WebError", @"WebErrorCannotShowDirectory description")

#define WebErrorDescriptionCannotShowURL NSLocalizedStringFromTable (@"Can't show the specified URL", @"WebError", @"WebErrorCannotShowURL description")