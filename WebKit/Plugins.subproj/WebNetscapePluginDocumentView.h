/*
        WebNetscapePluginDocumentView.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <AppKit/AppKit.h>

#import <WebKit/WebBaseNetscapePluginView.h>

@protocol WebDocumentView;

@interface WebNetscapePluginDocumentView : WebBaseNetscapePluginView   <WebDocumentView>
{
    WebDataSource *dataSource;

    BOOL needsLayout;
}

@end
