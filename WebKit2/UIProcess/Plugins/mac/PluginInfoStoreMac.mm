/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "PluginInfoStore.h"

#include <wtf/HashSet.h>
#include <wtf/RetainPtr.h>

using namespace WebCore;

namespace WebKit {

Vector<String> PluginInfoStore::pluginDirectories()
{
    Vector<String> pluginDirectories;

    pluginDirectories.append([NSHomeDirectory() stringByAppendingPathComponent:@"Library/Internet Plug-Ins"]);
    pluginDirectories.append("/Library/Internet Plug-Ins");
    
    return pluginDirectories;
}

Vector<String> PluginInfoStore::pluginPathsInDirectory(const String& directory)
{
    Vector<String> pluginPaths;

    NSString *directoryNSString = directory;
    NSArray *filenames = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:directory error:nil];
    for (NSString *filename in filenames)
        pluginPaths.append([directoryNSString stringByAppendingPathComponent:filename]);
    
    return pluginPaths;
}

static bool getPluginArchitecture(CFBundleRef bundle, cpu_type_t& pluginArchitecture)
{
    RetainPtr<CFArrayRef> pluginArchitecturesArray(AdoptCF, CFBundleCopyExecutableArchitectures(bundle));
    if (!pluginArchitecturesArray)
        return false;

    // Turn the array into a set.
    HashSet<unsigned> architectures;
    for (CFIndex i = 0, numPluginArchitectures = CFArrayGetCount(pluginArchitecturesArray.get()); i < numPluginArchitectures; ++i) {
        CFNumberRef number = static_cast<CFNumberRef>(CFArrayGetValueAtIndex(pluginArchitecturesArray.get(), i));
        
        SInt32 architecture;
        if (!CFNumberGetValue(number, kCFNumberSInt32Type, &architecture))
            continue;
        architectures.add(architecture);
    }
    
#ifdef __x86_64__
    // We only support 64-bit Intel plug-ins on 64-bit Intel.
    if (architectures.contains(kCFBundleExecutableArchitectureX86_64)) {
        pluginArchitecture = CPU_TYPE_X86_64;
        return true;
    }

    // We also support 32-bit Intel plug-ins on 64-bit Intel.
    if (architectures.contains(kCFBundleExecutableArchitectureI386)) {
        pluginArchitecture = CPU_TYPE_X86;
        return true;
    }
#elif defined(__i386__)
    // We only support 32-bit Intel plug-ins on 32-bit Intel.
    if (architectures.contains(kCFBundleExecutableArchitectureI386)) {
        pluginArchitecture = CPU_TYPE_X86;
        return true;
    }
#elif defined(__ppc64__)
    // We only support 64-bit PPC plug-ins on 64-bit PPC.
    if (architectures.contains(kCFBundleExecutableArchitecturePPC64)) {
        pluginArchitecture = CPU_TYPE_POWERPC64;
        return true;
    }
#elif defined(__ppc__)
    // We only support 32-bit PPC plug-ins on 32-bit PPC.
    if (architectures.contains(kCFBundleExecutableArchitecturePPC)) {
        pluginArchitecture = CPU_TYPE_POWERPC;
        return true;
    }
#else
#error "Unhandled architecture"
#endif

    return false;
}

