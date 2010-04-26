/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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

#import <WebKit/WebBasePluginPackage.h>

#import <WebCore/WebCoreObjCExtras.h>
#import <WebKit/WebKitNSStringExtras.h>
#import <WebKit/WebNSObjectExtras.h>
#import <WebKit/WebNetscapePluginPackage.h>
#import <WebKit/WebPluginPackage.h>
#import <runtime/InitializeThreading.h>
#import <wtf/Assertions.h>
#import <wtf/Threading.h>
#import <wtf/Vector.h>

#import <WebKitSystemInterface.h>

#import "WebKitLogging.h"
#import "WebTypesInternal.h"

#import <mach-o/arch.h>
#import <mach-o/fat.h>
#import <mach-o/loader.h>


#define JavaCocoaPluginIdentifier   @"com.apple.JavaPluginCocoa"
#define JavaCarbonPluginIdentifier  @"com.apple.JavaAppletPlugin"
#define JavaCFMPluginFilename       @"Java Applet Plugin Enabler"

#define QuickTimeCarbonPluginIdentifier       @"com.apple.QuickTime Plugin.plugin"
#define QuickTimeCocoaPluginIdentifier        @"com.apple.quicktime.webplugin"

@interface NSArray (WebPluginExtensions)
- (NSArray *)_web_lowercaseStrings;
@end;

@implementation WebBasePluginPackage

+ (void)initialize
{
    JSC::initializeThreading();
    WTF::initializeMainThreadToProcessMainThread();
#ifndef BUILDING_ON_TIGER
    WebCoreObjCFinalizeOnMainThread(self);
#endif
}

+ (WebBasePluginPackage *)pluginWithPath:(NSString *)pluginPath
{
    
    WebBasePluginPackage *pluginPackage = [[WebPluginPackage alloc] initWithPath:pluginPath];

    if (!pluginPackage) {
#if ENABLE(NETSCAPE_PLUGIN_API)
        pluginPackage = [[WebNetscapePluginPackage alloc] initWithPath:pluginPath];
#else
        return nil;
#endif
    }

    return [pluginPackage autorelease];
}

+ (NSString *)preferredLocalizationName
{
    return WebCFAutorelease(WKCopyCFLocalizationPreferredName(NULL));
}

- (NSString *)pathByResolvingSymlinksAndAliasesInPath:(NSString *)thePath
{
    NSString *newPath = [thePath stringByResolvingSymlinksInPath];

    FSRef fref;
    OSStatus err;

    err = FSPathMakeRef((const UInt8 *)[thePath fileSystemRepresentation], &fref, NULL);
    if (err != noErr)
        return newPath;

    Boolean targetIsFolder;
    Boolean wasAliased;
    err = FSResolveAliasFileWithMountFlags(&fref, TRUE, &targetIsFolder, &wasAliased, kResolveAliasFileNoUI);
    if (err != noErr)
        return newPath;

    if (wasAliased) {
        CFURLRef URL = CFURLCreateFromFSRef(kCFAllocatorDefault, &fref);
        newPath = [(NSURL *)URL path];
        CFRelease(URL);
    }

    return newPath;
}

- (id)initWithPath:(NSString *)pluginPath
{
    if (!(self = [super init]))
        return nil;
        
    path = [[self pathByResolvingSymlinksAndAliasesInPath:pluginPath] retain];
    bundle = [[NSBundle alloc] initWithPath:path];
#ifndef __ppc__
    // 32-bit PowerPC is the only platform where non-bundled CFM plugins are supported
    if (!bundle) {
        [self release];
        return nil;
    }
#endif
    cfBundle = CFBundleCreate(NULL, (CFURLRef)[NSURL fileURLWithPath:path]);
    extensionToMIME = [[NSMutableDictionary alloc] init];
    
    return self;
}

