/*
    WebPluginViewFactoryPrivate.h
    Copyright 2004, Apple, Inc. All rights reserved.
 
*/

#import <WebKit/WebPluginViewFactory.h>

typedef enum {
    WebPlugInModeEmbed = 0,
    WebPlugInModeFull  = 1
} WebPlugInMode;

/*!
    @constant WebPlugInModeKey REQUIRED. Number with one of the values from the WebPlugInMode enum.
*/
extern NSString *WebPlugInModeKey;