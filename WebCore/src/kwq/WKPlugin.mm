//
//  WKPlugins.m
//  
//
//  Created by Chris Blumenberg on Tue Dec 11 2001.
//  Copyright (c) 2001 __MyCompanyName__. All rights reserved.
//

#import "WKPlugin.h"
#include "kwqdebug.h"


@implementation WKPlugin

- (BOOL)initializeWithPath:(NSString *)plugin{
    NSFileManager *fileManager;
    NSDictionary *fileInfo;
    CFBundleRef bundle;
    NSBundle *bundle2;
    CFURLRef pluginURL;
    SInt16 resRef;
    FSRef fref;
    OSErr err;
    UInt32 type;
    
    fileManager = [NSFileManager defaultManager];
    fileInfo = [fileManager fileAttributesAtPath:plugin traverseLink:YES];
    if([[fileInfo objectForKey:@"NSFileType"] isEqualToString:@"NSFileTypeRegular"]){ 
        if([[fileInfo objectForKey:@"NSFileHFSTypeCode"] unsignedLongValue] == 1112690764){ // 1112690764 = 'BRPL'
            name = [plugin lastPathComponent];
            executablePath = plugin;
            err = FSPathMakeRef((UInt8 *)[plugin cString], &fref, NULL);
            if(err != noErr){
                KWQDebug("WKPlugin: FSPathMakeRef failed. Error=%d\n", err);
                return FALSE;
            }
            resRef = FSOpenResFile(&fref, fsRdPerm);
            if(resRef <= noErr){
                KWQDebug("WKPlugin: FSOpenResFile failed. Can't open resource file: %s, Error=%d\n", [plugin cString], err);
                return FALSE;
            }
            mimeTypes = getMimeTypesForResourceFile(resRef);
            if(mimeTypes == nil) return FALSE;
        }else return FALSE;
    }else if([[fileInfo objectForKey:@"NSFileType"] isEqualToString:@"NSFileTypeDirectory"]){
        pluginURL = CFURLCreateWithFileSystemPath(NULL, (CFStringRef)plugin, kCFURLPOSIXPathStyle, TRUE);
        bundle = CFBundleCreate(NULL, pluginURL);
        bundle2 = [NSBundle bundleWithPath:plugin]; // CFBundleCopyExecutableURL doesn't return full path! Have to use NSBundle
        CFBundleGetPackageInfo(bundle, &type, NULL);
        if(type == 1112690764){  // 1112690764 = 'BRPL'
            name = [plugin lastPathComponent];
            executablePath = [bundle2 executablePath];
            resRef = CFBundleOpenBundleResourceMap(bundle);
            mimeTypes = getMimeTypesForResourceFile(resRef);
            if(mimeTypes == nil) return FALSE;
        }else return FALSE;
        CFRelease(bundle);
        CFRelease(pluginURL);

    }else{
        return FALSE;
    }
    [mimeTypes retain];
    [name retain];
    [executablePath retain];
    isLoaded = FALSE;
    return TRUE;
}


