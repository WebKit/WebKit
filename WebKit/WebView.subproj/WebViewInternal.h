// This header contains WebView declarations that can be used anywhere in the Web Kit, but are neither SPI nor API.

#import <WebKit/WebViewPrivate.h>

@interface WebView (WebInternal)
- (WebFrame *)_frameForCurrentSelection;
- (WebBridge *)_bridgeForCurrentSelection;
- (BOOL)_isLoading;
- (void)_updateFontPanel;
@end;

@interface WebView (WebViewEditingExtras)
- (BOOL)_interceptEditingKeyEvent:(NSEvent *)event;
- (BOOL)_shouldBeginEditingInDOMRange:(DOMRange *)range;
- (BOOL)_shouldEndEditingInDOMRange:(DOMRange *)range;
@end
