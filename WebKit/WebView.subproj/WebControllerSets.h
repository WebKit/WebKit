/*	
        WebControllerSets.h
	Copyright 2002, Apple Computer, Inc.

*/

#import <Foundation/Foundation.h>

@class WebController;

@interface WebControllerSets : NSObject
+(void)addController:(WebController *)controller toSetNamed:(NSString *)name;
+(void)removeController:(WebController *)controller fromSetNamed:(NSString *)name;
+(NSEnumerator *)controllersInSetNamed:(NSString *)name;
@end



