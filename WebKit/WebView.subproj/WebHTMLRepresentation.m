/*	
    WebHTMLRepresentation.mm
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebHTMLRepresentation.h>

#import <WebKit/WebDataSource.h>
#import <WebKit/WebBridge.h>
#import <WebKit/WebKitStatisticsPrivate.h>
#import <WebKit/WebFramePrivate.h>

@interface WebHTMLRepresentationPrivate : NSObject
{
@public
    WebBridge *bridge;
}
@end

@implementation WebHTMLRepresentationPrivate
@end

@implementation WebHTMLRepresentation

- init
{
    [super init];
    
    _private = [[WebHTMLRepresentationPrivate alloc] init];
    
    ++WebHTMLRepresentationCount;
    
    return self;
}

- (void)dealloc
{
    --WebHTMLRepresentationCount;
    
    [_private release];

    [super dealloc];
}

- (WebBridge *)_bridge
{
    return _private->bridge;
}

- (void)setDataSource:(WebDataSource *)dataSource
{
    [[dataSource webFrame] _changeBridge];
    _private->bridge = [[dataSource webFrame] _bridge];
}


- (void)receivedData:(NSData *)data withDataSource:(WebDataSource *)dataSource
{
    [_private->bridge receivedData:data withDataSource:dataSource];
}

- (void)receivedError:(WebError *)error withDataSource:(WebDataSource *)dataSource
{

}

- (void)finishedLoadingWithDataSource:(WebDataSource *)dataSource
{

}

@end
