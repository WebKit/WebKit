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
    @abstract The URL specified by the SRC attribute of the EMBED or PARAM tag.
*/
- (NSString *)contentURL;

/*!
    @method pluginPageURL
    @abstract The URL specified by the PLUGINSPAGE attribute of the EMBED or PARAM tag.
    @discussion This is the URL for the page that has information about the plug-in. May be nil.
*/
- (NSString *)pluginPageURL;

/*!
    @method pluginName
    @abstract The name of the plug-in that had the error. May be nil.
*/
- (NSString *)pluginName;

/*!
    @method MIMEType
    @abstract The MIME type specified by the TYPE attribute of the EMBED or PARAM tag.
    @discussion The MIME type of the content. May be nil.
*/
- (NSString *)MIMEType;

@end
