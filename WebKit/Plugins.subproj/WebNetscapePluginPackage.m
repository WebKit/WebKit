/*
        WebNetscapePluginPackage.m
	Copyright (c) 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebNetscapePluginPackage.h>

#import <WebKit/WebKitLogging.h>

#import <CoreFoundation/CFBundlePriv.h>

#import <Foundation/NSBundle_Private.h>

typedef void (* FunctionPointer) (void);
typedef void (* TransitionVector) (void);
static FunctionPointer functionPointerForTVector(TransitionVector);
static TransitionVector tVectorForFunctionPointer(FunctionPointer);

@implementation WebNetscapePluginPackage

+ (void)initialize
{
    // The Shockwave plugin requires a valid file in CurApRefNum.
    // But it doesn't seem to matter what file it is.
    // If we're called inside a Cocoa application which won't have a
    // CurApRefNum, we set it to point to the system resource file.
    if (LMGetCurApRefNum() == -1) {
        // To get the refNum for the system resource file, we have to do
        // UseResFile(kSystemResFile) and then look at CurResFile().
        short savedCurResFile = CurResFile();
        UseResFile(kSystemResFile);
        LMSetCurApRefNum(CurResFile());
        UseResFile(savedCurResFile);
    }
}

+ (NSString *)preferredLocalizationName
{
    SInt32 languageCode;
    SInt32 regionCode;
    SInt32 scriptCode;
    CFStringEncoding stringEncoding;

    CFBundleGetLocalizationInfoForLocalization(NULL, &languageCode, &regionCode, &scriptCode, &stringEncoding);
    NSString *localizationName = (NSString *)CFBundleCopyLocalizationForLocalizationInfo(languageCode, regionCode, scriptCode, stringEncoding);
    return [localizationName autorelease];
}

- (SInt16)openResourceFile
{
    FSRef fref;
    OSErr err;
    
    if (isBundle) {
        return CFBundleOpenBundleResourceMap([bundle _cfBundle]);
    } else {
        err = FSPathMakeRef((const UInt8 *)[path fileSystemRepresentation], &fref, NULL);
        if (err != noErr) {
            return -1;
        }
        
        return FSOpenResFile(&fref, fsRdPerm);
    }
}

- (void)closeResourceFile:(SInt16)resRef
{
    if (isBundle) {
        CFBundleCloseBundleResourceMap([bundle _cfBundle], resRef);
    } else {
        CloseResFile(resRef);
    }
}

- (NSString *)stringForStringListID:(SInt16)stringListID andIndex:(SInt16)index
{
    Str255 pString;
    char cString[256];
        
    GetIndString(pString, stringListID, index);
    if (pString[0] == 0) {
        return nil;
    }

    CopyPascalStringToC(pString, cString);
    
    return [NSString stringWithCString:cString];
}

- (BOOL)getPluginInfoFromResources
{
    SInt16 resRef = [self openResourceFile];
    if (resRef == -1) {
        return NO;
    }
    
    UseResFile(resRef);
    if (ResError() != noErr) {
        return NO;
    }

    NSString *MIME, *extensionsList, *description;
    NSArray *extensions;
    NSRange r;
    uint i;
    
    NSMutableDictionary *MIMEToExtensionsDictionary = [NSMutableDictionary dictionary];
    NSMutableDictionary *MIMEToDescriptionDictionary = [NSMutableDictionary dictionary];

    for (i=1; 1; i+=2) {
        MIME = [self stringForStringListID:128 andIndex:i];
        if (!MIME) {
            break;
        }

        // FIXME: Avoid mime types with semi-colons because KJS can't properly parse them using KWQKConfigBase
        r = [MIME rangeOfString:@";"];
        if (r.length > 0) {
            continue;
        }

        extensionsList = [self stringForStringListID:128 andIndex:i+1];
        if (extensionsList) {
            extensions = [extensionsList componentsSeparatedByString:@","];
            [MIMEToExtensionsDictionary setObject:extensions forKey:MIME];
        } else {
            // DRM and WMP claim MIMEs without extensions. Use a @"" extension in this case.
            [MIMEToExtensionsDictionary setObject:[NSArray arrayWithObject:@""] forKey:MIME];
        }
        
        description = [self stringForStringListID:127 andIndex:[MIMEToExtensionsDictionary count]];
        if (description) {
            [MIMEToDescriptionDictionary setObject:description forKey:MIME];
        } else {
            [MIMEToDescriptionDictionary setObject:@"" forKey:MIME];
        }
    }

    // Workaround for this bug:
    // Radar 3134219 (MPEG-4 files don't work with the QuickTime plugin in Safari, work fine in Mozilla, IE)
    // For beta 1, when getting the MIME information for the QuickTime plugin, we directly insert the 
    // information to handle MP4. 
    // In the future, we will use the additional plugin entry points to dynamically load this information
    // from the plugin itself.
    if ([[self filename] isEqualToString:@"QuickTime Plugin.plugin"]) {
        NSArray *extensions = [NSArray arrayWithObjects:@"mp4", @"mpg4", nil];
        [MIMEToExtensionsDictionary setObject:extensions forKey:@"video/mp4"];
        [MIMEToDescriptionDictionary setObject:@"MP4 Video" forKey:@"video/mp4"];
        [MIMEToExtensionsDictionary setObject:extensions forKey:@"audio/mp4"];
        [MIMEToDescriptionDictionary setObject:@"MP4 Audio" forKey:@"audio/mp4"];
    }

    [self setMIMEToDescriptionDictionary:MIMEToDescriptionDictionary];
    [self setMIMEToExtensionsDictionary:MIMEToExtensionsDictionary];

    NSString *filename = [self filename];
    
    description = [self stringForStringListID:126 andIndex:1];
    if (!description) {
        description = filename;
    }
    [self setPluginDescription:description];
    
    
    NSString *theName = [self stringForStringListID:126 andIndex:2];
    if (!theName) {
        theName = filename;
    }
    [self setName:theName];
    
    [self closeResourceFile:resRef];
    
    return YES;
}

- (NSDictionary *)pListForPath:(NSString *)pListPath createFile:(BOOL)createFile
{
    if (createFile && [self load] && BP_CreatePluginMIMETypesPreferences) {
        BP_CreatePluginMIMETypesPreferences();
    }

    NSDictionary *pList = nil;
    NSData *data = [NSData dataWithContentsOfFile:pListPath];
    if (data) {
        pList = [NSPropertyListSerialization propertyListFromData:data
                                                 mutabilityOption:NSPropertyListImmutable
                                                           format:nil
                                                 errorDescription:nil];
    }

    return pList;
}

- (BOOL)getPluginInfoFromPLists
{
    if (!bundle) {
        return NO;
    }

    NSDictionary *MIMETypes = nil;
    NSString *pListFilename = [bundle objectForInfoDictionaryKey:WebPluginMIMETypesFilenameKey];

    // Check if the MIME types are claimed in a plist in the user's preferences directory.
    if (pListFilename) {
        NSString *pListPath = [NSString stringWithFormat:@"%@/Library/Preferences/%@", NSHomeDirectory(), pListFilename];
        NSDictionary *pList = [self pListForPath:pListPath createFile:NO];
        if (pList) {
            // If the plist isn't localized, have the plug-in recreate it in the preferred language.
            NSString *localizationName = [pList objectForKey:WebPluginLocalizationNameKey];
            if (![localizationName isEqualToString:[[self class] preferredLocalizationName]]) {
                pList = [self pListForPath:pListPath createFile:YES];
            }
            MIMETypes = [pList objectForKey:WebPluginMIMETypesKey];
        } else {
            // Plist doesn't exist, ask the plug-in to create it.
            MIMETypes = [[self pListForPath:pListPath createFile:YES] objectForKey:WebPluginMIMETypesKey];
        }
    }

    // Pass the MIME dictionary to the superclass to parse it.
    return [self getPluginInfoFromBundleAndMIMEDictionary:MIMETypes];
}

- initWithPath:(NSString *)pluginPath
{
    [super initWithPath:pluginPath];

    resourceRef = -1;
    
    OSType type = 0;

    // Bundle
    if (bundle) {
        isBundle = YES;
        CFBundleGetPackageInfo([bundle _cfBundle], &type, NULL);
    }
#ifdef __ppc__
    // Single-file plug-in with resource fork
    else {
        type = [[[NSFileManager defaultManager] fileAttributesAtPath:path traverseLink:YES] fileHFSTypeCode];
        isBundle = NO;
        isCFM = YES;
    }
#endif
    
    if (type != FOUR_CHAR_CODE('BRPL')) {
        [self release];
        return nil;
    }

    // Check if the executable is Mach-O or CFM.
    if (bundle) {
        NSURL *executableURL = (NSURL *)CFBundleCopyExecutableURL([bundle _cfBundle]);
        NSFileHandle *executableFile = [NSFileHandle fileHandleForReadingAtPath:[executableURL path]];
        [executableURL release];
        NSData *data = [executableFile readDataOfLength:8];
        [executableFile closeFile];
        if (!data) {
            [self release];
            return nil;
        }
        isCFM = memcmp([data bytes], "Joy!peff", 8) == 0;
#ifndef __ppc__
        // CFM is PPC-only.
        if (isCFM) {
            [self release];
            return nil;
        }
#endif
    }

    if (![self getPluginInfoFromPLists]) {
        if (![self getPluginInfoFromResources]) {
            [self release];
            return nil;
        }
    }

    return self;
}

- (WebExecutableType)executableType
{
    if (isCFM) {
        return WebCFMExecutableType;
    } else {
        return WebMachOExecutableType;
    }
}

- (BOOL)isLoaded
{
    return isLoaded;
}

- (void)unloadWithoutShutdown
{
    if (!isLoaded) {
        return;
    }

    if (resourceRef != -1) {
        [self closeResourceFile:resourceRef];
    }

    if (isBundle) {
        CFBundleUnloadExecutable([bundle _cfBundle]);
    } else {
        CloseConnection(&connID);
    }

    LOG(Plugins, "Plugin Unloaded");
    isLoaded = NO;
}

- (BOOL)load
{    
    NP_GetEntryPointsFuncPtr NP_GetEntryPoints = NULL;
    NP_InitializeFuncPtr NP_Initialize = NULL;
    MainFuncPtr pluginMainFunc;
    NPError npErr;
    OSErr err;

    if (isLoaded) {
        return YES;
    }
    
    if (isBundle) {
        CFBundleRef cfBundle = [bundle _cfBundle];
        if (!CFBundleLoadExecutable(cfBundle)) {
            goto abort;
        }
        
        isLoaded = YES;
        
        if (isCFM) {
            pluginMainFunc = (MainFuncPtr)CFBundleGetFunctionPointerForName(cfBundle, CFSTR("main") );
            if(!pluginMainFunc) {
                goto abort;
            }
        } else {
            NP_Initialize = (NP_InitializeFuncPtr)CFBundleGetFunctionPointerForName(cfBundle, CFSTR("NP_Initialize"));
            NP_GetEntryPoints = (NP_GetEntryPointsFuncPtr)CFBundleGetFunctionPointerForName(cfBundle, CFSTR("NP_GetEntryPoints"));
            NPP_Shutdown = (NPP_ShutdownProcPtr)CFBundleGetFunctionPointerForName(cfBundle, CFSTR("NP_Shutdown"));
            if (NP_Initialize || !NP_GetEntryPoints || !NPP_Shutdown) {
                goto abort;
            }
        }
        BP_CreatePluginMIMETypesPreferences = (BP_CreatePluginMIMETypesPreferencesFuncPtr)CFBundleGetFunctionPointerForName(cfBundle, CFSTR("BP_CreatePluginMIMETypesPreferences"));
    } else {
        // single CFM file
        FSSpec spec;
        FSRef fref;
        
        err = FSPathMakeRef((UInt8 *)[path fileSystemRepresentation], &fref, NULL);
        if (err != noErr) {
            ERROR("FSPathMakeRef failed. Error=%d", err);
            goto abort;
        }
        err = FSGetCatalogInfo(&fref, kFSCatInfoNone, NULL, NULL, &spec, NULL);
        if (err != noErr) {
            ERROR("FSGetCatalogInfo failed. Error=%d", err);
            goto abort;
        }
        err = GetDiskFragment(&spec, 0, kCFragGoesToEOF, nil, kPrivateCFragCopy, &connID, (Ptr *)&pluginMainFunc, nil);
        if (err != noErr) {
            ERROR("GetDiskFragment failed. Error=%d", err);
            goto abort;
        }
        
        isLoaded = YES;
        
        pluginMainFunc = (MainFuncPtr)functionPointerForTVector((TransitionVector)pluginMainFunc);
        if (!pluginMainFunc) {
            goto abort;
        }
            
        isCFM = TRUE;
    }
    
    // Plugins (at least QT) require that you call UseResFile on the resource file before loading it.
    resourceRef = [self openResourceFile];
    if (resourceRef != -1) {
        UseResFile(resourceRef);
    }
    
    // swap function tables
    if (isCFM) {
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
        if (npErr != NPERR_NO_ERROR) {
            [self unload];
            return NO;
        }
        
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
    } else {
        // no function pointer conversion necessary for mach-o
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

        npErr = NP_Initialize(&browserFuncs);
        if (npErr != NPERR_NO_ERROR) {
            [self unload];
            return NO;
        }
        npErr = NP_GetEntryPoints(&pluginFuncs);
        if (npErr != NPERR_NO_ERROR) {
            [self unload];
            return NO;
        }
        
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
    return YES;

abort:
    [self unloadWithoutShutdown];
    return NO;
}
    
- (void)unload
{
    if (!isLoaded) {
        return;
    }

    NPP_Shutdown();

    [self unloadWithoutShutdown];
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
