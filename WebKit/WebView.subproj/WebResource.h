/*	
    WebResource.h
    Copyright (C) 2004 Apple Computer, Inc. All rights reserved.    
 
    Public header file.
*/

#import <Foundation/Foundation.h>

@class WebArchivePrivate;
@class WebResourcePrivate;

extern NSString *WebArchivePboardType;

@interface WebResource : NSObject 
{
@private
    WebResourcePrivate *_private;
}

- (id)initWithData:(NSData *)data URL:(NSURL *)URL MIMEType:(NSString *)MIMEType textEncodingName:(NSString *)textEncodingName;

- (NSData *)data;
- (NSURL *)URL;
- (NSString *)MIMEType;
- (NSString *)textEncodingName;

@end


@interface WebArchive : NSObject 
{
@private
    WebArchivePrivate *_private;
}

- (id)initWithMainResource:(WebResource *)mainResource subresources:(NSArray *)subresources;
- (id)initWithData:(NSData *)data;

- (WebResource *)mainResource;
- (NSArray *)subresources;

- (NSData *)dataRepresentation;

@end