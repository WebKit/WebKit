/*
        WebKitErrorsPrivate.h
	Copyright 2003, Apple, Inc. All rights reserved.

        Private header file.
*/

#import <WebKit/WebKitErrors.h>

@interface NSError (WebKitExtras)
+ (NSError *)_webKitErrorWithCode:(int)code failingURL:(NSString *)URL;

- (id)_initWithPluginErrorCode:(int)code
              contentURLString:(NSString *)contentURL
           pluginPageURLString:(NSString *)pluginPageURL
                    pluginName:(NSString *)pluginName
                      MIMEType:(NSString *)MIMEType;
@end

