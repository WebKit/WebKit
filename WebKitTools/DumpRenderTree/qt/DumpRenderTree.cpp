/*
 * Copyright (C) 2005, 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2009 Torch Mobile Inc. http://www.torchmobile.com/
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

#include "DumpRenderTree.h"
#include "jsobjects.h"
#include "testplugin.h"
#include "WorkQueue.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QTimer>
#include <QBoxLayout>
#include <QScrollArea>
#include <QApplication>
#include <QUrl>
#include <QFocusEvent>
#include <QFontDatabase>
#include <QNetworkRequest>

#include <qwebpage.h>
#include <qwebframe.h>
#include <qwebview.h>
#include <qwebsettings.h>
#include <qwebsecurityorigin.h>

#ifdef Q_WS_X11
#include <fontconfig/fontconfig.h>
#endif

#include <unistd.h>
#include <qdebug.h>

extern void qt_drt_run(bool b);
extern void qt_dump_set_accepts_editing(bool b);
extern void qt_dump_frame_loader(bool b);
extern void qt_drt_clearFrameName(QWebFrame* qFrame);
extern void qt_drt_overwritePluginDirectories();

namespace WebCore {

// Choose some default values.
const unsigned int maxViewWidth = 800;
const unsigned int maxViewHeight = 600;

class WebPage : public QWebPage {
    Q_OBJECT
public:
    WebPage(QWidget *parent, DumpRenderTree *drt);

    QWebPage *createWindow(QWebPage::WebWindowType);

    void javaScriptAlert(QWebFrame *frame, const QString& message);
    void javaScriptConsoleMessage(const QString& message, int lineNumber, const QString& sourceID);
    bool javaScriptConfirm(QWebFrame *frame, const QString& msg);
    bool javaScriptPrompt(QWebFrame *frame, const QString& msg, const QString& defaultValue, QString* result);

    void resetSettings();

public slots:
    bool shouldInterruptJavaScript() { return false; }

protected:
    bool acceptNavigationRequest(QWebFrame* frame, const QNetworkRequest& request, NavigationType type);

private slots:
    void setViewGeometry(const QRect &r)
    {
        QWidget *v = view();
        if (v)
            v->setGeometry(r);
    }
private:
    DumpRenderTree *m_drt;
};

WebPage::WebPage(QWidget *parent, DumpRenderTree *drt)
    : QWebPage(parent), m_drt(drt)
{
    QWebSettings* globalSettings = QWebSettings::globalSettings();

    globalSettings->setFontSize(QWebSettings::MinimumFontSize, 5);
    globalSettings->setFontSize(QWebSettings::MinimumLogicalFontSize, 5);
    globalSettings->setFontSize(QWebSettings::DefaultFontSize, 16);
    globalSettings->setFontSize(QWebSettings::DefaultFixedFontSize, 13);

    globalSettings->setAttribute(QWebSettings::JavascriptCanOpenWindows, true);
    globalSettings->setAttribute(QWebSettings::JavascriptCanAccessClipboard, true);
    globalSettings->setAttribute(QWebSettings::LinksIncludedInFocusChain, false);
    globalSettings->setAttribute(QWebSettings::PluginsEnabled, true);
    globalSettings->setAttribute(QWebSettings::LocalContentCanAccessRemoteUrls, true);
    globalSettings->setAttribute(QWebSettings::JavascriptEnabled, true);
    globalSettings->setAttribute(QWebSettings::PrivateBrowsingEnabled, false);
    globalSettings->setAttribute(QWebSettings::OfflineWebApplicationCacheEnabled, false);

    connect(this, SIGNAL(geometryChangeRequested(const QRect &)),
            this, SLOT(setViewGeometry(const QRect & )));

    setPluginFactory(new TestPlugin(this));
}

void WebPage::resetSettings()
{
    // After each layout test, reset the settings that may have been changed by
    // layoutTestController.overridePreference() or similar.

    settings()->resetFontSize(QWebSettings::DefaultFontSize);

    settings()->resetAttribute(QWebSettings::JavascriptCanOpenWindows);
    settings()->resetAttribute(QWebSettings::JavascriptEnabled);
    settings()->resetAttribute(QWebSettings::PrivateBrowsingEnabled);
    settings()->resetAttribute(QWebSettings::LinksIncludedInFocusChain);
    settings()->resetAttribute(QWebSettings::OfflineWebApplicationCacheEnabled);
}

QWebPage *WebPage::createWindow(QWebPage::WebWindowType)
{
    return m_drt->createWindow();
}

void WebPage::javaScriptAlert(QWebFrame*, const QString& message)
{
    fprintf(stdout, "ALERT: %s\n", message.toUtf8().constData());
}

void WebPage::javaScriptConsoleMessage(const QString& message, int lineNumber, const QString&)
{
    fprintf (stdout, "CONSOLE MESSAGE: line %d: %s\n", lineNumber, message.toUtf8().constData());
}

bool WebPage::javaScriptConfirm(QWebFrame*, const QString& msg)
{
    fprintf(stdout, "CONFIRM: %s\n", msg.toUtf8().constData());
    return true;
}

bool WebPage::javaScriptPrompt(QWebFrame*, const QString& msg, const QString& defaultValue, QString* result)
{
    fprintf(stdout, "PROMPT: %s, default text: %s\n", msg.toUtf8().constData(), defaultValue.toUtf8().constData());
    *result = defaultValue;
    return true;
}

bool WebPage::acceptNavigationRequest(QWebFrame* frame, const QNetworkRequest& request, NavigationType type)
{
    if (m_drt->layoutTestController()->waitForPolicy()) {
        QString url = QString::fromUtf8(request.url().toEncoded());
        QString typeDescription;

        switch (type) {
        case NavigationTypeLinkClicked:
            typeDescription = "link clicked";
            break;
        case NavigationTypeFormSubmitted:
            typeDescription = "form submitted";
            break;
        case NavigationTypeBackOrForward:
            typeDescription = "back/forward";
            break;
        case NavigationTypeReload:
            typeDescription = "reload";
            break;
        case NavigationTypeFormResubmitted:
            typeDescription = "form resubmitted";
            break;
        case NavigationTypeOther:
            typeDescription = "other";
            break;
        default:
            typeDescription = "illegal value";
        }

        fprintf(stdout, "Policy delegate: attempt to load %s with navigation type '%s'\n",
                url.toUtf8().constData(), typeDescription.toUtf8().constData());
        m_drt->layoutTestController()->notifyDone();
    }
    return QWebPage::acceptNavigationRequest(frame, request, type);
}

DumpRenderTree::DumpRenderTree()
    : m_dumpPixels(false)
    , m_stdin(0)
    , m_notifier(0)
{
    qt_drt_overwritePluginDirectories();
    QWebSettings::enablePersistentStorage();

    m_controller = new LayoutTestController(this);
    connect(m_controller, SIGNAL(done()), this, SLOT(dump()));

    QWebView *view = new QWebView(0);
    view->resize(QSize(maxViewWidth, maxViewHeight));
    m_page = new WebPage(view, this);
    view->setPage(m_page);
    connect(m_page, SIGNAL(frameCreated(QWebFrame *)), this, SLOT(connectFrame(QWebFrame *)));
    connectFrame(m_page->mainFrame());

    connect(m_page->mainFrame(), SIGNAL(loadFinished(bool)), m_controller, SLOT(maybeDump(bool)));

    m_page->mainFrame()->setScrollBarPolicy(Qt::Horizontal, Qt::ScrollBarAlwaysOff);
    m_page->mainFrame()->setScrollBarPolicy(Qt::Vertical, Qt::ScrollBarAlwaysOff);
    connect(m_page->mainFrame(), SIGNAL(titleChanged(const QString&)),
            SLOT(titleChanged(const QString&)));
    connect(m_page, SIGNAL(databaseQuotaExceeded(QWebFrame*,QString)),
            this, SLOT(dumpDatabaseQuota(QWebFrame*,QString)));
    connect(m_page, SIGNAL(statusBarMessage(const QString&)),
            this, SLOT(statusBarMessage(const QString&)));

    m_eventSender = new EventSender(m_page);
    m_textInputController = new TextInputController(m_page);
    m_gcController = new GCController(m_page);

    QObject::connect(this, SIGNAL(quit()), qApp, SLOT(quit()), Qt::QueuedConnection);
    qt_drt_run(true);
    QFocusEvent event(QEvent::FocusIn, Qt::ActiveWindowFocusReason);
    QApplication::sendEvent(view, &event);
}

DumpRenderTree::~DumpRenderTree()
{
    delete m_page;

    delete m_stdin;
    delete m_notifier;
}

void DumpRenderTree::open()
{
    if (!m_stdin) {
        m_stdin = new QFile;
        m_stdin->open(stdin, QFile::ReadOnly);
    }

    if (!m_notifier) {
        m_notifier = new QSocketNotifier(STDIN_FILENO, QSocketNotifier::Read);
        connect(m_notifier, SIGNAL(activated(int)), this, SLOT(readStdin(int)));
    }
}

void DumpRenderTree::resetToConsistentStateBeforeTesting()
{
    closeRemainingWindows();

    // Reset so that any current loads are stopped
    m_page->blockSignals(true);
    m_page->triggerAction(QWebPage::Stop);
    m_page->blockSignals(false);

    m_page->mainFrame()->setZoomFactor(1.0);

    static_cast<WebPage*>(m_page)->resetSettings();
    qt_drt_clearFrameName(m_page->mainFrame());

    WorkQueue::shared()->clear();
    // Causes timeout, why?
    //WorkQueue::shared()->setFrozen(false);

    m_controller->reset();
    QWebSecurityOrigin::resetOriginAccessWhiteLists();

    setlocale(LC_ALL, "");
}

void DumpRenderTree::open(const QUrl& aurl)
{
    resetToConsistentStateBeforeTesting();

    QUrl url = aurl;
    m_expectedHash = QString();
    if (m_dumpPixels) {
        // single quote marks the pixel dump hash
        QString str = url.toString();
        int i = str.indexOf('\'');
        if (i > -1) {
            m_expectedHash = str.mid(i + 1, str.length());
            str.remove(i, str.length());
            url = QUrl(str);
        }
    }

    // W3C SVG tests expect to be 480x360
    bool isW3CTest = url.toString().contains("svg/W3C-SVG-1.1");
    int width = isW3CTest ? 480 : maxViewWidth;
    int height = isW3CTest ? 360 : maxViewHeight;
    m_page->view()->resize(QSize(width, height));
    m_page->setFixedContentsSize(QSize());
    m_page->setViewportSize(QSize(width, height));

    QFocusEvent ev(QEvent::FocusIn);
    m_page->event(&ev);

    QFontDatabase::removeAllApplicationFonts();
#if defined(Q_WS_X11)
    initializeFonts();
#endif

    qt_dump_frame_loader(url.toString().contains("loading/"));
    m_page->mainFrame()->load(url);
}

void DumpRenderTree::readStdin(int /* socket */)
{
    // Read incoming data from stdin...
    QByteArray line = m_stdin->readLine();
    if (line.endsWith('\n'))
        line.truncate(line.size()-1);
    //fprintf(stderr, "\n    opening %s\n", line.constData());
    if (line.isEmpty())
        quit();

    if (line.startsWith("http:") || line.startsWith("https:"))
        open(QUrl(line));
    else {
        QFileInfo fi(line);
        open(QUrl::fromLocalFile(fi.absoluteFilePath()));
    }

    fflush(stdout);
}

