/*	
    WebDefaultUIDelegate.h
    Copyright 2003, Apple Computer, Inc.
    
    Private header file.
*/

#import <Foundation/Foundation.h>

@interface WebDefaultUIDelegate : NSObject
{
    IBOutlet NSMenu *defaultMenu;
}
+ (WebDefaultUIDelegate *)sharedUIDelegate;
@end
