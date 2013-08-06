/*
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
    Copyright (C) 2013 Apple Inc. All rights reserved.

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
#include "Logging.h"

#if !LOG_DISABLED

#include <QDebug>
#include <wtf/text/WTFString.h>

namespace WebCore {

String logLevelString()
{
    QByteArray loggingEnv = qgetenv("QT_WEBKIT_LOG");
    if (loggingEnv.isEmpty())
        return emptyString();

#if defined(NDEBUG)
    qWarning("This is a release build. Setting QT_WEBKIT_LOG will have no effect.");
#else

    // To disable logging notImplemented set the DISABLE_NI_WARNING environment variable to 1.
    String logLevel = "NotYetImplemented,";
    logLevel.append(QString::fromLocal8Bit(loggingEnv));
    return logLevel;
#endif
}

} // namespace WebCore

#endif // !LOG_DISABLED
