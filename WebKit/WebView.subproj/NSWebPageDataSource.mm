/*	NSWebPageDataSource.h
	Copyright 2001, Apple, Inc. All rights reserved.
*/
#import <WebKit/NSWebPageDataSource.h>

@implementation NSWebPageDataSource

+ (void)initialize {

    NSAutoreleasePool *localPool;
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSArray *fontSizeArray = [NSArray arrayWithObjects:@"6", @"7", @"8", @"9", @"10", @"12", @"14", @"16", nil];
    
    NSDictionary *dict = [NSDictionary dictionaryWithObjectsAndKeys:
        @"0xffffffff", 		@"WebKitLogLevel",
        @"Arial", 		@"WebKitStandardFont",
        @"Courier",  		@"WebKitFixedFont",
        @"Times-Roman", 	@"WebKitSerifFont",
        @"Arial", 		@"WebKitSansSerifFont", 
        @"Times-Roman", 	@"WebKitCursiveFont",
        @"Times-Roman", 	@"WebKitFantasyFont",
        @"6", 			@"WebKitMinimumFontSize",
        fontSizeArray,		@"WebKitFontSizes",
        nil];

    [defaults registerDefaults:dict];
}

@end
