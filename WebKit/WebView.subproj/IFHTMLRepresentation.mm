/*	
    IFHTMLRepresentation.mm
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/IFHTMLRepresentation.h>
#import <WebKit/IFWebDataSource.h>
#import <WebKit/IFWebCoreBridge.h>
#import <KWQKHTMLPartImpl.h>

@interface IFHTMLRepresentationPrivate : NSObject
{
    IFWebCoreBridge *bridge;
}
@end

@implementation IFHTMLRepresentationPrivate
@end

@implementation IFHTMLRepresentation

- init
{
    [super init];
    
    _private = [[IFHTMLRepresentationPrivate alloc] init];
    _private->bridge = [[IFWebCoreBridge alloc] init];
    
    return self;
}

- (void)dealloc
{
    [_private->bridge release];
    [_private release];

    [super dealloc];
}

- (IFWebCoreBridge *)_bridge
{
    return _private->bridge;
}

- (KHTMLPart *)part
{
    return [_private->bridge KHTMLPart];
}

- (void)receivedData:(NSData *)data withDataSource:(IFWebDataSource *)dataSource
{
    [_private->bridge receivedData:data withDataSource:dataSource];
}

- (void)receivedError:(IFError *)error withDataSource:(IFWebDataSource *)dataSource
{

}

- (void)finishedLoadingWithDataSource:(IFWebDataSource *)dataSource
{

}

@end
