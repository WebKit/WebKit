/*	WCURLHandle.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#ifndef WCURLHANDLE_H
#define WCURLHANDLE_H

#import <Foundation/Foundation.h>

@protocol WCURLHandleClient 

- (void)WCURLHandleResourceDidBeginLoading:(id)sender userData:(void *)userData;
- (void)WCURLHandleResourceDidCancelLoading:(id)sender userData:(void *)userData;
- (void)WCURLHandleResourceDidFinishLoading:(id)sender data: (NSData *)data userData:(void *)userData;
- (void)WCURLHandle:(id)sender resourceDataDidBecomeAvailable:(NSData *)data userData:(void *)userData;
- (void)WCURLHandle:(id)sender resourceDidFailLoadingWithResult:(int)result userData:(void *)userData;

@end

@protocol WCHTTPURLHandle
-(NSString *)responseHeaderForKey:(NSString *)key;
@end

#if defined(__cplusplus)
extern "C" {
#endif

// *** Function to create WCURLHandle instance

id WCURLHandleCreate(NSURL *url, id handleClient, void *userData); 

#if defined(__cplusplus)
} // extern "C"
#endif

#endif