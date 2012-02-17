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

#import "config.h"
#import "ThreadLauncher.h"

#import "WebProcess.h"
#import "WebSystemInterface.h"
#import <runtime/InitializeThreading.h>
#import <WebCore/RunLoop.h>
#import <wtf/MainThread.h>
#import <wtf/Threading.h>

using namespace WebCore;

namespace WebKit {

static void webThreadBody(void* context)
{
    mach_port_t serverPort = static_cast<mach_port_t>(reinterpret_cast<uintptr_t>(context));

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    InitWebCoreSystemInterface();
    JSC::initializeThreading();
    WTF::initializeMainThread();

    WebProcess::shared().initialize(serverPort, RunLoop::current());

    [pool drain];

    RunLoop::current()->run();
}

CoreIPC::Connection::Identifier ThreadLauncher::createWebThread()
{
    // Create the service port.
     mach_port_t listeningPort;
     mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &listeningPort);
     
     // Insert a send right so we can send to it.
     mach_port_insert_right(mach_task_self(), listeningPort, listeningPort, MACH_MSG_TYPE_MAKE_SEND);

    if (!createThread(webThreadBody, reinterpret_cast<void*>(listeningPort), "WebKit2: WebThread")) {
        mach_port_destroy(mach_task_self(), listeningPort);
        return MACH_PORT_NULL;
    }

    return listeningPort;
}

} // namespace WebKit
