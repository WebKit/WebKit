/*
     WebDefaultContextMenuHandler.h
     Copyright 2002, Apple, Inc. All rights reserved.

     Public header file.
*/


#import <Foundation/Foundation.h>

@protocol WebContextMenuHandler;

/*!
    @class WebDefaultContextMenuHandler
*/
@interface WebDefaultContextMenuHandler : NSObject <WebContextMenuHandler>
{
    NSDictionary *element;
}

/*!
    @method addMenuItemWithTitle:action:target:toArray:
    @param title
    @param selector
    @param target
    @param menuItems
*/
+ (void)addMenuItemWithTitle:(NSString *)title action:(SEL)selector target:(id)target toArray:(NSMutableArray *)menuItems;

@end