void DumpRenderTree::setDumpPixels(bool dump)
{
    m_dumpPixels = dump;
}

void DumpRenderTree::closeRemainingWindows()
{
    foreach(QWidget *widget, windows)
        delete widget;
    windows.clear();
}

void DumpRenderTree::initJSObjects()
{
    QWebFrame *frame = qobject_cast<QWebFrame*>(sender());
    Q_ASSERT(frame);
    frame->addToJavaScriptWindowObject(QLatin1String("layoutTestController"), m_controller);
    frame->addToJavaScriptWindowObject(QLatin1String("eventSender"), m_eventSender);
    frame->addToJavaScriptWindowObject(QLatin1String("textInputController"), m_textInputController);
    frame->addToJavaScriptWindowObject(QLatin1String("GCController"), m_gcController);
}


QString DumpRenderTree::dumpFramesAsText(QWebFrame* frame)
{
    if (!frame)
        return QString();

    QString result;
    QWebFrame *parent = qobject_cast<QWebFrame *>(frame->parent());
    if (parent) {
        result.append(QLatin1String("\n--------\nFrame: '"));
        result.append(frame->frameName());
        result.append(QLatin1String("'\n--------\n"));
    }

    result.append(frame->toPlainText());
    result.append(QLatin1String("\n"));

    if (m_controller->shouldDumpChildrenAsText()) {
        QList<QWebFrame *> children = frame->childFrames();
        for (int i = 0; i < children.size(); ++i)
            result += dumpFramesAsText(children.at(i));
    }

    return result;
}

