/*
        WebBaseNetscapePluginView.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebBaseNetscapePluginView.h>

@class NSURLRequest;

@interface WebBaseNetscapePluginView (WebNPPCallbacks)

- (NPError)loadRequest:(NSURLRequest *)request inTarget:(NSString *)target withNotifyData:(void *)notifyData sendNotification:(BOOL)sendNotification;
- (NPError)getURLNotify:(const char *)URL target:(const char *)target notifyData:(void *)notifyData;
- (NPError)getURL:(const char *)URL target:(const char *)target;
- (NPError)postURLNotify:(const char *)URL target:(const char *)target len:(UInt32)len buf:(const char *)buf file:(NPBool)file notifyData:(void *)notifyData;
- (NPError)postURL:(const char *)URL target:(const char *)target len:(UInt32)len buf:(const char *)buf file:(NPBool)file;
- (NPError)newStream:(NPMIMEType)type target:(const char *)target stream:(NPStream**)stream;
- (NPError)write:(NPStream*)stream len:(SInt32)len buffer:(void *)buffer;
- (NPError)destroyStream:(NPStream*)stream reason:(NPReason)reason;
- (void)status:(const char *)message;
- (const char *)userAgent;
- (void)invalidateRect:(NPRect *)invalidRect;
- (void)invalidateRegion:(NPRegion)invalidateRegion;
- (void)forceRedraw;
- (NPError)getVariable:(NPNVariable)variable value:(void *)value;
@end
