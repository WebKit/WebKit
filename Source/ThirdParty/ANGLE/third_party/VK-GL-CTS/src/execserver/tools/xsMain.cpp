/*-------------------------------------------------------------------------
 * drawElements Quality Program Execution Server
 * ---------------------------------------------
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
 * \brief ExecServer main().
 *//*--------------------------------------------------------------------*/

#include "xsExecutionServer.hpp"
#include "deCommandLine.hpp"
#include "deString.h"

#if (DE_OS == DE_OS_WIN32)
#include "xsWin32TestProcess.hpp"
#else
#include "xsPosixTestProcess.hpp"
#endif

#include <iostream>

namespace opt
{

DE_DECLARE_COMMAND_LINE_OPT(Port, int);
DE_DECLARE_COMMAND_LINE_OPT(SingleExec, bool);

void registerOptions(de::cmdline::Parser &parser)
{
    using de::cmdline::NamedValue;
    using de::cmdline::Option;

    parser << Option<Port>("p", "port", "Port", "50016")
           << Option<SingleExec>("s", "single", "Kill execserver after first session");
}

} // namespace opt

int main(int argc, const char *const *argv)
{
    de::cmdline::CommandLine cmdLine;

#if (DE_OS == DE_OS_WIN32)
    xs::Win32TestProcess testProcess;
#else
    xs::PosixTestProcess testProcess;

    // Set line buffered mode to stdout so executor gets any log messages in a timely manner.
    setvbuf(stdout, nullptr, _IOLBF, 4 * 1024);
#endif

    // Parse command line.
    {
        de::cmdline::Parser parser;
        opt::registerOptions(parser);

        if (!parser.parse(argc, argv, &cmdLine, std::cerr))
        {
            parser.help(std::cout);
            return -1;
        }
    }

    try
    {
        const xs::ExecutionServer::RunMode runMode = cmdLine.getOption<opt::SingleExec>() ?
                                                         xs::ExecutionServer::RUNMODE_SINGLE_EXEC :
                                                         xs::ExecutionServer::RUNMODE_FOREVER;
        const int port                             = cmdLine.getOption<opt::Port>();
        xs::ExecutionServer server(&testProcess, DE_SOCKETFAMILY_INET4, port, runMode);

        std::cout << "Listening on port " << port << ".\n";
        server.runServer();
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << "\n";
        return -1;
    }

    return 0;
}
