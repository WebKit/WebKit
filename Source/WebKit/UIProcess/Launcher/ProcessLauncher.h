/*
 * Copyright (C) 2010-2019 Apple Inc. All rights reserved.
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

#pragma once

#include "Connection.h"
#include <WebCore/ProcessIdentifier.h>
#include <wtf/AbstractCanMakeCheckedPtr.h>
#include <wtf/CheckedPtr.h>
#include <wtf/Condition.h>
#include <wtf/Deque.h>
#include <wtf/Function.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/MonotonicTime.h>
#include <wtf/ProcessID.h>
#include <wtf/RefPtr.h>
#include <wtf/RunLoop.h>
#include <wtf/Seconds.h>
#include <wtf/SetForScope.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/Threading.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(WIN)
#include <wtf/win/Win32Handle.h>
#endif

#if PLATFORM(COCOA)
#include "XPCEventHandler.h"
#endif

#if USE(EXTENSIONKIT)
#include "ExtensionProcess.h"
OBJC_CLASS BEWebContentProcess;
OBJC_CLASS BENetworkingProcess;
OBJC_CLASS BERenderingProcess;
#endif

#if USE(GLIB) && OS(LINUX)
#include <wtf/glib/GSocketMonitor.h>
#endif

#if ENABLE(BUBBLEWRAP_SANDBOX)
#include "glib/XDGDBusProxy.h"
#endif

namespace WebKit {

#if PLATFORM(GTK) || PLATFORM(WPE)
enum class SandboxPermission {
    ReadOnly,
    ReadWrite,
};
#endif

enum class ProcessLaunchType {
    Web,
    Network,
#if ENABLE(GPU_PROCESS)
    GPU,
#endif
#if ENABLE(BUBBLEWRAP_SANDBOX)
    DBusProxy,
#endif
#if ENABLE(MODEL_PROCESS)
    Model,
#endif
};

struct ProcessLaunchOptions {
    WebCore::ProcessIdentifier processIdentifier;
    ProcessLaunchType processType { ProcessLaunchType::Web };
    HashMap<String, String> extraInitializationData { };
    bool nonValidInjectedCodeAllowed { false };
    bool shouldMakeProcessLaunchFailForTesting { false };
    // Whether ProcessLauncher::waitForLaunch() can wait for the launch, which only Cocoa supports.
    bool canWaitForLaunch { false };

#if PLATFORM(GTK) || PLATFORM(WPE)
    HashMap<UTF8CString, SandboxPermission> extraSandboxPaths { };
#if ENABLE(DEVELOPER_MODE)
    String processCmdPrefix { };
#endif
#endif

#if PLATFORM(PLAYSTATION)
    String processPath { };
    int32_t userId { -1 };
#endif
};

#if USE(EXTENSIONKIT)
class LaunchGrant : public ThreadSafeRefCounted<LaunchGrant> {
public:
    static Ref<LaunchGrant> create(ExtensionProcess&);
    ~LaunchGrant();

private:
    explicit LaunchGrant(ExtensionProcess&);

    ExtensionCapabilityGrant m_grant;
};
#endif

class ProcessLauncher : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<ProcessLauncher> {
public:
    using ProcessType = ProcessLaunchType;
    using LaunchOptions = ProcessLaunchOptions;

    class Client : public AbstractCanMakeCheckedPtr {
    public:
        virtual ~Client() { }
        
        virtual void ref() const = 0;
        virtual void deref() const = 0;
        virtual void didFinishLaunching(ProcessLauncher*, IPC::Connection::Identifier&&) = 0;
        virtual bool shouldConfigureJSCForTesting() const { return false; }
        virtual bool isJITEnabled() const { return true; }
        virtual bool shouldEnableLockdownMode() const { return false; }
        virtual bool shouldEnableEnhancedSecurity() const { return false; }
        virtual bool shouldDisableJITCage() const { return false; }
#if PLATFORM(COCOA)
        virtual RefPtr<XPCEventHandler> xpcEventHandler() const { return nullptr; }
#endif
    };

    static Ref<ProcessLauncher> create(Client* client, LaunchOptions&& launchOptions)
    {
        return adoptRef(*new ProcessLauncher(client, WTF::move(launchOptions)));
    }

    virtual ~ProcessLauncher();

    bool isLaunching() const { return m_isLaunching; }
    ProcessID processID() const { return m_processID; }

    void terminateProcess(const String& reason = { });
    void invalidate();

    // Blocks the main thread until the process has finished launching, or until the timeout, and
    // tells the client right away if it has. Only for processes launched with canWaitForLaunch.
    // Returns whether the launch has finished.
    bool waitForLaunch(Seconds timeout);

#if USE(EXTENSIONKIT)
    const std::optional<ExtensionProcess>& extensionProcess() const LIFETIME_BOUND { return m_process; }
    void setIsRetryingLaunch() { m_isRetryingLaunch = true; }
    bool isRetryingLaunch() const { return m_isRetryingLaunch; }
    LaunchGrant* launchGrant() const { return m_launchGrant.get(); }
    void releaseLaunchGrant() { m_launchGrant = nullptr; }
    static bool hasExtensionsInAppBundle();
#endif

private:
    ProcessLauncher(Client*, LaunchOptions&&);

    void launchProcess();
    void finishLaunchingProcess(ASCIILiteral name, int retriesRemaining = 2);
    void tryFinishLaunchingProcess(ASCIILiteral name, Function<void()>&& onFailure);
    void didFinishLaunchingProcess(ProcessID, IPC::Connection::Identifier&&);

    void platformInvalidate();
    void platformDestroy();

#if PLATFORM(COCOA)
    void terminateXPCConnection();

    class LaunchSteps;
    // On the main thread, by way of the launch steps if the launch can be waited for.
    static void callOnMainRunLoopForLaunch(LaunchSteps*, Function<void()>&&);
    void didReceiveBootstrapReply(xpc_object_t reply, mach_port_t listeningPort);
#endif

    CheckedPtr<Client> m_client;

#if PLATFORM(COCOA)
    OSObjectPtr<xpc_connection_t> m_xpcConnection;
    // Set for the whole of a launch that can be waited for.
    RefPtr<LaunchSteps> m_launchSteps;
#endif

#if USE(EXTENSIONKIT)
    RefPtr<LaunchGrant> m_launchGrant;
    std::optional<ExtensionProcess> m_process;
    bool m_isRetryingLaunch { false };
#endif

#if PLATFORM(WIN)
    WTF::Win32Handle m_hProcess;
#endif

    const LaunchOptions m_launchOptions;
    bool m_isLaunching { true };
    ProcessID m_processID { 0 };

#if ENABLE(BUBBLEWRAP_SANDBOX)
    XDGDBusProxy m_dbusProxy;
#endif

#if USE(GLIB) && OS(LINUX)
    GSocketMonitor m_socketMonitor;
#endif
};

#if PLATFORM(COCOA)
// The steps of a launch that can be waited for that run on the main thread: those after the extension
// has launched, and the handling of the reply to the bootstrap message and of errors. They come in on
// other queues, and run in order on the main thread, either on its own turn or while waitForLaunch()
// blocks it.
class ProcessLauncher::LaunchSteps : public ThreadSafeRefCounted<LaunchSteps> {
public:
    static Ref<LaunchSteps> create()
    {
        return adoptRef(*new LaunchSteps);
    }

    // From any thread.
    void append(Function<void()>&& step)
    {
        {
            Locker locker { m_lock };
            m_steps.append(WTF::move(step));
            m_condition.notifyAll();
        }
        RunLoop::mainSingleton().dispatch([protectedThis = Ref { *this }] {
            protectedThis->runPendingSteps();
        });
    }

    void runPendingSteps()
    {
        ASSERT(RunLoop::isMain());
        // The steps run in order, so not from within one, such as from a message it dispatches.
        if (m_isRunningStep)
            return;
        while (auto step = takeStep()) {
            SetForScope isRunningStep { m_isRunningStep, true };
            step();
        }
    }

    bool isRunningStep() const
    {
        ASSERT(RunLoop::isMain());
        return m_isRunningStep;
    }

    // Whether there are steps to run before the deadline.
    bool waitForSteps(MonotonicTime deadline)
    {
        ASSERT(RunLoop::isMain());
        Locker locker { m_lock };
        return m_condition.waitUntil(m_lock, deadline, [&] {
            assertIsHeld(m_lock);
            return !m_steps.isEmpty();
        });
    }

private:
    LaunchSteps() = default;

    Function<void()> takeStep()
    {
        Locker locker { m_lock };
        if (m_steps.isEmpty())
            return nullptr;
        return m_steps.takeFirst();
    }

    Lock m_lock;
    Condition m_condition;
    Deque<Function<void()>> m_steps WTF_GUARDED_BY_LOCK(m_lock);
    bool m_isRunningStep { false };
};
#endif

} // namespace WebKit
