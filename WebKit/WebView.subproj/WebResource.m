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

#define WebResourceVersion 1

@interface WebResourcePrivate : NSObject
{
@public
    NSData *data;
    NSURL *URL;
    NSString *frameName;
    NSString *MIMEType;
    NSString *textEncodingName;
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
    return [self _initWithData:data URL:URL MIMEType:MIMEType textEncodingName:textEncodingName frameName:frameName copyData:YES];
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

- (id)_initWithData:(NSData *)data URL:(NSURL *)URL MIMEType:(NSString *)MIMEType textEncodingName:(NSString *)textEncodingName frameName:(NSString *)frameName copyData:(BOOL)copyData
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
    
    return self;
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
             textEncodingName:[propertyList _web_stringForKey:WebResourceTextEncodingNameKey]
                    frameName:[propertyList _web_stringForKey:WebResourceFrameNameKey]];
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
    if (_private->frameName) {
        [propertyList setObject:_private->frameName forKey:WebResourceFrameNameKey];
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

