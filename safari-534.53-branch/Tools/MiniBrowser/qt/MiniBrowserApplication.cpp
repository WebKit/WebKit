/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2010 University of Szeged
 *
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "MiniBrowserApplication.h"

#include "utils.h"
#include <QRegExp>

MiniBrowserApplication::MiniBrowserApplication(int& argc, char** argv)
    : QApplication(argc, argv, QApplication::GuiServer)
    , m_windowOptions()
    , m_isRobotized(false)
    , m_robotTimeoutSeconds(0)
    , m_robotExtraTimeSeconds(0)
{
    setOrganizationName("Nokia");
    setApplicationName("QtMiniBrowser");
    setApplicationVersion("0.1");

    handleUserOptions();
}

void MiniBrowserApplication::handleUserOptions()
{
    QStringList args = arguments();
    QFileInfo program(args.at(0));
    QString programName("MiniBrowser");
    if (program.exists())
        programName = program.baseName();

    if (args.contains("-help")) {
        qDebug() << "Usage:" << programName.toLatin1().data()
             << "[-maximize]"
             << "[-r list]"
             << "[-robot-timeout seconds]"
             << "[-robot-extra-time seconds]"
             << "[-chunked-drawing-area]"
             <<  "[-separate-web-process-per-window]"
             << "[-print-loaded-urls]"
             << "URLs";
        appQuit(0);
    }

    if (args.contains("-maximize"))
        m_windowOptions.startMaximized = true;

    int robotIndex = args.indexOf("-r");
    if (robotIndex != -1) {
        QString listFile = takeOptionValue(&args, robotIndex);
        if (listFile.isEmpty())
            appQuit(1, "-r needs a list file to start in robotized mode");
        if (!QFile::exists(listFile))
            appQuit(1, "The list file supplied to -r does not exist.");

        m_isRobotized = true;
        m_urls = QStringList(listFile);
    } else {
        int lastArg = args.lastIndexOf(QRegExp("^-.*"));
        m_urls = (lastArg != -1) ? args.mid(++lastArg) : args.mid(1);
    }

    int robotTimeoutIndex = args.indexOf("-robot-timeout");
    if (robotTimeoutIndex != -1)
        m_robotTimeoutSeconds = takeOptionValue(&args, robotTimeoutIndex).toInt();

    int robotExtraTimeIndex = args.indexOf("-robot-extra-time");
    if (robotExtraTimeIndex != -1)
        m_robotExtraTimeSeconds = takeOptionValue(&args, robotExtraTimeIndex).toInt();

    if (args.contains("-chunked-drawing-area"))
        m_windowOptions.useTiledBackingStore = false;

    if (args.contains("-separate-web-process-per-window"))
        m_windowOptions.useSeparateWebProcessPerWindow = true;

    if (args.contains("-print-loaded-urls"))
        m_windowOptions.printLoadedUrls = true;
}
