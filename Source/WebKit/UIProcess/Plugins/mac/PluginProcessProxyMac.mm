/*
 * Copyright (C) 2010-2018 Apple Inc. All rights reserved.
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
#import <WebCore/FileSystem.h>
#import <crt_externs.h>
#import <mach-o/dyld.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <spawn.h>
#import <wtf/ProcessPrivilege.h>
#import <wtf/URL.h>
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
using namespace WebCore;

    
void PluginProcessProxy::platformGetLaunchOptionsWithAttributes(ProcessLauncher::LaunchOptions& launchOptions, const PluginProcessAttributes& pluginProcessAttributes)
{
    if (pluginProcessAttributes.moduleInfo.pluginArchitecture == CPU_TYPE_X86)
        launchOptions.processType = ProcessLauncher::ProcessType::Plugin32;
    else
        launchOptions.processType = ProcessLauncher::ProcessType::Plugin64;

    launchOptions.extraInitializationData.add("plugin-path", pluginProcessAttributes.moduleInfo.path);

    if (PluginProcessManager::singleton().experimentalPlugInSandboxProfilesEnabled())
        launchOptions.extraInitializationData.add("experimental-sandbox-plugin", "1");

    if (pluginProcessAttributes.sandboxPolicy == PluginProcessSandboxPolicyUnsandboxed) {
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

#if HAVE(HOSTED_CORE_ANIMATION)
    parameters.acceleratedCompositingPort = MachSendRight::create([CARemoteLayerServer sharedServer].serverPort);
#endif
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

void PluginProcessProxy::launchProcess(const String& launchPath, const Vector<String>& arguments, bool& result)
{
    if (!shouldLaunchProcess(m_pluginProcessAttributes, launchPath, arguments)) {
        result = false;
        return;
    }

    result = true;

    RetainPtr<NSMutableArray> argumentsArray = adoptNS([[NSMutableArray alloc] initWithCapacity:arguments.size()]);
    for (size_t i = 0; i < arguments.size(); ++i)
        [argumentsArray addObject:(NSString *)arguments[i]];

    [NSTask launchedTaskWithLaunchPath:launchPath arguments:argumentsArray.get()];
}

static bool isJavaUpdaterURL(const PluginProcessAttributes& pluginProcessAttributes, const String& urlString)
{
    NSURL *url = [NSURL URLWithString:urlString];
    if (![url isFileURL])
        return false;

    NSArray *javaUpdaterAppNames = [NSArray arrayWithObjects:@"Java Updater.app", @"JavaUpdater.app", nil];

    for (NSString *javaUpdaterAppName in javaUpdaterAppNames) {
        NSString *javaUpdaterPath = [NSString pathWithComponents:[NSArray arrayWithObjects:(NSString *)pluginProcessAttributes.moduleInfo.path, @"Contents/Resources", javaUpdaterAppName, nil]];
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

void PluginProcessProxy::launchApplicationAtURL(const String& urlString, const Vector<String>& arguments, bool& result)
{
    if (!shouldLaunchApplicationAtURL(m_pluginProcessAttributes, urlString)) {
        result = false;
        return;
    }

    result = true;

    RetainPtr<NSMutableArray> argumentsArray = adoptNS([[NSMutableArray alloc] initWithCapacity:arguments.size()]);
    for (size_t i = 0; i < arguments.size(); ++i)
        [argumentsArray addObject:(NSString *)arguments[i]];

    NSDictionary *configuration = [NSDictionary dictionaryWithObject:argumentsArray.get() forKey:NSWorkspaceLaunchConfigurationArguments];
    [[NSWorkspace sharedWorkspace] launchApplicationAtURL:[NSURL URLWithString:urlString] options:NSWorkspaceLaunchAsync configuration:configuration error:nullptr];
}

static bool isSilverlightPreferencesURL(const PluginProcessAttributes& pluginProcessAttributes, const String& urlString)
{
    NSURL *silverlightPreferencesURL = [NSURL fileURLWithPathComponents:[NSArray arrayWithObjects:(NSString *)pluginProcessAttributes.moduleInfo.path, @"Contents/Resources/Silverlight Preferences.app", nil]];

    return [[NSURL URLWithString:urlString] isEqual:silverlightPreferencesURL];
}

static bool shouldOpenURL(const PluginProcessAttributes& pluginProcessAttributes, const String& urlString)
{
    if (pluginProcessAttributes.moduleInfo.bundleIdentifier == "com.microsoft.SilverlightPlugin")
        return isSilverlightPreferencesURL(pluginProcessAttributes, urlString);

    return false;
}

void PluginProcessProxy::openURL(const String& urlString, bool& result, int32_t& status, String& launchedURLString)
{
    if (!shouldOpenURL(m_pluginProcessAttributes, urlString)) {
        result = false;
        return;
    }

    result = true;
    CFURLRef launchedURL;
    status = LSOpenCFURLRef(URL({ }, urlString).createCFURL().get(), &launchedURL);

    if (launchedURL) {
        launchedURLString = URL(launchedURL).string();
        CFRelease(launchedURL);
    }
}

static bool shouldOpenFile(const PluginProcessAttributes& pluginProcessAttributes, const String& fullPath)
{
    if (pluginProcessAttributes.moduleInfo.bundleIdentifier == "com.macromedia.Flash Player.plugin") {
        if (fullPath == "/Library/PreferencePanes/Flash Player.prefPane")
            return true;
    }

    return false;
}

void PluginProcessProxy::openFile(const String& fullPath, bool& result)
{
    if (!shouldOpenFile(m_pluginProcessAttributes, fullPath)) {
        result = false;
        return;
    }

    result = true;
    [[NSWorkspace sharedWorkspace] openFile:fullPath];
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
