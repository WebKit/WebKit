/*	
        WebKitErrors.h
	Copyright 2002, Apple, Inc. All rights reserved.
        
        Public header file.
*/

// WebFoundation error codes < 10000
// WebKit error codes >= 10000

/*!
    @enum WebErrorCode
    @constant WebErrorNoError
    @constant WebErrorCannotFindFile
    @constant WebErrorCannotCreateFile
    @constant WebErrorCannotOpenFile
    @constant WebErrorCannotReadFile
    @constant WebErrorCannotWriteToFile
    @constant WebErrorCannotRemoveFile
    @constant WebErrorCannotFindApplicationForFile
    @constant WebErrorFinderCannotOpenDirectory
    @constant WebErrorCannotShowDirectory
    @constant WebErrorCannotShowMIMEType
    @constant WebErrorCannotShowURL
    @constant WebErrorCannotNotFindApplicationForURL
*/
typedef enum {
    WebErrorNoError = 0,
    WebErrorCannotFindFile = 10000,
    WebErrorCannotCreateFile = 10001,
    WebErrorCannotOpenFile = 10002,
    WebErrorCannotReadFile = 10003,
    WebErrorCannotWriteToFile = 10004,
    WebErrorCannotRemoveFile = 10005,
    WebErrorCannotFindApplicationForFile = 10006,
    WebErrorFinderCannotOpenDirectory = 10007,
    WebErrorCannotShowDirectory = 10008,
    WebErrorCannotShowMIMEType = 10009,
    WebErrorCannotShowURL = 10010,
    WebErrorCannotNotFindApplicationForURL = 10011
} WebErrorCode;

#define WebErrorDescriptionCannotFindFile NSLocalizedStringFromTable (@"Cannot find file", @"WebError", @"WebErrorCannotFindFile description")

#define WebErrorDescriptionCannotCreateFile NSLocalizedStringFromTable (@"Cannot create file", @"WebError", @"WebErrorCannotCreateFile description")

#define WebErrorDescriptionCannotOpenFile NSLocalizedStringFromTable (@"Cannot open file", @"WebError", @"WebErrorCannotOpenFile description")

#define WebErrorDescriptionCannotReadFile NSLocalizedStringFromTable (@"Cannot read file", @"WebError", @"WebErrorCannotReadFile description")

#define WebErrorDescriptionCannotWriteToFile NSLocalizedStringFromTable (@"Cannot write file", @"WebError", @"WebErrorCannotWriteToFile description")

#define WebErrorDescriptionCannotRemoveFile NSLocalizedStringFromTable (@"Cannot remove file", @"WebError", @"WebErrorCannotRemoveFile description")

#define WebErrorDescriptionCannotFindApplicationForFile NSLocalizedStringFromTable (@"Cannot find application for file", @"WebError", @"WebErrorCannotFindApplicationForFile description")

#define WebErrorDescriptionFinderCannotOpenDirectory NSLocalizedStringFromTable (@"Finder cannot open directory", @"WebError", @"WebErrorFinderCannotOpenDirectory description")

#define WebErrorDescriptionCannotShowDirectory NSLocalizedStringFromTable (@"Cannot show a file directory", @"WebError", @"WebErrorCannotShowDirectory description")

#define WebErrorDescriptionCannotShowMIMEType NSLocalizedStringFromTable (@"Cannot show content with specified mime type", @"WebError", @"WebErrorCannotShowMIMEType description")

#define WebErrorDescriptionCannotShowURL NSLocalizedStringFromTable (@"Cannot show URL", @"WebError", @"WebErrorCannotShowURL description")

#define WebErrorDescriptionCannotFindApplicationForURL NSLocalizedStringFromTable (@"Cannot find application for URL", @"WebError", @"WebErrorCannotNotFindApplicationForURL description")




