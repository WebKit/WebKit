// This header contains WebFrame declarations that can be used anywhere in WebKit, but are neither SPI nor API.

#import <WebKit/WebFramePrivate.h>

@interface WebFrame (WebInternal)

- (void)_updateDrawsBackground;

@end
