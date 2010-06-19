/*
 * Copyright (C) 2005, 2006, 2007 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#if ENABLE(NETSCAPE_PLUGIN_API)
#import "WebNetscapePluginPackage.h"

#import "WebKitLogging.h"
#import "WebKitNSStringExtras.h"
#import "WebNSFileManagerExtras.h"
#import "WebNSObjectExtras.h"
#import "WebNetscapeDeprecatedFunctions.h"
#import <WebCore/npruntime_impl.h>

#if USE(PLUGIN_HOST_PROCESS)
#import "NetscapePluginHostManager.h"

using namespace WebKit;
#endif

#ifdef SUPPORT_CFM
typedef void (* FunctionPointer)(void);
typedef void (* TransitionVector)(void);
static FunctionPointer functionPointerForTVector(TransitionVector);
static TransitionVector tVectorForFunctionPointer(FunctionPointer);
#endif

#define PluginNameOrDescriptionStringNumber     126
#define MIMEDescriptionStringNumber             127
#define MIMEListStringStringNumber              128

#define RealPlayerAppIndentifier                @"com.RealNetworks.RealOne Player"
#define RealPlayerPluginFilename                @"RealPlayer Plugin"

@interface WebNetscapePluginPackage (Internal)
- (void)_unloadWithShutdown:(BOOL)shutdown;
@end

@implementation WebNetscapePluginPackage

#ifndef __LP64__
+ (void)initialize
{
    // The Shockwave plugin requires a valid file in CurApRefNum.
    // But it doesn't seem to matter what file it is.
    // If we're called inside a Cocoa application which won't have a
    // CurApRefNum, we set it to point to the system resource file.

    // Call CurResFile before testing the result of WebLMGetCurApRefNum.
    // If we are called before the bundle resource map has been opened
    // for a Carbon application (or a Cocoa app with Resource Manager
    // resources) we *do not* want to set CurApRefNum to point at the
    // system resource file. CurResFile triggers Resource Manager lazy
    // initialization, and will open the bundle resource map as necessary.

    CurResFile();

    if (WebLMGetCurApRefNum() == -1) {
        // To get the refNum for the system resource file, we have to do
        // UseResFile(kSystemResFile) and then look at CurResFile().
        short savedCurResFile = CurResFile();
        UseResFile(kSystemResFile);
        WebLMSetCurApRefNum(CurResFile());
        UseResFile(savedCurResFile);
    }
}
#endif

- (ResFileRefNum)openResourceFile
{
#ifdef SUPPORT_CFM
    if (!isBundle) {
        FSRef fref;
        OSErr err = FSPathMakeRef((const UInt8 *)[path fileSystemRepresentation], &fref, NULL);
        if (err != noErr)
            return -1;
        
        return FSOpenResFile(&fref, fsRdPerm);
    }
#endif

    return CFBundleOpenBundleResourceMap(cfBundle);
}

- (void)closeResourceFile:(ResFileRefNum)resRef
{
#ifdef SUPPORT_CFM
    if (!isBundle) {
        CloseResFile(resRef);
        return;
    }
#endif

    CFBundleCloseBundleResourceMap(cfBundle, resRef);
}

- (NSString *)stringForStringListID:(SInt16)stringListID andIndex:(SInt16)index
{
    // Get resource, and dereference the handle.
    Handle stringHandle = Get1Resource('STR#', stringListID);
    if (stringHandle == NULL) {
        return nil;
    }
    unsigned char *p = (unsigned char *)*stringHandle;
    if (!p)
        return nil;
    
    // Check the index against the length of the string list, then skip the length.
    if (index < 1 || index > *(SInt16 *)p)
        return nil;
    p += sizeof(SInt16);
    
    // Skip any strings that come before the one we are looking for.
    while (--index)
        p += 1 + *p;
    
    // Convert the one we found into an NSString.
    return [[[NSString alloc] initWithBytes:(p + 1) length:*p encoding:[NSString _web_encodingForResource:stringHandle]] autorelease];
}

- (BOOL)getPluginInfoFromResources
{
    SInt16 resRef = [self openResourceFile];
    if (resRef == -1)
        return NO;
    
    UseResFile(resRef);
    if (ResError() != noErr)
        return NO;

    NSString *MIME, *extensionsList, *description;
    NSArray *extensions;
    unsigned i;
    
    NSMutableDictionary *MIMEToExtensionsDictionary = [NSMutableDictionary dictionary];
    NSMutableDictionary *MIMEToDescriptionDictionary = [NSMutableDictionary dictionary];

    for (i=1; 1; i+=2) {
        MIME = [[self stringForStringListID:MIMEListStringStringNumber
                                   andIndex:i] lowercaseString];
        if (!MIME)
            break;

        extensionsList = [[self stringForStringListID:MIMEListStringStringNumber andIndex:i+1] lowercaseString];
        if (extensionsList) {
            extensions = [extensionsList componentsSeparatedByString:@","];
            [MIMEToExtensionsDictionary setObject:extensions forKey:MIME];
        } else
            // DRM and WMP claim MIMEs without extensions. Use a @"" extension in this case.
            [MIMEToExtensionsDictionary setObject:[NSArray arrayWithObject:@""] forKey:MIME];
        
        description = [self stringForStringListID:MIMEDescriptionStringNumber
                                         andIndex:[MIMEToExtensionsDictionary count]];
        if (description)
            [MIMEToDescriptionDictionary setObject:description forKey:MIME];
        else
            [MIMEToDescriptionDictionary setObject:@"" forKey:MIME];
    }

    [self setMIMEToDescriptionDictionary:MIMEToDescriptionDictionary];
    [self setMIMEToExtensionsDictionary:MIMEToExtensionsDictionary];

    NSString *filename = [self filename];
    
    description = [self stringForStringListID:PluginNameOrDescriptionStringNumber andIndex:1];
    if (!description)
        description = filename;
    [self setPluginDescription:description];
    
    
    NSString *theName = [self stringForStringListID:PluginNameOrDescriptionStringNumber andIndex:2];
    if (!theName)
        theName = filename;
    [self setName:theName];
    
    [self closeResourceFile:resRef];
    
    return YES;
}

- (BOOL)_initWithPath:(NSString *)pluginPath
{
    resourceRef = -1;
    
    OSType type = 0;

    if (bundle) {
        // Bundle
        CFBundleGetPackageInfo(cfBundle, &type, NULL);
#ifdef SUPPORT_CFM
        isBundle = YES;
#endif
    } else {
#ifdef SUPPORT_CFM
        // Single-file plug-in with resource fork
        NSString *destinationPath = [[NSFileManager defaultManager] destinationOfSymbolicLinkAtPath:path error:0];
        type = [[[NSFileManager defaultManager] attributesOfItemAtPath:destinationPath error:0] fileHFSTypeCode];
        isBundle = NO;
        isCFM = YES;
#else
        return NO;
#endif
    }
    
    if (type != FOUR_CHAR_CODE('BRPL'))
        return NO;

    // Check if the executable is Mach-O or CFM.
    if (bundle) {
        NSFileHandle *executableFile = [NSFileHandle fileHandleForReadingAtPath:[bundle executablePath]];
        NSData *data = [executableFile readDataOfLength:512];
        [executableFile closeFile];
        // Check the length of the data before calling memcmp. We think this fixes 3782543.
        if (data == nil || [data length] < 8)
            return NO;
        BOOL hasCFMHeader = memcmp([data bytes], "Joy!peff", 8) == 0;
#ifdef SUPPORT_CFM
        isCFM = hasCFMHeader;
#else
        if (hasCFMHeader)
            return NO;
#endif

#if USE(PLUGIN_HOST_PROCESS)
        NSArray *archs = [bundle executableArchitectures];
        
        if ([archs containsObject:[NSNumber numberWithInteger:NSBundleExecutableArchitectureX86_64]])
            pluginHostArchitecture = CPU_TYPE_X86_64;
        else if ([archs containsObject:[NSNumber numberWithInteger:NSBundleExecutableArchitectureI386]])
            pluginHostArchitecture = CPU_TYPE_X86;
        else
            return NO;
#else
        if (![self isNativeLibraryData:data])
            return NO;
#endif
    }

    if (![self getPluginInfoFromPLists] && ![self getPluginInfoFromResources])
        return NO;
    
    return YES;
}

- (id)initWithPath:(NSString *)pluginPath
{
    if (!(self = [super initWithPath:pluginPath]))
        return nil;
    
    // Initializing a plugin package can cause it to be loaded.  If there was an error initializing the plugin package,
    // ensure that it is unloaded before deallocating it (WebBasePluginPackage requires & asserts this).
    if (![self _initWithPath:pluginPath]) {
        [self _unloadWithShutdown:YES];
        [self release];
        return nil;
    }
        
    return self;
}

- (WebExecutableType)executableType
{
#ifdef SUPPORT_CFM
    if (isCFM)
        return WebCFMExecutableType;
#endif
    return WebMachOExecutableType;
}

#if USE(PLUGIN_HOST_PROCESS)
- (cpu_type_t)pluginHostArchitecture
{
    return pluginHostArchitecture;
}

- (void)createPropertyListFile
{
    NetscapePluginHostManager::createPropertyListFile(self);
}

#endif

- (void)launchRealPlayer
{
    CFURLRef appURL = NULL;
    OSStatus error = LSFindApplicationForInfo(kLSUnknownCreator, (CFStringRef)RealPlayerAppIndentifier, NULL, NULL, &appURL);
    if (!error) {
        LSLaunchURLSpec URLSpec;
        bzero(&URLSpec, sizeof(URLSpec));
        URLSpec.launchFlags = kLSLaunchDefaults | kLSLaunchDontSwitch;
        URLSpec.appURL = appURL;
        LSOpenFromURLSpec(&URLSpec, NULL);
        CFRelease(appURL);
    }
}

- (void)_applyDjVuWorkaround
{
    if (!cfBundle)
        return;
    
    if ([(NSString *)CFBundleGetIdentifier(cfBundle) isEqualToString:@"com.lizardtech.NPDjVu"]) {
        // The DjVu plug-in will crash copying the vtable if it's too big so we cap it to 
        // what the plug-in expects here. 
        // size + version + 40 function pointers.
        browserFuncs.size = 2 + 2 + sizeof(void *) * 40;
    }
        
}

- (void)unload
{
    [self _unloadWithShutdown:YES];
}

- (BOOL)_tryLoad
{
    NP_GetEntryPointsFuncPtr NP_GetEntryPoints = NULL;
    NP_InitializeFuncPtr NP_Initialize = NULL;
    NPError npErr;

#ifdef SUPPORT_CFM
    MainFuncPtr pluginMainFunc = NULL;
#endif

#if !LOG_DISABLED
    CFAbsoluteTime start = CFAbsoluteTimeGetCurrent();
    CFAbsoluteTime currentTime;
    CFAbsoluteTime duration;
#endif
    LOG(Plugins, "%f Load timing started for: %@", start, [self name]);

    if (isLoaded)
        return YES;
    
#ifdef SUPPORT_CFM
    if (isBundle) {
#endif
        if (!CFBundleLoadExecutable(cfBundle))
            return NO;
#if !LOG_DISABLED
        currentTime = CFAbsoluteTimeGetCurrent();
        duration = currentTime - start;
#endif
        LOG(Plugins, "%f CFBundleLoadExecutable took %f seconds", currentTime, duration);
        isLoaded = YES;
        
#ifdef SUPPORT_CFM
        if (isCFM) {
            pluginMainFunc = (MainFuncPtr)CFBundleGetFunctionPointerForName(cfBundle, CFSTR("main") );
            if (!pluginMainFunc)
                return NO;
        } else {
#endif
            NP_Initialize = (NP_InitializeFuncPtr)CFBundleGetFunctionPointerForName(cfBundle, CFSTR("NP_Initialize"));
            NP_GetEntryPoints = (NP_GetEntryPointsFuncPtr)CFBundleGetFunctionPointerForName(cfBundle, CFSTR("NP_GetEntryPoints"));
            NP_Shutdown = (NPP_ShutdownProcPtr)CFBundleGetFunctionPointerForName(cfBundle, CFSTR("NP_Shutdown"));
            if (!NP_Initialize || !NP_GetEntryPoints || !NP_Shutdown)
                return NO;
#ifdef SUPPORT_CFM
        }
    } else {
        // single CFM file
        FSSpec spec;
        FSRef fref;
        OSErr err;
        
        err = FSPathMakeRef((UInt8 *)[path fileSystemRepresentation], &fref, NULL);
        if (err != noErr) {
            LOG_ERROR("FSPathMakeRef failed. Error=%d", err);
            return NO;
        }
        err = FSGetCatalogInfo(&fref, kFSCatInfoNone, NULL, NULL, &spec, NULL);
        if (err != noErr) {
            LOG_ERROR("FSGetCatalogInfo failed. Error=%d", err);
            return NO;
        }
        err = WebGetDiskFragment(&spec, 0, kCFragGoesToEOF, nil, kPrivateCFragCopy, &connID, (Ptr *)&pluginMainFunc, nil);
        if (err != noErr) {
            LOG_ERROR("WebGetDiskFragment failed. Error=%d", err);
            return NO;
        }
#if !LOG_DISABLED
        currentTime = CFAbsoluteTimeGetCurrent();
        duration = currentTime - start;
#endif
        LOG(Plugins, "%f WebGetDiskFragment took %f seconds", currentTime, duration);
        isLoaded = YES;
        
        pluginMainFunc = (MainFuncPtr)functionPointerForTVector((TransitionVector)pluginMainFunc);
        if (!pluginMainFunc) {
            return NO;
        }

        // NOTE: pluginMainFunc is freed after it is called. Be sure not to return before that.
        
        isCFM = YES;
    }
#endif /* SUPPORT_CFM */
    
    // Plugins (at least QT) require that you call UseResFile on the resource file before loading it.
    resourceRef = [self openResourceFile];
    if (resourceRef != -1) {
        UseResFile(resourceRef);
    }
    
    // swap function tables
