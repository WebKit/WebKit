// This header contains WebFrame declarations that can be used anywhere in WebKit, but are neither SPI nor API.

#import <WebKit/WebFramePrivate.h>

#define WebFrameDidFinishLoadingNotification @"WebFrameDidFinishLoadingNotification"

#define WebFrameLoadError @"WebFrameLoadError"

@interface WebFrame (WebInternal)

- (void)_updateDrawsBackground;
- (void)_setInternalLoadDelegate:(id)internalLoadDelegate;
- (id)_internalLoadDelegate;

@end

@interface NSObject (WebInternalFrameLoadDelegate)
- (void)webFrame:(WebFrame *)webFrame didFinishLoadWithError:(NSError *)error;
@end;