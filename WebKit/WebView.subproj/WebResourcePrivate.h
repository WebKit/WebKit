/*	
    WebResourcePrivate.h
    Copyright (C) 2004 Apple Computer, Inc. All rights reserved.    
 
    Private header file.
*/

#import <WebKit/WebResource.h>

@interface WebResource (WebResourcePrivate)

+ (NSArray *)_resourcesFromPropertyLists:(NSArray *)propertyLists;
+ (NSArray *)_propertyListsFromResources:(NSArray *)resources;

- (id)_initWithPropertyList:(id)propertyList;
- (id)_initWithCachedResponse:(NSCachedURLResponse *)response originalURL:(NSURL *)originalURL;

- (NSFileWrapper *)_fileWrapperRepresentation;
- (id)_propertyListRepresentation;
- (NSURLResponse *)_response;

@end