QString DumpRenderTree::dumpBackForwardList()
{
    QString result;
    result.append(QLatin1String("\n============== Back Forward List ==============\n"));
    result.append(QLatin1String("FIXME: Unimplemented!\n"));
    result.append(QLatin1String("===============================================\n"));
    return result;
}

static const char *methodNameStringForFailedTest(LayoutTestController *controller)
{
    const char *errorMessage;
    if (controller->shouldDumpAsText())
        errorMessage = "[documentElement innerText]";
    // FIXME: Add when we have support
    //else if (controller->dumpDOMAsWebArchive())
    //    errorMessage = "[[mainFrame DOMDocument] webArchive]";
    //else if (controller->dumpSourceAsWebArchive())
    //    errorMessage = "[[mainFrame dataSource] webArchive]";
    else
        errorMessage = "[mainFrame renderTreeAsExternalRepresentation]";

    return errorMessage;
}

void DumpRenderTree::dump()
{
    QWebFrame *mainFrame = m_page->mainFrame();

    //fprintf(stderr, "    Dumping\n");
    if (!m_notifier) {
        // Dump markup in single file mode...
        QString markup = mainFrame->toHtml();
        fprintf(stdout, "Source:\n\n%s\n", markup.toUtf8().constData());
    }

    // Dump render text...
    QString resultString;
    if (m_controller->shouldDumpAsText())
        resultString = dumpFramesAsText(mainFrame);
    else
        resultString = mainFrame->renderTreeDump();

    if (!resultString.isEmpty()) {
        fprintf(stdout, "%s", resultString.toUtf8().constData());

        if (m_controller->shouldDumpBackForwardList())
            fprintf(stdout, "%s", dumpBackForwardList().toUtf8().constData());

    } else
        printf("ERROR: nil result from %s", methodNameStringForFailedTest(m_controller));

    // signal end of text block
    fputs("#EOF\n", stdout);
    fputs("#EOF\n", stderr);

    if (m_dumpPixels) {
        QImage image(m_page->viewportSize(), QImage::Format_ARGB32);
        image.fill(Qt::white);
        QPainter painter(&image);
        mainFrame->render(&painter);
        painter.end();

        QCryptographicHash hash(QCryptographicHash::Md5);
        for (int row = 0; row < image.height(); ++row)
            hash.addData(reinterpret_cast<const char*>(image.scanLine(row)), image.width() * 4);
        QString actualHash = hash.result().toHex();

        fprintf(stdout, "\nActualHash: %s\n", qPrintable(actualHash));

        bool dumpImage = true;

        if (!m_expectedHash.isEmpty()) {
            Q_ASSERT(m_expectedHash.length() == 32);
            fprintf(stdout, "\nExpectedHash: %s\n", qPrintable(m_expectedHash));

            if (m_expectedHash == actualHash)
                dumpImage = false;
        }

        if (dumpImage) {
            QBuffer buffer;
            buffer.open(QBuffer::WriteOnly);
            image.save(&buffer, "PNG");
            buffer.close();
            const QByteArray &data = buffer.data();

            printf("Content-Type: %s\n", "image/png");
            printf("Content-Length: %lu\n", static_cast<unsigned long>(data.length()));

            const char *ptr = data.data();
            for(quint32 left = data.length(); left; ) {
                quint32 block = qMin(left, quint32(1 << 15));
                quint32 written = fwrite(ptr, 1, block, stdout);
                ptr += written;
                left -= written;
                if (written == block)
                    break;
            }
        }

        fflush(stdout);
    }

    puts("#EOF");   // terminate the (possibly empty) pixels block

    fflush(stdout);
    fflush(stderr);

    if (!m_notifier)
        quit(); // Exit now in single file mode...
}

