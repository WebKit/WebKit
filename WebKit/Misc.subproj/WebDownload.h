/*	
    WebDownload.h
    Copyright (C) 2003 Apple Computer, Inc. All rights reserved.    
    
    Public header file.
*/

#import <Foundation/NSURLDownload.h>

@class WebDownloadInternal;

/*!
    @class WebDownload
    @discussion A WebDownload works just like an NSURLDownload, with
    one extra feature: if you do not implement the
    authentication-related delegate methods, it will automatically
    prompt for authentication using the standard WebKit authentication
    panel, as either a sheet or window. It provides no extra methods,
    but does have one additional delegate method.
*/


@interface WebDownload : NSURLDownload
{
@private
    WebDownloadInternal *_webInternal;
}

@end


/*!
    @protocol WebDownloadDelegate
    @discussion The WebDownloadDelegate delegate has one extra method used to choose
    the right window when automatically prompting with a sheet.
*/
@interface NSObject (WebDownloadDelegate)

/*!
    @method downloadWindowForAuthenticationSheet:
    @abstract
    @param
    @result
*/
- (NSWindow *)downloadWindowForAuthenticationSheet:(WebDownload *)download;

@end
