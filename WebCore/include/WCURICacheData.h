/*	WCURICacheData.h
	Copyright 2001, Apple, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

@protocol WCURICacheData

-(id)jobID;
-(id)status;
-(id)error;
-(NSURL *)url;
-(unsigned char *)cacheData;
-(int)cacheDataSize;
-(void *)userData;

@end
