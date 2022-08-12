/*
 * Copyright 2020 RDK Management
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
#include "BreakpadExceptionHandler.h"

#if ENABLE(BREAKPAD)

#include <breakpad/client/linux/handler/exception_handler.h>
#include <mutex>
#include <signal.h>
#include <wtf/FileSystem.h>
#include <wtf/NeverDestroyed.h>

namespace WebKit {

void installBreakpadExceptionHandler()
{
    static std::once_flag onceFlag;
    static MainThreadLazyNeverDestroyed<google_breakpad::ExceptionHandler> exceptionHandler;
    static String breakpadMinidumpDir = String::fromUTF8(getenv("BREAKPAD_MINIDUMP_DIR"));

#ifdef BREAKPAD_MINIDUMP_DIR
    if (breakpadMinidumpDir.isEmpty())
        breakpadMinidumpDir = StringImpl::createFromCString(BREAKPAD_MINIDUMP_DIR);
#endif

    if (breakpadMinidumpDir.isEmpty())
        return;

    if (FileSystem::fileType(breakpadMinidumpDir) != FileSystem::FileType::Directory) {
        WTFLogAlways("Breakpad dir \"%s\" is not a directory, not installing handler", breakpadMinidumpDir.utf8().data());
        return;
    }

    std::call_once(onceFlag, []() {
        exceptionHandler.construct(google_breakpad::MinidumpDescriptor(breakpadMinidumpDir.utf8().data()), nullptr,
            [](const google_breakpad::MinidumpDescriptor&, void*, bool succeeded) -> bool {
                return succeeded;
            }, nullptr, true, -1);
    });
}
}
#endif