- (BOOL)getPluginInfoFromBundleAndMIMEDictionary:(NSDictionary *)MIMETypes
{
    if (!bundle)
        return NO;
    
    if (!MIMETypes) {
        MIMETypes = [bundle objectForInfoDictionaryKey:WebPluginMIMETypesKey];
        if (!MIMETypes)
            return NO;
    }

    NSMutableDictionary *MIMEToExtensionsDictionary = [NSMutableDictionary dictionary];
    NSMutableDictionary *MIMEToDescriptionDictionary = [NSMutableDictionary dictionary];
    NSEnumerator *keyEnumerator = [MIMETypes keyEnumerator];
    NSDictionary *MIMEDictionary;
    NSString *MIME, *description;
    NSArray *extensions;

    while ((MIME = [keyEnumerator nextObject]) != nil) {
        MIMEDictionary = [MIMETypes objectForKey:MIME];
        
        // FIXME: Consider storing disabled MIME types.
        NSNumber *isEnabled = [MIMEDictionary objectForKey:WebPluginTypeEnabledKey];
        if (isEnabled && [isEnabled boolValue] == NO)
            continue;

        extensions = [[MIMEDictionary objectForKey:WebPluginExtensionsKey] _web_lowercaseStrings];
        if ([extensions count] == 0)
            extensions = [NSArray arrayWithObject:@""];

        MIME = [MIME lowercaseString];

        [MIMEToExtensionsDictionary setObject:extensions forKey:MIME];

        description = [MIMEDictionary objectForKey:WebPluginTypeDescriptionKey];
        if (!description)
            description = @"";

        [MIMEToDescriptionDictionary setObject:description forKey:MIME];
    }

    [self setMIMEToExtensionsDictionary:MIMEToExtensionsDictionary];
    [self setMIMEToDescriptionDictionary:MIMEToDescriptionDictionary];

    NSString *filename = [self filename];

    NSString *theName = [bundle objectForInfoDictionaryKey:WebPluginNameKey];
    if (!theName)
        theName = filename;
    [self setName:theName];

    description = [bundle objectForInfoDictionaryKey:WebPluginDescriptionKey];
    if (!description)
        description = filename;
    [self setPluginDescription:description];

    return YES;
}

- (void)unload
{
}

- (void)createPropertyListFile
{
    if ([self load] && BP_CreatePluginMIMETypesPreferences) {
        BP_CreatePluginMIMETypesPreferences();
        [self unload];
    }
}

- (NSDictionary *)pListForPath:(NSString *)pListPath createFile:(BOOL)createFile
{
    if (createFile)
        [self createPropertyListFile];
    
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
    if (!bundle)
        return NO;
    
    NSDictionary *MIMETypes = nil;
    NSString *pListFilename = [bundle objectForInfoDictionaryKey:WebPluginMIMETypesFilenameKey];
    
    // Check if the MIME types are claimed in a plist in the user's preferences directory.
    if (pListFilename) {
        NSString *pListPath = [NSString stringWithFormat:@"%@/Library/Preferences/%@", NSHomeDirectory(), pListFilename];
        NSDictionary *pList = [self pListForPath:pListPath createFile:NO];
        if (pList) {
            // If the plist isn't localized, have the plug-in recreate it in the preferred language.
            NSString *localizationName = [pList objectForKey:WebPluginLocalizationNameKey];
            if (![localizationName isEqualToString:[[self class] preferredLocalizationName]])
                pList = [self pListForPath:pListPath createFile:YES];
            MIMETypes = [pList objectForKey:WebPluginMIMETypesKey];
        } else
            // Plist doesn't exist, ask the plug-in to create it.
            MIMETypes = [[self pListForPath:pListPath createFile:YES] objectForKey:WebPluginMIMETypesKey];
    }
    
    // Pass the MIME dictionary to the superclass to parse it.
    return [self getPluginInfoFromBundleAndMIMEDictionary:MIMETypes];
}

- (BOOL)load
{
    if (bundle && !BP_CreatePluginMIMETypesPreferences)
        BP_CreatePluginMIMETypesPreferences = (BP_CreatePluginMIMETypesPreferencesFuncPtr)CFBundleGetFunctionPointerForName(cfBundle, CFSTR("BP_CreatePluginMIMETypesPreferences"));
    
    return YES;
}

