/*	NSWebPageDataSource.h
	Copyright 2001, Apple, Inc. All rights reserved.
*/
#import <WebKit/NSWebPageDataSource.h>

@implementation NSWebPageDataSource

+ (void)initialize {

    NSAutoreleasePool *localPool;
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSArray *fontSizeArray = [NSArray arrayWithObjects:@"6", @"8", @"10", @"12", @"14", @"16", @"18", @"20", nil];
    
    NSDictionary *dict = [NSDictionary dictionaryWithObjectsAndKeys:
        @"Arial", 		@"stdFontName",
        @"Courier",  		@"fixedFontName",
        @"Times-Roman", 	@"serifFontName",
        @"Arial", 		@"sansSerifFontName", 
        @"Times-Roman", 	@"cursiveFontName",
        @"Times-Roman", 	@"fantasyFontName",
        @"6", 			@"minFontSize",
        fontSizeArray,		@"fontSizes",
        nil];

    [defaults registerDefaults:dict];

}

@end
