/*
    Copyright (C) 2008, 2009, 2012 Nokia Corporation and/or its subsidiary(-ies)
    Copyright (C) 2007 Staikos Computing Services Inc.
    Copyright (C) 2007 Apple Inc.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/
#include "config.h"

#include "UserAgentQt.h"

#include <QCoreApplication>

#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>
#if defined Q_OS_WIN32
#include <SystemInfo.h>
#endif // Q_OS_WIN32

namespace WebCore {

/*!
    This function is called when a user agent for HTTP requests is needed.

    This implementation returns the following value:

    "Mozilla/5.0 (%Platform%%Security%%Subplatform%) AppleWebKit/%WebKitVersion% (KHTML, like Gecko) %AppVersion Safari/%WebKitVersion%"

    In this string the following values are replaced the first time the function is called:
    \list
    \li %Platform% expands to the windowing system followed by "; " if it is not Windows (e.g. "X11; ").
    \li %Security% expands to "N; " if SSL is disabled.
    \li %Subplatform% expands to the operating system version (e.g. "Windows NT 6.1" or "Intel Mac OS X 10.5").
    \li %WebKitVersion% is the version of WebKit the application was compiled against.
    /endlist

    The following value is replaced each time the funciton is called
    \list
    \li %AppVersion% expands to QCoreApplication::applicationName()/QCoreApplication::applicationVersion() if they're set; otherwise defaulting to Qt and the current Qt version.
    \endlist
*/
String UserAgentQt::standardUserAgent(const String &applicationNameForUserAgent, unsigned int webkitMajorVersion, unsigned int webkitMinorVersion)
{
    static QString ua;

    if (ua.isNull()) {

        ua = QLatin1String("Mozilla/5.0 (%1%2%3) AppleWebKit/%4 (KHTML, like Gecko) %99 Safari/%5");

        // Platform.
        ua = ua.arg(QLatin1String(
#ifdef Q_WS_MAC
            "Macintosh; "
#elif defined Q_WS_QWS
            "QtEmbedded; "
#elif defined Q_WS_WIN
            // Nothing.
#elif defined Q_WS_X11
            "X11; "
#else
            "Unknown; "
#endif
        ));

        // Security strength.
        QString securityStrength;
#if defined(QT_NO_OPENSSL)
        securityStrength = QLatin1String("N; ");
#endif
        ua = ua.arg(securityStrength);

        // Operating system.
        ua = ua.arg(QLatin1String(
#ifdef Q_OS_AIX
            "AIX"
#elif defined Q_OS_WIN32
            windowsVersionForUAString().latin1().data()
#elif defined Q_OS_DARWIN
#ifdef __i386__ || __x86_64__
            "Intel Mac OS X"
#else
            "PPC Mac OS X"
#endif

#elif defined Q_OS_BSDI
            "BSD"
#elif defined Q_OS_BSD4
            "BSD Four"
#elif defined Q_OS_CYGWIN
            "Cygwin"
#elif defined Q_OS_DGUX
            "DG/UX"
#elif defined Q_OS_DYNIX
            "DYNIX/ptx"
#elif defined Q_OS_FREEBSD
            "FreeBSD"
#elif defined Q_OS_HPUX
            "HP-UX"
#elif defined Q_OS_HURD
            "GNU Hurd"
#elif defined Q_OS_IRIX
            "SGI Irix"
#elif defined Q_OS_LINUX

#if defined(__x86_64__)
            "Linux x86_64"
#elif defined(__i386__)
            "Linux i686"
#else
            "Linux"
#endif

#elif defined Q_OS_LYNX
            "LynxOS"
#elif defined Q_OS_NETBSD
            "NetBSD"
#elif defined Q_OS_OS2
            "OS/2"
#elif defined Q_OS_OPENBSD
            "OpenBSD"
#elif defined Q_OS_OS2EMX
            "OS/2"
#elif defined Q_OS_OSF
            "HP Tru64 UNIX"
#elif defined Q_OS_QNX6
            "QNX RTP Six"
#elif defined Q_OS_QNX
            "QNX"
#elif defined Q_OS_RELIANT
            "Reliant UNIX"
#elif defined Q_OS_SCO
            "SCO OpenServer"
#elif defined Q_OS_SOLARIS
            "Sun Solaris"
#elif defined Q_OS_ULTRIX
            "DEC Ultrix"
#elif defined Q_OS_UNIX
            "UNIX BSD/SYSV system"
#elif defined Q_OS_UNIXWARE
            "UnixWare Seven, Open UNIX Eight"
#else
            "Unknown"
#endif
        ));

        // WebKit version.
        // FIXME "+" in the version string?
        QString version = QString(QLatin1String("%1.%2")).arg(QString::number(webkitMajorVersion),
                                                               QString::number(webkitMinorVersion));
        ua = ua.arg(version, version);
    }

    QString appName;
    if (applicationNameForUserAgent.isEmpty())
        appName = QCoreApplication::applicationName();
    else
        appName = applicationNameForUserAgent;

    if (!appName.isEmpty()) {
        QString appVer = QCoreApplication::applicationVersion();
        if (!appVer.isEmpty())
            appName.append(QLatin1Char('/') + appVer);
    } else {
        // Qt version.
        appName = QLatin1String("Qt/") + QLatin1String(qVersion());
    }

    return ua.arg(appName);
}

}
