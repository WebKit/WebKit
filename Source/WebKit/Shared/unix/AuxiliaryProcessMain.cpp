/*
 * Copyright (C) 2014 Igalia S.L.
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
#include "AuxiliaryProcessMain.h"

#include <JavaScriptCore/Options.h>
#include <WebCore/ProcessIdentifier.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/CStringView.h>
#include <wtf/text/StringToIntegerConversion.h>
#include <wtf/text/StringView.h>
#include <wtf/unix/UnixFileDescriptor.h>

#if ENABLE(BREAKPAD)
#include "unix/BreakpadExceptionHandler.h"
#endif

namespace WebKit {

AuxiliaryProcessMainCommon::AuxiliaryProcessMainCommon()
{
#if ENABLE(BREAKPAD)
    installBreakpadExceptionHandler();
#endif
}

// The command line is constructed in ProcessLauncher::launchProcess.
bool AuxiliaryProcessMainCommon::parseCommandLine(int argc, char** argv)
{
    int argIndex = 1; // Start from argv[1], since argv[0] is the program name.
    std::span argvSpan = unsafeMakeSpan(argv, argc);

    // Ensure we have enough arguments for processIdentifier and connectionIdentifier
    if (argc < argIndex + 2)
        return false;

    if (auto processIdentifier = parseInteger<uint64_t>(StringView::fromLatin1(argvSpan[argIndex++]))) {
        if (!ObjectIdentifier<WebCore::ProcessIdentifierType>::isValidIdentifier(*processIdentifier))
            return false;
        m_parameters.processIdentifier = ObjectIdentifier<WebCore::ProcessIdentifierType>(*processIdentifier);
    } else
        return false;

    if (auto connectionIdentifier = parseInteger<int>(StringView::fromLatin1(argvSpan[argIndex++])))
        m_parameters.connectionIdentifier = IPC::Connection::Identifier { { *connectionIdentifier, UnixFileDescriptor::Adopt } };
    else
        return false;

    if (!m_parameters.processIdentifier->toRawValue() || m_parameters.connectionIdentifier.handle.value() <= 0)
        return false;

#if ENABLE(DEVELOPER_MODE)
    // Check last remaining options for JSC testing
    for (auto& arg : argvSpan.subspan(argIndex)) {
        if (CStringView::unsafeFromUTF8(arg) == "--configure-jsc-for-testing"_s)
            JSC::Config::configureForTesting();
    }
#endif

    return true;
}

void AuxiliaryProcess::platformInitialize(const AuxiliaryProcessInitializationParameters&)
{
    struct sigaction signalAction { };
    RELEASE_ASSERT(!sigemptyset(&signalAction.sa_mask));
    signalAction.sa_handler = SIG_IGN;
    RELEASE_ASSERT(!sigaction(SIGPIPE, &signalAction, nullptr));
}

} // namespace WebKit
