/*	
    WebResource.m
    Copyright (C) 2004 Apple Computer, Inc. All rights reserved.    
*/

#import <WebKit/WebResourcePrivate.h>
#import <WebKit/WebNSURLExtras.h>

#import <Foundation/NSDictionary_NSURLExtras.h>

NSString *WebHTMLPboardType =               @"Apple Web Kit pasteboard type";
NSString *WebMainResourceKey =              @"WebMainResource";
NSString *WebResourceDataKey =              @"WebResourceData";
NSString *WebResourceMIMETypeKey =          @"WebResourceMIMEType";
NSString *WebResourceURLKey =               @"WebResourceURL";
NSString *WebResourceTextEncodingNameKey =  @"WebResourceTextEncodingName";
NSString *WebSubresourcesKey =              @"WebSubresources";

@interface WebResourcePrivate : NSObject
{
@public
    NSData *data;
    NSURL *URL;
    NSString *MIMEType;
    NSString *textEncodingName;
}
@end

@implementation WebResourcePrivate

- (void)dealloc
{
    [data release];
    [URL release];
    [MIMEType release];
    [textEncodingName release];
    [super dealloc];
}

@end

@implementation WebResource

- (id)initWithData:(NSData *)data URL:(NSURL *)URL MIMEType:(NSString *)MIMEType textEncodingName:(NSString *)textEncodingName
{
    _private = [[WebResourcePrivate alloc] init];
    
    if (!data) {
        [self release];
        return nil;
    }
    _private->data = [data copy];
    
    if (!URL) {
        [self release];
        return nil;
    }
    _private->URL = [URL copy];
    
    if (!MIMEType) {
        [self release];
        return nil;
    }
    _private->MIMEType = [MIMEType copy];
    
    _private->textEncodingName = [textEncodingName copy];
    
    return self;
}

- (void)dealloc
{
    [_private release];
    [super dealloc];
}

- (NSData *)data
{
    return _private->data;
}

- (NSURL *)URL
{
    return _private->URL;
}

- (NSString *)MIMEType
{
    return _private->MIMEType;
}

- (NSString *)textEncodingName
{
    return _private->textEncodingName;
}

@end

@implementation WebResource (WebResourcePrivate)

+ (NSArray *)_resourcesFromPropertyLists:(NSArray *)propertyLists
{
    if (![propertyLists isKindOfClass:[NSArray class]]) {
        return nil;
    }
    NSEnumerator *enumerator = [propertyLists objectEnumerator];
    NSMutableArray *resources = [NSMutableArray array];
    NSDictionary *propertyList;
    while ((propertyList = [enumerator nextObject]) != nil) {
        WebResource *resource = [[WebResource alloc] _initWithPropertyList:propertyList];
        if (resource) {
            [resources addObject:resource];
            [resource release];
        }
    }
    return resources;
}

+ (NSArray *)_propertyListsFromResources:(NSArray *)resources
{
    NSEnumerator *enumerator = [resources objectEnumerator];
    NSMutableArray *propertyLists = [NSMutableArray array];
    WebResource *resource;
    while ((resource = [enumerator nextObject]) != nil) {
        [propertyLists addObject:[resource _propertyListRepresentation]];
    }
    return propertyLists;
}

- (id)_initWithPropertyList:(id)propertyList
{
    if (![propertyList isKindOfClass:[NSDictionary class]]) {
        [self release];
        return nil;
    }
    NSData *data = [propertyList objectForKey:WebResourceDataKey];
    NSString *URLString = [propertyList _web_stringForKey:WebResourceURLKey];
    return [self initWithData:[data isKindOfClass:[NSData class]] ? data : nil
                          URL:URLString ? [NSURL _web_URLWithDataAsString:URLString] : nil
                     MIMEType:[propertyList _web_stringForKey:WebResourceMIMETypeKey]
             textEncodingName:[propertyList _web_stringForKey:WebResourceTextEncodingNameKey]];
}

- (id)_initWithCachedResponse:(NSCachedURLResponse *)cachedResponse originalURL:(NSURL *)originalURL
{
    NSURLResponse *response = [cachedResponse response];
    return [self initWithData:[cachedResponse data]
                          URL:originalURL
                     MIMEType:[response MIMEType]
             textEncodingName:[response textEncodingName]];
}

- (id)_propertyListRepresentation
{
    NSMutableDictionary *propertyList = [NSMutableDictionary dictionary];
    [propertyList setObject:_private->data forKey:WebResourceDataKey];
    [propertyList setObject:[_private->URL _web_originalDataAsString] forKey:WebResourceURLKey];
    [propertyList setObject:_private->MIMEType forKey:WebResourceMIMETypeKey];
    if (_private->textEncodingName) {
        [propertyList setObject:_private->textEncodingName forKey:WebResourceTextEncodingNameKey];
    }
    return propertyList;
}

- (NSCachedURLResponse *)_cachedResponseRepresentation
{
    unsigned length = [_private->data length];
    NSURLResponse *response = [[NSURLResponse alloc] initWithURL:_private->URL
                                                        MIMEType:_private->MIMEType 
                                           expectedContentLength:length
                                                textEncodingName:_private->textEncodingName];
    NSCachedURLResponse *cachedResponse = [[[NSCachedURLResponse alloc] initWithResponse:response data:_private->data] autorelease];
    [response release];
    return cachedResponse;
}

@end
