/*	
    WebDefaultPolicyDelegate.h
    Copyright 2003, Apple Computer, Inc.
    
    Private header file.
*/

#import <Foundation/Foundation.h>


@interface WebDefaultUIDelegate : NSObject
{
    NSDictionary *element;
}

+ (WebDefaultUIDelegate *)sharedUIDelegate;
@end
