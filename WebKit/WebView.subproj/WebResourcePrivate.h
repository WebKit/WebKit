/*	
    WebResourcePrivate.h
    Copyright (C) 2004 Apple Computer, Inc. All rights reserved.    
 
    Private header file.
*/

#import <WebKit/WebResource.h>

@interface WebResource (WebResourcePrivate)

- (id)_initWithData:(NSData *)data URL:(NSURL *)URL MIMEType:(NSString *)MIMEType textEncodingName:(NSString *)textEncodingName frameName:(NSString *)frameName copyData:(BOOL)copyData;

+ (NSArray *)_resourcesFromPropertyLists:(NSArray *)propertyLists;
+ (NSArray *)_propertyListsFromResources:(NSArray *)resources;

- (id)_initWithPropertyList:(id)propertyList;

- (NSFileWrapper *)_fileWrapperRepresentation;
- (id)_propertyListRepresentation;
- (NSURLResponse *)_response;
- (NSString *)_stringValue;

@end