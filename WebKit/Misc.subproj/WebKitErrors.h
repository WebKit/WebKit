/*	
        WebKitErrors.h
	Copyright 2002, Apple, Inc. All rights reserved.
        
        Public header file.
*/

// WebFoundation error codes < 10000
// WebKit error codes >= 10000

extern NSString *WebErrorDomainWebKit;

/*!
    @enum
    @constant WebKitErrorCannotCreateFile
    @constant WebKitErrorCannotOpenFile
    @constant WebKitErrorCannotWriteToFile
    @constant WebKitErrorCannotRemoveFile
    @constant WebKitErrorCannotFindApplicationForFile
    @constant WebKitErrorFinderCannotOpenDirectory
    @constant WebKitErrorCannotShowDirectory
    @constant WebKitErrorCannotShowMIMEType
    @constant WebKitErrorCannotShowURL
    @constant WebKitErrorCannotNotFindApplicationForURL
    @constant WebKitErrorLocationChangeInterruptedByPolicyChange
    @constant WebKitErrorDownloadDecodingFailedMidStream
    @constant WebKitErrorDownloadDecodingFailedToComplete
*/
enum {
    WebKitErrorCannotCreateFile,
    WebKitErrorCannotOpenFile,
    WebKitErrorCannotWriteToFile,
    WebKitErrorCannotRemoveFile,
    WebKitErrorCannotFindApplicationForFile,
    WebKitErrorFinderCannotOpenDirectory,
    WebKitErrorCannotShowDirectory,
    WebKitErrorCannotShowMIMEType,
    WebKitErrorCannotShowURL,
    WebKitErrorCannotNotFindApplicationForURL,
    WebKitErrorLocationChangeInterruptedByPolicyChange,
    WebKitErrorResourceLoadInterruptedByPolicyChange,
    WebKitErrorCannotFindPlugin,
    WebKitErrorCannotLoadPlugin,
    WebKitErrorJavaUnavailable,
    WebKitErrorDownloadDecodingFailedMidStream,
    WebKitErrorDownloadDecodingFailedToComplete,
};
