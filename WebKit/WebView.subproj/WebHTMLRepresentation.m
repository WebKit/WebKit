/*	
    IFHTMLRepresentation.mm
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/IFHTMLRepresentation.h>
#import <WebKit/IFWebDataSource.h>
#import <KWQKHTMLPartImpl.h>

@implementation IFHTMLRepresentation

- init
{
    part = new KHTMLPart();
    isFirstChunk = YES;
    
    return self;
}

- (void)dealloc
{
    part->deref();
}

- (KHTMLPart *)part
{
    return part;
}

- (void)receivedData:(NSData *)data withDataSource:(IFWebDataSource *)dataSource isComplete:(BOOL)isComplete
{
    if(isFirstChunk){
        // FIXME [rjw]:  Do any work need in the kde engine.  This should be removed.
        // We should move any code needed out of KWQ.
        part->openURL([[[dataSource inputURL] absoluteString] cString]);
        part->impl->setDataSource(dataSource);
    }
    
    part->impl->slotData([dataSource encoding], (const char *)[data bytes], [data length], isComplete);
    
    isFirstChunk = NO;
}

- (void)receivedError:(IFError *)error withDataSource:(IFWebDataSource *)dataSource
{

}

@end
