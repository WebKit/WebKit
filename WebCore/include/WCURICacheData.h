/*	WCURICacheData.h
	Copyright 2001, Apple, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

#import <WCURICacheJobID.h>

@protocol WCURICacheData

-(id <WCURICacheJobID>)jobID;
-(id)status;
-(id)error;
-(NSURL *)url;
-(unsigned char *)cacheData;
-(int)cacheDataSize;
-(void *)userInfo;

@end
