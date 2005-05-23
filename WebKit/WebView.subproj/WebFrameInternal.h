// This header contains WebFrame declarations that can be used anywhere in WebKit, but are neither SPI nor API.

#import <WebKit/WebFramePrivate.h>

@interface WebFrame (WebInternal)

- (void)_updateDrawsBackground;
- (void)_setInternalLoadDelegate:(id)internalLoadDelegate;
- (id)_internalLoadDelegate;
- (void)_unmarkAllMisspellings;

- (NSURLRequest *)_requestFromDelegateForRequest:(NSURLRequest *)request identifier:(id *)identifier error:(NSError **)error;
- (void)_sendRemainingDelegateMessagesWithIdentifier:(id)identifier response:(NSURLResponse *)response length:(unsigned)length error:(NSError *)error;
- (void)_saveResourceAndSendRemainingDelegateMessagesWithRequest:(NSURLRequest *)request
                                                      identifier:(id)identifier 
                                                        response:(NSURLResponse *)response 
                                                            data:(NSData *)data
                                                           error:(NSError *)error;
@end

@interface NSObject (WebInternalFrameLoadDelegate)
- (void)webFrame:(WebFrame *)webFrame didFinishLoadWithError:(NSError *)error;
@end;