- (void)load{    
    OSErr err;
    FSSpec spec;
    FSRef fref;
    CFragConnectionID connID;  
    mainFuncPtr pluginMainFunc;
    
    if(isLoaded){
        return;
    }
    err = FSPathMakeRef((UInt8 *)[executablePath cString], &fref, NULL);
    if(err != noErr){
        KWQDebug("WKPlugin: load: FSPathMakeRef failed. Error=%d\n", err);
        return;
    }
    err = FSGetCatalogInfo(&fref, kFSCatInfoNone, NULL, NULL, &spec, NULL);
    if(err != noErr){
        KWQDebug("WKPlugin: load: FSGetCatalogInfo failed. Error=%d\n", err);
        return;
    }
    err = GetDiskFragment(&spec, 0, kCFragGoesToEOF, nil, kPrivateCFragCopy, &connID, (Ptr *)&pluginMainFunc, nil);
    if(err != noErr){
        KWQDebug("WKPlugin: load: GetDiskFragment failed. Error=%d\n", err);
        return;
    }
        NPError npErr;
    
    browserFuncs.version = 11;
    browserFuncs.size = sizeof(NPNetscapeFuncs);
    browserFuncs.geturl = tVectorForFunctionPointer(NPN_GetURLNotify);
    browserFuncs.posturl = tVectorForFunctionPointer(NPN_PostURLNotify);
    browserFuncs.requestread = tVectorForFunctionPointer(NPN_RequestRead);
    browserFuncs.newstream = tVectorForFunctionPointer(NPN_NewStream);
    browserFuncs.write = tVectorForFunctionPointer(NPN_Write);
    browserFuncs.destroystream = tVectorForFunctionPointer(NPN_DestroyStream);
    browserFuncs.status = tVectorForFunctionPointer(NPN_Status);
    browserFuncs.uagent = tVectorForFunctionPointer(NPN_UserAgent);
    browserFuncs.memalloc = tVectorForFunctionPointer(NPN_MemAlloc);
    browserFuncs.memfree = tVectorForFunctionPointer(NPN_MemFree);
    browserFuncs.memflush = tVectorForFunctionPointer(NPN_MemFlush);
    browserFuncs.reloadplugins = tVectorForFunctionPointer(NPN_ReloadPlugins);
    browserFuncs.geturlnotify = tVectorForFunctionPointer(NPN_GetURLNotify);
    browserFuncs.posturlnotify = tVectorForFunctionPointer(NPN_PostURLNotify);
    browserFuncs.getvalue = tVectorForFunctionPointer(NPN_GetValue);
    browserFuncs.setvalue = tVectorForFunctionPointer(NPN_SetValue);
    browserFuncs.invalidaterect = tVectorForFunctionPointer(NPN_InvalidateRect);
    browserFuncs.invalidateregion = tVectorForFunctionPointer(NPN_InvalidateRegion);
    browserFuncs.forceredraw = tVectorForFunctionPointer(NPN_ForceRedraw);
    
    pluginMainFunc = functionPointerForTVector(pluginMainFunc);
    npErr = pluginMainFunc(&browserFuncs, &pluginFuncs, &NPP_Shutdown);
    
    pluginSize = pluginFuncs.size;
    pluginVersion = pluginFuncs.version;
    KWQDebug("pluginMainFunc: %d, size=%d, version=%d\n", npErr, pluginSize, pluginVersion);
    
    NPP_New = functionPointerForTVector(pluginFuncs.newp);
    NPP_Destroy = functionPointerForTVector(pluginFuncs.destroy);
    NPP_SetWindow = functionPointerForTVector(pluginFuncs.setwindow);
    NPP_NewStream = functionPointerForTVector(pluginFuncs.newstream);
    NPP_DestroyStream = functionPointerForTVector(pluginFuncs.destroystream);
    NPP_StreamAsFile = functionPointerForTVector(pluginFuncs.asfile);
    NPP_WriteReady = functionPointerForTVector(pluginFuncs.writeready);
    NPP_Write = functionPointerForTVector(pluginFuncs.write);
    NPP_Print = functionPointerForTVector(pluginFuncs.print);
    NPP_HandleEvent = functionPointerForTVector(pluginFuncs.event);
    NPP_URLNotify = functionPointerForTVector(pluginFuncs.urlnotify);
    NPP_GetValue = functionPointerForTVector(pluginFuncs.getvalue);
    NPP_SetValue = functionPointerForTVector(pluginFuncs.setvalue);
    
    KWQDebug("Plugin Loaded\n");
    isLoaded = TRUE;
    
}

- (void)unload{
    // unload library here
    NPP_Shutdown();
}

- (NPP_SetWindowProcPtr)NPP_SetWindow{
    return NPP_SetWindow;
}

- (NPP_NewProcPtr)NPP_New{
    return NPP_New;
}

- (NPP_NewStreamProcPtr)NPP_NewStream{
    return NPP_NewStream;
}

- (NPP_DestroyStreamProcPtr)NPP_DestroyStream{
    return NPP_DestroyStream;
}

- (NPP_WriteReadyProcPtr)NPP_WriteReady{
    return NPP_WriteReady;
}
- (NPP_WriteProcPtr)NPP_Write{
    return NPP_Write;
}

- (NPP_HandleEventProcPtr)NPP_HandleEvent{
    return NPP_HandleEvent;
}

- (NSDictionary *)mimeTypes{
    return mimeTypes;
}

- (NSString *)name{
    return name;
}

- (NSString *)executablePath{
    return executablePath;
}

- (BOOL)isLoaded{
    return isLoaded;
}

- (NSString *)description{
    NSMutableString *desc;
    
    desc = [NSMutableString stringWithCapacity:100];
    [desc appendString:@"\n"];
    [desc appendString:@"name: "];
    [desc appendString:name];
    [desc appendString:@"\n"];
    [desc appendString:@"executablePath: "];
    [desc appendString:executablePath];
    [desc appendString:@"\n"];
    [desc appendString:@"isLoaded: "];
    if(isLoaded){
        [desc appendString:@"TRUE\n"];
    }else{
        [desc appendString:@"FALSE\n"];
    }
    [desc appendString:@"mimeTypes: "];
    [desc appendString:[mimeTypes description]];
    [desc appendString:@"\n"];
    return desc;
}
@end

NSMutableDictionary *getMimeTypesForResourceFile(SInt16 resRef){
    NSMutableDictionary *mimeDict;
    Str255 theString;
    char mimeString[200], extString[200];
    int n;
    
    mimeDict = [NSMutableDictionary dictionaryWithCapacity:1];
    UseResFile(resRef);
    for(n=1; 1; n+=2){
        GetIndString(theString, 128, n);
        CopyPascalStringToC(theString, mimeString);
        if(!strcmp(mimeString, "")) break;
        GetIndString(theString, 128, n+1);
        CopyPascalStringToC(theString, extString);
        [mimeDict setObject:[NSString stringWithCString:extString] forKey:[NSString stringWithCString:mimeString]];
    }
    return mimeDict;
}






