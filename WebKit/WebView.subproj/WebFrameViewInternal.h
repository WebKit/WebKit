#import <WebKit/WebFrameView.h>

@class WebView;

@interface WebFrameView (WebInternal)

- (WebView *)_webView;
- (void)_setDocumentView:(NSView <WebDocumentView> *)view;
- (NSView <WebDocumentView> *)_makeDocumentViewForDataSource:(WebDataSource *)dataSource;
- (void)_setWebView:(WebView *)webView;
- (int)_marginWidth;
- (int)_marginHeight;
- (void)_setMarginWidth:(int)w;
- (void)_setMarginHeight:(int)h;
- (NSClipView *)_contentView;
- (float)_verticalPageScrollDistance;
+ (NSMutableDictionary *)_viewTypesAllowImageTypeOmission:(BOOL)allowImageTypeOmission;
+ (Class)_viewClassForMIMEType:(NSString *)MIMEType;
+ (BOOL)_canShowMIMETypeAsHTML:(NSString *)MIMEType;
- (NSScrollView *)_scrollView;
- (void)_setHasBorder:(BOOL)hasBorder;

@end
