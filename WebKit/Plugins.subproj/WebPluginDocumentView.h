/*	
    WebPluginDocumentView.h
    Copyright 2004, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebDocument.h>

@class WebPluginController;

@interface WebPluginDocumentView : NSView <WebDocumentView, WebDocumentRepresentation>
{
    WebPluginController *pluginController;
    BOOL needsLayout;
}

@end
