/*	
        WKWebDataSource.h
	Copyright 2001, Apple, Inc. All rights reserved.
*/
#import <WebKit/WKWebDataSource.h>

@implementation WKWebDataSource

+ (void)initialize {

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSArray *fontSizeArray = [NSArray arrayWithObjects:@"7", @"8", @"9", @"10", @"12", @"13", @"14", @"16", nil];
    NSNumber *pluginsEnabled = [NSNumber numberWithBool:TRUE];
    
    NSDictionary *dict = [NSDictionary dictionaryWithObjectsAndKeys:
//        @"0xffffffff", 		@"WebKitLogLevel",
        @"0x0", 		@"WebKitLogLevel",
        @"Arial", 		@"WebKitStandardFont",
        @"Courier",  		@"WebKitFixedFont",
        @"Times-Roman", 	@"WebKitSerifFont",
        @"Arial", 		@"WebKitSansSerifFont", 
        @"Times-Roman", 	@"WebKitCursiveFont",
        @"Times-Roman", 	@"WebKitFantasyFont",
        @"6", 			@"WebKitMinimumFontSize",
        fontSizeArray,		@"WebKitFontSizes",
        pluginsEnabled,		@"WebKitPluginsEnabled",
        nil];

    [defaults registerDefaults:dict];
}

@end
