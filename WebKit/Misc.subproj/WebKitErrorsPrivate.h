/*
        WebKitErrorsPrivate.h
	Copyright 2003, Apple, Inc. All rights reserved.

        Private header file.
*/

#import <WebKit/WebKitErrors.h>

@interface NSError (WebKitExtras)
+ (NSError *)_webKitErrorWithCode:(int)code failingURL:(NSString *)URL;
+ (NSError *)_webKitErrorWithDomain:(NSString *)domain code:(int)code URL:(NSURL *)URL;

- (id)_initWithPluginErrorCode:(int)code
                    contentURL:(NSURL *)contentURL
                 pluginPageURL:(NSURL *)pluginPageURL
                    pluginName:(NSString *)pluginName
                      MIMEType:(NSString *)MIMEType;

// This method is deprecated. Use _initWithPluginErrorCode:contentURL:pluginPageURL:pluginName:MIMEType: instead.
- (id)_initWithPluginErrorCode:(int)code
              contentURLString:(NSString *)contentURL
           pluginPageURLString:(NSString *)pluginPageURL
                    pluginName:(NSString *)pluginName
                      MIMEType:(NSString *)MIMEType;

@end

