/*
 * Copyright (C) 2001, 2002 Apple Computer, Inc.  All rights reserved.
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

#import "IFPlugin.h"
#import "WebKitDebug.h"

typedef void (* FunctionPointer) (void);
typedef void (* TransitionVector) (void);
FunctionPointer functionPointerForTVector(TransitionVector);
TransitionVector tVectorForFunctionPointer(FunctionPointer);

@implementation IFPlugin

- (SInt16)_openResourceFile
{
    FSRef fref;
    OSErr err;
    
    if(isBundle){
        return CFBundleOpenBundleResourceMap(bundle);
    }else{
        err = FSPathMakeRef((UInt8 *)[path cString], &fref, NULL);
        if(err != noErr){
            return -1;
        }
            
        return FSOpenResFile(&fref, fsRdPerm);
    }
}

- (void)_closeResourceFile:(SInt16)resRef
{
    if(isBundle){
        CFBundleCloseBundleResourceMap(bundle, resRef);
    }else{
        CloseResFile(resRef);
    }
}

- (BOOL)_getPluginInfo
{
    Str255 theString;
    char temp[256], description[256];
    NSMutableArray *mime; // mime is an array containing the mime type, extension(s) and descriptions for that mime type.
    NSString *tempString;
    uint n, i;
    
    SInt16 resRef = [self _openResourceFile];
    if(resRef == -1)
        return NO;
    
    UseResFile(resRef);
    
    mimeTypes = [NSMutableArray arrayWithCapacity:1];
    for(n=1, i=0; 1; n+=2, i++){
        GetIndString(theString, 128, n);
        if (theString[0] == 0)
            break;
        CopyPascalStringToC(theString, temp);
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
    pluginDescription = [[NSString stringWithCString:description] retain];
    
    GetIndString(theString, 126, 2); 
    CopyPascalStringToC(theString, temp);
    name = [[NSString stringWithCString:temp] retain]; // plugin's name
    [mimeTypes retain];
    
    [self _closeResourceFile:resRef];
    
    return YES;
}

- initWithPath:(NSString *)pluginPath
{
    NSFileManager *fileManager;
    NSDictionary *fileInfo;
    UInt32 type;
    CFURLRef pluginURL;
    
    path = pluginPath;
    fileManager = [NSFileManager defaultManager];
    fileInfo = [fileManager fileAttributesAtPath:pluginPath traverseLink:YES];
    
    // single-file plug-in with resource fork
    if([[fileInfo objectForKey:@"NSFileType"] isEqualToString:@"NSFileTypeRegular"]){
        type = [[fileInfo objectForKey:@"NSFileHFSTypeCode"] unsignedLongValue];
        isBundle = NO;
    
    // bundle
    }else if([[fileInfo objectForKey:@"NSFileType"] isEqualToString:@"NSFileTypeDirectory"]){
        pluginURL = CFURLCreateWithFileSystemPath(NULL, (CFStringRef)pluginPath, kCFURLPOSIXPathStyle, TRUE);
        bundle = CFBundleCreate(NULL, pluginURL);
        CFRelease(pluginURL);
        CFBundleGetPackageInfo(bundle, &type, NULL);
        isBundle = YES;
    }else{
        return nil;
    }
    
    if(type == FOUR_CHAR_CODE('BRPL') || type == FOUR_CHAR_CODE('IEPL') ){
        if(![self _getPluginInfo]){
            return nil;
        }
    }else{
        return nil;
    }
    
    filename = [[path lastPathComponent] retain];
    [path retain];
    isLoaded = NO;
    return self;
}

- (BOOL)load
{    
    OSErr err;
    FSSpec spec;
    FSRef fref; 
    mainFuncPtr pluginMainFunc;
    initializeFuncPtr NP_Initialize = NULL;
    getEntryPointsFuncPtr NP_GetEntryPoints = NULL;
    NPError npErr;
    Boolean didLoad;
    NSBundle *tempBundle;
    NSFileHandle *executableFile;
    NSData *data;
    
    if(isLoaded)
        return YES;

    if(isBundle){ //CFM or Mach-o bundle
        tempBundle = [NSBundle bundleWithPath:path];
        executableFile = [NSFileHandle fileHandleForReadingAtPath:[tempBundle executablePath]];
        data = [executableFile readDataOfLength:8];
        if(!memcmp([data bytes], "Joy!peff", 8)){
            isCFM = TRUE;
        }else{
            isCFM = FALSE;
        }
        [executableFile closeFile];
        didLoad = CFBundleLoadExecutable(bundle);
        if (!didLoad)
            return NO;

        if(isCFM){
            pluginMainFunc = (mainFuncPtr)CFBundleGetFunctionPointerForName(bundle, CFSTR("main") );
            if(!pluginMainFunc)
                return NO;
        }else{
            NP_Initialize = (initializeFuncPtr)CFBundleGetFunctionPointerForName(bundle, CFSTR("NP_Initialize") );
            NP_GetEntryPoints = (getEntryPointsFuncPtr)CFBundleGetFunctionPointerForName(bundle, CFSTR("NP_GetEntryPoints") );
            NPP_Shutdown = (NPP_ShutdownProcPtr)CFBundleGetFunctionPointerForName(bundle, CFSTR("NP_Shutdown") );
            if(!NP_Initialize || !NP_GetEntryPoints || !NPP_Shutdown)
                return NO;
        }
    }else{ // single CFM file
        err = FSPathMakeRef((UInt8 *)[path cString], &fref, NULL);
        if(err != noErr){
            WEBKITDEBUG("IFPlugin: load: FSPathMakeRef failed. Error=%d\n", err);
            return NO;
        }
        err = FSGetCatalogInfo(&fref, kFSCatInfoNone, NULL, NULL, &spec, NULL);
        if(err != noErr){
            WEBKITDEBUG("IFPlugin: load: FSGetCatalogInfo failed. Error=%d\n", err);
            return NO;
        }
        err = GetDiskFragment(&spec, 0, kCFragGoesToEOF, nil, kPrivateCFragCopy, &connID, (Ptr *)&pluginMainFunc, nil);
        if(err != noErr){
            WEBKITDEBUG("IFPlugin: load: GetDiskFragment failed. Error=%d\n", err);
            return NO;
        }
        pluginMainFunc = (mainFuncPtr)functionPointerForTVector((TransitionVector)pluginMainFunc);
        if(!pluginMainFunc)
            return NO;
            
        isCFM = TRUE;
    }
    
    // Plugins (at least QT) require that you call UseResFile on the resource file before loading it.
    
    resourceRef = [self _openResourceFile];
    if(resourceRef == -1)
        return NO;
    
    UseResFile(resourceRef);
    
    // swap function tables
    if(isCFM){
        browserFuncs.version = 11;
        browserFuncs.size = sizeof(NPNetscapeFuncs);
        browserFuncs.geturl = (NPN_GetURLProcPtr)tVectorForFunctionPointer((FunctionPointer)NPN_GetURL);
        browserFuncs.posturl = (NPN_PostURLProcPtr)tVectorForFunctionPointer((FunctionPointer)NPN_PostURL);
        browserFuncs.requestread = (NPN_RequestReadProcPtr)tVectorForFunctionPointer((FunctionPointer)NPN_RequestRead);
        browserFuncs.newstream = (NPN_NewStreamProcPtr)tVectorForFunctionPointer((FunctionPointer)NPN_NewStream);
        browserFuncs.write = (NPN_WriteProcPtr)tVectorForFunctionPointer((FunctionPointer)NPN_Write);
        browserFuncs.destroystream = (NPN_DestroyStreamProcPtr)tVectorForFunctionPointer((FunctionPointer)NPN_DestroyStream);
        browserFuncs.status = (NPN_StatusProcPtr)tVectorForFunctionPointer((FunctionPointer)NPN_Status);
        browserFuncs.uagent = (NPN_UserAgentProcPtr)tVectorForFunctionPointer((FunctionPointer)NPN_UserAgent);
        browserFuncs.memalloc = (NPN_MemAllocProcPtr)tVectorForFunctionPointer((FunctionPointer)NPN_MemAlloc);
        browserFuncs.memfree = (NPN_MemFreeProcPtr)tVectorForFunctionPointer((FunctionPointer)NPN_MemFree);
        browserFuncs.memflush = (NPN_MemFlushProcPtr)tVectorForFunctionPointer((FunctionPointer)NPN_MemFlush);
        browserFuncs.reloadplugins = (NPN_ReloadPluginsProcPtr)tVectorForFunctionPointer((FunctionPointer)NPN_ReloadPlugins);
        browserFuncs.geturlnotify = (NPN_GetURLNotifyProcPtr)tVectorForFunctionPointer((FunctionPointer)NPN_GetURLNotify);
        browserFuncs.posturlnotify = (NPN_PostURLNotifyProcPtr)tVectorForFunctionPointer((FunctionPointer)NPN_PostURLNotify);
        browserFuncs.getvalue = (NPN_GetValueProcPtr)tVectorForFunctionPointer((FunctionPointer)NPN_GetValue);
        browserFuncs.setvalue = (NPN_SetValueProcPtr)tVectorForFunctionPointer((FunctionPointer)NPN_SetValue);
        browserFuncs.invalidaterect = (NPN_InvalidateRectProcPtr)tVectorForFunctionPointer((FunctionPointer)NPN_InvalidateRect);
        browserFuncs.invalidateregion = (NPN_InvalidateRegionProcPtr)tVectorForFunctionPointer((FunctionPointer)NPN_InvalidateRegion);
        browserFuncs.forceredraw = (NPN_ForceRedrawProcPtr)tVectorForFunctionPointer((FunctionPointer)NPN_ForceRedraw);
        browserFuncs.getJavaEnv = (NPN_GetJavaEnvProcPtr)tVectorForFunctionPointer((FunctionPointer)NPN_GetJavaEnv);
        browserFuncs.getJavaPeer = (NPN_GetJavaPeerProcPtr)tVectorForFunctionPointer((FunctionPointer)NPN_GetJavaPeer);
        
        npErr = pluginMainFunc(&browserFuncs, &pluginFuncs, &NPP_Shutdown);
        
        pluginSize = pluginFuncs.size;
        pluginVersion = pluginFuncs.version;
        WEBKITDEBUG("pluginMainFunc: %d, size=%d, version=%d\n", npErr, pluginSize, pluginVersion);
        
        NPP_New = (NPP_NewProcPtr)functionPointerForTVector((TransitionVector)pluginFuncs.newp);
        NPP_Destroy = (NPP_DestroyProcPtr)functionPointerForTVector((TransitionVector)pluginFuncs.destroy);
        NPP_SetWindow = (NPP_SetWindowProcPtr)functionPointerForTVector((TransitionVector)pluginFuncs.setwindow);
        NPP_NewStream = (NPP_NewStreamProcPtr)functionPointerForTVector((TransitionVector)pluginFuncs.newstream);
        NPP_DestroyStream = (NPP_DestroyStreamProcPtr)functionPointerForTVector((TransitionVector)pluginFuncs.destroystream);
        NPP_StreamAsFile = (NPP_StreamAsFileProcPtr)functionPointerForTVector((TransitionVector)pluginFuncs.asfile);
        NPP_WriteReady = (NPP_WriteReadyProcPtr)functionPointerForTVector((TransitionVector)pluginFuncs.writeready);
        NPP_Write = (NPP_WriteProcPtr)functionPointerForTVector((TransitionVector)pluginFuncs.write);
        NPP_Print = (NPP_PrintProcPtr)functionPointerForTVector((TransitionVector)pluginFuncs.print);
        NPP_HandleEvent = (NPP_HandleEventProcPtr)functionPointerForTVector((TransitionVector)pluginFuncs.event);
        NPP_URLNotify = (NPP_URLNotifyProcPtr)functionPointerForTVector((TransitionVector)pluginFuncs.urlnotify);
        NPP_GetValue = (NPP_GetValueProcPtr)functionPointerForTVector((TransitionVector)pluginFuncs.getvalue);
        NPP_SetValue = (NPP_SetValueProcPtr)functionPointerForTVector((TransitionVector)pluginFuncs.setvalue);
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
        browserFuncs.getJavaEnv = NPN_GetJavaEnv;
        browserFuncs.getJavaPeer = NPN_GetJavaPeer;
        
        NP_Initialize(&browserFuncs);
        NP_GetEntryPoints(&pluginFuncs);
        
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
    WEBKITDEBUG("Plugin Loaded\n");
    isLoaded = TRUE;
    return YES;
}

- (void)unload
{
    NPP_Shutdown();
    
    [self _closeResourceFile:resourceRef];
    
    if(isBundle){
        CFBundleUnloadExecutable(bundle);
        CFRelease(bundle);
    }else{
        CloseConnection(&connID);
    }
    WEBKITDEBUG("Plugin Unloaded\n");
    isLoaded = FALSE;
}

- (NSString *)mimeTypeForExtension:(NSString *)extension;
{
    uint n;
    NSRange hasExtension;

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
    
    return [NSString stringWithFormat:@"name: %@\npath: %@\nisLoaded: %d\nmimeTypes:\n%@\npluginDescription:%@",
        name, path, isLoaded, [mimeTypes description], pluginDescription];
}
@end


// function pointer converters

FunctionPointer functionPointerForTVector(TransitionVector tvp)
{
    uint32 temp[6] = {0x3D800000, 0x618C0000, 0x800C0000, 0x804C0004, 0x7C0903A6, 0x4E800420};
    uint32 *newGlue = NULL;

    if (tvp != NULL) {
        newGlue = (uint32 *)malloc(sizeof(temp));
        if (newGlue != NULL) {
            unsigned i;
            for (i = 0; i < 6; i++) newGlue[i] = temp[i];
            newGlue[0] |= ((UInt32)tvp >> 16);
            newGlue[1] |= ((UInt32)tvp & 0xFFFF);
            MakeDataExecutable(newGlue, sizeof(temp));
        }
    }
    
    return (FunctionPointer)newGlue;
}

TransitionVector tVectorForFunctionPointer(FunctionPointer fp)
{
    FunctionPointer *newGlue = NULL;
    if (fp != NULL) {
        newGlue = (FunctionPointer *)malloc(2 * sizeof(FunctionPointer));
        if (newGlue != NULL) {
            newGlue[0] = fp;
            newGlue[1] = NULL;
        }
    }
    return (TransitionVector)newGlue;
}





