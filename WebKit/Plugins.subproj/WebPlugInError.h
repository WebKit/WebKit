/*
    WebPlugInError.h
    Copyright 2002, Apple, Inc. All rights reserved.

    Public header file.
 */

#import <Foundation/Foundation.h>

#if !defined(MAC_OS_X_VERSION_10_3) || (MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_3)
#import <WebFoundation/NSError.h>
#endif

@class WebPlugInErrorPrivate;

/*!
    @class WebPlugInError
    @discussion WebPlugInError is a subclass of NSError that is specific to plug-in related errors.
*/
@interface WebPlugInError : NSError
{
@private
    WebPlugInErrorPrivate *_private;
}

/*!
    @method contentURL
    @result The URL of the data that caused the error
*/
- (NSString *)contentURLString;

/*!
    @method plugInPageURL
    @result Description forthcoming
*/
- (NSString *)plugInPageURLString;

/*!
    @method pluInName
    @result The name of plugin that experienced the error
*/
- (NSString *)plugInName;

/*!
    @method MIMEType
    @result The MIME type of the data that caused the error
*/
- (NSString *)MIMEType;

@end