- (void)dealloc
{
    ASSERT(!pluginDatabases || [pluginDatabases count] == 0);
    [pluginDatabases release];
    
    [name release];
    [path release];
    [pluginDescription release];

    [MIMEToDescription release];
    [MIMEToExtensions release];
    [extensionToMIME release];

    [bundle release];
    if (cfBundle)
        CFRelease(cfBundle);
    
    [super dealloc];
}

- (void)finalize
{
    ASSERT_MAIN_THREAD();
    ASSERT(!pluginDatabases || [pluginDatabases count] == 0);
    [pluginDatabases release];

    if (cfBundle)
        CFRelease(cfBundle);

    [super finalize];
}

- (NSString *)name
{
    return name;
}

- (NSString *)path
{
    return path;
}

- (NSString *)filename
{
    return [path lastPathComponent];
}

- (NSString *)pluginDescription
{
    return pluginDescription;
}

- (NSEnumerator *)extensionEnumerator
{
    return [extensionToMIME keyEnumerator];
}

- (NSEnumerator *)MIMETypeEnumerator
{
    return [MIMEToExtensions keyEnumerator];
}

- (NSString *)descriptionForMIMEType:(NSString *)MIMEType
{
    return [MIMEToDescription objectForKey:MIMEType];
}

- (NSString *)MIMETypeForExtension:(NSString *)extension
{
    return [extensionToMIME objectForKey:extension];
}

- (NSArray *)extensionsForMIMEType:(NSString *)MIMEType
{
    return [MIMEToExtensions objectForKey:MIMEType];
}

- (NSBundle *)bundle
{
    return bundle;
}

- (void)setName:(NSString *)theName
{
    [name release];
    name = [theName retain];
}

- (void)setPath:(NSString *)thePath
{
    [path release];
    path = [thePath retain];
}

- (void)setPluginDescription:(NSString *)description
{
    [pluginDescription release];
    pluginDescription = [description retain];
}

- (void)setMIMEToDescriptionDictionary:(NSDictionary *)MIMEToDescriptionDictionary
{
    [MIMEToDescription release];
    MIMEToDescription = [MIMEToDescriptionDictionary retain];
}

- (void)setMIMEToExtensionsDictionary:(NSDictionary *)MIMEToExtensionsDictionary
{
    [MIMEToExtensions release];
    MIMEToExtensions = [MIMEToExtensionsDictionary retain];

    // Reverse the mapping
    [extensionToMIME removeAllObjects];

    NSEnumerator *MIMEEnumerator = [MIMEToExtensions keyEnumerator], *extensionEnumerator;
    NSString *MIME, *extension;
    NSArray *extensions;
    
    while ((MIME = [MIMEEnumerator nextObject]) != nil) {
        extensions = [MIMEToExtensions objectForKey:MIME];
        extensionEnumerator = [extensions objectEnumerator];

        while ((extension = [extensionEnumerator nextObject]) != nil) {
            if (![extension isEqualToString:@""])
                [extensionToMIME setObject:MIME forKey:extension];
        }
    }
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"name: %@\npath: %@\nmimeTypes:\n%@\npluginDescription:%@",
        name, path, [MIMEToExtensions description], [MIMEToDescription description], pluginDescription];
}

- (BOOL)isQuickTimePlugIn
{
    NSString *bundleIdentifier = [[self bundle] bundleIdentifier];
    return [bundleIdentifier _webkit_isCaseInsensitiveEqualToString:QuickTimeCarbonPluginIdentifier] || 
        [bundleIdentifier _webkit_isCaseInsensitiveEqualToString:QuickTimeCocoaPluginIdentifier];
}

- (BOOL)isJavaPlugIn
{
    NSString *bundleIdentifier = [[self bundle] bundleIdentifier];
    return [bundleIdentifier _webkit_isCaseInsensitiveEqualToString:JavaCocoaPluginIdentifier] || 
        [bundleIdentifier _webkit_isCaseInsensitiveEqualToString:JavaCarbonPluginIdentifier] ||
        [[path lastPathComponent] _webkit_isCaseInsensitiveEqualToString:JavaCFMPluginFilename];
}

static inline void swapIntsInHeader(uint8_t* bytes, unsigned length)
{
    for (unsigned i = 0; i < length; i += 4) 
        *(uint32_t*)(bytes + i) = OSSwapInt32(*(uint32_t *)(bytes + i));
}

