/*
        WebKitErrorsPrivate.h
	Copyright 2003, Apple, Inc. All rights reserved.

        Private header file.
*/

#import <WebKit/WebKitErrors.h>

#if !defined(MAC_OS_X_VERSION_10_3) || (MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_3)
#import <WebFoundation/NSError.h>
#endif

@interface NSError (WebKitExtras)
+ (NSError *)_webKitErrorWithCode:(int)code failingURL:(NSString *)URL;

- (id)_initWithPluginErrorCode:(int)code
              contentURLString:(NSString *)contentURL
           pluginPageURLString:(NSString *)pluginPageURL
                    pluginName:(NSString *)pluginName
                      MIMEType:(NSString *)MIMEType;
@end

