/*	
    WebResourcePrivate.h
    Copyright (C) 2004 Apple Computer, Inc. All rights reserved.    
 
    Private header file.
*/

#import <WebKit/WebResource.h>

extern NSString *WebMainResourceKey;
extern NSString *WebResourceDataKey;
extern NSString *WebResourceMIMETypeKey;
extern NSString *WebResourceURLKey;
extern NSString *WebResourceTextEncodingNameKey;
extern NSString *WebSubresourcesKey;

@interface WebResource (WebResourcePrivate)

+ (NSArray *)_resourcesFromPropertyLists:(NSArray *)propertyLists;
+ (NSArray *)_propertyListsFromResources:(NSArray *)resources;

+ (BOOL)_parseWebArchive:(NSData *)webArchive mainResource:(WebResource **)mainResource subresources:(NSArray **)subresources;
+ (NSData *)_webArchiveWithMainResource:(WebResource *)mainResource subresources:(NSArray *)subresources;

- (id)_initWithPropertyList:(id)propertyList;
- (id)_initWithCachedResponse:(NSCachedURLResponse *)response originalURL:(NSURL *)originalURL;

- (NSFileWrapper *)_fileWrapperRepresentation;
- (id)_propertyListRepresentation;
- (NSURLResponse *)_response;

@end