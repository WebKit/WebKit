/*	
    WebResource.h
    Copyright (C) 2004 Apple Computer, Inc. All rights reserved.    
 
    Public header file.
*/

#import <Foundation/Foundation.h>

@class WebMainResourcePrivate;
@class WebResourcePrivate;

@interface WebResource : NSObject 
{
@private
    WebResourcePrivate *_private;
}

- (id)initWithData:(NSData *)data URL:(NSURL *)URL MIMEType:(NSString *)MIMEType textEncodingName:(NSString *)textEncodingName frameName:(NSString *)frameName;

- (NSData *)data;
- (NSURL *)URL;
- (NSString *)MIMEType;
- (NSString *)textEncodingName;
- (NSString *)frameName;

@end
