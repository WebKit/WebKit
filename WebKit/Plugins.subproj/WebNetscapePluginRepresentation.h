/*
        WebNetscapePluginRepresentation.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

#import <WebKit/WebBaseNetscapePluginStream.h>

@class WebDataSource;
@class WebError;
@protocol WebDocumentRepresentation;

@interface WebNetscapePluginRepresentation : WebBaseNetscapePluginStream <WebDocumentRepresentation>
{
    unsigned _dataLengthReceived;
    WebDataSource *_dataSource;
    WebError *_error;
}

- (void)redeliverStream;

@end
