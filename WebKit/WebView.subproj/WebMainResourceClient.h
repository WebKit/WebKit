/*	
    IFMainURLHandleClient.h

    Private header.
    
    Copyright 2001, Apple, Inc. All rights reserved.
*/

#import <Cocoa/Cocoa.h>

#import <WebFoundation/WebFoundation.h>

#import <WebKit/IFWebDataSourcePrivate.h>

@interface IFMainURLHandleClient : NSObject <IFURLHandleClient>
{
    id dataSource;
    KHTMLPart *part;
}
- initWithDataSource: (IFWebDataSource *)ds part: (KHTMLPart *)p;
@end

