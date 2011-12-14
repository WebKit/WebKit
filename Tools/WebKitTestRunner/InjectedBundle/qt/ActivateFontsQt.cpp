/*
 * Copyright (C) 2005, 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ActivateFonts.h"

#include <QApplication>
#include <QByteArray>
#include <QDir>
#include <QFont>
#include <QWidget>
#include <QWindowsStyle>

#if HAVE(FONTCONFIG)
#include <fontconfig/fontconfig.h>
#endif

#include <limits.h>

namespace WTR {

void activateFonts()
{
#if HAVE(FONTCONFIG)
    FcInit();

    static int numFonts = -1;

    // Some test cases may add or remove application fonts (via @font-face).
    // Make sure to re-initialize the font set if necessary.
    FcFontSet* appFontSet = FcConfigGetFonts(0, FcSetApplication);
    if (appFontSet && numFonts >= 0 && appFontSet->nfont == numFonts)
        return;

    char* const fontDir = getenv("WEBKIT_TESTFONTS");
    const QString fontDirString = QString::fromLocal8Bit(fontDir);
    if (fontDirString.isEmpty() || !QDir(fontDirString).exists()) {
        fprintf(stderr,
                "\n\n"
                "----------------------------------------------------------------------\n"
                "WEBKIT_TESTFONTS environment variable is not set correctly.\n"
                "This variable has to point to the directory containing the fonts\n"
                "you can clone from git://gitorious.org/qtwebkit/testfonts.git\n"
                "----------------------------------------------------------------------\n"
               );
        exit(1);
    }
    char currentPath[PATH_MAX+1];
    if (!getcwd(currentPath, PATH_MAX))
        qFatal("Couldn't get current working directory");
    QByteArray configFile = currentPath;
    FcConfig* config = FcConfigCreate();
    configFile += "/Tools/DumpRenderTree/qt/fonts.conf";
    if (!FcConfigParseAndLoad (config, (FcChar8*) configFile.data(), true))
        qFatal("Couldn't load font configuration file");
    if (!FcConfigAppFontAddDir (config, (FcChar8*) fontDir))
        qFatal("Couldn't add font dir!");
    FcConfigSetCurrent(config);

    appFontSet = FcConfigGetFonts(config, FcSetApplication);
    numFonts = appFontSet->nfont;
#endif

    QApplication::setGraphicsSystem(QLatin1String("raster"));
#if HAVE(QSTYLE)
    QApplication::setStyle(new QWindowsStyle);
#endif

#if defined(Q_WS_X11)
    QX11Info::setAppDpiX(0, 96);
    QX11Info::setAppDpiY(0, 96);
#endif

   /*
    * QApplication will initialize the default application font based
    * on the application DPI at construction time, which might be
    * different from the DPI we explicitly set using QX11Info above.
    * See: https://bugreports.qt.nokia.com/browse/QTBUG-21603
    *
    * To ensure that the application font DPI matches the application
    * DPI, we override the application font using the font we get from
    * a QWidget, which has already been resolved against the existing
    * default font, but with the correct paint-device DPI.
   */
    QApplication::setFont(QWidget().font());
}

}
