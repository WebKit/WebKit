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
    @constant WebErrorLocationChangeInterruptedByURLPolicyChange
*/
enum {
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
    WebErrorCannotNotFindApplicationForURL = 10011,
    WebErrorLocationChangeInterruptedByURLPolicyChange = 10012
};
