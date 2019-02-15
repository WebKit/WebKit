/*
 * Copyright (C) 2010-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#import "config.h"
#import "PluginProcess.h"

#if ENABLE(NETSCAPE_PLUGIN_API)

#import "ArgumentCoders.h"
#import "NetscapePlugin.h"
#import "PluginInfoStore.h"
#import "PluginProcessCreationParameters.h"
#import "PluginProcessProxyMessages.h"
#import "PluginProcessShim.h"
#import "PluginSandboxProfile.h"
#import "SandboxInitializationParameters.h"
#import "SandboxUtilities.h"
#import <CoreAudio/AudioHardware.h>
#import <WebCore/LocalizedStrings.h>
#import <WebCore/RuntimeEnabledFeatures.h>
#import <dlfcn.h>
#import <mach-o/dyld.h>
#import <mach-o/getsect.h>
#import <mach/mach_vm.h>
#import <mach/vm_statistics.h>
#import <objc/runtime.h>
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <pal/spi/cocoa/LaunchServicesSPI.h>
#import <pal/spi/mac/HIToolboxSPI.h>
#import <pal/spi/mac/NSApplicationSPI.h>
#import <pal/spi/mac/NSWindowSPI.h>
#import <sysexits.h>
#import <wtf/HashSet.h>
#import <wtf/NeverDestroyed.h>

const CFStringRef kLSPlugInBundleIdentifierKey = CFSTR("LSPlugInBundleIdentifierKey");

// These values were chosen to match default NSURLCache sizes at the time of this writing.
const NSUInteger pluginMemoryCacheSize = 512000;
const NSUInteger pluginDiskCacheSize = 20000000;

namespace WebKit {

class FullscreenWindowTracker {
    WTF_MAKE_NONCOPYABLE(FullscreenWindowTracker);

public:
    FullscreenWindowTracker() { }
    
    template<typename T> void windowShown(T window);
    template<typename T> void windowHidden(T window);

private:
    HashSet<CGWindowID> m_windows;
};

static bool rectCoversAnyScreen(NSRect rect)
{
    for (NSScreen *screen in [NSScreen screens]) {
        if (NSContainsRect(rect, [screen frame]))
            return YES;
    }
    return NO;
}

#ifndef NP_NO_CARBON
static bool windowCoversAnyScreen(WindowRef window)
{
    HIRect bounds;
    HIWindowGetBounds(window, kWindowStructureRgn, kHICoordSpaceScreenPixel, &bounds);

    // Convert to Cocoa-style screen coordinates that use a Y offset relative to the zeroth screen's origin.
    bounds.origin.y = NSHeight([(NSScreen *)[[NSScreen screens] objectAtIndex:0] frame]) - CGRectGetMaxY(bounds);

    return rectCoversAnyScreen(NSRectFromCGRect(bounds));
}

static CGWindowID cgWindowID(WindowRef window)
{
    return reinterpret_cast<CGWindowID>(GetNativeWindowFromWindowRef(window));
}

#endif

static bool windowCoversAnyScreen(NSWindow *window)
{
    return rectCoversAnyScreen(window.frame);
}

static CGWindowID cgWindowID(NSWindow *window)
{
    return window.windowNumber;
}

template<typename T>
void FullscreenWindowTracker::windowShown(T window)
{
    CGWindowID windowID = cgWindowID(window);
    if (!windowID)
        return;

    // If this window is already visible then there is nothing to do.
    if (m_windows.contains(windowID))
        return;
    
    // If the window is not full-screen then we're not interested in it.
    if (!windowCoversAnyScreen(window))
        return;

    bool windowSetWasEmpty = m_windows.isEmpty();

    m_windows.add(windowID);
    
    // If this is the first full screen window to be shown, notify the UI process.
    if (windowSetWasEmpty)
        PluginProcess::singleton().setFullscreenWindowIsShowing(true);
}

template<typename T>
void FullscreenWindowTracker::windowHidden(T window)
{
    CGWindowID windowID = cgWindowID(window);
    if (!windowID)
        return;

    // If this is not a window that we're tracking then there is nothing to do.
    if (!m_windows.remove(windowID))
        return;

    // If this was the last full screen window that was visible, notify the UI process.
    if (m_windows.isEmpty())
        PluginProcess::singleton().setFullscreenWindowIsShowing(false);
}

static FullscreenWindowTracker& fullscreenWindowTracker()
{
    static NeverDestroyed<FullscreenWindowTracker> fullscreenWindowTracker;
    return fullscreenWindowTracker;
}

#if defined(__i386__)

static bool shouldCallRealDebugger()
{
    static bool isUserbreakSet = false;
    static dispatch_once_t flag;
    dispatch_once(&flag, ^{
        char* var = getenv("USERBREAK");

        if (var)
            isUserbreakSet = atoi(var);
    });
    
    return isUserbreakSet;
}

static bool isWindowActive(WindowRef windowRef, bool& result)
{
#ifndef NP_NO_CARBON
    if (NetscapePlugin* plugin = NetscapePlugin::netscapePluginFromWindow(windowRef)) {
        result = plugin->isWindowActive();
        return true;
    }
#endif
    return false;
}

static UInt32 getCurrentEventButtonState()
{
#ifndef NP_NO_CARBON
    return NetscapePlugin::buttonState();
#else
    ASSERT_NOT_REACHED();
    return 0;
#endif
}

static void carbonWindowShown(WindowRef window)
{
#ifndef NP_NO_CARBON
    fullscreenWindowTracker().windowShown(window);
#endif
}

static void carbonWindowHidden(WindowRef window)
{
#ifndef NP_NO_CARBON
    fullscreenWindowTracker().windowHidden(window);
#endif
}

static bool openCFURLRef(CFURLRef url, int32_t& status, CFURLRef* launchedURL)
{
    String launchedURLString;
    if (!PluginProcess::singleton().openURL(URL(url).string(), status, launchedURLString))
        return false;

    if (!launchedURLString.isNull() && launchedURL)
        *launchedURL = URL(URL(), launchedURLString).createCFURL().leakRef();
    return true;
}

static bool isMallocTinyMemoryTag(int tag)
{
    switch (tag) {
    case VM_MEMORY_MALLOC_TINY:
        return true;

    default:
        return false;
    }
}

static bool shouldMapMallocMemoryExecutable;

static bool shouldMapMemoryExecutable(int flags)
{
    if (!shouldMapMallocMemoryExecutable)
        return false;

    if (!isMallocTinyMemoryTag((flags >> 24) & 0xff))
        return false;

    return true;
}

#endif

static void setModal(bool modalWindowIsShowing)
{
    PluginProcess::singleton().setModalWindowIsShowing(modalWindowIsShowing);
}

static unsigned modalCount;

static void beginModal()
{
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    // Make sure to make ourselves the front process
    ProcessSerialNumber psn;
    GetCurrentProcess(&psn);
    SetFrontProcess(&psn);
    ALLOW_DEPRECATED_DECLARATIONS_END

    if (!modalCount++)
        setModal(true);
}

static void endModal()
{
    if (!--modalCount)
        setModal(false);
}

static IMP NSApplication_RunModalForWindow;

static NSInteger replacedRunModalForWindow(id self, SEL _cmd, NSWindow* window)
{
    beginModal();
    NSInteger result = ((NSInteger (*)(id, SEL, NSWindow *))NSApplication_RunModalForWindow)(self, _cmd, window);
    endModal();

    return result;
}

static bool oldPluginProcessNameShouldEqualNewPluginProcessNameForAdobeReader;

static bool isAdobeAcrobatAddress(const void* address)
{
    Dl_info imageInfo;
    if (!dladdr(address, &imageInfo))
        return false;

    const char* pathSuffix = "/Contents/Frameworks/Acrobat.framework/Acrobat";

    int pathSuffixLength = strlen(pathSuffix);
    int imageFilePathLength = strlen(imageInfo.dli_fname);

    if (imageFilePathLength < pathSuffixLength)
        return false;

    if (strcmp(imageInfo.dli_fname + (imageFilePathLength - pathSuffixLength), pathSuffix))
        return false;

    return true;
}

static bool stringCompare(CFStringRef a, CFStringRef b, CFStringCompareFlags options, void* returnAddress, CFComparisonResult& result)
{
    if (pthread_main_np() != 1)
        return false;

    if (!oldPluginProcessNameShouldEqualNewPluginProcessNameForAdobeReader)
        return false;

    if (options != kCFCompareCaseInsensitive)
        return false;

    const char* aCString = CFStringGetCStringPtr(a, kCFStringEncodingASCII);
    if (!aCString)
        return false;

    const char* bCString = CFStringGetCStringPtr(b, kCFStringEncodingASCII);
    if (!bCString)
        return false;

    if (strcmp(aCString, "com.apple.WebKit.PluginProcess"))
        return false;

    if (strcmp(bCString, "com.apple.WebKit.Plugin.64"))
        return false;

    // Check if the LHS string comes from the Acrobat framework.
    if (!isAdobeAcrobatAddress(a))
        return false;

    // Check if the return adress is part of the Acrobat framework as well.
    if (!isAdobeAcrobatAddress(returnAddress))
        return false;

    result = kCFCompareEqualTo;
    return true;
}

static void initializeShim()
{
    // Initialize the shim for 32-bit only.
    const PluginProcessShimCallbacks callbacks = {
#if defined(__i386__)
        shouldCallRealDebugger,
        isWindowActive,
        getCurrentEventButtonState,
        beginModal,
        endModal,
        carbonWindowShown,
        carbonWindowHidden,
        setModal,
        openCFURLRef,
        shouldMapMemoryExecutable,
#endif
        stringCompare,
    };

    PluginProcessShimInitializeFunc initFunc = reinterpret_cast<PluginProcessShimInitializeFunc>(dlsym(RTLD_DEFAULT, "WebKitPluginProcessShimInitialize"));
    initFunc(callbacks);
}

static void (*NSConcreteTask_launch)(NSTask *, SEL);

static void replacedNSConcreteTask_launch(NSTask *self, SEL _cmd)
{
    String launchPath = self.launchPath;

    Vector<String> arguments;
    arguments.reserveInitialCapacity(self.arguments.count);
    for (NSString *argument in self.arguments)
        arguments.uncheckedAppend(argument);

    if (PluginProcess::singleton().launchProcess(launchPath, arguments))
        return;

    NSConcreteTask_launch(self, _cmd);
}

static NSRunningApplication *(*NSWorkspace_launchApplicationAtURL_options_configuration_error)(NSWorkspace *, SEL, NSURL *, NSWorkspaceLaunchOptions, NSDictionary *, NSError **);

static NSRunningApplication *replacedNSWorkspace_launchApplicationAtURL_options_configuration_error(NSWorkspace *self, SEL _cmd, NSURL *url, NSWorkspaceLaunchOptions options, NSDictionary *configuration, NSError **error)
{
    Vector<String> arguments;
    if (NSArray *argumentsArray = [configuration objectForKey:NSWorkspaceLaunchConfigurationArguments]) {
        if ([argumentsArray isKindOfClass:[NSArray array]]) {
            for (NSString *argument in argumentsArray) {
                if ([argument isKindOfClass:[NSString class]])
                    arguments.append(argument);
            }
        }
    }

    if (PluginProcess::singleton().launchApplicationAtURL(URL(url).string(), arguments)) {
        if (error)
            *error = nil;
        return nil;
    }

    return NSWorkspace_launchApplicationAtURL_options_configuration_error(self, _cmd, url, options, configuration, error);
}

static BOOL (*NSWorkspace_openFile)(NSWorkspace *, SEL, NSString *);

static BOOL replacedNSWorkspace_openFile(NSWorkspace *self, SEL _cmd, NSString *fullPath)
{
    if (PluginProcess::singleton().openFile(fullPath))
        return true;

    return NSWorkspace_openFile(self, _cmd, fullPath);
}

static void initializeCocoaOverrides()
{
    // Override -[NSConcreteTask launch:]
    Method launchMethod = class_getInstanceMethod(objc_getClass("NSConcreteTask"), @selector(launch));
    NSConcreteTask_launch = reinterpret_cast<void (*)(NSTask *, SEL)>(method_setImplementation(launchMethod, reinterpret_cast<IMP>(replacedNSConcreteTask_launch)));

    // Override -[NSWorkspace launchApplicationAtURL:options:configuration:error:]
    Method launchApplicationAtURLOptionsConfigurationErrorMethod = class_getInstanceMethod(objc_getClass("NSWorkspace"), @selector(launchApplicationAtURL:options:configuration:error:));
    NSWorkspace_launchApplicationAtURL_options_configuration_error = reinterpret_cast<NSRunningApplication *(*)(NSWorkspace *, SEL, NSURL *, NSWorkspaceLaunchOptions, NSDictionary *, NSError **)>(method_setImplementation(launchApplicationAtURLOptionsConfigurationErrorMethod, reinterpret_cast<IMP>(replacedNSWorkspace_launchApplicationAtURL_options_configuration_error)));

    // Override -[NSWorkspace openFile:]
    Method openFileMethod = class_getInstanceMethod(objc_getClass("NSWorkspace"), @selector(openFile:));
    NSWorkspace_openFile = reinterpret_cast<BOOL (*)(NSWorkspace *, SEL, NSString *)>(method_setImplementation(openFileMethod, reinterpret_cast<IMP>(replacedNSWorkspace_openFile)));

    // Override -[NSApplication runModalForWindow:]
    Method runModalForWindowMethod = class_getInstanceMethod(objc_getClass("NSApplication"), @selector(runModalForWindow:));
    NSApplication_RunModalForWindow = method_setImplementation(runModalForWindowMethod, reinterpret_cast<IMP>(replacedRunModalForWindow));

    NSNotificationCenter *defaultCenter = [NSNotificationCenter defaultCenter];

    // Track when any Cocoa window is about to be be shown.
    id orderOnScreenObserver = [defaultCenter addObserverForName:NSWindowWillOrderOnScreenNotification
                                                          object:nil
                                                           queue:nil
                                                           usingBlock:^(NSNotification *notification) { fullscreenWindowTracker().windowShown([notification object]); }];
    // Track when any Cocoa window is about to be hidden.
    id orderOffScreenObserver = [defaultCenter addObserverForName:NSWindowWillOrderOffScreenNotification
                                                           object:nil
                                                            queue:nil
                                                       usingBlock:^(NSNotification *notification) { fullscreenWindowTracker().windowHidden([notification object]); }];

    // Leak the two observers so that they observe notifications for the lifetime of the process.
    CFRetain((__bridge CFTypeRef)orderOnScreenObserver);
    CFRetain((__bridge CFTypeRef)orderOffScreenObserver);
}

void PluginProcess::setModalWindowIsShowing(bool modalWindowIsShowing)
{
    parentProcessConnection()->send(Messages::PluginProcessProxy::SetModalWindowIsShowing(modalWindowIsShowing), 0);
}

void PluginProcess::setFullscreenWindowIsShowing(bool fullscreenWindowIsShowing)
{
    parentProcessConnection()->send(Messages::PluginProcessProxy::SetFullscreenWindowIsShowing(fullscreenWindowIsShowing), 0);
}

bool PluginProcess::launchProcess(const String& launchPath, const Vector<String>& arguments)
{
    bool result;
    if (!parentProcessConnection()->sendSync(Messages::PluginProcessProxy::LaunchProcess(launchPath, arguments), Messages::PluginProcessProxy::LaunchProcess::Reply(result), 0))
        return false;

    return result;
}

bool PluginProcess::launchApplicationAtURL(const String& urlString, const Vector<String>& arguments)
{
    bool result = false;
    if (!parentProcessConnection()->sendSync(Messages::PluginProcessProxy::LaunchApplicationAtURL(urlString, arguments), Messages::PluginProcessProxy::LaunchProcess::Reply(result), 0))
        return false;

    return result;
}

bool PluginProcess::openURL(const String& urlString, int32_t& status, String& launchedURLString)
{
    bool result;
    if (!parentProcessConnection()->sendSync(Messages::PluginProcessProxy::OpenURL(urlString), Messages::PluginProcessProxy::OpenURL::Reply(result, status, launchedURLString), 0))
        return false;

    return result;
}

bool PluginProcess::openFile(const String& fullPath)
{
    bool result;
    if (!parentProcessConnection()->sendSync(Messages::PluginProcessProxy::OpenFile(fullPath), Messages::PluginProcessProxy::OpenFile::Reply(result), 0))
        return false;

    return result;
}

static void muteAudio(void)
{
    AudioObjectPropertyAddress propertyAddress = { kAudioHardwarePropertyProcessIsAudible, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster };
    UInt32 propertyData = 0;
    OSStatus result = AudioObjectSetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, 0, sizeof(UInt32), &propertyData);
    ASSERT_UNUSED(result, result == noErr);
}

void PluginProcess::platformInitializePluginProcess(PluginProcessCreationParameters&& parameters)
{
    m_compositingRenderServerPort = WTFMove(parameters.acceleratedCompositingPort);
    if (parameters.processType == PluginProcessTypeSnapshot)
        muteAudio();

    [NSURLCache setSharedURLCache:adoptNS([[NSURLCache alloc]
        initWithMemoryCapacity:pluginMemoryCacheSize
        diskCapacity:pluginDiskCacheSize
        diskPath:m_nsurlCacheDirectory]).get()];

#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400
    // Disable Dark Mode in the plugin process to avoid rendering issues.
    [NSApp setAppearance:[NSAppearance appearanceNamed:NSAppearanceNameAqua]];
#endif
}

void PluginProcess::platformInitializeProcess(const AuxiliaryProcessInitializationParameters& parameters)
{
    initializeShim();

    initializeCocoaOverrides();

    bool experimentalPlugInSandboxProfilesEnabled = parameters.extraInitializationData.get("experimental-sandbox-plugin") == "1";
    WebCore::RuntimeEnabledFeatures::sharedFeatures().setExperimentalPlugInSandboxProfilesEnabled(experimentalPlugInSandboxProfilesEnabled);

    // FIXME: It would be better to proxy SetCursor calls over to the UI process instead of
    // allowing plug-ins to change the mouse cursor at any time.
    // FIXME: SetsCursorInBackground connection property is deprecated in favor of kCGSSetsCursorInBackgroundTagBit window tag bit.
    // <rdar://problem/7752422> asks for an API to set cursor from background processes.
    CGSConnectionID cid = CGSMainConnectionID();
    CGSSetConnectionProperty(cid, cid, CFSTR("SetsCursorInBackground"), (CFTypeRef)kCFBooleanTrue);

    RetainPtr<CFURLRef> pluginURL = adoptCF(CFURLCreateWithFileSystemPath(0, m_pluginPath.createCFString().get(), kCFURLPOSIXPathStyle, false));
    if (!pluginURL)
        return;

    RetainPtr<CFBundleRef> pluginBundle = adoptCF(CFBundleCreate(kCFAllocatorDefault, pluginURL.get()));
    if (!pluginBundle)
        return;

    m_pluginBundleIdentifier = CFBundleGetIdentifier(pluginBundle.get());

    if (m_pluginBundleIdentifier == "com.adobe.acrobat.pdfviewerNPAPI")
        oldPluginProcessNameShouldEqualNewPluginProcessNameForAdobeReader = true;

#if defined(__i386__)
    if (m_pluginBundleIdentifier == "com.microsoft.SilverlightPlugin") {
        // Set this so that any calls to mach_vm_map for pages reserved by malloc will be executable.
        shouldMapMallocMemoryExecutable = true;

        // Go through the address space looking for already existing malloc regions and change the
        // protection to make them executable.
        mach_vm_size_t size;
        uint32_t depth = 0;
        struct vm_region_submap_info_64 info = { };
        mach_msg_type_number_t count = VM_REGION_SUBMAP_INFO_COUNT_64;
        for (mach_vm_address_t addr = 0; ; addr += size) {
            kern_return_t kr = mach_vm_region_recurse(mach_task_self(), &addr, &size, &depth, (vm_region_recurse_info_64_t)&info, &count);
            if (kr != KERN_SUCCESS)
                break;

            if (isMallocTinyMemoryTag(info.user_tag))
                mach_vm_protect(mach_task_self(), addr, size, false, info.protection | VM_PROT_EXECUTE);
        }

        // Silverlight expects the data segment of its coreclr library to be executable.
        // Register with dyld to get notified when libraries are bound, then look for the
        // coreclr image and make its __DATA segment executable.
        _dyld_register_func_for_add_image([](const struct mach_header* mh, intptr_t vmaddr_slide) {
            Dl_info imageInfo;
            if (!dladdr(mh, &imageInfo))
                return;

            const char* pathSuffix = "/Silverlight.plugin/Contents/MacOS/CoreCLR.bundle/Contents/MacOS/coreclr";

            int pathSuffixLength = strlen(pathSuffix);
            int imageFilePathLength = strlen(imageInfo.dli_fname);

            if (imageFilePathLength < pathSuffixLength)
                return;

            if (strcmp(imageInfo.dli_fname + (imageFilePathLength - pathSuffixLength), pathSuffix))
                return;

            unsigned long segmentSize;
            const uint8_t* segmentData = getsegmentdata(mh, "__DATA", &segmentSize);
            if (!segmentData)
                return;

            mach_vm_size_t size;
            uint32_t depth = 0;
            struct vm_region_submap_info_64 info = { };
            mach_msg_type_number_t count = VM_REGION_SUBMAP_INFO_COUNT_64;
            for (mach_vm_address_t addr = reinterpret_cast<mach_vm_address_t>(segmentData); addr < reinterpret_cast<mach_vm_address_t>(segmentData) + segmentSize ; addr += size) {
                kern_return_t kr = mach_vm_region_recurse(mach_task_self(), &addr, &size, &depth, (vm_region_recurse_info_64_t)&info, &count);
                if (kr != KERN_SUCCESS)
                    break;

                mach_vm_protect(mach_task_self(), addr, size, false, info.protection | VM_PROT_EXECUTE);
            }
        });
    }
#endif

    // FIXME: Workaround for Java not liking its plugin process to be suppressed - <rdar://problem/14267843>
    if (m_pluginBundleIdentifier == "com.oracle.java.JavaAppletPlugin")
        (new UserActivity("com.oracle.java.JavaAppletPlugin"))->start();
    
    if (!pluginHasSandboxProfile(m_pluginBundleIdentifier)) {
        // Allow Apple Events from Citrix plugin. This can be removed when <rdar://problem/14012823> is fixed.
        setenv("__APPLEEVENTSSERVICENAME", "com.apple.coreservices.appleevents", 1);
    }
}

void PluginProcess::initializeProcessName(const AuxiliaryProcessInitializationParameters& parameters)
{
    NSString *applicationName = [NSString stringWithFormat:WEB_UI_STRING("%@ (%@ Internet plug-in)", "visible name of the plug-in host process. The first argument is the plug-in name and the second argument is the application name."), [[(NSString *)m_pluginPath lastPathComponent] stringByDeletingPathExtension], (NSString *)parameters.uiProcessName];
    _LSSetApplicationInformationItem(kLSDefaultSessionID, _LSGetCurrentApplicationASN(), _kLSDisplayNameKey, (CFStringRef)applicationName, nullptr);
    if (!m_pluginBundleIdentifier.isEmpty())
        _LSSetApplicationInformationItem(kLSDefaultSessionID, _LSGetCurrentApplicationASN(), kLSPlugInBundleIdentifierKey, m_pluginBundleIdentifier.createCFString().get(), nullptr);
}

void PluginProcess::initializeSandbox(const AuxiliaryProcessInitializationParameters& parameters, SandboxInitializationParameters& sandboxParameters)
{
    // PluginProcess may already be sandboxed if its parent process was sandboxed, and launched a child process instead of an XPC service.
    // This is generally not expected, however we currently always spawn a child process to create a MIME type preferences file.
    if (currentProcessIsSandboxed()) {
        RELEASE_ASSERT(!parameters.connectionIdentifier.xpcConnection);
        return;
    }

    char cacheDirectory[PATH_MAX];
    if (!confstr(_CS_DARWIN_USER_CACHE_DIR, cacheDirectory, sizeof(cacheDirectory))) {
        WTFLogAlways("PluginProcess: couldn't retrieve system cache directory path: %d\n", errno);
        exit(EX_OSERR);
    }

    m_nsurlCacheDirectory = [[[NSFileManager defaultManager] stringWithFileSystemRepresentation:cacheDirectory length:strlen(cacheDirectory)] stringByAppendingPathComponent:[[NSBundle mainBundle] bundleIdentifier]];
    if (![[NSFileManager defaultManager] createDirectoryAtURL:[NSURL fileURLWithPath:m_nsurlCacheDirectory isDirectory:YES] withIntermediateDirectories:YES attributes:nil error:nil]) {
        WTFLogAlways("PluginProcess: couldn't create NSURL cache directory '%s'\n", cacheDirectory);
        exit(EX_OSERR);
    }

    if (PluginInfoStore::shouldAllowPluginToRunUnsandboxed(m_pluginBundleIdentifier))
        return;

    bool parentIsSandboxed = parameters.connectionIdentifier.xpcConnection && connectedProcessIsSandboxed(parameters.connectionIdentifier.xpcConnection.get());

    if (parameters.extraInitializationData.get("disable-sandbox") == "1") {
        if (parentIsSandboxed) {
            WTFLogAlways("Sandboxed processes may not disable plug-in sandbox, terminating %s.", parameters.clientIdentifier.utf8().data());
            exit(EX_OSERR);
        }
        return;
    }

    String sandboxProfile = pluginSandboxProfile(m_pluginBundleIdentifier);
    if (sandboxProfile.isEmpty()) {
        if (parentIsSandboxed) {
            WTFLogAlways("Sandboxed processes may only use sandboxed plug-ins, terminating %s.", parameters.clientIdentifier.utf8().data());
            exit(EX_OSERR);
        }
        return;
    }

    sandboxParameters.setSandboxProfile(sandboxProfile);

    char temporaryDirectory[PATH_MAX];
    if (!confstr(_CS_DARWIN_USER_TEMP_DIR, temporaryDirectory, sizeof(temporaryDirectory))) {
        WTFLogAlways("PluginProcess: couldn't retrieve system temporary directory path: %d\n", errno);
        exit(EX_OSERR);
    }

    if (strlcpy(temporaryDirectory, [[[[NSFileManager defaultManager] stringWithFileSystemRepresentation:temporaryDirectory length:strlen(temporaryDirectory)] stringByAppendingPathComponent:@"WebKitPlugin-XXXXXX"] fileSystemRepresentation], sizeof(temporaryDirectory)) >= sizeof(temporaryDirectory)
        || !mkdtemp(temporaryDirectory)) {
        WTFLogAlways("PluginProcess: couldn't create private temporary directory '%s'\n", temporaryDirectory);
        exit(EX_OSERR);
    }

    sandboxParameters.setUserDirectorySuffix([[[[NSFileManager defaultManager] stringWithFileSystemRepresentation:temporaryDirectory length:strlen(temporaryDirectory)] lastPathComponent] fileSystemRepresentation]);

    sandboxParameters.addPathParameter("PLUGIN_PATH", m_pluginPath);
    sandboxParameters.addPathParameter("NSURL_CACHE_DIR", m_nsurlCacheDirectory);

    [[NSUserDefaults standardUserDefaults] registerDefaults:@{ @"NSUseRemoteSavePanel" : @YES }];

    AuxiliaryProcess::initializeSandbox(parameters, sandboxParameters);
}

bool PluginProcess::shouldOverrideQuarantine()
{
    return m_pluginBundleIdentifier != "com.cisco.webex.plugin.gpc64";
}

void PluginProcess::stopRunLoop()
{
    AuxiliaryProcess::stopNSAppRunLoop();
}

} // namespace WebKit

#endif // ENABLE(NETSCAPE_PLUGIN_API)
