/*
    WebNSURLExtras.h
    Private (SPI) header
    Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

@interface NSURL (WebNSURLExtras)

+ (NSURL *)_web_URLWithDataAsString:(NSString *)string;
+ (NSURL *)_web_URLWithDataAsString:(NSString *)string relativeToURL:(NSURL *)baseURL;

+ (NSURL *)_web_URLWithData:(NSData *)data;
+ (NSURL *)_web_URLWithData:(NSData *)data relativeToURL:(NSURL *)baseURL;

- (NSData *)_web_originalData;

- (NSString *)_web_displayableString;
- (int)_web_URLStringLength;

// FIXME: change these names back to _web_ when identically-named
// methods are removed from Foundation
- (NSURL *)_webkit_canonicalize;
- (NSURL *)_webkit_URLByRemovingFragment;
- (BOOL)_webkit_isJavaScriptURL;
- (NSString *)_webkit_scriptIfJavaScriptURL;
- (BOOL)_webkit_isFTPDirectoryURL;
- (BOOL)_webkit_shouldLoadAsEmptyDocument;

@end

@interface NSString (WebNSURLExtras)

// FIXME: change these names back to _web_ when identically-named
// methods are removed from Foundation
- (NSString *)_webkit_stringByReplacingValidPercentEscapes;
- (NSString *)_webkit_scriptIfJavaScriptURL;
- (BOOL)_webkit_isJavaScriptURL;
- (BOOL)_webkit_isFTPDirectoryURL;

@end
