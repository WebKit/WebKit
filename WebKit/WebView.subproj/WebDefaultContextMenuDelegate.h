/*
     WebDefaultContextMenuHandler.h

     Copyright 2002, Apple, Inc. All rights reserved.

     Private header file.
*/


#import <Foundation/Foundation.h>

@protocol WebContextMenuHandler;

@interface WebDefaultContextMenuHandler : NSObject <WebContextMenuHandler> {
    NSDictionary *element;
}

+ (void)addMenuItemWithTitle:(NSString *)title action:(SEL)selector target:(id)target toArray:(NSMutableArray *)menuItems;

@end
