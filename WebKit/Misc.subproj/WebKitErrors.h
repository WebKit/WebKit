/*	
        WebKitErrors.h
	Copyright 2002, Apple, Inc. All rights reserved.
        
        Public header file.
*/

extern NSString *WebKitErrorDomain;

/*!
    @enum
    @description Download and file I/O errors
    @constant WebKitErrorCannotCreateFile
    @constant WebKitErrorCannotOpenFile
    @constant WebKitErrorCannotCloseFile
    @constant WebKitErrorCannotWriteToFile
    @constant WebKitErrorCannotRemoveFile
    @constant WebKitErrorCannotMoveFile
*/
enum {
    WebKitErrorCannotCreateFile = 				0,
    WebKitErrorCannotOpenFile = 				1,
    WebKitErrorCannotCloseFile = 				2,
    WebKitErrorCannotWriteToFile = 				3,
    WebKitErrorCannotRemoveFile = 				4,
    WebKitErrorCannotMoveFile = 				5,
    WebKitErrorDownloadDecodingFailedMidStream = 		6,
    WebKitErrorDownloadDecodingFailedToComplete = 		7,
};

/*!
    @enum
    @description Policy errors
    @constant WebKitErrorCannotShowMIMEType
    @constant WebKitErrorCannotShowURL
    @constant WebKitErrorLocationChangeInterruptedByPolicyChange
    @constant WebKitErrorResourceLoadInterruptedByPolicyChange
*/
enum {
    WebKitErrorCannotShowMIMEType = 				100,
    WebKitErrorCannotShowURL = 					101,
    WebKitErrorLocationChangeInterruptedByPolicyChange = 	102,
};

/*!
    @enum
    @description Plug-in and java errors
    @constant WebKitErrorCannotFindPlugin
    @constant WebKitErrorCannotLoadPlugin
    @constant WebKitErrorJavaUnavailable
*/
enum {
    WebKitErrorCannotFindPlugin = 				200,
    WebKitErrorCannotLoadPlugin = 				201,
    WebKitErrorJavaUnavailable = 				202,
};
