/*	WCURICacheData.h
	Copyright 2001, Apple, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

@protocol WCURICacheData

-(NSURL *)url;
-(id)status;
-(id)error;
-(NSDictionary *)headers;
-(UInt8 *)cacheData;
-(int)cacheDataSize;
-(void *)userData;

@end
