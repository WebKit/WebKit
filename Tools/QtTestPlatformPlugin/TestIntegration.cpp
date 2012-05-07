/*
 * Copyright (C) 2012 University of Szeged. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY UNIVERSITY OF SZEGED ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL UNIVERSITY OF SZEGED OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "TestIntegration.h"
#include <QCoreApplication>
#include <QStringList>
#include <QtGlobal>
#include <QtGui/private/qplatformintegrationfactory_qpa_p.h>
#include <qsystemdetection.h>

TestIntegration::TestIntegration()
{
    QString defaultPlatform =
#if defined(Q_OS_MAC)
        QLatin1String("cocoa");
#elif defined (Q_OS_WIN)
        QLatin1String("windows");
#elif !defined (QT_NO_XCB)
        QLatin1String("xcb");
#elif !defined (QT_NO_WAYLAND)
        QLatin1String("wayland");
#else
        QLatin1String("minimal");
#endif

    QByteArray originalPlatform = qgetenv("QT_WEBKIT_ORIGINAL_PLATFORM");
    QByteArray originalPluginPath = qgetenv("QT_WEBKIT_ORIGINAL_PLATFORM_PLUGIN_PATH");
    QString platform = originalPlatform.isEmpty() ? defaultPlatform : QString::fromLatin1(originalPlatform.data());
    QString pluginPath = originalPluginPath.isEmpty() ? QString() : QString::fromLatin1(originalPluginPath.data());

    m_integration.reset(QPlatformIntegrationFactory::create(platform, pluginPath));
}

#if !defined(Q_OS_MAC)
QPlatformFontDatabase* TestIntegration::fontDatabase() const
{
    return m_integration->fontDatabase();
}
#endif
