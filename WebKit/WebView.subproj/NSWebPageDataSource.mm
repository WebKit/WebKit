/*	NSWebPageDataSource.h
	Copyright 2001, Apple, Inc. All rights reserved.
*/
#import <WebKit/NSWebPageDataSource.h>

@implementation NSWebPageDataSource

+ (void)initialize {

    NSAutoreleasePool *localPool;
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSDictionary *dict = [NSDictionary dictionaryWithObjectsAndKeys:
        @"Arial", 		@"stdFontName",
        @"Courier",  		@"fixedFontName",
        @"Times-Roman", 	@"serifFontName",
        @"Arial", 		@"sansSerifFontName", 
        @"Times-Roman", 	@"cursiveFontName",
        @"Times-Roman", 	@"fantasyFontName", nil];

    [defaults registerDefaults:dict];

}

@end
