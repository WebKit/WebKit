// This header contains WebView declarations that can be used anywhere in the Web Kit, but are neither SPI nor API.

#import <WebKit/WebViewPrivate.h>

@interface WebView (WebInternal)
- (BOOL)_isLoading;
- (void)_updateFontPanel;
@end;
