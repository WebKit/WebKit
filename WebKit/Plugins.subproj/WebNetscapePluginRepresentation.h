/*
        WebNetscapePluginRepresentation.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

#import <WebKit/WebBaseNetscapePluginStream.h>

@protocol WebDocumentRepresentation;

@interface WebNetscapePluginRepresentation : WebBaseNetscapePluginStream <WebDocumentRepresentation>
{

}

@end
