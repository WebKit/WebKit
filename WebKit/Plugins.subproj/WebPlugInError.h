//
//  WebPluginError.h
//  WebKit
//
//  Created by Chris Blumenberg on Fri Nov 01 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>

#import <WebFoundation/WebError.h>

@class WebPluginErrorPrivate;

/*!
    @class WebPluginError
    @discussion WebPluginError is a subclass of WebError that is specific to plug-in related errors.
*/
@interface WebPluginError : WebError
{
@private
    WebPluginErrorPrivate *_private;
}

/*!
    @method contentURL
    @result The URL of the data that caused the error
*/
- (NSString *)contentURL;
/*!
    @method pluginPageURL
    @result Description forthcoming
*/
- (NSString *)pluginPageURL;
/*!
    @method pluginName
    @result The name of plugin that experienced the error
*/
- (NSString *)pluginName;
/*!
    @method MIMEType
    @result The MIME type of the data that caused the error
*/
- (NSString *)MIMEType;

@end
