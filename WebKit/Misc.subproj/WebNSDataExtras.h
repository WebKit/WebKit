/*
    WebNSDataExtras.h
    Private (SPI) header
    Copyright 2003, Apple, Inc. All rights reserved.
 */

#import <Foundation/Foundation.h>

#define WEB_GUESS_MIME_TYPE_PEEK_LENGTH 1024

@interface NSData (WebNSDateExtras)

-(BOOL)_web_isCaseInsensitiveEqualToCString:(const char *)string;
-(NSMutableDictionary *)_webkit_parseRFC822HeaderFields;
-(NSString *)_webkit_guessedMIMEType;

@end