void DumpRenderTree::titleChanged(const QString &s)
{
    if (m_controller->shouldDumpTitleChanges())
        printf("TITLE CHANGED: %s\n", s.toUtf8().data());
}

void DumpRenderTree::connectFrame(QWebFrame *frame)
{
    connect(frame, SIGNAL(javaScriptWindowObjectCleared()), this, SLOT(initJSObjects()));
    connect(frame, SIGNAL(provisionalLoad()),
            layoutTestController(), SLOT(provisionalLoad()));
}

void DumpRenderTree::dumpDatabaseQuota(QWebFrame* frame, const QString& dbName)
{
    if (!m_controller->shouldDumpDatabaseCallbacks())
        return;
    QWebSecurityOrigin origin = frame->securityOrigin();
    printf("UI DELEGATE DATABASE CALLBACK: exceededDatabaseQuotaForSecurityOrigin:{%s, %s, %i} database:%s\n",
           origin.scheme().toUtf8().data(),
           origin.host().toUtf8().data(),
           origin.port(),
           dbName.toUtf8().data());
    origin.setDatabaseQuota(5 * 1024 * 1024);
}

void DumpRenderTree::statusBarMessage(const QString& message)
{
    if (!m_controller->shouldDumpStatusCallbacks())
        return;

    printf("UI DELEGATE STATUS CALLBACK: setStatusText:%s\n", message.toUtf8().constData());
}

