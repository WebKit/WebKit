/*
 * Copyright 2011 Hewlett-Packard Development Company, L.P. All rights reserved.
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
 * 3.  Neither the name of Hewlett-Packard nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "InitWebCoreQt.h"

#include "Image.h"
#include "NotImplemented.h"
#include "PlatformStrategiesQt.h"
#include "RenderThemeQStyle.h"
#include "ScriptController.h"
#include "ScrollbarThemeQStyle.h"
#include "SecurityPolicy.h"
#if USE(QTKIT)
#include "WebSystemInterface.h"
#endif

#include "qwebelement_p.h"
#include <JavaScriptCore/runtime/InitializeThreading.h>
#include <QApplication>
#include <QStyle>
#include <wtf/MainThread.h>

namespace WebKit {

// Called also from WebKit2's WebProcess.
Q_DECL_EXPORT void initializeWebKit2Theme()
{
    if (qgetenv("QT_WEBKIT_THEME_NAME") == "qstyle")
        WebCore::RenderThemeQt::setCustomTheme(WebCore::RenderThemeQStyle::create, new WebCore::ScrollbarThemeQStyle);
}

}

namespace WebCore {

void initializeWebCoreQt()
{
    static bool initialized = false;
    if (initialized)
        return;

    WebCore::initializeLoggingChannelsIfNecessary();
    ScriptController::initializeThreading();
    WTF::initializeMainThread();
    WebCore::SecurityPolicy::setLocalLoadPolicy(WebCore::SecurityPolicy::AllowLocalLoadsForLocalAndSubstituteData);

    PlatformStrategiesQt::initialize();
    QtWebElementRuntime::initialize();

    RenderThemeQt::setCustomTheme(RenderThemeQStyle::create, new ScrollbarThemeQStyle);

#if USE(QTKIT)
    InitWebCoreSystemInterface();
#endif

    // QWebSettings::SearchCancelButtonGraphic
    Image::setPlatformResource("searchCancelButton", QApplication::style()->standardPixmap(QStyle::SP_DialogCloseButton));
    // QWebSettings::SearchCancelButtonPressedGraphic
    Image::setPlatformResource("searchCancelButtonPressed", QApplication::style()->standardPixmap(QStyle::SP_DialogCloseButton));

    initialized = true;
}

}
