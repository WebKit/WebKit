/*
     WebDefaultContextMenuDelegate.h
     Copyright 2002, Apple, Inc. All rights reserved.

     Private header file.
*/


#import <Foundation/Foundation.h>

@protocol WebContextMenuDelegate;

/*!
    @class WebDefaultContextMenuHandler
*/
@interface WebDefaultContextMenuDelegate : NSObject <WebContextMenuDelegate>
{
    NSDictionary *element;
}

/*!
    @method addMenuItemWithTitle:action:target:toArray:
    @abstract Convenience method that creates and adds a menu item an array. Usually used by the
    WebContextMenuDelegate when constructing the array of menu items returned in
    contextMenuItemsForElement:defaultMenuItems:
    @param title Title of the menu item.
    @param selector The menu item's selector.
    @param target The target of the selector.
    @param menuItems The array of menu items that should eventually be returned in contextMenuItemsForElement:defaultMenuItems:
*/
+ (void)addMenuItemWithTitle:(NSString *)title action:(SEL)selector target:(id)target toArray:(NSMutableArray *)menuItems;

@end