QWebPage *DumpRenderTree::createWindow()
{
    if (!m_controller->canOpenWindows())
        return 0;
    QWidget *container = new QWidget(0);
    container->resize(0, 0);
    container->move(-1, -1);
    container->hide();
    QWebPage *page = new WebPage(container, this);
    connectFrame(page->mainFrame());
    connect(m_page, SIGNAL(frameCreated(QWebFrame *)), this, SLOT(connectFrame(QWebFrame *)));
    windows.append(container);
    return page;
}

int DumpRenderTree::windowCount() const
{
    int count = 0;
    foreach(QWidget *w, windows) {
        if (w->children().count())
            ++count;
    }
    return count + 1;
}

#if defined(Q_WS_X11)
void DumpRenderTree::initializeFonts()
{
    static int numFonts = -1;

    // Some test cases may add or remove application fonts (via @font-face).
    // Make sure to re-initialize the font set if necessary.
    FcFontSet* appFontSet = FcConfigGetFonts(0, FcSetApplication);
    if (appFontSet && numFonts >= 0 && appFontSet->nfont == numFonts)
        return;

    QByteArray fontDir = getenv("WEBKIT_TESTFONTS");
    if (fontDir.isEmpty() || !QDir(fontDir).exists()) {
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
    getcwd(currentPath, PATH_MAX);
    QByteArray configFile = currentPath;
    FcConfig *config = FcConfigCreate();
    configFile += "/WebKitTools/DumpRenderTree/qt/fonts.conf";
    if (!FcConfigParseAndLoad (config, (FcChar8*) configFile.data(), true))
        qFatal("Couldn't load font configuration file");
    if (!FcConfigAppFontAddDir (config, (FcChar8*) fontDir.data()))
        qFatal("Couldn't add font dir!");
    FcConfigSetCurrent(config);

    appFontSet = FcConfigGetFonts(config, FcSetApplication);
    numFonts = appFontSet->nfont;
}
#endif

}

#include "DumpRenderTree.moc"
