//
//  WebNetscapePluginDocumentView.h
//  WebKit
//
//  Created by Administrator on Mon Sep 30 2002.
//  Copyright (c) 2002 __MyCompanyName__. All rights reserved.
//

#import <AppKit/AppKit.h>

#import <WebKit/WebBaseNetscapePluginView.h>

@protocol WebDocumentView;

@interface WebNetscapePluginDocumentView : WebBaseNetscapePluginView   <WebDocumentView>
{
    WebDataSource *dataSource;

    BOOL needsLayout;
}

@end
