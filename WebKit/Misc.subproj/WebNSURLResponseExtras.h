/*
    WebNSURLResponseExtras.h
    Copyright 2003, Apple, Inc. All rights reserved.

    Public header file.
*/

#import <WebFoundation/NSURLResponse.h>

@interface NSURLResponse (WebNSURLResponseExtras)

/*!
    @method suggestedFilenameForSaving
    @abstract Returns a suggested filename if the resource were saved to disk.
    @discussion The method first checks if the server has specified a filename using the
    content disposition header. If no valid filename is specified using that mechanism,
    this method checks the last path component of the URL. If no valid filename can be
    obtained using the last path component, this method uses the URL's host as the filename.
    If the URL's host can't be converted to a valid filename, the filename "unknown" is used.
    In mose cases, this method appends the proper file extension based on the MIME type.
    This method always returns a valid filename.
    @result A suggested filename to use if saving the resource to disk.
*/
- (NSString *)suggestedFilenameForSaving;

@end
