/*	
    IFMainURLHandleClient.h

    Private header.
    
    Copyright 2001, Apple, Inc. All rights reserved.
*/

#import <WebKit/IFMIMEHandler.h>

@class IFDownloadHandler;
@class IFWebDataSource;
@protocol IFURLHandleClient;

class KHTMLPart;

@interface IFMainURLHandleClient : NSObject <IFURLHandleClient>
{
    id dataSource;
    KHTMLPart *part;
    BOOL sentFakeDocForNonHTMLContentType, typeChecked, downloadStarted;
    IFMIMEHandler *mimeHandler;
    IFMIMEHandlerType handlerType;
    IFDownloadHandler *downloadHandler;
}
- initWithDataSource: (IFWebDataSource *)ds part: (KHTMLPart *)p;
@end

