/*	IFURLFileDatabase.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

#import "IFDatabase.h"


@interface IFURLFileDatabase : IFDatabase 
{
    NSLock *mutex;
}
@end
