/*	WCURICache.h
	Copyright 2001, Apple, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

#import <WCURICacheJobID.h>

@protocol WCURICache 

-(NSString *)requestWithURL:(NSURL *)url requestor:(id)requestor;
-(NSString *)requestWithString:(NSString *)uriString requestor:(id)requestor;

-(void)cancelRequest:(id <WCURICacheJobID>)jobID;
-(void)cancelAllRequestsWithURL:(NSURL *)url;
-(void)cancelAllRequestsWithString:(NSString *)uriString;

@end

#if defined(__cplusplus)
extern "C" {
#endif

// *** Function to access WCURICache singleton

id <WCURICache> WCGetDefaultURICache(); 

#if defined(__cplusplus)
} // extern "C"
#endif
