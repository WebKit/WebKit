/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#if ENABLE(PLUGIN_PROCESS)

#import "NetscapePlugin.h"
#import "PluginProcessShim.h"
#import "PluginProcessProxyMessages.h"
#import "PluginProcessCreationParameters.h"
#import <CoreAudio/AudioHardware.h>
#import <WebCore/LocalizedStrings.h>
#import <WebKitSystemInterface.h>
#import <dlfcn.h>
#import <objc/runtime.h>
#import <wtf/HashSet.h>

#import "NetscapeSandboxFunctions.h"

namespace WebKit {

class FullscreenWindowTracker {
    WTF_MAKE_NONCOPYABLE(FullscreenWindowTracker);

public:
    FullscreenWindowTracker() { }
    
    template<typename T> void windowShown(T window);
    template<typename T> void windowHidden(T window);

private:
    typedef HashSet<void*> WindowSet;
    WindowSet m_windows;
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
#endif

static bool windowCoversAnyScreen(NSWindow* window)
{
    return rectCoversAnyScreen([window frame]);
}

template<typename T> void FullscreenWindowTracker::windowShown(T window)
{
    // If this window is already visible then there is nothing to do.
    WindowSet::iterator it = m_windows.find(window);
    if (it != m_windows.end())
        return;
    
    // If the window is not full-screen then we're not interested in it.
    if (!windowCoversAnyScreen(window))
        return;

    bool windowSetWasEmpty = m_windows.isEmpty();

    m_windows.add(window);
    
    // If this is the first full screen window to be shown, notify the UI process.
    if (windowSetWasEmpty)
        PluginProcess::shared().setFullscreenWindowIsShowing(true);
}

template<typename T> void FullscreenWindowTracker::windowHidden(T window)
{
    // If this is not a window that we're tracking then there is nothing to do.
    WindowSet::iterator it = m_windows.find(window);
    if (it == m_windows.end())
        return;

    m_windows.remove(it);

    // If this was the last full screen window that was visible, notify the UI process.
    if (m_windows.isEmpty())
        PluginProcess::shared().setFullscreenWindowIsShowing(false);
}

static FullscreenWindowTracker& fullscreenWindowTracker()
{
    DEFINE_STATIC_LOCAL(FullscreenWindowTracker, fullscreenWindowTracker, ());
    return fullscreenWindowTracker;
}

#if defined(__i386__)

static pthread_once_t shouldCallRealDebuggerOnce = PTHREAD_ONCE_INIT;
static bool isUserbreakSet = false;

static void initShouldCallRealDebugger()
{
    char* var = getenv("USERBREAK");
    
    if (var)
        isUserbreakSet = atoi(var);
}

static bool shouldCallRealDebugger()
{
    pthread_once(&shouldCallRealDebuggerOnce, initShouldCallRealDebugger);
    
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

#endif

static void setModal(bool modalWindowIsShowing)
{
    PluginProcess::shared().setModalWindowIsShowing(modalWindowIsShowing);
}

static unsigned modalCount = 0;

static void beginModal()
{
#if COMPILER(CLANG)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
    // Make sure to make ourselves the front process
    ProcessSerialNumber psn;
    GetCurrentProcess(&psn);
    SetFrontProcess(&psn);
#if COMPILER(CLANG)
#pragma clang diagnostic pop
#endif

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

#if defined(__i386__)
static void initializeShim()
{
    // Initialize the shim for 32-bit only.
    const PluginProcessShimCallbacks callbacks = {
        shouldCallRealDebugger,
        isWindowActive,
        getCurrentEventButtonState,
        beginModal,
        endModal,
        carbonWindowShown,
        carbonWindowHidden,
        setModal,
    };

    PluginProcessShimInitializeFunc initFunc = reinterpret_cast<PluginProcessShimInitializeFunc>(dlsym(RTLD_DEFAULT, "WebKitPluginProcessShimInitialize"));
    initFunc(callbacks);
}
#endif

static void initializeCocoaOverrides()
{
    // Override -[NSApplication runModalForWindow:]
    Method runModalForWindowMethod = class_getInstanceMethod(objc_getClass("NSApplication"), @selector(runModalForWindow:));
    NSApplication_RunModalForWindow = method_setImplementation(runModalForWindowMethod, reinterpret_cast<IMP>(replacedRunModalForWindow));

    NSNotificationCenter *defaultCenter = [NSNotificationCenter defaultCenter];

    // Track when any Cocoa window is about to be be shown.
    id orderOnScreenObserver = [defaultCenter addObserverForName:WKWindowWillOrderOnScreenNotification()
                                                          object:nil
                                                           queue:nil
                                                           usingBlock:^(NSNotification *notification) { fullscreenWindowTracker().windowShown([notification object]); }];
    // Track when any Cocoa window is about to be hidden.
    id orderOffScreenObserver = [defaultCenter addObserverForName:WKWindowWillOrderOffScreenNotification()
                                                           object:nil
                                                            queue:nil
                                                       usingBlock:^(NSNotification *notification) { fullscreenWindowTracker().windowHidden([notification object]); }];

    // Leak the two observers so that they observe notifications for the lifetime of the process.
    CFRetain(orderOnScreenObserver);
    CFRetain(orderOffScreenObserver);
}

void PluginProcess::platformInitializeProcess(const ChildProcessInitializationParameters&)
{
#if defined(__i386__)
    // Initialize the shim.
    initializeShim();
#endif

    // Initialize Cocoa overrides.
    initializeCocoaOverrides();

    // FIXME: It would be better to proxy SetCursor calls over to the UI process instead of
    // allowing plug-ins to change the mouse cursor at any time.
    WKEnableSettingCursorWhenInBackground();
}

void PluginProcess::setModalWindowIsShowing(bool modalWindowIsShowing)
{
    m_connection->send(Messages::PluginProcessProxy::SetModalWindowIsShowing(modalWindowIsShowing), 0);
}

void PluginProcess::setFullscreenWindowIsShowing(bool fullscreenWindowIsShowing)
{
    m_connection->send(Messages::PluginProcessProxy::SetFullscreenWindowIsShowing(fullscreenWindowIsShowing), 0);
}

static void initializeSandbox(const String& pluginPath, const String& sandboxProfileDirectoryPath)
{
    if (sandboxProfileDirectoryPath.isEmpty())
        return;

    RetainPtr<CFURLRef> pluginURL = adoptCF(CFURLCreateWithFileSystemPath(0, pluginPath.createCFString().get(), kCFURLPOSIXPathStyle, false));
    if (!pluginURL)
        return;

    RetainPtr<CFBundleRef> pluginBundle = adoptCF(CFBundleCreate(kCFAllocatorDefault, pluginURL.get()));
    if (!pluginBundle)
        return;
    
    CFStringRef bundleIdentifier = CFBundleGetIdentifier(pluginBundle.get());
    if (!bundleIdentifier)
        return;

    RetainPtr<CFURLRef> sandboxProfileDirectory = adoptCF(CFURLCreateWithFileSystemPath(0, sandboxProfileDirectoryPath.createCFString().get(), kCFURLPOSIXPathStyle, TRUE));

    RetainPtr<CFStringRef> sandboxFileName = CFStringCreateWithFormat(0, 0, CFSTR("%@.sb"), bundleIdentifier);
    RetainPtr<CFURLRef> sandboxURL = adoptCF(CFURLCreateWithFileSystemPathRelativeToBase(0, sandboxFileName.get(), kCFURLPOSIXPathStyle, FALSE, sandboxProfileDirectory.get()));

    RetainPtr<NSString> profileString = [[NSString alloc] initWithContentsOfURL:(NSURL *)sandboxURL.get() encoding:NSUTF8StringEncoding error:NULL];
    if (!profileString)
        return;

    enterSandbox([profileString.get() UTF8String], 0, 0);
}

static void muteAudio(void)
{
    AudioObjectPropertyAddress propertyAddress = { kAudioHardwarePropertyProcessIsAudible, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster };
    UInt32 propertyData = 0;
    OSStatus result = AudioObjectSetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, 0, sizeof(UInt32), &propertyData);
    ASSERT_UNUSED(result, result == noErr);
}

void PluginProcess::platformInitializePluginProcess(const PluginProcessCreationParameters& parameters)
{
    m_compositingRenderServerPort = parameters.acceleratedCompositingPort.port();

    NSString *applicationName = [NSString stringWithFormat:WEB_UI_STRING("%@ (%@ Internet plug-in)",
                                                                     "visible name of the plug-in host process. The first argument is the plug-in name "
                                                                     "and the second argument is the application name."),
                                 [[(NSString *)parameters.pluginPath lastPathComponent] stringByDeletingPathExtension], 
                                 (NSString *)parameters.parentProcessName];
    
    WKSetVisibleApplicationName((CFStringRef)applicationName);

    WebKit::initializeSandbox(m_pluginPath, parameters.sandboxProfileDirectoryPath);

    if (parameters.processType == TypeSnapshotProcess)
        muteAudio();
}

} // namespace WebKit

#endif // ENABLE(PLUGIN_PROCESS)
