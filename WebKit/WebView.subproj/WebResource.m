/*	
    WebResource.m
    Copyright (C) 2004 Apple Computer, Inc. All rights reserved.    
*/

#import <WebKit/WebResourcePrivate.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebNSURLExtras.h>

#import <Foundation/NSDictionary_NSURLExtras.h>
#import <Foundation/NSURL_NSURLExtras.h>

NSString *WebArchivePboardType =            @"Apple Web Archive pasteboard type";
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

+ (BOOL)_parseWebArchive:(NSData *)webArchive mainResource:(WebResource **)mainResource subresources:(NSArray **)subresources
{
#if !LOG_DISABLED
    CFAbsoluteTime start = CFAbsoluteTimeGetCurrent();
#endif
    NSDictionary *propertyList = [NSPropertyListSerialization propertyListFromData:webArchive 
                                                                  mutabilityOption:NSPropertyListImmutable 
                                                                            format:nil
                                                                  errorDescription:nil];
#if !LOG_DISABLED
    CFAbsoluteTime end = CFAbsoluteTimeGetCurrent();
    CFAbsoluteTime duration = end - start;
#endif
    LOG(Timing, "Parsing web archive with [NSPropertyListSerialization propertyListFromData::::] took %f seconds", duration);
    if ([propertyList isKindOfClass:[NSDictionary class]]) {
        NSDictionary *resourcePropertyList = [propertyList objectForKey:WebMainResourceKey];
        if (resourcePropertyList) {
            *mainResource = [[[WebResource alloc] _initWithPropertyList:resourcePropertyList] autorelease];
            if (*mainResource) {
                *subresources = [WebResource _resourcesFromPropertyLists:[propertyList objectForKey:WebSubresourcesKey]];
                return YES;
            }
        }
    }
    return NO;
}

+ (NSData *)_webArchiveWithMainResource:(WebResource *)mainResource subresources:(NSArray *)subresources
{
    NSMutableDictionary *propertyList = [NSMutableDictionary dictionary];
    [propertyList setObject:[mainResource _propertyListRepresentation] forKey:WebMainResourceKey];
    NSArray *propertyLists = [self _propertyListsFromResources:subresources];
    if ([propertyLists count] > 0) {
        [propertyList setObject:propertyLists forKey:WebSubresourcesKey];
    }
#if !LOG_DISABLED
    CFAbsoluteTime start = CFAbsoluteTimeGetCurrent();
#endif
    NSData *webArchive = [NSPropertyListSerialization dataFromPropertyList:propertyList format:NSPropertyListBinaryFormat_v1_0 errorDescription:nil];
#if !LOG_DISABLED
    CFAbsoluteTime end = CFAbsoluteTimeGetCurrent();
    CFAbsoluteTime duration = end - start;
#endif
    LOG(Timing, "Creating web archive with [NSPropertyListSerialization dataFromPropertyList:::] took %f seconds", duration);
    return webArchive;
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

- (NSFileWrapper *)_fileWrapperRepresentation
{
    NSFileWrapper *wrapper = [[[NSFileWrapper alloc] initRegularFileWithContents:_private->data] autorelease];
    [wrapper setPreferredFilename:[_private->URL _web_suggestedFilenameWithMIMEType:_private->MIMEType]];
    return wrapper;
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

- (NSURLResponse *)_response
{
    return [[[NSURLResponse alloc] initWithURL:_private->URL
                                      MIMEType:_private->MIMEType 
                         expectedContentLength:[_private->data length]
                              textEncodingName:_private->textEncodingName] autorelease];
}

@end
