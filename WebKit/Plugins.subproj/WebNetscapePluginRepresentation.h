/*
        WebNetscapePluginRepresentation.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

#import <WebKit/WebBaseNetscapePluginStream.h>

@class NSError;
@class WebDataSource;
@protocol WebDocumentRepresentation;

@interface WebNetscapePluginRepresentation : WebBaseNetscapePluginStream <WebDocumentRepresentation>
{
    unsigned _dataLengthReceived;
    WebDataSource *_dataSource;
    NSError *_error;
}

- (void)redeliverStream;

@end
