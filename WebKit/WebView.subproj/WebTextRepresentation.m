/*	
    WebTextRepresentation.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import "WebTextRepresentation.h"

#import <WebKit/WebDataSourcePrivate.h>
#import <WebFoundation/NSURLResponse.h>

@implementation WebTextRepresentation

- (void)dealloc
{
    [RTFSource release];
    [super dealloc];
}

- (void)setDataSource:(WebDataSource *)dataSource
{
    hasRTFSource = [[[dataSource response] contentType] isEqualToString:@"text/rtf"];
    if (hasRTFSource){
        RTFSource = [[dataSource _stringWithData: [dataSource data]] retain];
    }
}

- (void)receivedData:(NSData *)data withDataSource:(WebDataSource *)dataSource
{

}

- (void)receivedError:(WebError *)error withDataSource:(WebDataSource *)dataSource
{

}

- (void)finishedLoadingWithDataSource:(WebDataSource *)dataSource
{

}

- (BOOL)canProvideDocumentSource
{
    return hasRTFSource;
}

- (NSString *)documentSource
{
    return RTFSource;
}

@end
