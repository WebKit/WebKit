/*	
    IFMainURLHandleClient.h

    Private header.
    
    Copyright 2001, Apple, Inc. All rights reserved.
*/

#import <WebKit/IFMIMEHandler.h>
#import <WebKit/IFLocationChangeHandler.h>
#import <WebFoundation/IFURLHandle.h>

@class IFDownloadHandler;
@class IFWebDataSource;
@protocol IFURLHandleClient;

class KHTMLPart;

@interface IFMainURLHandleClient : NSObject <IFURLHandleClient>
{
    id dataSource;
    KHTMLPart *part;
    BOOL sentFakeDocForNonHTMLContentType, checkedMIMEType, downloadStarted, loadFinished, sentInitialData;
    IFMIMEHandler *mimeHandler;
    IFMIMEHandlerType handlerType;
    IFDownloadHandler *downloadHandler;
    IFContentPolicy contentPolicy;
    NSData *resourceData;
    IFURLHandle *urlHandle;
}
- initWithDataSource: (IFWebDataSource *)ds part: (KHTMLPart *)p;
- (void)setContentPolicy:(IFContentPolicy)theContentPolicy;

- (void) processData:(NSData *)data isComplete:(BOOL)complete;
- (void) finishProcessingData:(NSData *)data;
@end

