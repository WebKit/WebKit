/*	
    WebArchive.m
    Copyright (C) 2004 Apple Computer, Inc. All rights reserved.    
*/

#import <WebKit/WebArchive.h>

#import <WebKit/WebKitLogging.h>
#import <WebKit/WebResourcePrivate.h>

NSString *WebArchivePboardType =            @"Apple Web Archive pasteboard type";
NSString *WebMainResourceKey =              @"WebMainResource";
NSString *WebSubresourcesKey =              @"WebSubresources";
NSString *WebSubframeArchivesKey =          @"WebSubframeArchives";

@interface WebArchivePrivate : NSObject
{
    @public
    WebResource *mainResource;
    NSArray *subresources;
    NSArray *subframeArchives;
}
@end

@implementation WebArchivePrivate

- (void)dealloc
{
    [mainResource release];
    [subresources release];
    [subframeArchives release];
    [super dealloc];
}

@end

@implementation WebArchive

- (id)init
{
    [super init];
    _private = [[WebArchivePrivate alloc] init];
    return self;
}

- (id)initWithMainResource:(WebResource *)mainResource subresources:(NSArray *)subresources subframeArchives:(NSArray *)subframeArchives
{
    [self init];
    
    _private->mainResource = [mainResource retain];
    _private->subresources = [subresources retain];
    _private->subframeArchives = [subframeArchives retain];
    
    if (!_private->mainResource) {
        [self release];
        return nil;
    }
    
    return self;
}

- (id)_initWithPropertyList:(id)propertyList
{
    [self init];
    
    if (![propertyList isKindOfClass:[NSDictionary class]]) {
        [self release];
        return nil;
    }
    
    _private->mainResource = [[WebResource alloc] _initWithPropertyList:[propertyList objectForKey:WebMainResourceKey]];    
    if (!_private->mainResource) {
        [self release];
        return nil;
    }
    
    _private->subresources = [[WebResource _resourcesFromPropertyLists:[propertyList objectForKey:WebSubresourcesKey]] retain];
    
    NSEnumerator *enumerator = [[propertyList objectForKey:WebSubframeArchivesKey] objectEnumerator];
    _private->subframeArchives = [[NSMutableArray alloc] init];
    NSDictionary *archivePropertyList;
    while ((archivePropertyList = [enumerator nextObject]) != nil) {
        WebArchive *archive = [[WebArchive alloc] _initWithPropertyList:archivePropertyList];
        if (archive) {
            [(NSMutableArray *)_private->subframeArchives addObject:archive];
            [archive release];
        }
    }
           
    return self;
}

- (id)initWithData:(NSData *)data
{
#if !LOG_DISABLED
    CFAbsoluteTime start = CFAbsoluteTimeGetCurrent();
#endif
    NSDictionary *propertyList = [NSPropertyListSerialization propertyListFromData:data 
                                                                  mutabilityOption:NSPropertyListImmutable 
                                                                            format:nil
                                                                  errorDescription:nil];
#if !LOG_DISABLED
    CFAbsoluteTime end = CFAbsoluteTimeGetCurrent();
    CFAbsoluteTime duration = end - start;
#endif
    LOG(Timing, "Parsing web archive with [NSPropertyListSerialization propertyListFromData::::] took %f seconds", duration);
    
    return [self _initWithPropertyList:propertyList];
}

- (void)dealloc
{
    [_private release];
    [super dealloc];
}

- (WebResource *)mainResource
{
    return [[_private->mainResource retain] autorelease];
}

- (NSArray *)subresources
{
    return [[_private->subresources retain] autorelease];
}

- (NSArray *)subframeArchives
{
    return [[_private->subframeArchives retain] autorelease];
}

- (NSDictionary *)_propertyListRepresentation
{
    NSMutableDictionary *propertyList = [NSMutableDictionary dictionary];
    if (_private->mainResource) {
        [propertyList setObject:[_private->mainResource _propertyListRepresentation] forKey:WebMainResourceKey];
    }
    NSArray *propertyLists = [WebResource _propertyListsFromResources:_private->subresources];
    if ([propertyLists count] > 0) {
        [propertyList setObject:propertyLists forKey:WebSubresourcesKey];
    }
    NSEnumerator *enumerator = [_private->subframeArchives objectEnumerator];
    NSMutableArray *subarchivePropertyLists = [[NSMutableArray alloc] init];
    WebArchive *archive;
    while ((archive = [enumerator nextObject]) != nil) {
        [subarchivePropertyLists addObject:[archive _propertyListRepresentation]];
    }
    if ([subarchivePropertyLists count] > 0) {
        [propertyList setObject:subarchivePropertyLists forKey:WebSubframeArchivesKey];
    }
    return propertyList;
}

- (NSData *)data
{
    NSDictionary *propertyList = [self _propertyListRepresentation];
#if !LOG_DISABLED
    CFAbsoluteTime start = CFAbsoluteTimeGetCurrent();
#endif
    NSData *data = [NSPropertyListSerialization dataFromPropertyList:propertyList format:NSPropertyListBinaryFormat_v1_0 errorDescription:nil];
#if !LOG_DISABLED
    CFAbsoluteTime end = CFAbsoluteTimeGetCurrent();
    CFAbsoluteTime duration = end - start;
#endif
    LOG(Timing, "Serializing web archive with [NSPropertyListSerialization dataFromPropertyList:::] took %f seconds", duration);
    return data;
}

@end