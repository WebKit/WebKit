/*
 * Copyright (C) 2001 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "WCPlugin.h"
#include "kwqdebug.h"


@implementation WCPlugin

- (BOOL)initializeWithPath:(NSString *)pluginPath
{
    NSFileManager *fileManager;
    NSDictionary *fileInfo;
    SInt16 resRef;
    FSRef fref;
    OSErr err;
    UInt32 type;
    CFURLRef pluginURL;
        
    fileManager = [NSFileManager defaultManager];
    fileInfo = [fileManager fileAttributesAtPath:pluginPath traverseLink:YES];
    if([[fileInfo objectForKey:@"NSFileType"] isEqualToString:@"NSFileTypeRegular"]){  // plug-in with resource fork
        if([[fileInfo objectForKey:@"NSFileHFSTypeCode"] unsignedLongValue] == 1112690764){ // 1112690764 = 'BRPL'
            err = FSPathMakeRef((UInt8 *)[pluginPath cString], &fref, NULL);
            if(err != noErr){
                KWQDebug("WCPlugin: FSPathMakeRef failed. Error=%d\n", err);
                return FALSE;
            }
            resRef = FSOpenResFile(&fref, fsRdPerm);
            if(resRef <= noErr){
                KWQDebug("WCPlugin: FSOpenResFile failed. Can't open resource file: %s, Error=%d\n", [pluginPath cString], err);
                return FALSE;
            }
            [self getPluginInfoForResourceFile:resRef];
            isBundle = FALSE;
        }else return FALSE;
        
    }else if([[fileInfo objectForKey:@"NSFileType"] isEqualToString:@"NSFileTypeDirectory"]){ //bundle plug-in
        pluginURL = CFURLCreateWithFileSystemPath(NULL, (CFStringRef)pluginPath, kCFURLPOSIXPathStyle, TRUE);
        bundle = CFBundleCreate(NULL, pluginURL);
        CFBundleGetPackageInfo(bundle, &type, NULL);
        if(type == 1112690764){  // 1112690764 = 'BRPL'
            resRef = CFBundleOpenBundleResourceMap(bundle);
            [self getPluginInfoForResourceFile:resRef];
            isBundle = TRUE;
        }else{
            return FALSE;
        }
        CFRelease(pluginURL);
    }else{
        return FALSE;
    }
    
    filename = [pluginPath lastPathComponent];
    path = pluginPath;
    [path retain];
    [filename retain];
    isLoaded = FALSE;
    return TRUE;
}

- (void)getPluginInfoForResourceFile:(SInt16)resRef
{
    Str255 theString;
    char temp[255], description[255];
    NSMutableArray *mime; // mime is an array containing the mime type, extension(s) and descriptions for that mime type.
    NSString *tempString;
    uint n, i;
    
    mimeTypes = [NSMutableArray arrayWithCapacity:1];
    UseResFile(resRef);
    for(n=1, i=0; 1; n+=2, i++){
        GetIndString(theString, 128, n);
        CopyPascalStringToC(theString, temp);
        if(!strcmp(temp, "")) break;
        mime = [NSMutableArray arrayWithCapacity:3];
        [mimeTypes insertObject:mime atIndex:i];
        //FIXME: Because our JS engine poops on semi-colons, I'm removing ";version=1.3"
        //Scott Adler is checking if semi-colons are allowed to be in mime-types
        if(!strcmp(temp, "application/x-java-applet;version=1.3")){
            strcpy(temp, "application/x-java-applet");
        }
        tempString = [NSString stringWithCString:temp];
        [mime insertObject:tempString atIndex:0]; // mime type
        
        GetIndString(theString, 128, n+1);
        CopyPascalStringToC(theString, temp);
        tempString = [NSString stringWithCString:temp];
        [mime insertObject:tempString atIndex:1]; // mime's extension
    }
    for(i=1; i<=[mimeTypes count]; i++){
        GetIndString(theString, 127, i);
        CopyPascalStringToC(theString, temp);
        tempString = [NSString stringWithCString:temp];
        mime = [mimeTypes objectAtIndex:(i-1)];
        [mime insertObject:tempString atIndex:2]; // mime's description
    }
    GetIndString(theString, 126, 1);
    CopyPascalStringToC(theString, description);
    pluginDescription = [NSString stringWithCString:description];
    [pluginDescription retain];
    
    GetIndString(theString, 126, 2); 
    CopyPascalStringToC(theString, temp);
    name = [NSString stringWithCString:temp]; // plugin's name
    [name retain];
    [mimeTypes retain];
}

- (void)load
{    
    OSErr err;
    FSSpec spec;
    FSRef fref; 
    mainFuncPtr pluginMainFunc;
    initializeFuncPtr NPP_Initialize;
    getEntryPointsFuncPtr NPP_GetEntryPoints;
    NPError npErr;
    Boolean didLoad;
    NSBundle *tempBundle;
    NSFileHandle *executableFile;
    NSData *data;
    
    if(isLoaded){
        return;
    }
    if(isBundle){ //CFM or Mach-o bundle
        tempBundle = [NSBundle bundleWithPath:path];
        executableFile = [NSFileHandle fileHandleForReadingAtPath:[tempBundle executablePath]];
        data = [executableFile readDataOfLength:8];
        if(!memcmp([data bytes], "Joy!peff", 8)){
            isCFM = TRUE;
        }else{
            isCFM = TRUE; //FIXME
        }
        [executableFile closeFile];
        didLoad = CFBundleLoadExecutable(bundle);
        if (!didLoad) {
            return;
        }
        if(isCFM){
            pluginMainFunc = (void*)CFBundleGetFunctionPointerForName(bundle, CFSTR("main") );
        }else{
            NPP_Initialize = (void*)CFBundleGetFunctionPointerForName(bundle, CFSTR("NPP_Initialize") );
            NPP_GetEntryPoints = (void*)CFBundleGetFunctionPointerForName(bundle, CFSTR("NPP_GetEntryPoints") );
            NPP_Shutdown = (void*)CFBundleGetFunctionPointerForName(bundle, CFSTR("NPP_Shutdown") );
        }
    }else{ // single CFM file
        err = FSPathMakeRef((UInt8 *)[path cString], &fref, NULL);
        if(err != noErr){
            KWQDebug("WCPlugin: load: FSPathMakeRef failed. Error=%d\n", err);
            return;
        }
        err = FSGetCatalogInfo(&fref, kFSCatInfoNone, NULL, NULL, &spec, NULL);
        if(err != noErr){
            KWQDebug("WCPlugin: load: FSGetCatalogInfo failed. Error=%d\n", err);
            return;
        }
        err = GetDiskFragment(&spec, 0, kCFragGoesToEOF, nil, kPrivateCFragCopy, &connID, (Ptr *)&pluginMainFunc, nil);
        if(err != noErr){
            KWQDebug("WCPlugin: load: GetDiskFragment failed. Error=%d\n", err);
            return;
        }
        pluginMainFunc = functionPointerForTVector(pluginMainFunc);
        isCFM = TRUE;
    }
    if(isCFM){
        browserFuncs.version = 11;
        browserFuncs.size = sizeof(NPNetscapeFuncs);
        browserFuncs.geturl = tVectorForFunctionPointer(NPN_GetURL);
        browserFuncs.posturl = tVectorForFunctionPointer(NPN_PostURL);
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
    }else{ // no function pointer conversion necessary for mach-o
        browserFuncs.version = 11;
        browserFuncs.size = sizeof(NPNetscapeFuncs);
        browserFuncs.geturl = NPN_GetURL;
        browserFuncs.posturl = NPN_PostURL;
        browserFuncs.requestread = NPN_RequestRead;
        browserFuncs.newstream = NPN_NewStream;
        browserFuncs.write = NPN_Write;
        browserFuncs.destroystream = NPN_DestroyStream;
        browserFuncs.status = NPN_Status;
        browserFuncs.uagent = NPN_UserAgent;
        browserFuncs.memalloc = NPN_MemAlloc;
        browserFuncs.memfree = NPN_MemFree;
        browserFuncs.memflush = NPN_MemFlush;
        browserFuncs.reloadplugins = NPN_ReloadPlugins;
        browserFuncs.geturlnotify = NPN_GetURLNotify;
        browserFuncs.posturlnotify = NPN_PostURLNotify;
        browserFuncs.getvalue = NPN_GetValue;
        browserFuncs.setvalue = NPN_SetValue;
        browserFuncs.invalidaterect = NPN_InvalidateRect;
        browserFuncs.invalidateregion = NPN_InvalidateRegion;
        browserFuncs.forceredraw = NPN_ForceRedraw;
        
        NPP_Initialize(&browserFuncs);
        NPP_GetEntryPoints(&pluginFuncs);
        
        pluginSize = pluginFuncs.size;
        pluginVersion = pluginFuncs.version;
        
        NPP_New = pluginFuncs.newp;
        NPP_Destroy = pluginFuncs.destroy;
        NPP_SetWindow = pluginFuncs.setwindow;
        NPP_NewStream = pluginFuncs.newstream;
        NPP_DestroyStream = pluginFuncs.destroystream;
        NPP_StreamAsFile = pluginFuncs.asfile;
        NPP_WriteReady = pluginFuncs.writeready;
        NPP_Write = pluginFuncs.write;
        NPP_Print = pluginFuncs.print;
        NPP_HandleEvent = pluginFuncs.event;
        NPP_URLNotify = pluginFuncs.urlnotify;
        NPP_GetValue = pluginFuncs.getvalue;
        NPP_SetValue = pluginFuncs.setvalue;
    }
    KWQDebug("Plugin Loaded\n");
    isLoaded = TRUE;
}

- (void)unload
{
    NPP_Shutdown();
    if(isBundle){
        CFBundleUnloadExecutable(bundle);
        CFRelease(bundle);
    }else{
        CloseConnection(&connID);
    }
    KWQDebug("Plugin Unloaded\n");
    isLoaded = FALSE;
}

- (NSString *)mimeTypeForURL:(NSString *)URL
{
    uint n;
    NSRange hasExtension;
    NSString *extension;
    
    extension = [URL pathExtension];
    for(n=0; n<[mimeTypes count]; n++){
        hasExtension = [[[mimeTypes objectAtIndex:n] objectAtIndex:1] rangeOfString:extension];
        if(hasExtension.length){
            return [[mimeTypes objectAtIndex:n] objectAtIndex:0];
        }
    }
    return nil;
}

- (NPP_SetWindowProcPtr)NPP_SetWindow{
    return NPP_SetWindow;
}

- (NPP_NewProcPtr)NPP_New{
    return NPP_New;
}

- (NPP_DestroyProcPtr)NPP_Destroy{
    return NPP_Destroy;
}

- (NPP_NewStreamProcPtr)NPP_NewStream{
    return NPP_NewStream;
}

- (NPP_StreamAsFileProcPtr)NPP_StreamAsFile{
    return NPP_StreamAsFile;
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

-(NPP_URLNotifyProcPtr)NPP_URLNotify{
    return NPP_URLNotify;
}

-(NPP_GetValueProcPtr)NPP_GetValue{
    return NPP_GetValue;
}

-(NPP_SetValueProcPtr)NPP_SetValue{
    return NPP_SetValue;
}

-(NPP_PrintProcPtr)NPP_Print{
    return NPP_Print;
}

- (NSArray *)mimeTypes{
    return mimeTypes;
}

- (NSString *)name{
    return name;
}

- (NSString *)filename{
    return filename;
}

- (NSString *)path{
    return path;
}

- (BOOL)isLoaded{
    return isLoaded;
}

- (NSString *)pluginDescription{
    return pluginDescription;
}
- (NSString *)description{
    NSMutableString *desc;
    
    desc = [NSMutableString stringWithCapacity:100];
    [desc appendString:@"\n"];
    [desc appendString:@"name: "];
    [desc appendString:name];
    [desc appendString:@"\n"];
    [desc appendString:@"path: "];
    [desc appendString:path];
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
    [desc appendString:@"pluginDescription: "];
    [desc appendString:pluginDescription];
    [desc appendString:@"\n"];
    return desc;
}

@end






