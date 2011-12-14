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

#ifndef MiniBrowserApplication_h
#define MiniBrowserApplication_h

#include <QStringList>
#include <QtGui>

struct WindowOptions {
    WindowOptions()
        : useTiledBackingStore(true)
        , useSeparateWebProcessPerWindow(false)
        , printLoadedUrls(false)
#if defined(Q_OS_SYMBIAN)
        , startMaximized(true)
#else
        , startMaximized(false)
#endif
    {
    }

    bool useTiledBackingStore;
    bool useSeparateWebProcessPerWindow;
    bool printLoadedUrls;
    bool startMaximized;
};

class MiniBrowserApplication : public QApplication {
    Q_OBJECT

public:
    MiniBrowserApplication(int& argc, char** argv);
    QStringList urls() const { return m_urls; }
    bool isRobotized() const { return m_isRobotized; }
    int robotTimeout() const { return m_robotTimeoutSeconds; }
    int robotExtraTime() const { return m_robotExtraTimeSeconds; }

    WindowOptions m_windowOptions;

private:
    void handleUserOptions();

private:
    bool m_isRobotized;
    int m_robotTimeoutSeconds;
    int m_robotExtraTimeSeconds;
    QStringList m_urls;
};

#endif
