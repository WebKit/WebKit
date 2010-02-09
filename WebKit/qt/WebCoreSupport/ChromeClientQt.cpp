/*
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#include "config.h"
#include "ChromeClientQt.h"

#include "FileChooser.h"
#include "Frame.h"
#include "FrameLoadRequest.h"
#include "FrameLoader.h"
#include "FrameLoaderClientQt.h"
#include "FrameView.h"
#include "HitTestResult.h"
#include "NotImplemented.h"
#include "WindowFeatures.h"
#include "DatabaseTracker.h"
#include "QtFallbackWebPopup.h"
#include "QWebPageClient.h"
#include "SecurityOrigin.h"

#include <qdebug.h>
#include <qeventloop.h>
#include <qtextdocument.h>
#include <qtooltip.h>

#include "qwebpage.h"
#include "qwebpage_p.h"
#include "qwebframe_p.h"
#include "qwebsecurityorigin.h"
#include "qwebsecurityorigin_p.h"
#include "qwebview.h"

#if USE(ACCELERATED_COMPOSITING)
#include "GraphicsLayerQt.h"
#endif

namespace WebCore {

ChromeClientQt::ChromeClientQt(QWebPage* webPage)
    : m_webPage(webPage)
    , m_eventLoop(0)
{
    toolBarsVisible = statusBarVisible = menuBarVisible = true;
}

ChromeClientQt::~ChromeClientQt()
{
    if (m_eventLoop)
        m_eventLoop->exit();
}

void ChromeClientQt::setWindowRect(const FloatRect& rect)
{
    if (!m_webPage)
        return;
    emit m_webPage->geometryChangeRequested(QRect(qRound(rect.x()), qRound(rect.y()),
                            qRound(rect.width()), qRound(rect.height())));
}


FloatRect ChromeClientQt::windowRect()
{
    if (!m_webPage)
        return FloatRect();

    QWidget* view = m_webPage->view();
    if (!view)
        return FloatRect();
    return IntRect(view->topLevelWidget()->geometry());
}


FloatRect ChromeClientQt::pageRect()
{
    if (!m_webPage)
        return FloatRect();
    return FloatRect(QRectF(QPointF(0,0), m_webPage->viewportSize()));
}


float ChromeClientQt::scaleFactor()
{
    notImplemented();
    return 1;
}


void ChromeClientQt::focus()
{
    if (!m_webPage)
        return;
    QWidget* view = m_webPage->view();
    if (!view)
        return;

    view->setFocus();
}


void ChromeClientQt::unfocus()
{
    if (!m_webPage)
        return;
    QWidget* view = m_webPage->view();
    if (!view)
        return;
    view->clearFocus();
}

bool ChromeClientQt::canTakeFocus(FocusDirection)
{
    // This is called when cycling through links/focusable objects and we
    // reach the last focusable object. Then we want to claim that we can
    // take the focus to avoid wrapping.
    return true;
}

void ChromeClientQt::takeFocus(FocusDirection)
{
    // don't do anything. This is only called when cycling to links/focusable objects,
    // which in turn is called from focusNextPrevChild. We let focusNextPrevChild
    // call QWidget::focusNextPrevChild accordingly, so there is no need to do anything
    // here.
}


void ChromeClientQt::focusedNodeChanged(WebCore::Node*)
{
}


Page* ChromeClientQt::createWindow(Frame*, const FrameLoadRequest& request, const WindowFeatures& features)
{
    QWebPage *newPage = m_webPage->createWindow(features.dialog ? QWebPage::WebModalDialog : QWebPage::WebBrowserWindow);
    if (!newPage)
        return 0;
    newPage->mainFrame()->load(request.resourceRequest().url());
    return newPage->d->page;
}

void ChromeClientQt::show()
{
    if (!m_webPage)
        return;
    QWidget* view = m_webPage->view();
    if (!view)
        return;
    view->topLevelWidget()->show();
}


bool ChromeClientQt::canRunModal()
{
    return true;
}


void ChromeClientQt::runModal()
{
    m_eventLoop = new QEventLoop();
    QEventLoop* eventLoop = m_eventLoop;
    m_eventLoop->exec();
    delete eventLoop;
}


void ChromeClientQt::setToolbarsVisible(bool visible)
{
    toolBarsVisible = visible;
    emit m_webPage->toolBarVisibilityChangeRequested(visible);
}


bool ChromeClientQt::toolbarsVisible()
{
    return toolBarsVisible;
}


void ChromeClientQt::setStatusbarVisible(bool visible)
{
    emit m_webPage->statusBarVisibilityChangeRequested(visible);
    statusBarVisible = visible;
}


bool ChromeClientQt::statusbarVisible()
{
    return statusBarVisible;
    return false;
}


void ChromeClientQt::setScrollbarsVisible(bool)
{
    notImplemented();
}


bool ChromeClientQt::scrollbarsVisible()
{
    notImplemented();
    return true;
}


void ChromeClientQt::setMenubarVisible(bool visible)
{
    menuBarVisible = visible;
    emit m_webPage->menuBarVisibilityChangeRequested(visible);
}

bool ChromeClientQt::menubarVisible()
{
    return menuBarVisible;
}

void ChromeClientQt::setResizable(bool)
{
    notImplemented();
}

void ChromeClientQt::addMessageToConsole(MessageSource, MessageType, MessageLevel, const String& message,
                                         unsigned int lineNumber, const String& sourceID)
{
    QString x = message;
    QString y = sourceID;
    m_webPage->javaScriptConsoleMessage(x, lineNumber, y);
}

void ChromeClientQt::chromeDestroyed()
{
    delete this;
}

bool ChromeClientQt::canRunBeforeUnloadConfirmPanel()
{
    return true;
}

bool ChromeClientQt::runBeforeUnloadConfirmPanel(const String& message, Frame* frame)
{
    return runJavaScriptConfirm(frame, message);
}

void ChromeClientQt::closeWindowSoon()
{
    m_webPage->mainFrame()->d->frame->loader()->stopAllLoaders();
    emit m_webPage->windowCloseRequested();
}

void ChromeClientQt::runJavaScriptAlert(Frame* f, const String& msg)
{
    QString x = msg;
    FrameLoaderClientQt *fl = static_cast<FrameLoaderClientQt*>(f->loader()->client());
    m_webPage->javaScriptAlert(fl->webFrame(), x);
}

bool ChromeClientQt::runJavaScriptConfirm(Frame* f, const String& msg)
{
    QString x = msg;
    FrameLoaderClientQt *fl = static_cast<FrameLoaderClientQt*>(f->loader()->client());
    return m_webPage->javaScriptConfirm(fl->webFrame(), x);
}

bool ChromeClientQt::runJavaScriptPrompt(Frame* f, const String& message, const String& defaultValue, String& result)
{
    QString x = result;
    FrameLoaderClientQt *fl = static_cast<FrameLoaderClientQt*>(f->loader()->client());
    bool rc = m_webPage->javaScriptPrompt(fl->webFrame(), (QString)message, (QString)defaultValue, &x);

    // Fix up a quirk in the QInputDialog class. If no input happened the string should be empty
    // but it is null. See https://bugs.webkit.org/show_bug.cgi?id=30914.
    if (rc && x.isNull())
        result = String("");
    else
        result = x;

    return rc;
}

void ChromeClientQt::setStatusbarText(const String& msg)
{
    QString x = msg;
    emit m_webPage->statusBarMessage(x);
}

bool ChromeClientQt::shouldInterruptJavaScript()
{
    bool shouldInterrupt = false;
    QMetaObject::invokeMethod(m_webPage, "shouldInterruptJavaScript", Qt::DirectConnection, Q_RETURN_ARG(bool, shouldInterrupt));
    return shouldInterrupt;
}

bool ChromeClientQt::tabsToLinks() const
{
    return m_webPage->settings()->testAttribute(QWebSettings::LinksIncludedInFocusChain);
}

IntRect ChromeClientQt::windowResizerRect() const
{
    return IntRect();
}

void ChromeClientQt::repaint(const IntRect& windowRect, bool contentChanged, bool, bool)
{
    // No double buffer, so only update the QWidget if content changed.
    if (contentChanged) {
        if (platformPageClient()) {
            QRect rect(windowRect);
            rect = rect.intersected(QRect(QPoint(0, 0), m_webPage->viewportSize()));
            if (!rect.isEmpty())
                platformPageClient()->update(rect);
        }
        emit m_webPage->repaintRequested(windowRect);
    }

    // FIXME: There is no "immediate" support for window painting.  This should be done always whenever the flag
    // is set.
}

void ChromeClientQt::scroll(const IntSize& delta, const IntRect& scrollViewRect, const IntRect&)
{
    if (platformPageClient())
        platformPageClient()->scroll(delta.width(), delta.height(), scrollViewRect);
    emit m_webPage->scrollRequested(delta.width(), delta.height(), scrollViewRect);
}

IntRect ChromeClientQt::windowToScreen(const IntRect& rect) const
{
    notImplemented();
    return rect;
}

IntPoint ChromeClientQt::screenToWindow(const IntPoint& point) const
{
    notImplemented();
    return point;
}

PlatformPageClient ChromeClientQt::platformPageClient() const
{
    return m_webPage->d->client;
}

void ChromeClientQt::contentsSizeChanged(Frame* frame, const IntSize& size) const
{
    emit QWebFramePrivate::kit(frame)->contentsSizeChanged(size);
}

void ChromeClientQt::mouseDidMoveOverElement(const HitTestResult& result, unsigned)
{
    TextDirection dir;
    if (result.absoluteLinkURL() != lastHoverURL
        || result.title(dir) != lastHoverTitle
        || result.textContent() != lastHoverContent) {
        lastHoverURL = result.absoluteLinkURL();
        lastHoverTitle = result.title(dir);
        lastHoverContent = result.textContent();
        emit m_webPage->linkHovered(lastHoverURL.prettyURL(),
                lastHoverTitle, lastHoverContent);
    }
}

void ChromeClientQt::setToolTip(const String &tip, TextDirection)
{
#ifndef QT_NO_TOOLTIP
    QWidget* view = m_webPage->view();
    if (!view)
        return;

    if (tip.isEmpty()) {
        view->setToolTip(QString());
        QToolTip::hideText();
    } else {
        QString dtip = QLatin1String("<p>") + Qt::escape(tip) + QLatin1String("</p>");
        view->setToolTip(dtip);
    }
#else
    Q_UNUSED(tip);
#endif
}

void ChromeClientQt::print(Frame *frame)
{
    emit m_webPage->printRequested(QWebFramePrivate::kit(frame));
}

#if ENABLE(DATABASE)
void ChromeClientQt::exceededDatabaseQuota(Frame* frame, const String& databaseName)
{
    quint64 quota = QWebSettings::offlineStorageDefaultQuota();

    if (!DatabaseTracker::tracker().hasEntryForOrigin(frame->document()->securityOrigin()))
        DatabaseTracker::tracker().setQuota(frame->document()->securityOrigin(), quota);

    emit m_webPage->databaseQuotaExceeded(QWebFramePrivate::kit(frame), databaseName);
}
#endif

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
void ChromeClientQt::reachedMaxAppCacheSize(int64_t)
{
    // FIXME: Free some space.
    notImplemented();
}
#endif

void ChromeClientQt::runOpenPanel(Frame* frame, PassRefPtr<FileChooser> prpFileChooser)
{
    RefPtr<FileChooser> fileChooser = prpFileChooser;
    bool supportMulti = m_webPage->supportsExtension(QWebPage::ChooseMultipleFilesExtension);

    if (fileChooser->allowsMultipleFiles() && supportMulti) {
        QWebPage::ChooseMultipleFilesExtensionOption option;
        option.parentFrame = QWebFramePrivate::kit(frame);

        if (!fileChooser->filenames().isEmpty())
            for (unsigned i = 0; i < fileChooser->filenames().size(); ++i)
                option.suggestedFileNames += fileChooser->filenames()[i];

        QWebPage::ChooseMultipleFilesExtensionReturn output;
        m_webPage->extension(QWebPage::ChooseMultipleFilesExtension, &option, &output);

        if (!output.fileNames.isEmpty()) {
            Vector<String> names;
            for (int i = 0; i < output.fileNames.count(); ++i)
                names.append(output.fileNames.at(i));
            fileChooser->chooseFiles(names);
        }
    } else {
        QString suggestedFile;
        if (!fileChooser->filenames().isEmpty())
            suggestedFile = fileChooser->filenames()[0];
        QString file = m_webPage->chooseFile(QWebFramePrivate::kit(frame), suggestedFile);
        if (!file.isEmpty())
            fileChooser->chooseFile(file);
    }
}

bool ChromeClientQt::setCursor(PlatformCursorHandle)
{
    notImplemented();
    return false;
}

void ChromeClientQt::requestGeolocationPermissionForFrame(Frame*, Geolocation*)
{
    // See the comment in WebCore/page/ChromeClient.h
    notImplemented();
}

#if USE(ACCELERATED_COMPOSITING)
void ChromeClientQt::attachRootGraphicsLayer(Frame* frame, GraphicsLayer* graphicsLayer)
{    
    if (platformPageClient())
        platformPageClient()->setRootGraphicsLayer(graphicsLayer ? graphicsLayer->nativeLayer() : 0);
}

void ChromeClientQt::setNeedsOneShotDrawingSynchronization()
{
    // we want the layers to synchronize next time we update the screen anyway
    if (platformPageClient())
        platformPageClient()->markForSync(false);
}

void ChromeClientQt::scheduleCompositingLayerSync()
{
    // we want the layers to synchronize ASAP
    if (platformPageClient())
        platformPageClient()->markForSync(true);
}
#endif

QtAbstractWebPopup* ChromeClientQt::createSelectPopup()
{
    return new QtFallbackWebPopup;
}

}
