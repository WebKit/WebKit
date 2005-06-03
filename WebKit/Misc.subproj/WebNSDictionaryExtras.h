/*
    WebNSDictionaryExtras.h
    Private (SPI) header
    Copyright 2005, Apple, Inc. All rights reserved.
 */

#import <Foundation/Foundation.h>

@interface NSDictionary (WebNSDictionaryExtras)
- (int)_webkit_intForKey:(id)key;
- (NSString *)_webkit_stringForKey:(id)key; // Returns nil if the value is not an NSString.

// Searches for the full MIME type, then the prefix (e.g., "text/" for "text/html")
- (id)_webkit_objectForMIMEType:(NSString *)MIMEType;
@end

@interface NSMutableDictionary (WebNSDictionaryExtras)
- (void)_webkit_setObject:(id)object forUncopiedKey:(id)key;
- (void)_webkit_setInt:(int)value forKey:(id)key;
- (void)_webkit_setBool:(BOOL)value forKey:(id)key;
@end