#ifdef SUPPORT_CFM
    if (isCFM) {
        browserFuncs.version = NP_VERSION_MINOR;
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
        browserFuncs.pushpopupsenabledstate = (NPN_PushPopupsEnabledStateProcPtr)tVectorForFunctionPointer((FunctionPointer)NPN_PushPopupsEnabledState);
        browserFuncs.poppopupsenabledstate = (NPN_PopPopupsEnabledStateProcPtr)tVectorForFunctionPointer((FunctionPointer)NPN_PopPopupsEnabledState);
        browserFuncs.pluginthreadasynccall = (NPN_PluginThreadAsyncCallProcPtr)tVectorForFunctionPointer((FunctionPointer)NPN_PluginThreadAsyncCall);
        browserFuncs.getvalueforurl = (NPN_GetValueForURLProcPtr)tVectorForFunctionPointer((FunctionPointer)NPN_GetValueForURL);
        browserFuncs.setvalueforurl = (NPN_SetValueForURLProcPtr)tVectorForFunctionPointer((FunctionPointer)NPN_SetValueForURL);
        browserFuncs.getauthenticationinfo = (NPN_GetAuthenticationInfoProcPtr)tVectorForFunctionPointer((FunctionPointer)NPN_GetAuthenticationInfo);
        browserFuncs.scheduletimer = (NPN_ScheduleTimerProcPtr)tVectorForFunctionPointer((FunctionPointer)NPN_ScheduleTimer);
        browserFuncs.unscheduletimer = (NPN_UnscheduleTimerProcPtr)tVectorForFunctionPointer((FunctionPointer)NPN_UnscheduleTimer);
        browserFuncs.popupcontextmenu = (NPN_PopUpContextMenuProcPtr)tVectorForFunctionPointer((FunctionPointer)NPN_PopUpContextMenu);
        browserFuncs.convertpoint = (NPN_ConvertPointProcPtr)tVectorForFunctionPointer((FunctionPointer)NPN_ConvertPoint);
        
        browserFuncs.releasevariantvalue = (NPN_ReleaseVariantValueProcPtr)tVectorForFunctionPointer((FunctionPointer)_NPN_ReleaseVariantValue);
        browserFuncs.getstringidentifier = (NPN_GetStringIdentifierProcPtr)tVectorForFunctionPointer((FunctionPointer)_NPN_GetStringIdentifier);
        browserFuncs.getstringidentifiers = (NPN_GetStringIdentifiersProcPtr)tVectorForFunctionPointer((FunctionPointer)_NPN_GetStringIdentifiers);
        browserFuncs.getintidentifier = (NPN_GetIntIdentifierProcPtr)tVectorForFunctionPointer((FunctionPointer)_NPN_GetIntIdentifier);
        browserFuncs.identifierisstring = (NPN_IdentifierIsStringProcPtr)tVectorForFunctionPointer((FunctionPointer)_NPN_IdentifierIsString);
        browserFuncs.utf8fromidentifier = (NPN_UTF8FromIdentifierProcPtr)tVectorForFunctionPointer((FunctionPointer)_NPN_UTF8FromIdentifier);
        browserFuncs.intfromidentifier = (NPN_IntFromIdentifierProcPtr)tVectorForFunctionPointer((FunctionPointer)_NPN_IntFromIdentifier);
        browserFuncs.createobject = (NPN_CreateObjectProcPtr)tVectorForFunctionPointer((FunctionPointer)_NPN_CreateObject);
        browserFuncs.retainobject = (NPN_RetainObjectProcPtr)tVectorForFunctionPointer((FunctionPointer)_NPN_RetainObject);
        browserFuncs.releaseobject = (NPN_ReleaseObjectProcPtr)tVectorForFunctionPointer((FunctionPointer)_NPN_ReleaseObject);
        browserFuncs.hasmethod = (NPN_HasMethodProcPtr)tVectorForFunctionPointer((FunctionPointer)_NPN_HasProperty);
        browserFuncs.invoke = (NPN_InvokeProcPtr)tVectorForFunctionPointer((FunctionPointer)_NPN_Invoke);
        browserFuncs.invokeDefault = (NPN_InvokeDefaultProcPtr)tVectorForFunctionPointer((FunctionPointer)_NPN_InvokeDefault);
        browserFuncs.evaluate = (NPN_EvaluateProcPtr)tVectorForFunctionPointer((FunctionPointer)_NPN_Evaluate);
        browserFuncs.hasproperty = (NPN_HasPropertyProcPtr)tVectorForFunctionPointer((FunctionPointer)_NPN_HasProperty);
        browserFuncs.getproperty = (NPN_GetPropertyProcPtr)tVectorForFunctionPointer((FunctionPointer)_NPN_GetProperty);
        browserFuncs.setproperty = (NPN_SetPropertyProcPtr)tVectorForFunctionPointer((FunctionPointer)_NPN_SetProperty);
        browserFuncs.removeproperty = (NPN_RemovePropertyProcPtr)tVectorForFunctionPointer((FunctionPointer)_NPN_RemoveProperty);
        browserFuncs.setexception = (NPN_SetExceptionProcPtr)tVectorForFunctionPointer((FunctionPointer)_NPN_SetException);
        browserFuncs.enumerate = (NPN_EnumerateProcPtr)tVectorForFunctionPointer((FunctionPointer)_NPN_Enumerate);
        browserFuncs.construct = (NPN_ConstructProcPtr)tVectorForFunctionPointer((FunctionPointer)_NPN_Construct);
        
        [self _applyDjVuWorkaround];
        
#if !LOG_DISABLED
        CFAbsoluteTime mainStart = CFAbsoluteTimeGetCurrent();
#endif
        LOG(Plugins, "%f main timing started", mainStart);
        NPP_ShutdownProcPtr shutdownFunction;
        npErr = pluginMainFunc(&browserFuncs, &pluginFuncs, &shutdownFunction);
        NP_Shutdown = (NPP_ShutdownProcPtr)functionPointerForTVector((TransitionVector)shutdownFunction);
        if (!isBundle)
            // Don't free pluginMainFunc if we got it from a bundle because it is owned by CFBundle in that case.
            free(reinterpret_cast<void*>(pluginMainFunc));
        
        // Workaround for 3270576. The RealPlayer plug-in fails to load if its preference file is out of date.
        // Launch the RealPlayer application to refresh the file.
        if (npErr != NPERR_NO_ERROR) {
            if (npErr == NPERR_MODULE_LOAD_FAILED_ERROR && [[self filename] isEqualToString:RealPlayerPluginFilename])
                [self launchRealPlayer];
            return NO;
        }
#if !LOG_DISABLED
        currentTime = CFAbsoluteTimeGetCurrent();
        duration = currentTime - mainStart;
#endif
        LOG(Plugins, "%f main took %f seconds", currentTime, duration);
        
        pluginSize = pluginFuncs.size;
        pluginVersion = pluginFuncs.version;
        LOG(Plugins, "pluginMainFunc: %d, size=%d, version=%d", npErr, pluginSize, pluginVersion);
        
        pluginFuncs.newp = (NPP_NewProcPtr)functionPointerForTVector((TransitionVector)pluginFuncs.newp);
        pluginFuncs.destroy = (NPP_DestroyProcPtr)functionPointerForTVector((TransitionVector)pluginFuncs.destroy);
        pluginFuncs.setwindow = (NPP_SetWindowProcPtr)functionPointerForTVector((TransitionVector)pluginFuncs.setwindow);
        pluginFuncs.newstream = (NPP_NewStreamProcPtr)functionPointerForTVector((TransitionVector)pluginFuncs.newstream);
        pluginFuncs.destroystream = (NPP_DestroyStreamProcPtr)functionPointerForTVector((TransitionVector)pluginFuncs.destroystream);
        pluginFuncs.asfile = (NPP_StreamAsFileProcPtr)functionPointerForTVector((TransitionVector)pluginFuncs.asfile);
        pluginFuncs.writeready = (NPP_WriteReadyProcPtr)functionPointerForTVector((TransitionVector)pluginFuncs.writeready);
        pluginFuncs.write = (NPP_WriteProcPtr)functionPointerForTVector((TransitionVector)pluginFuncs.write);
        pluginFuncs.print = (NPP_PrintProcPtr)functionPointerForTVector((TransitionVector)pluginFuncs.print);
        pluginFuncs.event = (NPP_HandleEventProcPtr)functionPointerForTVector((TransitionVector)pluginFuncs.event);
        pluginFuncs.urlnotify = (NPP_URLNotifyProcPtr)functionPointerForTVector((TransitionVector)pluginFuncs.urlnotify);
        pluginFuncs.getvalue = (NPP_GetValueProcPtr)functionPointerForTVector((TransitionVector)pluginFuncs.getvalue);
        pluginFuncs.setvalue = (NPP_SetValueProcPtr)functionPointerForTVector((TransitionVector)pluginFuncs.setvalue);

        // LiveConnect support
        pluginFuncs.javaClass = (JRIGlobalRef)functionPointerForTVector((TransitionVector)pluginFuncs.javaClass);
        if (pluginFuncs.javaClass) {
            LOG(LiveConnect, "%@:  CFM entry point for NPP_GetJavaClass = %p", [self name], pluginFuncs.javaClass);
        } else {
            LOG(LiveConnect, "%@:  no entry point for NPP_GetJavaClass", [self name]);
        }

    } else {

#endif

        // no function pointer conversion necessary for Mach-O
        browserFuncs.version = NP_VERSION_MINOR;
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
        browserFuncs.pushpopupsenabledstate = NPN_PushPopupsEnabledState;
        browserFuncs.poppopupsenabledstate = NPN_PopPopupsEnabledState;
        browserFuncs.pluginthreadasynccall = NPN_PluginThreadAsyncCall;
        browserFuncs.getvalueforurl = NPN_GetValueForURL;
        browserFuncs.setvalueforurl = NPN_SetValueForURL;
        browserFuncs.getauthenticationinfo = NPN_GetAuthenticationInfo;
        browserFuncs.scheduletimer = NPN_ScheduleTimer;
        browserFuncs.unscheduletimer = NPN_UnscheduleTimer;
        browserFuncs.popupcontextmenu = NPN_PopUpContextMenu;
        browserFuncs.convertpoint = NPN_ConvertPoint;

        browserFuncs.releasevariantvalue = _NPN_ReleaseVariantValue;
        browserFuncs.getstringidentifier = _NPN_GetStringIdentifier;
        browserFuncs.getstringidentifiers = _NPN_GetStringIdentifiers;
        browserFuncs.getintidentifier = _NPN_GetIntIdentifier;
        browserFuncs.identifierisstring = _NPN_IdentifierIsString;
        browserFuncs.utf8fromidentifier = _NPN_UTF8FromIdentifier;
        browserFuncs.intfromidentifier = _NPN_IntFromIdentifier;
        browserFuncs.createobject = _NPN_CreateObject;
        browserFuncs.retainobject = _NPN_RetainObject;
        browserFuncs.releaseobject = _NPN_ReleaseObject;
        browserFuncs.hasmethod = _NPN_HasMethod;
        browserFuncs.invoke = _NPN_Invoke;
        browserFuncs.invokeDefault = _NPN_InvokeDefault;
        browserFuncs.evaluate = _NPN_Evaluate;
        browserFuncs.hasproperty = _NPN_HasProperty;
        browserFuncs.getproperty = _NPN_GetProperty;
        browserFuncs.setproperty = _NPN_SetProperty;
        browserFuncs.removeproperty = _NPN_RemoveProperty;
        browserFuncs.setexception = _NPN_SetException;
        browserFuncs.enumerate = _NPN_Enumerate;
        browserFuncs.construct = _NPN_Construct;
        
        [self _applyDjVuWorkaround];

#if !LOG_DISABLED
        CFAbsoluteTime initializeStart = CFAbsoluteTimeGetCurrent();
#endif
        LOG(Plugins, "%f NP_Initialize timing started", initializeStart);
        npErr = NP_Initialize(&browserFuncs);
        if (npErr != NPERR_NO_ERROR)
            return NO;
#if !LOG_DISABLED
        currentTime = CFAbsoluteTimeGetCurrent();
        duration = currentTime - initializeStart;
#endif
        LOG(Plugins, "%f NP_Initialize took %f seconds", currentTime, duration);

        pluginFuncs.size = sizeof(NPPluginFuncs);
        
        npErr = NP_GetEntryPoints(&pluginFuncs);
        if (npErr != NPERR_NO_ERROR)
            return NO;
        
        pluginSize = pluginFuncs.size;
        pluginVersion = pluginFuncs.version;
        
        if (pluginFuncs.javaClass)
            LOG(LiveConnect, "%@:  mach-o entry point for NPP_GetJavaClass = %p", [self name], pluginFuncs.javaClass);
        else
            LOG(LiveConnect, "%@:  no entry point for NPP_GetJavaClass", [self name]);

#ifdef SUPPORT_CFM
    }
