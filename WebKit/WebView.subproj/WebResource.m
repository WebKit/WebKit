/*	
    WebResource.m
    Copyright (C) 2004 Apple Computer, Inc. All rights reserved.    
*/
#import <WebKit/WebBridge.h>
#import <WebKit/WebResourcePrivate.h>
#import <WebKit/WebNSURLExtras.h>

#import <Foundation/NSDictionary_NSURLExtras.h>
#import <Foundation/NSURL_NSURLExtras.h>

NSString *WebResourceDataKey =              @"WebResourceData";
NSString *WebResourceFrameNameKey =         @"WebResourceFrameName";
NSString *WebResourceMIMETypeKey =          @"WebResourceMIMEType";
NSString *WebResourceURLKey =               @"WebResourceURL";
NSString *WebResourceTextEncodingNameKey =  @"WebResourceTextEncodingName";
NSString *WebResourceResponseKey =          @"WebResourceResponse";

#define WebResourceVersion 1

@interface WebResourcePrivate : NSObject
{
@public
    NSData *data;
    NSURL *URL;
    NSString *frameName;
    NSString *MIMEType;
    NSString *textEncodingName;
    NSURLResponse *response;
}
@end

@implementation WebResourcePrivate

- (void)dealloc
{
    [data release];
    [URL release];
    [frameName release];
    [MIMEType release];
    [textEncodingName release];
    [response release];
    [super dealloc];
}

@end

@implementation WebResource

- (id)init
{
    [super init];
    _private = [[WebResourcePrivate alloc] init];
    return self;
}

- (id)initWithData:(NSData *)data URL:(NSURL *)URL MIMEType:(NSString *)MIMEType textEncodingName:(NSString *)textEncodingName frameName:(NSString *)frameName
{
    return [self _initWithData:data URL:URL MIMEType:MIMEType textEncodingName:textEncodingName frameName:frameName response:nil copyData:YES];
}

- (id)initWithCoder:(NSCoder *)decoder
{    
    NS_DURING
        [self init];
        _private->data = [[decoder decodeObjectForKey:WebResourceDataKey] retain];
        _private->URL = [[decoder decodeObjectForKey:WebResourceURLKey] retain];
        _private->MIMEType = [[decoder decodeObjectForKey:WebResourceMIMETypeKey] retain];
        _private->textEncodingName = [[decoder decodeObjectForKey:WebResourceTextEncodingNameKey] retain];
        _private->frameName = [[decoder decodeObjectForKey:WebResourceFrameNameKey] retain];
        _private->response = [[decoder decodeObjectForKey:WebResourceResponseKey] retain];
    NS_HANDLER
        [self release];
        return nil;
    NS_ENDHANDLER
    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder
{
    [encoder encodeObject:_private->data forKey:WebResourceDataKey];
    [encoder encodeObject:_private->URL forKey:WebResourceURLKey];
    [encoder encodeObject:_private->MIMEType forKey:WebResourceMIMETypeKey];
    [encoder encodeObject:_private->textEncodingName forKey:WebResourceTextEncodingNameKey];
    [encoder encodeObject:_private->frameName forKey:WebResourceFrameNameKey];
    [encoder encodeObject:_private->response forKey:WebResourceResponseKey];
}

- (void)dealloc
{
    [_private release];
    [super dealloc];
}

- (id)copyWithZone:(NSZone *)zone
{
    return [self retain];
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

- (NSString *)frameName
{
    return _private->frameName;
}

- (id)description
{
    return [NSString stringWithFormat:@"<%@ %@>", [self className], [self URL]];
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

- (id)_initWithData:(NSData *)data 
                URL:(NSURL *)URL 
           MIMEType:(NSString *)MIMEType 
   textEncodingName:(NSString *)textEncodingName 
          frameName:(NSString *)frameName 
           response:(NSURLResponse *)response
           copyData:(BOOL)copyData
{
    [self init];    
    
    if (!data) {
        [self release];
        return nil;
    }
    _private->data = copyData ? [data copy] : [data retain];
    
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
    _private->frameName = [frameName copy];
    _private->response = [response retain];
        
    return self;
}

- (id)_initWithData:(NSData *)data URL:(NSURL *)URL response:(NSURLResponse *)response
{
    // Pass NO for copyData since the data doesn't need to be copied since we know that callers will no longer modify it.
    // Copying it will also cause a performance regression.
    return [self _initWithData:data
                           URL:URL
                      MIMEType:[response MIMEType]
              textEncodingName:[response textEncodingName]
                     frameName:nil
                      response:response
                      copyData:NO];    
}

- (id)_initWithPropertyList:(id)propertyList
{
    if (![propertyList isKindOfClass:[NSDictionary class]]) {
        [self release];
        return nil;
    }
    
    NSURLResponse *response = nil;
    NSData *responseData = [propertyList objectForKey:WebResourceResponseKey];
    if ([responseData isKindOfClass:[NSData class]]) {
        NSKeyedUnarchiver *unarchiver = [[NSKeyedUnarchiver alloc] initForReadingWithData:responseData];
        response = [unarchiver decodeObjectForKey:WebResourceResponseKey];
        [unarchiver finishDecoding];
        [unarchiver release];    
    }

    NSData *data = [propertyList objectForKey:WebResourceDataKey];
    NSString *URLString = [propertyList _web_stringForKey:WebResourceURLKey];
    return [self _initWithData:[data isKindOfClass:[NSData class]] ? data : nil
                           URL:URLString ? [NSURL _web_URLWithDataAsString:URLString] : nil
                      MIMEType:[propertyList _web_stringForKey:WebResourceMIMETypeKey]
              textEncodingName:[propertyList _web_stringForKey:WebResourceTextEncodingNameKey]
                     frameName:[propertyList _web_stringForKey:WebResourceFrameNameKey]
                      response:response
                      copyData:NO];
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
    if (_private->textEncodingName != nil) {
        [propertyList setObject:_private->textEncodingName forKey:WebResourceTextEncodingNameKey];
    }
    if (_private->frameName != nil) {
        [propertyList setObject:_private->frameName forKey:WebResourceFrameNameKey];
    }    
    if (_private->response != nil) {
        NSMutableData *responseData = [[NSMutableData alloc] init];
        NSKeyedArchiver *archiver = [[NSKeyedArchiver alloc] initForWritingWithMutableData:responseData];
        [archiver encodeObject:_private->response forKey:WebResourceResponseKey];
        [archiver finishEncoding];
        [archiver release];
        [propertyList setObject:responseData forKey:WebResourceResponseKey];
        [responseData release];
    }        
    return propertyList;
}

- (NSURLResponse *)_response
{
    if (_private->response != nil) {
        return _private->response;
    }
    return [[[NSURLResponse alloc] initWithURL:_private->URL
                                      MIMEType:_private->MIMEType 
                         expectedContentLength:[_private->data length]
                              textEncodingName:_private->textEncodingName] autorelease];
}

- (NSString *)_stringValue
{
    NSString *textEncodingName = [self textEncodingName];
    
    if(textEncodingName){
        return [WebBridge stringWithData:_private->data textEncodingName:textEncodingName];
    }else{
        return [WebBridge stringWithData:_private->data textEncoding:kCFStringEncodingISOLatin1];
    }
}

@end
