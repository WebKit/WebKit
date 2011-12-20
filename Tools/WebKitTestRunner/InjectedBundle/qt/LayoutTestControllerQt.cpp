/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 University of Szeged. All rights reserved.
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
#include "LayoutTestController.h"

#include "ActivateFonts.h"
#include "InjectedBundle.h"
#include <QDir>
#include <QFontDatabase>
#include <QObject>
#include <qwebsettings.h>

namespace WTR {

class WatchdogTimerHelper : public QObject {
    Q_OBJECT

public:
    static WatchdogTimerHelper* instance()
    {
        static WatchdogTimerHelper* theInstance = new WatchdogTimerHelper;
        return theInstance;
    }

public slots:
    void timerFired()
    {
        InjectedBundle::shared().layoutTestController()->waitToDumpWatchdogTimerFired();
    }

private:
    WatchdogTimerHelper() {}
};

void LayoutTestController::platformInitialize()
{
    // Make WebKit2 mimic the behaviour of DumpRenderTree, which is incorrect,
    // but tests are successfully passed. On the long run, Qt will move to QRawFont,
    // which makes the use of QFontDatabase unnecessary.
    // See https://bugs.webkit.org/show_bug.cgi?id=53427
    QWebSettings::clearMemoryCaches();
#if !(QT_VERSION <= QT_VERSION_CHECK(4, 6, 2))
    QFontDatabase::removeAllApplicationFonts();
#endif
    activateFonts();
    QObject::connect(&m_waitToDumpWatchdogTimer, SIGNAL(timeout()), WatchdogTimerHelper::instance(), SLOT(timerFired()));
}

void LayoutTestController::invalidateWaitToDumpWatchdogTimer()
{
    m_waitToDumpWatchdogTimer.stop();
}

void LayoutTestController::initializeWaitToDumpWatchdogTimerIfNeeded()
{
    if (qgetenv("QT_WEBKIT2_DEBUG") == "1")
        return;

    if (m_waitToDumpWatchdogTimer.isActive())
        return;

    m_waitToDumpWatchdogTimer.start(waitToDumpWatchdogTimerInterval * 1000);
}

JSRetainPtr<JSStringRef> LayoutTestController::pathToLocalResource(JSStringRef url)
{
    QString path = QDir::toNativeSeparators(QString(reinterpret_cast<const QChar*>(JSStringGetCharactersPtr(url)), JSStringGetLength(url)));
    return JSStringCreateWithCharacters(reinterpret_cast<const JSChar*>(path.constData()), path.length());
}

JSRetainPtr<JSStringRef> LayoutTestController::platformName()
{
    JSRetainPtr<JSStringRef> platformName(Adopt, JSStringCreateWithUTF8CString("qt"));
    return platformName;
}

} // namespace WTR

#include "LayoutTestControllerQt.moc"
