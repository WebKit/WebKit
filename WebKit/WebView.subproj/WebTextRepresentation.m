/*	
    WebTextRepresentation.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import "WebTextRepresentation.h"

#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebFrameView.h>
#import <WebKit/WebTextView.h>

#import <WebFoundation/NSURLResponse.h>
#import <WebFoundation/WebAssertions.h>

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
    
    if ([view isRichText]) {
        // FIXME: We should try to progressively load RTF.
        [view replaceCharactersInRange:NSMakeRange(0, [[view string] length])
                               withRTF:[dataSource data]];
    } else {
        [view replaceCharactersInRange:NSMakeRange([[view string] length], 0)
                            withString:[dataSource _stringWithData:data]];
    }
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
