/*	
    WebPluginDocumentView.h
    Copyright 2004, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebDocument.h>

@class WebPluginController;
@class WebPluginPackage;

@interface WebPluginDocumentView : NSView <WebDocumentView, WebDocumentRepresentation>
{
    WebPluginController *pluginController;
    WebPluginPackage *plugin;
    BOOL dataSourceHasBeenSet;
    BOOL needsLayout;
}

@end
