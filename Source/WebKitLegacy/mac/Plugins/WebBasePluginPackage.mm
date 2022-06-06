/*
 * Copyright (C) 2005 Apple Inc.  All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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

#import "WebBasePluginPackage.h"

#import "WebKitLogging.h"
#import "WebKitNSStringExtras.h"
#import "WebPluginPackage.h"
#import <JavaScriptCore/InitializeThreading.h>
#import <WebCore/WebCoreJITOperations.h>
#import <algorithm>
#import <mach-o/arch.h>
#import <mach-o/fat.h>
#import <mach-o/loader.h>
#import <wtf/Assertions.h>
#import <wtf/MainThread.h>
#import <wtf/RunLoop.h>
#import <wtf/Vector.h>
#import <wtf/text/CString.h>

static constexpr auto JavaCocoaPluginIdentifier ="com.apple.JavaPluginCocoa"_s;
static constexpr auto JavaCarbonPluginIdentifier = "com.apple.JavaAppletPlugin"_s;

static constexpr auto QuickTimeCocoaPluginIdentifier = "com.apple.quicktime.webplugin"_s;

@interface NSArray (WebPluginExtensions)
- (NSArray *)_web_lowercaseStrings;
@end;

@implementation WebBasePluginPackage

+ (void)initialize
{
#if !PLATFORM(IOS_FAMILY)
    JSC::initialize();
    WTF::initializeMainThread();
    WebCore::populateJITOperations();
#endif
}

+ (WebBasePluginPackage *)pluginWithPath:(NSString *)pluginPath
{
    return adoptNS([[WebPluginPackage alloc] initWithPath:pluginPath]).autorelease();
}

- (id)initWithPath:(NSString *)pluginPath
{
    if (!(self = [super init]))
        return nil;
    
    path = [pluginPath stringByResolvingSymlinksInPath];
    cfBundle = adoptCF(CFBundleCreate(kCFAllocatorDefault, (CFURLRef)[NSURL fileURLWithPath:path]));

    if (!cfBundle) {
        [self release];
        return nil;
    }

    return self;
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
    if (data)
        pList = [NSPropertyListSerialization propertyListWithData:data options:kCFPropertyListImmutable format:nil error:nil];
    
    return pList;
}

- (id)_objectForInfoDictionaryKey:(NSString *)key
{
    CFDictionaryRef bundleInfoDictionary = CFBundleGetInfoDictionary(cfBundle.get());
    if (!bundleInfoDictionary)
        return nil;

    return (__bridge id)CFDictionaryGetValue(bundleInfoDictionary, (__bridge CFStringRef)key);
}

- (BOOL)getPluginInfoFromPLists
{
    if (!cfBundle)
        return NO;
    
    NSDictionary *MIMETypes = [self _objectForInfoDictionaryKey:WebPluginMIMETypesKey];
    if (!MIMETypes)
        return NO;

    NSEnumerator *keyEnumerator = [MIMETypes keyEnumerator];
    NSDictionary *MIMEDictionary;
    NSString *MIME;

    while ((MIME = [keyEnumerator nextObject]) != nil) {
        MIMEDictionary = [MIMETypes objectForKey:MIME];
        
        // FIXME: Consider storing disabled MIME types.
        NSNumber *isEnabled = [MIMEDictionary objectForKey:WebPluginTypeEnabledKey];
        if (isEnabled && [isEnabled boolValue] == NO)
            continue;

        WebCore::MimeClassInfo mimeClassInfo;
        
        NSArray *extensions = [[MIMEDictionary objectForKey:WebPluginExtensionsKey] _web_lowercaseStrings];
        for (NSString *extension in extensions) {
            // The DivX plug-in lists multiple extensions in a comma separated string instead of using
            // multiple array elements in the property list. Work around this here by splitting the
            // extension string into components.
            for (NSString *component in [extension componentsSeparatedByString:@","])
                mimeClassInfo.extensions.append(component);
        }

        mimeClassInfo.type = AtomString { String(MIME).convertToASCIILowercase() };
        mimeClassInfo.desc = [MIMEDictionary objectForKey:WebPluginTypeDescriptionKey];

        pluginInfo.mimes.append(mimeClassInfo);
    }

    NSString *filename = [(NSString *)path lastPathComponent];
    pluginInfo.file = filename;

    NSString *theName = [self _objectForInfoDictionaryKey:WebPluginNameKey];
    if (!theName)
        theName = filename;
    pluginInfo.name = theName;

    NSString *description = [self _objectForInfoDictionaryKey:WebPluginDescriptionKey];
    if (!description)
        description = filename;
    pluginInfo.desc = description;

    pluginInfo.isApplicationPlugin = false;
    pluginInfo.clientLoadPolicy = WebCore::PluginLoadClientPolicy::Undefined;
#if PLATFORM(MAC)
    pluginInfo.bundleIdentifier = self.bundleIdentifier;
    pluginInfo.versionString = self.bundleVersion;
#endif

    return YES;
}

- (BOOL)load
{
    if (cfBundle && !BP_CreatePluginMIMETypesPreferences)
        BP_CreatePluginMIMETypesPreferences = (BP_CreatePluginMIMETypesPreferencesFuncPtr)CFBundleGetFunctionPointerForName(cfBundle.get(), CFSTR("BP_CreatePluginMIMETypesPreferences"));
    
    return YES;
}

- (void)dealloc
{
    ASSERT(!pluginDatabases || [pluginDatabases count] == 0);
    [pluginDatabases release];
    
    [super dealloc];
}

- (const String&)path
{
    return path;
}

- (const WebCore::PluginInfo&)pluginInfo
{
    return pluginInfo;
}

- (BOOL)supportsExtension:(const String&)extension
{
    ASSERT(extension.convertToASCIILowercase() == extension);
    
    for (auto& entry : pluginInfo.mimes) {
        if (entry.extensions.contains(extension))
            return YES;
    }

    return NO;
}

- (BOOL)supportsMIMEType:(const WTF::String&)mimeType
{
    ASSERT(mimeType.convertToASCIILowercase() == mimeType);
    
    for (auto& entry : pluginInfo.mimes) {
        if (entry.type == mimeType)
            return YES;
    }

    return NO;
}

- (NSString *)MIMETypeForExtension:(const String&)extension
{
    ASSERT(extension.convertToASCIILowercase() == extension);
    
    for (auto& entry : pluginInfo.mimes) {
        if (entry.extensions.contains(extension))
            return entry.type;
    }

    return nil;
}

- (BOOL)isQuickTimePlugIn
{
    const String& bundleIdentifier = [self bundleIdentifier];
    return bundleIdentifier == QuickTimeCocoaPluginIdentifier;
}

- (BOOL)isJavaPlugIn
{
    const String& bundleIdentifier = [self bundleIdentifier];
    return bundleIdentifier == JavaCocoaPluginIdentifier || bundleIdentifier == JavaCarbonPluginIdentifier;
}

static inline void swapIntsInHeader(uint32_t* rawData, size_t length)
{
    for (size_t i = 0; i < length; ++i) 
        rawData[i] = OSSwapInt32(rawData[i]);
}

- (BOOL)isNativeLibraryData:(NSData *)data
{
    NSUInteger sizeInBytes = [data length];
    Vector<uint32_t, 128> rawData((sizeInBytes + 3) / 4);
    memcpy(rawData.data(), [data bytes], sizeInBytes);
    
    unsigned numArchs = 0;
    struct fat_arch singleArch = { 0, 0, 0, 0, 0 };
    struct fat_arch* archs = 0;
       
    if (sizeInBytes >= sizeof(struct mach_header_64)) {
        uint32_t magic = *rawData.data();
        
        if (magic == MH_MAGIC || magic == MH_CIGAM) {
            // We have a 32-bit thin binary
            struct mach_header* header = (struct mach_header*)rawData.data();

            // Check if we need to swap the bytes
            if (magic == MH_CIGAM)
                swapIntsInHeader(rawData.data(), rawData.size());
    
            singleArch.cputype = header->cputype;
            singleArch.cpusubtype = header->cpusubtype;

            archs = &singleArch;
            numArchs = 1;
        } else if (magic == MH_MAGIC_64 || magic == MH_CIGAM_64) {
            // We have a 64-bit thin binary
            struct mach_header_64* header = (struct mach_header_64*)rawData.data();

            // Check if we need to swap the bytes
            if (magic == MH_CIGAM_64)
                swapIntsInHeader(rawData.data(), rawData.size());
            
            singleArch.cputype = header->cputype;
            singleArch.cpusubtype = header->cpusubtype;
            
            archs = &singleArch;
            numArchs = 1;
        } else if (magic == FAT_MAGIC || magic == FAT_CIGAM) {
            // We have a fat (universal) binary

            // Check if we need to swap the bytes
            if (magic == FAT_CIGAM)
                swapIntsInHeader(rawData.data(), rawData.size());
            
            static_assert(sizeof(struct fat_header) % sizeof(uint32_t) == 0, "struct fat header must be integral size of uint32_t");
            archs = reinterpret_cast<struct fat_arch*>(rawData.data() + sizeof(struct fat_header) / sizeof(uint32_t));
            numArchs = reinterpret_cast<struct fat_header*>(rawData.data())->nfat_arch;
            
            unsigned maxArchs = (sizeInBytes - sizeof(struct fat_header)) / sizeof(struct fat_arch);
            if (numArchs > maxArchs)
                numArchs = maxArchs;
        }            
    }
    
    if (!archs || !numArchs)
        return NO;
    
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    const NXArchInfo* localArch = NXGetLocalArchInfo();
ALLOW_DEPRECATED_DECLARATIONS_END
    if (!localArch)
        return NO;
    
    cpu_type_t cputype = localArch->cputype;
    cpu_subtype_t cpusubtype = localArch->cpusubtype;
    
#ifdef __x86_64__
    // NXGetLocalArchInfo returns CPU_TYPE_X86 even when running in 64-bit. 
    // See <rdar://problem/4996965> for more information.
    cputype = CPU_TYPE_X86_64;
#endif
    
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    return NXFindBestFatArch(cputype, cpusubtype, archs, numArchs) != 0;
ALLOW_DEPRECATED_DECLARATIONS_END
}

- (UInt32)versionNumber
{
    // CFBundleGetVersionNumber doesn't work with all possible versioning schemes, but we think for now it's good enough for us.
    return CFBundleGetVersionNumber(cfBundle.get());
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

- (String)bundleIdentifier
{
    return CFBundleGetIdentifier(cfBundle.get());
}

- (String)bundleVersion
{
    auto infoDictionary = CFBundleGetInfoDictionary(cfBundle.get());
    if (!infoDictionary)
        return String();

    return dynamic_cf_cast<CFStringRef>(CFDictionaryGetValue(infoDictionary, kCFBundleVersionKey));
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
