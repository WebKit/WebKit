/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Generic main().
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuCommandLine.hpp"
#include "tcuPlatform.hpp"
#include "tcuApp.hpp"
#include "tcuResource.hpp"
#include "tcuTestLog.hpp"
#include "tcuTestSessionExecutor.hpp"
#include "deUniquePtr.hpp"
#include "qpDebugOut.h"

#include <cstdio>

// Implement this in your platform port.
tcu::Platform *createPlatform(void);

namespace
{

bool disableRawWrites(int, const char *)
{
    return false;
}
bool disableFmtWrites(int, const char *, va_list)
{
    return false;
}
void disableStdout()
{
    qpRedirectOut(disableRawWrites, disableFmtWrites);
}

} // anonymous namespace

int main(int argc, char **argv)
{
    int exitStatus = EXIT_SUCCESS;

#if (DE_OS != DE_OS_WIN32)
    // Set stdout to line-buffered mode (will be fully buffered by default if stdout is pipe).
    setvbuf(stdout, nullptr, _IOLBF, 4 * 1024);
#endif

    try
    {
        tcu::CommandLine cmdLine(argc, argv);

        if (cmdLine.quietMode())
            disableStdout();

        tcu::DirArchive archive(cmdLine.getArchiveDir());
        tcu::TestLog log(cmdLine.getLogFileName(), cmdLine.getLogFlags());
        de::UniquePtr<tcu::Platform> platform(createPlatform());
        de::UniquePtr<tcu::App> app(new tcu::App(*platform, archive, log, cmdLine));

        // Main loop.
        for (;;)
        {
            if (!app->iterate())
            {
                if (cmdLine.getRunMode() == tcu::RUNMODE_EXECUTE &&
                    (!app->getResult().isComplete || app->getResult().numFailed))
                {
                    exitStatus = EXIT_FAILURE;
                }

                break;
            }
        }
    }
    catch (const std::exception &e)
    {
        tcu::die("%s", e.what());
    }

    return exitStatus;
}
