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

#include "WebKitSystemInterface.h"
#include <WebCore/WebCoreNSStringExtras.h>
#include <wtf/HashSet.h>
#include <wtf/RetainPtr.h>

using namespace WebCore;

namespace WebKit {

Vector<String> PluginInfoStore::pluginsDirectories()
{
    Vector<String> pluginsDirectories;

    pluginsDirectories.append([NSHomeDirectory() stringByAppendingPathComponent:@"Library/Internet Plug-Ins"]);
    pluginsDirectories.append("/Library/Internet Plug-Ins");
    
    return pluginsDirectories;
}

// FIXME: Once the UI process knows the difference between the main thread and the web thread we can drop this and just use
// String::createCFString.
static CFStringRef safeCreateCFString(const String& string)
{
    return CFStringCreateWithCharacters(0, reinterpret_cast<const UniChar*>(string.characters()), string.length());
}
    
Vector<String> PluginInfoStore::pluginPathsInDirectory(const String& directory)
{
    Vector<String> pluginPaths;

    RetainPtr<CFStringRef> directoryCFString(AdoptCF, safeCreateCFString(directory));
    
    NSArray *filenames = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:(NSString *)directoryCFString.get() error:nil];
    for (NSString *filename in filenames)
        pluginPaths.append([(NSString *)directoryCFString.get() stringByAppendingPathComponent:filename]);
    
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

class ResourceMap {
public:
    explicit ResourceMap(CFBundleRef bundle)
        : m_bundle(bundle)
        , m_currentResourceFile(CurResFile())
        , m_bundleResourceMap(CFBundleOpenBundleResourceMap(m_bundle))
    {
        UseResFile(m_bundleResourceMap);
    }

    ~ResourceMap()
    {
        // Close the resource map.
        CFBundleCloseBundleResourceMap(m_bundle, m_bundleResourceMap);
        
        // And restore the old resource.
        UseResFile(m_currentResourceFile);
    }

    bool isValid() const { return m_bundleResourceMap != -1; }

private:
    CFBundleRef m_bundle;
    ResFileRefNum m_currentResourceFile;
    ResFileRefNum m_bundleResourceMap;
};

static bool getStringListResource(ResID resourceID, Vector<String>& stringList) {
    Handle stringListHandle = Get1Resource('STR#', resourceID);
    if (!stringListHandle || !*stringListHandle)
        return false;

    // Get the string list size.
    Size stringListSize = GetHandleSize(stringListHandle);
    if (stringListSize < static_cast<Size>(sizeof(UInt16)))
        return false;
  
    CFStringEncoding stringEncoding = stringEncodingForResource(stringListHandle);

    unsigned char* ptr = reinterpret_cast<unsigned char*>(*stringListHandle);
    unsigned char* end = ptr + stringListSize;
    
    // Get the number of strings in the string list.
    UInt16 numStrings = *reinterpret_cast<UInt16*>(ptr);
    ptr += sizeof(UInt16);
                  
    for (UInt16 i = 0; i < numStrings; ++i) {
        // We're past the end of the string, bail.
        if (ptr >= end)
            return false;

        // Get the string length.
        unsigned char stringLength = *ptr++;

        RetainPtr<CFStringRef> cfString(AdoptCF, CFStringCreateWithBytesNoCopy(kCFAllocatorDefault, ptr, stringLength, stringEncoding, false, kCFAllocatorNull));
        if (!cfString.get())
            return false;

        stringList.append(cfString.get());
        ptr += stringLength;
    }

    if (ptr != end)
        return false;

    return true;
}

static const ResID PluginNameOrDescriptionStringNumber = 126;
static const ResID MIMEDescriptionStringNumber = 127;
static const ResID MIMEListStringStringNumber = 128;

static bool getPluginInfoFromCarbonResources(CFBundleRef bundle, PluginInfo& pluginInfo)
{
    ResourceMap resourceMap(bundle);
    if (!resourceMap.isValid())
        return false;

    // Get the name and description string list.
    Vector<String> nameAndDescription;
    if (!getStringListResource(PluginNameOrDescriptionStringNumber, nameAndDescription))
        return false;

    // Get the MIME types and extensions string list. This list needs to be a multiple of two.
    Vector<String> mimeTypesAndExtensions;
    if (!getStringListResource(MIMEListStringStringNumber, mimeTypesAndExtensions))
        return false;

    if (mimeTypesAndExtensions.size() % 2)
        return false;

    size_t numMimeTypes = mimeTypesAndExtensions.size() / 2;
    
    // Now get the MIME type descriptions string list. This string list needs to be the same length as the number of MIME types.
    Vector<String> mimeTypeDescriptions;
    if (!getStringListResource(MIMEDescriptionStringNumber, mimeTypeDescriptions))
        return false;

    if (mimeTypeDescriptions.size() != numMimeTypes)
        return false;

    // Add all MIME types.
    for (size_t i = 0; i < mimeTypesAndExtensions.size() / 2; ++i) {
        MimeClassInfo mimeClassInfo;
        
        const String& mimeType = mimeTypesAndExtensions[i * 2];
        const String& description = mimeTypeDescriptions[i];
        
        mimeClassInfo.type = mimeType.lower();
        mimeClassInfo.desc = description;
        
        Vector<String> extensions;
        mimeTypesAndExtensions[i * 2 + 1].split(',', extensions);
        
        for (size_t i = 0; i < extensions.size(); ++i)
            mimeClassInfo.extensions.append(extensions[i].lower());

        pluginInfo.mimes.append(mimeClassInfo);
    }

    // Set the name and description if they exist.
    if (nameAndDescription.size() > 0)
        pluginInfo.name = nameAndDescription[0];
    if (nameAndDescription.size() > 1)
        pluginInfo.desc = nameAndDescription[1];

    return true;
}

bool PluginInfoStore::getPluginInfo(const String& pluginPath, Plugin& plugin)
{
    RetainPtr<CFStringRef> bundlePath(AdoptCF, safeCreateCFString(pluginPath));
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
    if (!getPluginInfoFromPropertyLists(bundle.get(), plugin.info) &&
        !getPluginInfoFromCarbonResources(bundle.get(), plugin.info))
        return false;

    plugin.path = pluginPath;
    plugin.pluginArchitecture = pluginArchitecture;
    plugin.bundleIdentifier = CFBundleGetIdentifier(bundle.get());
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
    for (size_t i = 0; i < loadedPlugins.size(); ++i) {
        const Plugin& loadedPlugin = loadedPlugins[i];

        // If a plug-in with the same bundle identifier already exists, we don't want to load it.
        if (loadedPlugin.bundleIdentifier == plugin.bundleIdentifier)
            return false;
    }

    return true;
}

String PluginInfoStore::getMIMETypeForExtension(const String& extension)
{
    // FIXME: This should just call MIMETypeRegistry::getMIMETypeForExtension and be
    // strength reduced into the callsite once we can safely convert String
    // to CFStringRef off the main thread.

    RetainPtr<CFStringRef> extensionCFString(AdoptCF, safeCreateCFString(extension));
    return WKGetMIMETypeForExtension((NSString *)extensionCFString.get());
}

} // namespace WebKit
