/*	
    WebTextRepresentation.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import "WebTextRepresentation.h"

#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebFrameView.h>
#import <WebKit/WebTextView.h>

#import <Foundation/NSURLResponse.h>
#import <WebKit/WebAssertions.h>

@implementation WebTextRepresentation

- (void)dealloc
{
    [RTFSource release];
    [super dealloc];
}

- (void)setDataSource:(WebDataSource *)dataSource
{
    // FIXME: This is broken. [dataSource data] is incomplete at this point.
    hasRTFSource = [[[dataSource response] MIMEType] isEqualToString:@"text/rtf"];
    if (hasRTFSource){
        RTFSource = [[dataSource _stringWithData: [dataSource data]] retain];
    }
}

- (void)receivedData:(NSData *)data withDataSource:(WebDataSource *)dataSource
{
    WebTextView *view = (WebTextView *)[[[dataSource webFrame] frameView] documentView];
    ASSERT([view isKindOfClass:[WebTextView class]]);
    [view appendReceivedData:data fromDataSource:dataSource];
}

- (void)receivedError:(NSError *)error withDataSource:(WebDataSource *)dataSource
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

- (NSString *)title
{
    return nil;
}


@end