#endif

#if !LOG_DISABLED
    currentTime = CFAbsoluteTimeGetCurrent();
    duration = currentTime - start;
#endif
    LOG(Plugins, "%f Total load time: %f seconds", currentTime, duration);

    return YES;
}

- (BOOL)load
{    
    if ([self _tryLoad])
        return [super load];

    [self _unloadWithShutdown:NO];
    return NO;
}

- (NPPluginFuncs *)pluginFuncs
{
    return &pluginFuncs;
}

- (void)wasRemovedFromPluginDatabase:(WebPluginDatabase *)database
{
    [super wasRemovedFromPluginDatabase:database];
    
    // Unload when removed from final plug-in database
    if ([pluginDatabases count] == 0)
        [self _unloadWithShutdown:YES];
}

- (void)open
{
    instanceCount++;
    
    // Handle the case where all instances close a plug-in package, but another
    // instance opens the package before it is unloaded (which only happens when
    // the plug-in database is refreshed)
    needsUnload = NO;
    
    if (!isLoaded) {
        // Should load when the first instance opens the plug-in package
        ASSERT(instanceCount == 1);
        [self load];
    }
}

- (void)close
{
    ASSERT(instanceCount > 0);
    instanceCount--;
    if (instanceCount == 0 && needsUnload)
        [self _unloadWithShutdown:YES];
}

