/*	
        WebControllerSets.h
	Copyright 2002, Apple Computer, Inc.

*/

#import <Foundation/Foundation.h>

@class WebView;

@interface WebControllerSets : NSObject
+(void)addController:(WebView *)controller toSetNamed:(NSString *)name;
+(void)removeController:(WebView *)controller fromSetNamed:(NSString *)name;
+(NSEnumerator *)controllersInSetNamed:(NSString *)name;
@end



