/*	
        WebKitErrors.h
	Copyright 2002, Apple, Inc. All rights reserved.
        
        Public header file.
*/

extern NSString *WebErrorDomainWebKit;

/*!
    @enum
    @constant WebKitErrorCannotCreateFile
    @constant WebKitErrorCannotOpenFile
    @constant WebKitErrorCannotCloseFile
    @constant WebKitErrorCannotWriteToFile
    @constant WebKitErrorCannotRemoveFile
    @constant WebKitErrorCannotMoveFile
    @constant WebKitErrorCannotFindApplicationForFile
    @constant WebKitErrorCannotFindApplicationForURL
    @constant WebKitErrorFinderCannotOpenDirectory
    @constant WebKitErrorCannotShowDirectory
    @constant WebKitErrorCannotShowMIMEType
    @constant WebKitErrorCannotShowURL
    @constant WebKitErrorLocationChangeInterruptedByPolicyChange
    @constant WebKitErrorDownloadDecodingFailedMidStream
    @constant WebKitErrorDownloadDecodingFailedToComplete
*/
enum {
    WebKitErrorCannotCreateFile,
    WebKitErrorCannotOpenFile,
    WebKitErrorCannotCloseFile,
    WebKitErrorCannotWriteToFile,
    WebKitErrorCannotRemoveFile,
    WebKitErrorCannotMoveFile,
    WebKitErrorCannotFindApplicationForFile,
    WebKitErrorCannotFindApplicationForURL,
    WebKitErrorFinderCannotOpenDirectory,
    WebKitErrorCannotShowDirectory,
    WebKitErrorCannotShowMIMEType,
    WebKitErrorCannotShowURL,
    WebKitErrorLocationChangeInterruptedByPolicyChange,
    WebKitErrorResourceLoadInterruptedByPolicyChange,
    WebKitErrorCannotFindPlugin,
    WebKitErrorCannotLoadPlugin,
    WebKitErrorJavaUnavailable,
    WebKitErrorDownloadDecodingFailedMidStream,
    WebKitErrorDownloadDecodingFailedToComplete,
};
