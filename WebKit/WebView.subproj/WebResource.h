/*	
    WebResource.h
    Copyright (C) 2004 Apple Computer, Inc. All rights reserved.    
 
    Public header file.
*/

#import <Foundation/Foundation.h>

@class WebMainResourcePrivate;
@class WebResourcePrivate;


/*!
    @class WebResource
    @discussion A WebResource represents a fully downloaded URL. 
    It includes the data of the resource as well as the metadata associated with the resource.
*/
@interface WebResource : NSObject <NSCoding, NSCopying>
{
@private
    WebResourcePrivate *_private;
}

/*!
    @method initWithData:URL:MIMEType:textEncodingName:frameName
    @abstract The initializer for WebResource.
    @param data The data of the resource.
    @param URL The URL of the resource.
    @param MIMEType The MIME type of the resource.
    @param textEncodingName The text encoding name of the resource (can be nil).
    @param frameName The frame name of the resource if the resource represents the contents of an entire HTML frame (can be nil).
    @result An initialized WebResource.
*/
- (id)initWithData:(NSData *)data URL:(NSURL *)URL MIMEType:(NSString *)MIMEType textEncodingName:(NSString *)textEncodingName frameName:(NSString *)frameName;

/*!
    @method data
    @result The data of the resource.
*/
- (NSData *)data;

/*!
    @method URL
    @result The URL of the resource.
*/
- (NSURL *)URL;

/*!
    @method MIMEType
    @result The MIME type of the resource.
*/
- (NSString *)MIMEType;

/*!
    @method textEncodingName
    @result The text encoding name of the resource (can be nil).
*/
- (NSString *)textEncodingName;

/*!
    @method frameName
    @result The frame name of the resource if the resource represents the contents of an entire HTML frame (can be nil).
*/
- (NSString *)frameName;

@end
