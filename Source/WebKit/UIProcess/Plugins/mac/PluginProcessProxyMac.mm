/*
 * Copyright (C) 2010-2020 Apple Inc. All rights reserved.
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
#import "PluginProcessProxy.h"

#if ENABLE(NETSCAPE_PLUGIN_API)

#import "PluginProcessCreationParameters.h"
#import "PluginProcessManager.h"
#import "PluginProcessMessages.h"
#import "SandboxUtilities.h"
#import <QuartzCore/CARemoteLayerServer.h>
#import <crt_externs.h>
#import <mach-o/dyld.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <spawn.h>
#import <wtf/FileSystem.h>
#import <wtf/ProcessPrivilege.h>
#import <wtf/URL.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/CString.h>

@interface WKPlaceholderModalWindow : NSWindow 
@end

@implementation WKPlaceholderModalWindow

// Prevent NSApp from calling requestUserAttention: when the window is shown 
// modally, even if the app is inactive. See 6823049.
- (BOOL)_wantsUserAttention
{
    return NO;   
}

@end

namespace WebKit {
    
void PluginProcessProxy::platformGetLaunchOptionsWithAttributes(ProcessLauncher::LaunchOptions& launchOptions, const PluginProcessAttributes& pluginProcessAttributes)
{
    ASSERT(pluginProcessAttributes.moduleInfo.pluginArchitecture == CPU_TYPE_X86_64);
    launchOptions.processType = ProcessLauncher::ProcessType::Plugin;

    launchOptions.extraInitializationData.add("plugin-path", pluginProcessAttributes.moduleInfo.path);

    if (PluginProcessManager::singleton().experimentalPlugInSandboxProfilesEnabled())
        launchOptions.extraInitializationData.add("experimental-sandbox-plugin", "1");

    if (pluginProcessAttributes.sandboxPolicy == PluginProcessSandboxPolicy::Unsandboxed) {
        if (!currentProcessIsSandboxed())
            launchOptions.extraInitializationData.add("disable-sandbox", "1");
        else
            WTFLogAlways("Main process is sandboxed, ignoring plug-in sandbox policy");
    }
}

void PluginProcessProxy::platformInitializePluginProcess(PluginProcessCreationParameters& parameters)
{
    // For now only Flash is known to behave with asynchronous plug-in initialization.
    parameters.supportsAsynchronousPluginInitialization = m_pluginProcessAttributes.moduleInfo.bundleIdentifier == "com.macromedia.Flash Player.plugin";

    parameters.acceleratedCompositingPort = MachSendRight::create([CARemoteLayerServer sharedServer].serverPort);
    parameters.networkATSContext = adoptCF(_CFNetworkCopyATSContext());
}

bool PluginProcessProxy::getPluginProcessSerialNumber(ProcessSerialNumber& pluginProcessSerialNumber)
{
    pid_t pluginProcessPID = processIdentifier();
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    return GetProcessForPID(pluginProcessPID, &pluginProcessSerialNumber) == noErr;
    ALLOW_DEPRECATED_DECLARATIONS_END
}

void PluginProcessProxy::makePluginProcessTheFrontProcess()
{
    ProcessSerialNumber pluginProcessSerialNumber;
    if (!getPluginProcessSerialNumber(pluginProcessSerialNumber))
        return;

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    SetFrontProcess(&pluginProcessSerialNumber);
    ALLOW_DEPRECATED_DECLARATIONS_END
}

void PluginProcessProxy::makeUIProcessTheFrontProcess()
{
    ProcessSerialNumber processSerialNumber;
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    GetCurrentProcess(&processSerialNumber);
    SetFrontProcess(&processSerialNumber);            
    ALLOW_DEPRECATED_DECLARATIONS_END
}

void PluginProcessProxy::setFullscreenWindowIsShowing(bool fullscreenWindowIsShowing)
{
    if (m_fullscreenWindowIsShowing == fullscreenWindowIsShowing)
        return;

    m_fullscreenWindowIsShowing = fullscreenWindowIsShowing;
    if (m_fullscreenWindowIsShowing)
        enterFullscreen();
    else
        exitFullscreen();
}

void PluginProcessProxy::enterFullscreen()
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    // Get the current presentation options.
    m_preFullscreenAppPresentationOptions = [NSApp presentationOptions];

    // Figure out which presentation options to use.
    unsigned presentationOptions = m_preFullscreenAppPresentationOptions & ~(NSApplicationPresentationAutoHideDock | NSApplicationPresentationAutoHideMenuBar);
    presentationOptions |= NSApplicationPresentationHideDock | NSApplicationPresentationHideMenuBar;

    [NSApp setPresentationOptions:presentationOptions];
    makePluginProcessTheFrontProcess();
}

void PluginProcessProxy::exitFullscreen()
{
    // If the plug-in host is the current application then we should bring ourselves to the front when it exits full-screen mode.
    ProcessSerialNumber frontProcessSerialNumber;
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    GetFrontProcess(&frontProcessSerialNumber);
    ALLOW_DEPRECATED_DECLARATIONS_END

    // The UI process must be the front process in order to change the presentation mode.
    makeUIProcessTheFrontProcess();
    [NSApp setPresentationOptions:m_preFullscreenAppPresentationOptions];

    ProcessSerialNumber pluginProcessSerialNumber;
    if (!getPluginProcessSerialNumber(pluginProcessSerialNumber))
        return;

    // If the plug-in process was not the front process, switch back to the previous front process.
    // (Otherwise we'll keep the UI process as the front process).
    Boolean isPluginProcessFrontProcess;
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    SameProcess(&frontProcessSerialNumber, &pluginProcessSerialNumber, &isPluginProcessFrontProcess);
    ALLOW_DEPRECATED_DECLARATIONS_END
    if (!isPluginProcessFrontProcess) {
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        SetFrontProcess(&frontProcessSerialNumber);
        ALLOW_DEPRECATED_DECLARATIONS_END
    }
}

void PluginProcessProxy::setModalWindowIsShowing(bool modalWindowIsShowing)
{
    if (modalWindowIsShowing == m_modalWindowIsShowing)
        return;
    
    m_modalWindowIsShowing = modalWindowIsShowing;
    
    if (m_modalWindowIsShowing)
        beginModal();
    else
        endModal();
}

void PluginProcessProxy::beginModal()
{
    ASSERT(!m_placeholderWindow);
    ASSERT(!m_activationObserver);
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));

    m_placeholderWindow = adoptNS([[WKPlaceholderModalWindow alloc] initWithContentRect:NSMakeRect(0, 0, 1, 1) styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:YES]);
    [m_placeholderWindow setReleasedWhenClosed:NO];
    
    m_activationObserver = [[NSNotificationCenter defaultCenter] addObserverForName:NSApplicationWillBecomeActiveNotification object:NSApp queue:nil
                                                                         usingBlock:^(NSNotification *){ applicationDidBecomeActive(); }];

    // The call to -[NSApp runModalForWindow:] below will run a nested run loop, and if the plug-in process
    // crashes the PluginProcessProxy object can be destroyed. Protect against this here.
    Ref<PluginProcessProxy> protect(*this);

    [NSApp runModalForWindow:m_placeholderWindow.get()];
    
    [m_placeholderWindow orderOut:nil];
    m_placeholderWindow = nullptr;
}

void PluginProcessProxy::endModal()
{
    ASSERT(m_placeholderWindow);
    ASSERT(m_activationObserver);
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));

    [[NSNotificationCenter defaultCenter] removeObserver:m_activationObserver.get()];
    m_activationObserver = nullptr;
    
    [NSApp stopModal];

    makeUIProcessTheFrontProcess();
}
    
void PluginProcessProxy::applicationDidBecomeActive()
{
    makePluginProcessTheFrontProcess();
}

static bool isFlashUpdater(const String& launchPath, const Vector<String>& arguments)
{
    if (launchPath != "/Applications/Utilities/Adobe Flash Player Install Manager.app/Contents/MacOS/Adobe Flash Player Install Manager")
        return false;

    if (arguments.size() != 1)
        return false;

    if (arguments[0] != "-update")
        return false;

    return true;
}

static bool shouldLaunchProcess(const PluginProcessAttributes& pluginProcessAttributes, const String& launchPath, const Vector<String>& arguments)
{
    if (pluginProcessAttributes.moduleInfo.bundleIdentifier == "com.macromedia.Flash Player.plugin")
        return isFlashUpdater(launchPath, arguments);

    return false;
}

void PluginProcessProxy::launchProcess(const String& launchPath, const Vector<String>& arguments, CompletionHandler<void(bool)>&& completionHandler)
{
    if (!shouldLaunchProcess(m_pluginProcessAttributes, launchPath, arguments))
        return completionHandler(false);

    [NSTask launchedTaskWithLaunchPath:launchPath arguments:createNSArray(arguments).get()];
    completionHandler(true);
}

static bool isJavaUpdaterURL(const PluginProcessAttributes& pluginProcessAttributes, const String& urlString)
{
    NSURL *url = [NSURL URLWithString:urlString];
    if (![url isFileURL])
        return false;

    NSArray *javaUpdaterAppNames = @[@"Java Updater.app", @"JavaUpdater.app"];

    for (NSString *javaUpdaterAppName in javaUpdaterAppNames) {
        NSString *javaUpdaterPath = [NSString pathWithComponents:@[(NSString *)pluginProcessAttributes.moduleInfo.path, @"Contents/Resources", javaUpdaterAppName]];
        if ([url.path isEqualToString:javaUpdaterPath])
            return YES;
    }

    return NO;
}

static bool shouldLaunchApplicationAtURL(const PluginProcessAttributes& pluginProcessAttributes, const String& urlString)
{
    if (pluginProcessAttributes.moduleInfo.bundleIdentifier == "com.oracle.java.JavaAppletPlugin")
        return isJavaUpdaterURL(pluginProcessAttributes, urlString);

    return false;
}

void PluginProcessProxy::launchApplicationAtURL(const String& urlString, const Vector<String>& arguments, CompletionHandler<void(bool)>&& completionHandler)
{
    if (!shouldLaunchApplicationAtURL(m_pluginProcessAttributes, urlString))
        return completionHandler(false);

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    auto configuration = @{ NSWorkspaceLaunchConfigurationArguments: createNSArray(arguments).get() };
    [[NSWorkspace sharedWorkspace] launchApplicationAtURL:[NSURL URLWithString:urlString] options:NSWorkspaceLaunchAsync configuration:configuration error:nullptr];
ALLOW_DEPRECATED_DECLARATIONS_END
    completionHandler(true);
}

static bool isSilverlightPreferencesURL(const PluginProcessAttributes& pluginProcessAttributes, const String& urlString)
{
    NSURL *silverlightPreferencesURL = [NSURL fileURLWithPathComponents:@[(NSString *)pluginProcessAttributes.moduleInfo.path, @"Contents/Resources/Silverlight Preferences.app"]];

    return [[NSURL URLWithString:urlString] isEqual:silverlightPreferencesURL];
}

static bool shouldOpenURL(const PluginProcessAttributes& pluginProcessAttributes, const String& urlString)
{
    if (pluginProcessAttributes.moduleInfo.bundleIdentifier == "com.microsoft.SilverlightPlugin")
        return isSilverlightPreferencesURL(pluginProcessAttributes, urlString);

    return false;
}

void PluginProcessProxy::openURL(const String& urlString, CompletionHandler<void(bool result, int32_t status, String launchedURLString)>&& completionHandler)
{
    if (!shouldOpenURL(m_pluginProcessAttributes, urlString))
        return completionHandler(false, 0, { });

    CFURLRef launchedURL;
    uint32_t status = LSOpenCFURLRef(URL({ }, urlString).createCFURL().get(), &launchedURL);

    String launchedURLString;
    if (launchedURL) {
        launchedURLString = URL(launchedURL).string();
        CFRelease(launchedURL);
    }
    completionHandler(true, status, launchedURLString);
}

static bool shouldOpenFile(const PluginProcessAttributes& pluginProcessAttributes, const String& fullPath)
{
    if (pluginProcessAttributes.moduleInfo.bundleIdentifier == "com.macromedia.Flash Player.plugin") {
        if (fullPath == "/Library/PreferencePanes/Flash Player.prefPane")
            return true;
    }

    return false;
}

void PluginProcessProxy::openFile(const String& fullPath, CompletionHandler<void(bool)>&& completionHandler)
{
    if (!shouldOpenFile(m_pluginProcessAttributes, fullPath))
        return completionHandler(false);

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [[NSWorkspace sharedWorkspace] openFile:fullPath];
    ALLOW_DEPRECATED_DECLARATIONS_END
    completionHandler(true);
}

int pluginProcessLatencyQOS()
{
    static const int qos = [[NSUserDefaults standardUserDefaults] integerForKey:@"WebKitPluginProcessLatencyQOS"];
    return qos;
}

int pluginProcessThroughputQOS()
{
    static const int qos = [[NSUserDefaults standardUserDefaults] integerForKey:@"WebKitPluginProcessThroughputQOS"];
    return qos;
}

} // namespace WebKit

#endif // ENABLE(NETSCAPE_PLUGIN_API)
