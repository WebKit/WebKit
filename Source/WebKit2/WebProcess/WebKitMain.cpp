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

#include "config.h"
#include "CommandLine.h"

#include "PluginProcessMain.h"
#include "ProcessLauncher.h"
#include "WebProcessMain.h"
#include <wtf/text/CString.h>

#if ENABLE(NETWORK_PROCESS)
#include "NetworkProcessMain.h"
#endif

#if ENABLE(SHARED_WORKER_PROCESS)
#include "SharedWorkerProcessMain.h"
#endif

#if PLATFORM(MAC)
#include <objc/objc-auto.h>
#endif

using namespace WebKit;

static int WebKitMain(const CommandLine& commandLine)
{
    ProcessLauncher::ProcessType processType;    
    if (!ProcessLauncher::getProcessTypeFromString(commandLine["type"].utf8().data(), processType))
        return EXIT_FAILURE;

    switch (processType) {
        case ProcessLauncher::WebProcess:
            return WebProcessMain(commandLine);
#if ENABLE(PLUGIN_PROCESS)
        case ProcessLauncher::PluginProcess:
            return PluginProcessMain(commandLine);
#endif
#if ENABLE(NETWORK_PROCESS)
        case ProcessLauncher::NetworkProcess:
            return NetworkProcessMain(commandLine);
#endif
#if ENABLE(SHARED_WORKER_PROCESS)
        case ProcessLauncher::SharedWorkerProcess:
            return SharedWorkerProcessMain(commandLine);
#endif
    }

    return EXIT_FAILURE;
}

#if PLATFORM(MAC)

extern "C" WK_EXPORT int WebKitMain(int argc, char** argv);

int WebKitMain(int argc, char** argv)
{
    ASSERT(!objc_collectingEnabled());
    
    CommandLine commandLine;
    if (!commandLine.parse(argc, argv))
        return EXIT_FAILURE;
    
    return WebKitMain(commandLine);
}

#endif
