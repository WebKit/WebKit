/*	
    WebArchive.h
    Copyright (C) 2004 Apple Computer, Inc. All rights reserved.    
 
    Public header file.
*/

#import <Foundation/Foundation.h>

@class WebArchivePrivate;
@class WebResource;

/*!
    @const WebArchivePboardType
    @abstract The pasteboard type constant used when adding or accessing a WebArchive on the pasteboard.
*/
extern NSString *WebArchivePboardType;

/*!
    @class WebArchive
    @discussion WebArchive represents a main resource as well as all the subresources and subframes associated with the main resource.
    The main resource can be an entire web page, a portion of a web page, or some other kind of data such as an image.
    This class can be used for saving standalone web pages, representing portions of a web page on the pasteboard, or any other
    application where one class is needed to represent rich web content. 
*/
@interface WebArchive : NSObject <NSCoding, NSCopying>
{
    @private
    WebArchivePrivate *_private;
}

/*!
    @method initWithMainResource:subresources:subframeArchives:
    @abstract The initializer for WebArchive.
    @param mainResource The main resource of the archive.
    @param subresources The subresources of the archive (can be nil).
    @param subframeArchives The archives representing the subframes of the archive (can be nil).
    @result An initialized WebArchive.
*/
- (id)initWithMainResource:(WebResource *)mainResource subresources:(NSArray *)subresources subframeArchives:(NSArray *)subframeArchives;

/*!
    @method initWithData:
    @abstract The initializer for creating a WebArchive from data.
    @param data The data representing the archive. This can be obtained using WebArchive's data method.
    @result An initialized WebArchive.
*/
- (id)initWithData:(NSData *)data;

/*!
    @method mainResource
    @result The main resource of the archive.
*/
- (WebResource *)mainResource;

/*!
    @method subresources
    @result The subresource of the archive (can be nil).
*/
- (NSArray *)subresources;

/*!
    @method subframeArchives
    @result The archives representing the subframes of the archive (can be nil).
*/
- (NSArray *)subframeArchives;

/*!
    @method data
    @result The data representation of the archive.
    @discussion The data returned by this method can be used to save a web archive to a file or to place a web archive on the pasteboard
    using WebArchivePboardType. To create a WebArchive using the returned data, call initWithData:.
*/
- (NSData *)data;

@end
