/*
        WebNetscapePluginPackage.m
	Copyright (c) 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebKitLogging.h>
#import <WebKit/WebNetscapePluginPackage.h>

typedef void (* FunctionPointer) (void);
typedef void (* TransitionVector) (void);
FunctionPointer functionPointerForTVector(TransitionVector);
TransitionVector tVectorForFunctionPointer(FunctionPointer);

@implementation WebNetscapePluginPackage

- (SInt16)openResourceFile
{
    FSRef fref;
    OSErr err;
    
    if(isBundle){
        return CFBundleOpenBundleResourceMap(bundle);
    }else{
        err = FSPathMakeRef((const UInt8 *)[[NSFileManager defaultManager] fileSystemRepresentationWithPath:path], &fref, NULL);
        if(err != noErr){
            return -1;
        }
        
        return FSOpenResFile(&fref, fsRdPerm);
    }
}

- (void)closeResourceFile:(SInt16)resRef
{
    if(isBundle){
        CFBundleCloseBundleResourceMap(bundle, resRef);
    }else{
        CloseResFile(resRef);
    }
}

- (NSString *)stringForStringListID:(SInt16)stringListID andIndex:(SInt16)index
{
    Str255 pString;
    char cString[256];
        
    GetIndString(pString, stringListID, index);
    if (pString[0] == 0){
        return nil;
    }

    CopyPascalStringToC(pString, cString);
    
    return [NSString stringWithCString:cString];
}

- (BOOL)getPluginInfo
{
    SInt16 resRef = [self openResourceFile];
    if(resRef == -1){
        return NO;
    }
    
    UseResFile(resRef);
    if(ResError() != noErr){
        return NO;
    }

    NSString *MIME, *extension, *extensionsList, *description;
    NSArray *extensions;
    NSRange r;
    uint i;
    
    MIMEToExtensions = [[NSMutableDictionary dictionary] retain];
    MIMEToDescription = [[NSMutableDictionary dictionary] retain];
    extensionToMIME = [[NSMutableDictionary dictionary] retain];

    for(i=1; 1; i+=2){
        MIME = [self stringForStringListID:128 andIndex:i];
        if(!MIME){
            break;
        }

        // FIXME: Avoid mime types with semi-colons because KJS can't properly parse them using KWQKConfigBase
        r = [MIME rangeOfString:@";"];
        if(r.length > 0){
            continue;
        }

        extensionsList = [self stringForStringListID:128 andIndex:i+1];
        if(extensionsList){
            extensions = [extensionsList componentsSeparatedByString:@","];
            
            [MIMEToExtensions setObject:extensions forKey:MIME];
            
            // Reverse the mapping
            NSEnumerator *enumerator = [extensions objectEnumerator];
            while ((extension = [enumerator nextObject]) != nil) {
                [extensionToMIME setObject:MIME forKey:extension];
            }
        }else{
            // DRM and WMP claim MIMEs without extensions. Use a @"" extension in this case.
            [MIMEToExtensions setObject:[NSArray arrayWithObject:@""] forKey:MIME];
        }
        
        description = [self stringForStringListID:127 andIndex:[MIMEToExtensions count]];
        if(description){
            [MIMEToDescription setObject:description forKey:MIME];
        }else{
            [MIMEToDescription setObject:@"" forKey:MIME];
        }
    }
    
    pluginDescription = [self stringForStringListID:126 andIndex:1];
    if(!pluginDescription){
        pluginDescription = filename;
    }
    
    name = [self stringForStringListID:126 andIndex:2];
    if(!name){
        name = filename;
    }

    [pluginDescription retain];
    [name retain];
    
    [self closeResourceFile:resRef];
    
    return YES;
}

- (NSString *)stringByResolvingSymlinksAndAliasesInPath:(NSString *)thePath
{
    NSString *newPath = [thePath stringByResolvingSymlinksInPath];

    FSRef fref;
    OSErr err;

    err = FSPathMakeRef((const UInt8 *)[[NSFileManager defaultManager] fileSystemRepresentationWithPath:thePath], &fref, NULL);
    if(err != noErr){
        return newPath;
    }

    Boolean targetIsFolder;
    Boolean wasAliased;
    err = FSResolveAliasFile (&fref, TRUE, &targetIsFolder, &wasAliased);
    if(err != noErr){
        return newPath;
    }
    
    if(wasAliased){
        NSURL *URL = (NSURL *)CFURLCreateFromFSRef(kCFAllocatorDefault, &fref);
        newPath = [URL path];
        [URL release];
    }

    return newPath;
}

- initWithPath:(NSString *)pluginPath
{
    path = [self stringByResolvingSymlinksAndAliasesInPath:pluginPath];
    filename = [path lastPathComponent];
    
    NSFileManager *fileManager = [NSFileManager defaultManager];;
    NSDictionary *fileInfo = [fileManager fileAttributesAtPath:path traverseLink:YES];
    UInt32 type;

    // single-file plug-in with resource fork
    if([[fileInfo objectForKey:@"NSFileType"] isEqualToString:@"NSFileTypeRegular"]){
        type = [[fileInfo objectForKey:@"NSFileHFSTypeCode"] unsignedLongValue];
        isBundle = NO;
#ifndef __ppc__
        return nil;
#endif
    // bundle
    }else if([[fileInfo objectForKey:@"NSFileType"] isEqualToString:@"NSFileTypeDirectory"]){
        CFURLRef pluginURL = CFURLCreateWithFileSystemPath(NULL, (CFStringRef)path, kCFURLPOSIXPathStyle, TRUE);
        bundle = CFBundleCreate(NULL, pluginURL);
        CFRelease(pluginURL);
        
        CFBundleGetPackageInfo(bundle, &type, NULL);

        // Check if the executable is mach-o or CFM
        NSURL *executableURL = (NSURL *)CFBundleCopyExecutableURL(bundle);
        NSFileHandle *executableFile = [NSFileHandle fileHandleForReadingAtPath:[executableURL path]];
        [executableURL release];
        
        NSData *data = [executableFile readDataOfLength:8];
        if(!memcmp([data bytes], "Joy!peff", 8)){
            isCFM = TRUE;
#ifndef __ppc__
            return nil;
#endif
        }else{
            isCFM = FALSE;
        }
        [executableFile closeFile];
        
        isBundle = YES;
    }else{
        return nil;
    }
    
    if(type == FOUR_CHAR_CODE('BRPL') || type == FOUR_CHAR_CODE('IEPL') ){
        if(![self getPluginInfo]){
            return nil;
        }
    }else{
        return nil;
    }
    
    [filename retain];
    [path retain];

    return self;
}

- (BOOL)load
{    
    getEntryPointsFuncPtr NP_GetEntryPoints = NULL;
    initializeFuncPtr NP_Initialize = NULL;
    mainFuncPtr pluginMainFunc;
    NPError npErr;
    OSErr err;


    if(isLoaded){
        return YES;
    }
    
    if(isBundle){
        if (!CFBundleLoadExecutable(bundle)){
            return NO;
        }

        if(isCFM){
            pluginMainFunc = (mainFuncPtr)CFBundleGetFunctionPointerForName(bundle, CFSTR("main") );
            if(!pluginMainFunc)
                return NO;
        }else{
            NP_Initialize = (initializeFuncPtr)CFBundleGetFunctionPointerForName(bundle, CFSTR("NP_Initialize") );
            NP_GetEntryPoints = (getEntryPointsFuncPtr)CFBundleGetFunctionPointerForName(bundle, CFSTR("NP_GetEntryPoints") );
            NPP_Shutdown = (NPP_ShutdownProcPtr)CFBundleGetFunctionPointerForName(bundle, CFSTR("NP_Shutdown") );
            if(!NP_Initialize || !NP_GetEntryPoints || !NPP_Shutdown){
                return NO;
            }
        }
    }else{ // single CFM file
        FSSpec spec;
        FSRef fref;
        
        err = FSPathMakeRef((UInt8 *)[path cString], &fref, NULL);
        if(err != noErr){
            ERROR("FSPathMakeRef failed. Error=%d", err);
            return NO;
        }
        err = FSGetCatalogInfo(&fref, kFSCatInfoNone, NULL, NULL, &spec, NULL);
        if(err != noErr){
            ERROR("FSGetCatalogInfo failed. Error=%d", err);
            return NO;
        }
        err = GetDiskFragment(&spec, 0, kCFragGoesToEOF, nil, kPrivateCFragCopy, &connID, (Ptr *)&pluginMainFunc, nil);
        if(err != noErr){
            ERROR("GetDiskFragment failed. Error=%d", err);
            return NO;
        }
        pluginMainFunc = (mainFuncPtr)functionPointerForTVector((TransitionVector)pluginMainFunc);
        if(!pluginMainFunc){
            return NO;
        }
            
        isCFM = TRUE;
    }
    
    // Plugins (at least QT) require that you call UseResFile on the resource file before loading it.
    
    resourceRef = [self openResourceFile];
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
        LOG(Plugins, "pluginMainFunc: %d, size=%d, version=%d", npErr, pluginSize, pluginVersion);
        
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
    LOG(Plugins, "Plugin Loaded");
    isLoaded = TRUE;
    return YES;
}

- (void)unload
{
    NPP_Shutdown();
    
    [self closeResourceFile:resourceRef];
    
    if(isBundle){
        CFBundleUnloadExecutable(bundle);
        CFRelease(bundle);
    }else{
        CloseConnection(&connID);
    }
    LOG(Plugins, "Plugin Unloaded");
    isLoaded = FALSE;
}

- (NPP_SetWindowProcPtr)NPP_SetWindow
{
    return NPP_SetWindow;
}

- (NPP_NewProcPtr)NPP_New
{
    return NPP_New;
}

- (NPP_DestroyProcPtr)NPP_Destroy
{
    return NPP_Destroy;
}

- (NPP_NewStreamProcPtr)NPP_NewStream
{
    return NPP_NewStream;
}

- (NPP_StreamAsFileProcPtr)NPP_StreamAsFile
{
    return NPP_StreamAsFile;
}
- (NPP_DestroyStreamProcPtr)NPP_DestroyStream
{
    return NPP_DestroyStream;
}

- (NPP_WriteReadyProcPtr)NPP_WriteReady
{
    return NPP_WriteReady;
}
- (NPP_WriteProcPtr)NPP_Write
{
    return NPP_Write;
}

- (NPP_HandleEventProcPtr)NPP_HandleEvent
{
    return NPP_HandleEvent;
}

-(NPP_URLNotifyProcPtr)NPP_URLNotify
{
    return NPP_URLNotify;
}

-(NPP_GetValueProcPtr)NPP_GetValue
{
    return NPP_GetValue;
}

-(NPP_SetValueProcPtr)NPP_SetValue
{
    return NPP_SetValue;
}

-(NPP_PrintProcPtr)NPP_Print
{
    return NPP_Print;
}

- (NSDictionary *)MIMEToExtensionsDictionary
{
    return MIMEToExtensions;
}

- (NSDictionary *)extensionToMIMEDictionary
{
    return extensionToMIME;
}

- (NSDictionary *)MIMEToDescriptionDictionary
{
    return MIMEToDescription;
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

- (NSString *)pluginDescription
{
    return pluginDescription;
}

- (NSString *)description
{
    
    return [NSString stringWithFormat:@"name: %@\npath: %@\nisLoaded: %d\nmimeTypes:\n%@\npluginDescription:%@",
        name, path, isLoaded, [MIMEToExtensions description], [MIMEToDescription description], pluginDescription];
}
@end


// function pointer converters

FunctionPointer functionPointerForTVector(TransitionVector tvp)
{
    const uint32 temp[6] = {0x3D800000, 0x618C0000, 0x800C0000, 0x804C0004, 0x7C0903A6, 0x4E800420};
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
