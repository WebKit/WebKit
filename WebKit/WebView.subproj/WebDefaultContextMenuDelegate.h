/*
     WebDefaultContextMenuDelegate.h
     Copyright 2002, Apple, Inc. All rights reserved.

     Private header file.
*/


#import <Foundation/Foundation.h>

@protocol WebContextMenuDelegate;


@interface WebDefaultContextMenuDelegate : NSObject <WebContextMenuDelegate>
{
    NSDictionary *element;
}

@end
