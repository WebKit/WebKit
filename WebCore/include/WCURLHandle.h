/*	WCURLHandle.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#ifndef WCURLHANDLE_H
#define WCURLHANDLE_H

#import <Foundation/Foundation.h>

@protocol WCURLHandleClient 

- (void)WCURLHandleResourceDidBeginLoading:(id)sender userData:(void *)userData;
- (void)WCURLHandleResourceDidCancelLoading:(id)sender userData:(void *)userData;
- (void)WCURLHandleResourceDidFinishLoading:(id)sender userData:(void *)userData;
- (void)WCURLHandle:(id)sender resourceDataDidBecomeAvailable:(NSData *)data offset:(int)offset length:(int)length userData:(void *)userData;
- (void)WCURLHandle:(id)sender resourceDidFailLoadingWithResult:(int)result userData:(void *)userData;

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