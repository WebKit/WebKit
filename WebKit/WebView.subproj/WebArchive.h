/*	
    WebArchive.h
    Copyright (C) 2004 Apple Computer, Inc. All rights reserved.    
 
    Public header file.
*/

#import <Foundation/Foundation.h>

@class WebArchivePrivate;
@class WebResource;

extern NSString *WebArchivePboardType;

@interface WebArchive : NSObject 
{
    @private
    WebArchivePrivate *_private;
}

- (id)initWithMainResource:(WebResource *)mainResource subresources:(NSArray *)subresources;
- (id)initWithData:(NSData *)data;

- (WebResource *)mainResource;
- (NSArray *)subresources;

- (NSData *)data;

@end
