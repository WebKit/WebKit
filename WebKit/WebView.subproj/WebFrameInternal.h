// This header contains WebFrame declarations that can be used anywhere in WebKit, but are neither SPI nor API.

#import <WebKit/WebFramePrivate.h>

@interface WebFrame (WebInternal)

- (void)_updateDrawsBackground;
- (void)_setInternalLoadDelegate:(id)internalLoadDelegate;
- (id)_internalLoadDelegate;
- (void)_sendResourceLoadDelegateMessagesForURL:(NSURL *)URL response:(NSURLResponse *)response length:(unsigned)length;

@end

@interface NSObject (WebInternalFrameLoadDelegate)
- (void)webFrame:(WebFrame *)webFrame didFinishLoadWithError:(NSError *)error;
@end;