/*
    WebNSDataExtras.m
    Private (SPI) header
    Copyright 2003, Apple, Inc. All rights reserved.
 */

#import <WebKit/WebNSDataExtras.h>

#import <WebKit/WebAssertions.h>

@implementation NSDictionary (WebNSDictionaryExtras)
-(NSNumber *)_webkit_numberForKey:(id)key
{
    id object = [self objectForKey:key];
    return [object isKindOfClass:[NSNumber class]] ? object : nil;
}

-(int)_webkit_intForKey:(NSString *)key
{
    NSNumber *number = [self _webkit_numberForKey:key];
    return number == nil ? 0 : [number intValue];
}

-(NSString *)_webkit_stringForKey:(id)key
{
    id object = [self objectForKey:key];
    return [object isKindOfClass:[NSString class]] ? object : nil;
}

-(id)_webkit_objectForMIMEType:(NSString *)MIMEType
{
    id result;
    NSRange slashRange;
    
    result = [self objectForKey:MIMEType];
    if (result) {
        return result;
    }
    
    slashRange = [MIMEType rangeOfString:@"/"];
    if (slashRange.location == NSNotFound) {
        return nil;
    }
    
    return [self objectForKey:[MIMEType substringToIndex:slashRange.location + 1]];
}

@end

@implementation NSMutableDictionary (WebNSDictionaryExtras)
-(void)_webkit_setObject:(id)object forUncopiedKey:(id)key
{
    CFDictionarySetValue((CFMutableDictionaryRef)self, key, object);
}

-(void)_webkit_setInt:(int)value forKey:(id)key
{
    NSNumber *object = [[NSNumber alloc] initWithInt:value];
    [self setObject:object forKey:key];
    [object release];
}

-(void)_webkit_setBool:(BOOL)value forKey:(id)key
{
    NSNumber *object = [[NSNumber alloc] initWithBool:value];
    [self setObject:object forKey:key];
    [object release];
}

@end

