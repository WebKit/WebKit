/*
     WebDefaultContextMenuDelegate.h
     Copyright 2002, Apple, Inc. All rights reserved.

     Private header file.
*/


#import <Foundation/Foundation.h>



@interface WebDefaultContextMenuDelegate : NSObject
{
    NSDictionary *element;
}
+ (WebDefaultContextMenuDelegate *)sharedContextMenuDelegate;
@end
