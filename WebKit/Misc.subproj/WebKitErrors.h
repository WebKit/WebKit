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
    @constant WebErrorLocationChangeInterruptedByPolicyChange
    @constant WebErrorDownloadDecodingFailedMidStream
    @constant WebErrorDownloadDecodingFailedToComplete
*/
enum {
    WebErrorCannotFindFile,
    WebErrorCannotCreateFile,
    WebErrorCannotOpenFile,
    WebErrorCannotReadFile,
    WebErrorCannotWriteToFile,
    WebErrorCannotRemoveFile,
    WebErrorCannotFindApplicationForFile,
    WebErrorFinderCannotOpenDirectory,
    WebErrorCannotShowDirectory,
    WebErrorCannotShowMIMEType,
    WebErrorCannotShowURL,
    WebErrorCannotNotFindApplicationForURL,
    WebErrorLocationChangeInterruptedByPolicyChange,
    WebErrorResourceLoadInterruptedByPolicyChange,
    WebErrorCannotFindPlugin,
    WebErrorCannotLoadPlugin,
    WebErrorJavaUnavailable,
    WebErrorDownloadDecodingFailedMidStream,
    WebErrorDownloadDecodingFailedToComplete,
};
