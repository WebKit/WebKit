//
//  IFMIMEHandler.m
//  WebKit
//
//  Created by Chris Blumenberg on Fri Mar 22 2002.
//  Copyright (c) 2002 Apple Computer Inc.. All rights reserved.
//

#import <WebKit/IFMIMEHandler.h>
#import <WebKit/WebKitDebug.h>
#import <IFPluginDatabase.h>


// FIXME: This code should be replaced by the MIME type DB 2927855

@implementation IFMIMEHandler

+ (IFMIMEHandlerType) MIMEHandlerTypeForMIMEType:(NSString *)MIMEType
{
    NSArray *pluginTypes;
    
    if(!MIMEType){
        return IFMIMEHANDLERTYPE_NIL;
    }else if([MIMEType isEqualToString:@"text/html"]){
        return IFMIMEHANDLERTYPE_HTML;
    }else if([MIMEType hasPrefix:@"text/"] && ![MIMEType isEqualToString:@"text/rtf"]){
        return IFMIMEHANDLERTYPE_TEXT;
    }else if([MIMEType isEqualToString:@"image/jpg"] || [MIMEType isEqualToString:@"image/jpeg"] || 
             [MIMEType isEqualToString:@"image/gif"] || [MIMEType isEqualToString:@"image/png"]){
        return IFMIMEHANDLERTYPE_IMAGE;
    }else{
        pluginTypes = [[IFPluginDatabase installedPlugins] allHandledMIMETypes];
        if([pluginTypes containsObject:MIMEType]){
            return IFMIMEHANDLERTYPE_PLUGIN;
        }else{
            return IFMIMEHANDLERTYPE_APPLICATION;
        }
    }
}

+ (BOOL) canShowMIMEType:(NSString *)MIMEType
{
    NSArray *pluginTypes;
    
    if(!MIMEType)
        return YES;
    
    if(([MIMEType hasPrefix:@"text/"] && ![MIMEType isEqualToString:@"text/rtf"]) ||
       [MIMEType isEqualToString:@"image/jpg"]  ||
       [MIMEType isEqualToString:@"image/jpeg"] ||
       [MIMEType isEqualToString:@"image/gif"]  || 
       [MIMEType isEqualToString:@"image/png"]){
        return YES;
    }else{
        pluginTypes = [[IFPluginDatabase installedPlugins] allHandledMIMETypes];
        if([pluginTypes containsObject:MIMEType]){
            return YES;
        }else{
            return NO;
        }
    }
}

@end