static bool getPluginInfoFromPropertyLists(CFBundleRef bundle, PluginInfo& pluginInfo)
{
    // FIXME: Handle WebPluginMIMETypesFilenameKey.
    
    CFDictionaryRef mimeTypes = static_cast<CFDictionaryRef>(CFBundleGetValueForInfoDictionaryKey(bundle, CFSTR("WebPluginMIMETypes")));
    if (!mimeTypes || CFGetTypeID(mimeTypes) != CFDictionaryGetTypeID())
        return false;

    // Get the plug-in name.
    CFStringRef pluginName = static_cast<CFStringRef>(CFBundleGetValueForInfoDictionaryKey(bundle, CFSTR("WebPluginName")));
    if (pluginName && CFGetTypeID(pluginName) == CFStringGetTypeID())
        pluginInfo.name = pluginName;
    
    // Get the plug-in description.
    CFStringRef pluginDescription = static_cast<CFStringRef>(CFBundleGetValueForInfoDictionaryKey(bundle, CFSTR("WebPluginDescription")));
    if (pluginDescription && CFGetTypeID(pluginDescription) == CFStringGetTypeID())
        pluginInfo.desc = pluginDescription;
    
    // Get the MIME type mapping dictionary.
    CFIndex numMimeTypes = CFDictionaryGetCount(mimeTypes);
    Vector<CFStringRef> mimeTypesVector(numMimeTypes);
    Vector<CFDictionaryRef> mimeTypeInfoVector(numMimeTypes);
    CFDictionaryGetKeysAndValues(mimeTypes, reinterpret_cast<const void**>(mimeTypesVector.data()), reinterpret_cast<const void**>(mimeTypeInfoVector.data()));
    
    for (CFIndex i = 0; i < numMimeTypes; ++i) {
        MimeClassInfo mimeClassInfo;
        
        // If this MIME type is invalid, ignore it.
        CFStringRef mimeType = mimeTypesVector[i];
        if (!mimeType || CFGetTypeID(mimeType) != CFStringGetTypeID() || CFStringGetLength(mimeType) == 0)
            continue;

        // If this MIME type doesn't have a valid info dictionary, ignore it.
        CFDictionaryRef mimeTypeInfo = mimeTypeInfoVector[i];
        if (!mimeTypeInfo || CFGetTypeID(mimeTypeInfo) != CFDictionaryGetTypeID())
            continue;

        // Get the MIME type description.
        CFStringRef mimeTypeDescription = static_cast<CFStringRef>(CFDictionaryGetValue(mimeTypeInfo, CFSTR("WebPluginTypeDescription")));
        if (mimeTypeDescription && CFGetTypeID(mimeTypeDescription) != CFStringGetTypeID())
            mimeTypeDescription = 0;

        mimeClassInfo.type = String(mimeType).lower();
        mimeClassInfo.desc = mimeTypeDescription;

        // Now get the extensions for this MIME type.
        CFIndex numExtensions = 0;
        CFArrayRef extensionsArray = static_cast<CFArrayRef>(CFDictionaryGetValue(mimeTypeInfo, CFSTR("WebPluginExtensions")));
        if (extensionsArray && CFGetTypeID(extensionsArray) == CFArrayGetTypeID())
            numExtensions = CFArrayGetCount(extensionsArray);

        for (CFIndex i = 0; i < numExtensions; ++i) {
            CFStringRef extension = static_cast<CFStringRef>(CFArrayGetValueAtIndex(extensionsArray, i));
            if (!extension || CFGetTypeID(extension) != CFStringGetTypeID())
                continue;
            
            mimeClassInfo.extensions.append(String(extension).lower());
        }

        // Add this MIME type.
        pluginInfo.mimes.append(mimeClassInfo);
    }

    return true;    
}

bool PluginInfoStore::getPluginInfo(const WebCore::String& pluginPath, Plugin& plugin)
{
    RetainPtr<CFStringRef> bundlePath(AdoptCF, pluginPath.createCFString());
    RetainPtr<CFURLRef> bundleURL(AdoptCF, CFURLCreateWithFileSystemPath(kCFAllocatorDefault, bundlePath.get(), kCFURLPOSIXPathStyle, false));

    // Try to initialize the bundle.
    RetainPtr<CFBundleRef> bundle(AdoptCF, CFBundleCreate(kCFAllocatorDefault, bundleURL.get()));
    if (!bundle)
        return false;

    // Check if this bundle is an NPAPI plug-in.
    UInt32 packageType = 0;
    CFBundleGetPackageInfo(bundle.get(), &packageType, 0);
    if (packageType != FOUR_CHAR_CODE('BRPL'))
        return false;
    
    // Check that the architecture is valid.
    cpu_type_t pluginArchitecture = 0;
    if (!getPluginArchitecture(bundle.get(), pluginArchitecture))
        return false;

    // Check that there's valid info for this plug-in.
    if (!getPluginInfoFromPropertyLists(bundle.get(), plugin.info))
        return false;

    plugin.path = pluginPath;
    plugin.pluginArchitecture = pluginArchitecture;
    plugin.versionNumber = CFBundleGetVersionNumber(bundle.get());

    RetainPtr<CFStringRef> filename(AdoptCF, CFURLCopyLastPathComponent(bundleURL.get()));
    plugin.info.file = filename.get();
    
    if (plugin.info.name.isNull())
        plugin.info.name = plugin.info.file;
    if (plugin.info.desc.isNull())
        plugin.info.desc = plugin.info.file;
    
    return true;
}

bool PluginInfoStore::shouldUsePlugin(const Plugin& plugin, const Vector<Plugin>& loadedPlugins)
{
    // FIXME: Implement
    return true;
}

} // namespace WebKit
