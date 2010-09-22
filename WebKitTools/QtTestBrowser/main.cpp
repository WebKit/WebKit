/*
 * Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2009 Girish Ramakrishnan <girish@forwardbias.in>
 * Copyright (C) 2006 George Staikos <staikos@kde.org>
 * Copyright (C) 2006 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 Simon Hausmann <hausmann@kde.org>
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

#include "launcherwindow.h"
#include "urlloader.h"

WindowOptions windowOptions;

int launcherMain(const QApplication& app)
{
#ifndef NDEBUG
    int retVal = app.exec();
    DumpRenderTreeSupportQt::garbageCollectorCollect();
    QWebSettings::clearMemoryCaches();
    return retVal;
#else
    return app.exec();
#endif
}

class LauncherApplication : public QApplication {
    Q_OBJECT

public:
    LauncherApplication(int& argc, char** argv);
    QStringList urls() const { return m_urls; }
    bool isRobotized() const { return m_isRobotized; }
    int robotTimeout() const { return m_robotTimeoutSeconds; }
    int robotExtraTime() const { return m_robotExtraTimeSeconds; }

private:
    void handleUserOptions();
    void applyDefaultSettings();

private:
    bool m_isRobotized;
    int m_robotTimeoutSeconds;
    int m_robotExtraTimeSeconds;
    QStringList m_urls;
};

void LauncherApplication::applyDefaultSettings()
{
    QWebSettings::setMaximumPagesInCache(4);

    QWebSettings::setObjectCacheCapacities((16*1024*1024) / 8, (16*1024*1024) / 8, 16*1024*1024);

    QWebSettings::globalSettings()->setAttribute(QWebSettings::PluginsEnabled, true);
    QWebSettings::globalSettings()->setAttribute(QWebSettings::DeveloperExtrasEnabled, true);
    QWebSettings::enablePersistentStorage();
}

LauncherApplication::LauncherApplication(int& argc, char** argv)
    : QApplication(argc, argv, QApplication::GuiServer)
    , m_isRobotized(false)
    , m_robotTimeoutSeconds(0)
    , m_robotExtraTimeSeconds(0)
{
    // To allow QWebInspector's configuration persistence
    setOrganizationName("Nokia");
    setApplicationName("QtTestBrowser");
    setApplicationVersion("0.1");

    applyDefaultSettings();

    handleUserOptions();
}

static void requiresGraphicsView(const QString& option)
{
    if (windowOptions.useGraphicsView)
        return;
    appQuit(1, QString("%1 only works in combination with the -graphicsbased option").arg(option));
}

void LauncherApplication::handleUserOptions()
{
    QStringList args = arguments();
    QFileInfo program(args.at(0));
    QString programName("QtTestBrowser");
    if (program.exists())
        programName = program.baseName();

    QList<QString> updateModes(enumToKeys(QGraphicsView::staticMetaObject,
            "ViewportUpdateMode", "ViewportUpdate"));

    if (args.contains("-help")) {
        qDebug() << "Usage:" << programName.toLatin1().data()
             << "[-graphicsbased]"
             << "[-no-compositing]"
             << QString("[-viewport-update-mode %1]").arg(formatKeys(updateModes)).toLatin1().data()
             << "[-cache-webview]"
             << "[-show-fps]"
             << "[-r list]"
             << "[-robot-timeout seconds]"
             << "[-robot-extra-time seconds]"
             << "[-inspector-url location]"
             << "[-tiled-backing-store]"
             << "[-resizes-to-contents]"
             << "URLs";
        appQuit(0);
    }

    const bool defaultForAnimations = args.contains("-default-animations");
    if (args.contains("-graphicsbased") || defaultForAnimations)
        windowOptions.useGraphicsView = true;

    if (args.contains("-no-compositing")) {
        requiresGraphicsView("-no-compositing");
        windowOptions.useCompositing = false;
    }

    if (args.contains("-show-fps")) {
        requiresGraphicsView("-show-fps");
        windowOptions.showFrameRate = true;
    }

    if (args.contains("-cache-webview") || defaultForAnimations) {
        requiresGraphicsView("-cache-webview");
        windowOptions.cacheWebView = true;
    }

    if (args.contains("-tiled-backing-store")) {
        requiresGraphicsView("-tiled-backing-store");
        windowOptions.useTiledBackingStore = true;
    }

    if (args.contains("-resizes-to-contents")) {
        requiresGraphicsView("-resizes-to-contents");
        windowOptions.resizesToContents = true;
    }

    if (defaultForAnimations)
        windowOptions.viewportUpdateMode = QGraphicsView::BoundingRectViewportUpdate;

    QString arg1("-viewport-update-mode");
    int modeIndex = args.indexOf(arg1);
    if (modeIndex != -1) {
        requiresGraphicsView(arg1);

        QString mode = takeOptionValue(&args, modeIndex);
        if (mode.isEmpty())
            appQuit(1, QString("%1 needs a value of one of [%2]").arg(arg1).arg(formatKeys(updateModes)));
        int idx = updateModes.indexOf(mode);
        if (idx == -1)
            appQuit(1, QString("%1 value has to be one of [%2]").arg(arg1).arg(formatKeys(updateModes)));

        windowOptions.viewportUpdateMode = static_cast<QGraphicsView::ViewportUpdateMode>(idx);
    }
#ifdef QT_CONFIGURED_WITH_OPENGL
    if (args.contains("-gl-viewport") || defaultForAnimations) {
        requiresGraphicsView("-gl-viewport");
        windowOptions.useQGLWidgetViewport = true;
    }
#endif

    QString inspectorUrlArg("-inspector-url");
    int inspectorUrlIndex = args.indexOf(inspectorUrlArg);
    if (inspectorUrlIndex != -1)
       windowOptions.inspectorUrl = takeOptionValue(&args, inspectorUrlIndex);

    QString remoteInspectorPortArg("-remote-inspector-port");
    int remoteInspectorPortIndex = args.indexOf(remoteInspectorPortArg);
    if (remoteInspectorPortIndex != -1)
        windowOptions.remoteInspectorPort = takeOptionValue(&args, remoteInspectorPortIndex).toInt();

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
}


int main(int argc, char **argv)
{
    LauncherApplication app(argc, argv);

    if (app.isRobotized()) {
        LauncherWindow* window = new LauncherWindow();
        UrlLoader loader(window->page()->mainFrame(), app.urls().at(0), app.robotTimeout(), app.robotExtraTime());
        loader.loadNext();
        window->show();
        return launcherMain(app);
    }

    QStringList urls = app.urls();

    if (urls.isEmpty()) {
        QString defaultUrl = QString("file://%1/%2").arg(QDir::homePath()).arg(QLatin1String("index.html"));
        if (QDir(defaultUrl).exists())
            urls.append(defaultUrl);
        else
            urls.append("");
    }

    LauncherWindow* window = 0;
    foreach (QString url, urls) {
        LauncherWindow* newWindow;
        if (!window)
            newWindow = window = new LauncherWindow(&windowOptions);
        else
            newWindow = window->newWindow();

        newWindow->load(url);
    }

    window->show();
    return launcherMain(app);
}

#include "main.moc"