@end

#ifdef SUPPORT_CFM

// function pointer converters

FunctionPointer functionPointerForTVector(TransitionVector tvp)
{
    const uint32_t temp[6] = {0x3D800000, 0x618C0000, 0x800C0000, 0x804C0004, 0x7C0903A6, 0x4E800420};
    uint32_t *newGlue = NULL;

    if (tvp != NULL) {
        newGlue = (uint32_t *)malloc(sizeof(temp));
        if (newGlue != NULL) {
            unsigned i;
            for (i = 0; i < 6; i++) newGlue[i] = temp[i];
            newGlue[0] |= ((uintptr_t)tvp >> 16);
            newGlue[1] |= ((uintptr_t)tvp & 0xFFFF);
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

#endif

@implementation WebNetscapePluginPackage (Internal)

- (void)_unloadWithShutdown:(BOOL)shutdown
{
    if (!isLoaded)
        return;
    
    LOG(Plugins, "Unloading %@...", name);

    // Cannot unload a plug-in package while an instance is still using it
    if (instanceCount > 0) {
        needsUnload = YES;
        return;
    }

    if (shutdown && NP_Shutdown)
        NP_Shutdown();

    if (resourceRef != -1)
        [self closeResourceFile:resourceRef];

#ifdef SUPPORT_CFM
    if (!isBundle)
        WebCloseConnection(&connID);
#endif

    LOG(Plugins, "Plugin Unloaded");
    isLoaded = NO;
}

@end
#endif