- (BOOL)isNativeLibraryData:(NSData *)data
{
    Vector<uint8_t, 512> bytes([data length]);
    memcpy(bytes.data(), [data bytes], bytes.size());
    
    unsigned numArchs = 0;
    struct fat_arch singleArch = { 0, 0, 0, 0, 0 };
    struct fat_arch* archs = 0;
       
    if (bytes.size() >= sizeof(struct mach_header_64)) {
        uint32_t magic = *reinterpret_cast<uint32_t*>(bytes.data());
        
        if (magic == MH_MAGIC || magic == MH_CIGAM) {
            // We have a 32-bit thin binary
            struct mach_header* header = (struct mach_header*)bytes.data();

            // Check if we need to swap the bytes
            if (magic == MH_CIGAM)
                swapIntsInHeader(bytes.data(), bytes.size());
    
            singleArch.cputype = header->cputype;
            singleArch.cpusubtype = header->cpusubtype;

            archs = &singleArch;
            numArchs = 1;
        } else if (magic == MH_MAGIC_64 || magic == MH_CIGAM_64) {
            // We have a 64-bit thin binary
            struct mach_header_64* header = (struct mach_header_64*)bytes.data();

            // Check if we need to swap the bytes
            if (magic == MH_CIGAM_64)
                swapIntsInHeader(bytes.data(), bytes.size());
            
            singleArch.cputype = header->cputype;
            singleArch.cpusubtype = header->cpusubtype;
            
            archs = &singleArch;
            numArchs = 1;
        } else if (magic == FAT_MAGIC || magic == FAT_CIGAM) {
            // We have a fat (universal) binary

            // Check if we need to swap the bytes
            if (magic == FAT_CIGAM)
                swapIntsInHeader(bytes.data(), bytes.size());
            
            archs = (struct fat_arch*)(bytes.data() + sizeof(struct fat_header));            
            numArchs = ((struct fat_header *)bytes.data())->nfat_arch;
            
            unsigned maxArchs = (bytes.size() - sizeof(struct fat_header)) / sizeof(struct fat_arch);
            if (numArchs > maxArchs)
                numArchs = maxArchs;
        }            
    }
    
    if (!archs || !numArchs)
        return NO;
    
    const NXArchInfo* localArch = NXGetLocalArchInfo();
    if (!localArch)
        return NO;
    
    cpu_type_t cputype = localArch->cputype;
    cpu_subtype_t cpusubtype = localArch->cpusubtype;
    
#ifdef __x86_64__
    // NXGetLocalArchInfo returns CPU_TYPE_X86 even when running in 64-bit. 
    // See <rdar://problem/4996965> for more information.
    cputype = CPU_TYPE_X86_64;
#endif
    
    return NXFindBestFatArch(cputype, cpusubtype, archs, numArchs) != 0;
}

- (UInt32)versionNumber
{
    // CFBundleGetVersionNumber doesn't work with all possible versioning schemes, but we think for now it's good enough for us.
    return CFBundleGetVersionNumber(cfBundle);
}

- (void)wasAddedToPluginDatabase:(WebPluginDatabase *)database
{    
    if (!pluginDatabases)
        pluginDatabases = [[NSMutableSet alloc] init];
        
    ASSERT(![pluginDatabases containsObject:database]);
    [pluginDatabases addObject:database];
}

- (void)wasRemovedFromPluginDatabase:(WebPluginDatabase *)database
{
    ASSERT(pluginDatabases);
    ASSERT([pluginDatabases containsObject:database]);

    [pluginDatabases removeObject:database];
}

@end

@implementation NSArray (WebPluginExtensions)

- (NSArray *)_web_lowercaseStrings
{
    NSMutableArray *lowercaseStrings = [NSMutableArray arrayWithCapacity:[self count]];
    NSEnumerator *strings = [self objectEnumerator];
    NSString *string;

    while ((string = [strings nextObject]) != nil) {
        if ([string isKindOfClass:[NSString class]])
            [lowercaseStrings addObject:[string lowercaseString]];
    }

    return lowercaseStrings;
}

@end
