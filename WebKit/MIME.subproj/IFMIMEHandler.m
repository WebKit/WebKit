//
//  IFMIMEHandler.m
//  WebKit
//
//  Created by Chris Blumenberg on Fri Mar 22 2002.
//  Copyright (c) 2002 Apple Computer Inc.. All rights reserved.
//

#import <WebKit/IFMIMEHandler.h>
#import <WebKit/WebKitDebug.h>
#import <WCPluginDatabase.h>

static NSArray *MIMETypes = nil;

@implementation IFMIMEHandler

+ (NSArray *)showableMIMETypes
{
    if(!MIMETypes){
        MIMETypes = [[NSArray arrayWithObjects:
        @"text/plain",
        @"text/html",
    
        //@"image/pict",
        //@"application/postscript",
        //@"image/x-quicktime",
        //@"image/x-targa",
        //@"image/x-sgi",
        //@"image/x-rgb",
        //@"image/x-macpaint",
        //@"image/x-bmp",
        //@"image/tiff",
        //@"image/x-tiff",
        @"image/png",
        @"image/gif",
        @"image/jpg",
        @"image/jpeg", nil] arrayByAddingObjectsFromArray:[[WCPluginDatabase installedPlugins] allHandledMIMETypes]];
        [MIMETypes retain];
    }
    return MIMETypes;
}

- initWithMIMEType:(NSString *)MIME handlerType:(IFMIMEHandlerType)hType handlerName:(NSString *)handler
{
    MIMEType = [MIME retain];
    handlerName = [handler retain];
    handlerType = hType; 
    
    return self;
}


+ (void) saveFileWithPath:(NSString *)path andData:(NSData *)data
{
    NSFileManager *fileManager;
    
    // FIXME: Should probably not replace existing file
    // FIXME: Should report error if there is one
    fileManager = [NSFileManager defaultManager];
    [fileManager createFileAtPath:path contents:data attributes:nil];
    WEBKITDEBUGLEVEL(WEBKIT_LOG_DOWNLOAD, "Download complete. Saved to: %s", [path cString]);
    
    // Send Finder notification
    WEBKITDEBUGLEVEL(WEBKIT_LOG_DOWNLOAD, "Notifying Finder");
    FNNotifyByPath([[path stringByDeletingLastPathComponent] cString], kFNDirectoryModifiedMessage, kNilOptions);
}

+ (void) saveAndOpenFileWithPath:(NSString *)path andData:(NSData *)data
{
    CFURLRef pathURL;
    
    [IFMIMEHandler saveFileWithPath:path andData:data];
    pathURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, (CFStringRef)path, kCFURLPOSIXPathStyle, FALSE);
    WEBKITDEBUGLEVEL(WEBKIT_LOG_DOWNLOAD,"Opening: %s", [path cString]);
    LSOpenCFURLRef(pathURL, NULL);
    CFRelease(pathURL);
}

// Accessor methods
- (NSString *)MIMEType
{
    return MIMEType;
}

- (NSString *)handlerName
{
    return handlerName;
}

- (IFMIMEHandlerType)handlerType
{
    return handlerType;
}

- (NSString *) description
{
    NSString *handlerTypeString = nil;
    if(handlerType == IFMIMEHANDLERTYPE_HTML)
        handlerTypeString = @"IFMIMEHANDLERTYPE_HTML";
    else if(handlerType == IFMIMEHANDLERTYPE_IMAGE)
        handlerTypeString = @"IFMIMEHANDLERTYPE_IMAGE";
    else if(handlerType == IFMIMEHANDLERTYPE_TEXT)
        handlerTypeString = @"IFMIMEHANDLERTYPE_TEXT";
    else if(handlerType == IFMIMEHANDLERTYPE_PLUGIN)
        handlerTypeString = @"IFMIMEHANDLERTYPE_PLUGIN";
    else if(handlerType == IFMIMEHANDLERTYPE_APPLICATION)
        handlerTypeString = @"IFMIMEHANDLERTYPE_APPLICATION";
    return [NSString stringWithFormat:@"MIME TYPE: %@, HANDLER TYPE: %@, HANDLER NAME: %@", 
                MIMEType, handlerTypeString, handlerName];
}

@end
