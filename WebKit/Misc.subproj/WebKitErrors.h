/*	IFWebKitErrors.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

// WebFoundation error codes < 10000
// WebKit error codes >= 10000

typedef enum {
    IFErrorCodeNoError = 0,
    IFErrorCodeCantShowMIMEType = 10000,
    IFErrorCodeFileDoesntExist = 10001,
    IFErrorCodeCouldntFindApplicationForFile = 10002,
    IFErrorCodeCouldntFindApplicationForURL = 10003,
    IFErrorCodeFileNotReadable = 10004,
    IFErrorCodeFinderCouldntOpenDirectory = 10005,
    IFErrorCodeCantShowDirectory = 10006,
    IFErrorCodeCantShowURL = 10007,
} IFErrorCode;

#define IFErrorDescriptionCantShowMIMEType NSLocalizedStringFromTable (@"Can't show content with specified mime type", @"IFError", @"IFErrorCodeCantShowMIMEType description")

#define IFErrorDescriptionCouldntFindApplicationForFile NSLocalizedStringFromTable (@"Could not find application for specified file", @"IFError", @"IFErrorCodeCouldntFindApplicationForFile description")

#define IFErrorDescriptionCouldntFindApplicationForURL NSLocalizedStringFromTable (@"Could not find application for specified URL", @"IFError", @"IFErrorCodeCouldntFindApplicationForURL description")

#define IFErrorDescriptionFileDoesntExist NSLocalizedStringFromTable (@"Could not find file", @"IFError", @"IFErrorCodeFileDoesntExist description")
    
#define IFErrorDescriptionFileNotReadable NSLocalizedStringFromTable (@"Could not read file", @"IFError", @"IFErrorCodeFileNotReadable description")

#define IFErrorDescriptionFinderCouldntOpenDirectory NSLocalizedStringFromTable (@"Finder could not open directory", @"IFError", @"IFErrorCodeFinderCouldntOpenDirectory description")
    
#define IFErrorDescriptionCantShowDirectory NSLocalizedStringFromTable (@"Can't show a file directory", @"IFError", @"IFErrorCodeCantShowDirectory description")

#define IFErrorDescriptionCantShowURL NSLocalizedStringFromTable (@"Can't show the specified URL", @"IFError", @"IFErrorCodeCantShowURL description")