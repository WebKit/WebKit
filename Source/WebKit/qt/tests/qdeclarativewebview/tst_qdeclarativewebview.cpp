#include "../util.h"
#include <QAction>
#include <QColor>
#include <QDebug>
#include <QDeclarativeItem>
#include <QDeclarativeView>
#include <QDeclarativeComponent>
#include <QDeclarativeEngine>
#include <QDeclarativeProperty>
#include <QDir>
#include <QGraphicsWebView>
#include <QTest>
#include <QVariant>
#include <QWebFrame>
#include "qdeclarativewebview_p.h"

class tst_QDeclarativeWebView : public QObject {
    Q_OBJECT

public:
    tst_QDeclarativeWebView();

private slots:
    void initTestCase();
    void cleanupTestCase();

    void basicProperties();
    void elementAreaAt();
    void historyNav();
    void javaScript();
    void loadError();
    void multipleWindows();
    void newWindowComponent();
    void newWindowParent();
    void preferredWidthTest();
    void preferredHeightTest();
    void preferredWidthDefaultTest();
    void preferredHeightDefaultTest();
    void pressGrabTime();
    void renderingEnabled();
    void setHtml();
    void settings();
#if QT_VERSION >= 0x040704
    void backgroundColor();
#endif

private:
    void checkNoErrors(const QDeclarativeComponent&);
    QString tmpDir() const
    {
        static QString tmpd = QDir::tempPath() + "/tst_qdeclarativewebview-"
            + QDateTime::currentDateTime().toString(QLatin1String("yyyyMMddhhmmss"));
        return tmpd;
    }
};

tst_QDeclarativeWebView::tst_QDeclarativeWebView()
{
    Q_UNUSED(waitForSignal)
}

static QString strippedHtml(QString html)
{
    html.replace(QRegExp("\\s+"), "");
    return html;
}

static QString fileContents(const QString& filename)
{
    QFile file(filename);
    file.open(QIODevice::ReadOnly);
    return QString::fromUtf8(file.readAll());
}

static void removeRecursive(const QString& dirname)
{
    QDir dir(dirname);
    QFileInfoList entries(dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot));
    for (int i = 0; i < entries.count(); ++i)
        if (entries[i].isDir())
            removeRecursive(entries[i].filePath());
        else
            dir.remove(entries[i].fileName());
    QDir().rmdir(dirname);
}

void tst_QDeclarativeWebView::initTestCase()
{
    QWebSettings::setIconDatabasePath(tmpDir());
    QWebSettings::enablePersistentStorage(tmpDir());
}

void tst_QDeclarativeWebView::cleanupTestCase()
{
    removeRecursive(tmpDir());
}

void tst_QDeclarativeWebView::basicProperties()
{
    QDeclarativeEngine engine;
    QDeclarativeComponent component(&engine, QUrl("qrc:///resources/basic.qml"));
    checkNoErrors(component);
    QWebSettings::enablePersistentStorage(tmpDir());

    QObject* wv = component.create();
    QVERIFY(wv);
    waitForSignal(wv, SIGNAL(iconChanged()));
    QTRY_COMPARE(wv->property("progress").toDouble(), 1.0);
    QCOMPARE(wv->property("title").toString(), QLatin1String("Basic"));
    QTRY_COMPARE(qvariant_cast<QPixmap>(wv->property("icon")).width(), 48);
    QCOMPARE(qvariant_cast<QPixmap>(wv->property("icon")), QPixmap(":/resources/basic.png"));
    QCOMPARE(wv->property("statusText").toString(), QLatin1String("status here"));
    QCOMPARE(strippedHtml(fileContents(":/resources/basic.html")), strippedHtml(wv->property("html").toString()));
    QEXPECT_FAIL("", "TODO: get preferred width from QGraphicsWebView result", Continue);
    QCOMPARE(wv->property("preferredWidth").toInt(), 0);
    QEXPECT_FAIL("", "TODO: get preferred height from QGraphicsWebView result", Continue);
    QCOMPARE(wv->property("preferredHeight").toInt(), 0);
    QCOMPARE(wv->property("url").toUrl(), QUrl("qrc:///resources/basic.html"));
    QCOMPARE(wv->property("status").toInt(), int(QDeclarativeWebView::Ready));

    QAction* reloadAction = wv->property("reload").value<QAction*>();
    QVERIFY(reloadAction);
    QVERIFY(reloadAction->isEnabled());
    QAction* backAction = wv->property("back").value<QAction*>();
    QVERIFY(backAction);
    QVERIFY(!backAction->isEnabled());
    QAction* forwardAction = wv->property("forward").value<QAction*>();
    QVERIFY(forwardAction);
    QVERIFY(!forwardAction->isEnabled());
    QAction* stopAction = wv->property("stop").value<QAction*>();
    QVERIFY(stopAction);
    QVERIFY(!stopAction->isEnabled());

    wv->setProperty("pixelCacheSize", 0); // mainly testing that it doesn't crash or anything!
    QCOMPARE(wv->property("pixelCacheSize").toInt(), 0);
    reloadAction->trigger();
    QTRY_COMPARE(wv->property("progress").toDouble(), 1.0);
}

void tst_QDeclarativeWebView::elementAreaAt()
{
    W_QSKIP("This test should be changed to test 'heuristicZoom' instead.", SkipAll);
    QDeclarativeEngine engine;
    QDeclarativeComponent component(&engine, QUrl("qrc:///resources/elements.qml"));
    checkNoErrors(component);
    QObject* wv = component.create();
    QVERIFY(wv);
    QTRY_COMPARE(wv->property("progress").toDouble(), 1.0);

    // FIXME: Tests disabled since elementAreaAt isn't exported.
    // Areas from elements.html.
//    const QRect areaA(1, 1, 75, 54);
//    const QRect areaB(78, 3, 110, 50);
//    const QRect wholeView(0, 0, 310, 100);
//    const QRect areaBC(76, 1, 223, 54);

//    QCOMPARE(wv->elementAreaAt(40, 30, 100, 100), areaA);
//    QCOMPARE(wv->elementAreaAt(130, 30, 200, 100), areaB);
//    QCOMPARE(wv->elementAreaAt(40, 30, 400, 400), wholeView);
//    QCOMPARE(wv->elementAreaAt(130, 30, 280, 280), areaBC);
//    QCOMPARE(wv->elementAreaAt(130, 30, 400, 400), wholeView);
}

void tst_QDeclarativeWebView::historyNav()
{
    QDeclarativeEngine engine;
    QDeclarativeComponent component(&engine, QUrl("qrc:///resources/basic.qml"));
    checkNoErrors(component);
    QWebSettings::enablePersistentStorage(tmpDir());

    QObject* wv = component.create();
    QVERIFY(wv);
    waitForSignal(wv, SIGNAL(iconChanged()));

    QAction* reloadAction = wv->property("reload").value<QAction*>();
    QVERIFY(reloadAction);
    QAction* backAction = wv->property("back").value<QAction*>();
    QVERIFY(backAction);
    QAction* forwardAction = wv->property("forward").value<QAction*>();
    QVERIFY(forwardAction);
    QAction* stopAction = wv->property("stop").value<QAction*>();
    QVERIFY(stopAction);

    for (int i = 1; i <= 2; ++i) {
        QTRY_COMPARE(wv->property("progress").toDouble(), 1.0);
        QCOMPARE(wv->property("title").toString(), QLatin1String("Basic"));
        QTRY_COMPARE(qvariant_cast<QPixmap>(wv->property("icon")).width(), 48);
        QCOMPARE(qvariant_cast<QPixmap>(wv->property("icon")), QPixmap(":/resources/basic.png"));
        QCOMPARE(wv->property("statusText").toString(), QLatin1String("status here"));
        QCOMPARE(strippedHtml(fileContents(":/resources/basic.html")), strippedHtml(wv->property("html").toString()));
        QEXPECT_FAIL("", "TODO: get preferred width from QGraphicsWebView result", Continue);
        QCOMPARE(wv->property("preferredWidth").toDouble(), 0.0);
        QCOMPARE(wv->property("url").toUrl(), QUrl("qrc:///resources/basic.html"));
        QCOMPARE(wv->property("status").toInt(), int(QDeclarativeWebView::Ready));
        QVERIFY(reloadAction->isEnabled());
        QVERIFY(!backAction->isEnabled());
        QVERIFY(!forwardAction->isEnabled());
        QVERIFY(!stopAction->isEnabled());
        reloadAction->trigger();
        waitForSignal(wv, SIGNAL(iconChanged()));
    }

    wv->setProperty("url", QUrl("qrc:///resources/forward.html"));
    waitForSignal(wv, SIGNAL(iconChanged()));
    QTRY_COMPARE(wv->property("progress").toDouble(), 1.0);
    QCOMPARE(wv->property("title").toString(), QLatin1String("Forward"));
    QTRY_COMPARE(qvariant_cast<QPixmap>(wv->property("icon")).width(), 32);
    QCOMPARE(qvariant_cast<QPixmap>(wv->property("icon")), QPixmap(":/resources/forward.png"));
    QCOMPARE(strippedHtml(fileContents(":/resources/forward.html")), strippedHtml(wv->property("html").toString()));
    QCOMPARE(wv->property("url").toUrl(), QUrl("qrc:///resources/forward.html"));
    QCOMPARE(wv->property("status").toInt(), int(QDeclarativeWebView::Ready));
    QCOMPARE(wv->property("statusText").toString(), QString());

    QEXPECT_FAIL("", "https://bugs.webkit.org/show_bug.cgi?id=61042", Continue);
    QVERIFY(reloadAction->isEnabled());
    QVERIFY(backAction->isEnabled());
    QVERIFY(!forwardAction->isEnabled());
    QEXPECT_FAIL("", "https://bugs.webkit.org/show_bug.cgi?id=61042", Continue);
    QVERIFY(!stopAction->isEnabled());

    backAction->trigger();
    waitForSignal(wv, SIGNAL(loadFinished()));

    QTRY_COMPARE(wv->property("progress").toDouble(), 1.0);
    QCOMPARE(wv->property("title").toString(), QLatin1String("Basic"));
    QCOMPARE(strippedHtml(fileContents(":/resources/basic.html")), strippedHtml(wv->property("html").toString()));
    QCOMPARE(wv->property("url").toUrl(), QUrl("qrc:///resources/basic.html"));
    QCOMPARE(wv->property("status").toInt(), int(QDeclarativeWebView::Ready));

    QVERIFY(reloadAction->isEnabled());
    QVERIFY(!backAction->isEnabled());
    QVERIFY(forwardAction->isEnabled());
    QVERIFY(!stopAction->isEnabled());
}

static inline QVariant callEvaluateJavaScript(QObject *object, const QString& snippet)
{
    QVariant result;
    QMetaObject::invokeMethod(object, "evaluateJavaScript", Q_RETURN_ARG(QVariant, result), Q_ARG(QString, snippet));
    return result;
}

void tst_QDeclarativeWebView::javaScript()
{
    QDeclarativeEngine engine;
    QDeclarativeComponent component(&engine, QUrl("qrc:///resources/javaScript.qml"));
    checkNoErrors(component);
    QObject* wv = component.create();
    QVERIFY(wv);
    QTRY_COMPARE(wv->property("progress").toDouble(), 1.0);

    QCOMPARE(callEvaluateJavaScript(wv, "123").toInt(), 123);
    QCOMPARE(callEvaluateJavaScript(wv, "window.status").toString(), QLatin1String("status here"));
    QCOMPARE(callEvaluateJavaScript(wv, "window.myjsname.qmlprop").toString(), QLatin1String("qmlvalue"));
}

void tst_QDeclarativeWebView::loadError()
{
    QDeclarativeEngine engine;
    QDeclarativeComponent component(&engine, QUrl("qrc:///resources/loadError.qml"));
    checkNoErrors(component);
    QWebSettings::enablePersistentStorage(tmpDir());

    QObject* wv = component.create();
    QVERIFY(wv);
    QAction* reloadAction = wv->property("reload").value<QAction*>();
    QVERIFY(reloadAction);

    for (int i = 1; i <= 2; ++i) {
        QTRY_COMPARE(wv->property("progress").toDouble(), 1.0);
        QCOMPARE(wv->property("title").toString(), QString());
        QCOMPARE(wv->property("statusText").toString(), QString()); // HTML 'status bar' text, not error message
        QCOMPARE(wv->property("url").toUrl(), QUrl("qrc:///resources/does-not-exist.html")); // Unlike QWebPage, which loses url
        QCOMPARE(wv->property("status").toInt(), int(QDeclarativeWebView::Error));
        reloadAction->trigger();
    }
}

void tst_QDeclarativeWebView::multipleWindows()
{
    QDeclarativeEngine engine;
    QDeclarativeComponent component(&engine, QUrl("qrc:///resources/newwindows.qml"));
    checkNoErrors(component);

    QDeclarativeItem* rootItem = qobject_cast<QDeclarativeItem*>(component.create());
    QVERIFY(rootItem);

    QTRY_COMPARE(rootItem->property("pagesOpened").toInt(), 4);

    QDeclarativeProperty prop(rootItem, "firstPageOpened");
    QObject* firstPageOpened = qvariant_cast<QObject*>(prop.read());
    QVERIFY(firstPageOpened);

    QDeclarativeProperty xProp(firstPageOpened, "x");
    QTRY_COMPARE(xProp.read().toReal(), qreal(150.0));
}

void tst_QDeclarativeWebView::newWindowComponent()
{
    QDeclarativeEngine engine;
    QDeclarativeComponent component(&engine, QUrl("qrc:///resources/propertychanges.qml"));
    checkNoErrors(component);
    QDeclarativeItem* rootItem = qobject_cast<QDeclarativeItem*>(component.create());
    QVERIFY(rootItem);
    QObject* wv = rootItem->findChild<QObject*>("webView");
    QVERIFY(wv);
    QTRY_COMPARE(wv->property("progress").toDouble(), 1.0);

    QDeclarativeComponent substituteComponent(&engine);
    substituteComponent.setData("import QtQuick 1.0; WebView { objectName: 'newWebView'; url: 'basic.html'; }", QUrl::fromLocalFile(""));
    QSignalSpy newWindowComponentSpy(wv, SIGNAL(newWindowComponentChanged()));

    wv->setProperty("newWindowComponent", QVariant::fromValue(&substituteComponent));
    QCOMPARE(wv->property("newWindowComponent"), QVariant::fromValue(&substituteComponent));
    QCOMPARE(newWindowComponentSpy.count(), 1);

    wv->setProperty("newWindowComponent", QVariant::fromValue(&substituteComponent));
    QCOMPARE(newWindowComponentSpy.count(), 1);

    wv->setProperty("newWindowComponent", QVariant::fromValue((QDeclarativeComponent*)0));
    QCOMPARE(newWindowComponentSpy.count(), 2);
}

void tst_QDeclarativeWebView::newWindowParent()
{
    QDeclarativeEngine engine;
    QDeclarativeComponent component(&engine, QUrl("qrc:///resources/propertychanges.qml"));
    checkNoErrors(component);
    QDeclarativeItem* rootItem = qobject_cast<QDeclarativeItem*>(component.create());
    QVERIFY(rootItem);
    QObject* wv = rootItem->findChild<QObject*>("webView");
    QVERIFY(wv);
    QTRY_COMPARE(wv->property("progress").toDouble(), 1.0);

    QDeclarativeItem* oldWindowParent = rootItem->findChild<QDeclarativeItem*>("oldWindowParent");
    QCOMPARE(qvariant_cast<QDeclarativeItem*>(wv->property("newWindowParent")), oldWindowParent);
    QSignalSpy newWindowParentSpy(wv, SIGNAL(newWindowParentChanged()));

    QDeclarativeItem* newWindowParent = rootItem->findChild<QDeclarativeItem*>("newWindowParent");
    wv->setProperty("newWindowParent", QVariant::fromValue(newWindowParent));
    QVERIFY(newWindowParent);
    QVERIFY(oldWindowParent);
    QCOMPARE(oldWindowParent->childItems().count(), 0);
    QCOMPARE(wv->property("newWindowParent"), QVariant::fromValue(newWindowParent));
    QCOMPARE(newWindowParentSpy.count(), 1);

    wv->setProperty("newWindowParent", QVariant::fromValue(newWindowParent));
    QCOMPARE(newWindowParentSpy.count(), 1);

    wv->setProperty("newWindowParent", QVariant::fromValue((QDeclarativeItem*)0));
    QCOMPARE(newWindowParentSpy.count(), 2);
}

void tst_QDeclarativeWebView::preferredWidthTest()
{
    QDeclarativeEngine engine;
    QDeclarativeComponent component(&engine, QUrl("qrc:///resources/webviewtest.qml"));
    checkNoErrors(component);
    QObject* wv = component.create();
    QVERIFY(wv);
    wv->setProperty("testUrl", QUrl("qrc:///resources/sample.html"));
    QCOMPARE(wv->property("prefWidth").toInt(), 600);
}

void tst_QDeclarativeWebView::preferredHeightTest()
{
    QDeclarativeEngine engine;
    QDeclarativeComponent component(&engine, QUrl("qrc:///resources/webviewtest.qml"));
    checkNoErrors(component);
    QObject* wv = component.create();
    QVERIFY(wv);
    wv->setProperty("testUrl", QUrl("qrc:///resources/sample.html"));
    QCOMPARE(wv->property("prefHeight").toInt(), 500);
}

void tst_QDeclarativeWebView::preferredWidthDefaultTest()
{
    QGraphicsWebView view;
    view.load(QUrl("qrc:///resources/sample.html"));

    QDeclarativeEngine engine;
    QDeclarativeComponent component(&engine, QUrl("qrc:///resources/webviewtestdefault.qml"));
    checkNoErrors(component);
    QObject* wv = component.create();
    QVERIFY(wv);
    wv->setProperty("testUrl", QUrl("qrc:///resources/sample.html"));
    QCOMPARE(wv->property("prefWidth").toDouble(), view.preferredWidth());
}

void tst_QDeclarativeWebView::preferredHeightDefaultTest()
{
    QGraphicsWebView view;
    view.load(QUrl("qrc:///resources/sample.html"));

    QDeclarativeEngine engine;
    QDeclarativeComponent component(&engine, QUrl("qrc:///resources/webviewtestdefault.qml"));
    checkNoErrors(component);
    QObject* wv = component.create();
    QVERIFY(wv);
    wv->setProperty("testUrl", QUrl("qrc:///resources/sample.html"));
    QCOMPARE(wv->property("prefHeight").toDouble(), view.preferredHeight());
}

void tst_QDeclarativeWebView::pressGrabTime()
{
    QDeclarativeEngine engine;
    QDeclarativeComponent component(&engine, QUrl("qrc:///resources/propertychanges.qml"));
    checkNoErrors(component);
    QDeclarativeItem* rootItem = qobject_cast<QDeclarativeItem*>(component.create());
    QVERIFY(rootItem);
    QObject* wv = rootItem->findChild<QObject*>("webView");
    QVERIFY(wv);
    QTRY_COMPARE(wv->property("progress").toDouble(), 1.0);
    QCOMPARE(wv->property("pressGrabTime").toInt(), 200);
    QSignalSpy pressGrabTimeSpy(wv, SIGNAL(pressGrabTimeChanged()));

    wv->setProperty("pressGrabTime", 100);
    QCOMPARE(wv->property("pressGrabTime").toInt(), 100);
    QCOMPARE(pressGrabTimeSpy.count(), 1);

    wv->setProperty("pressGrabTime", 100);
    QCOMPARE(pressGrabTimeSpy.count(), 1);

    wv->setProperty("pressGrabTime", 0);
    QCOMPARE(pressGrabTimeSpy.count(), 2);
}

void tst_QDeclarativeWebView::renderingEnabled()
{
    QDeclarativeEngine engine;
    QDeclarativeComponent component(&engine, QUrl("qrc:///resources/propertychanges.qml"));
    checkNoErrors(component);
    QDeclarativeItem* rootItem = qobject_cast<QDeclarativeItem*>(component.create());
    QVERIFY(rootItem);
    QObject* wv = rootItem->findChild<QObject*>("webView");
    QVERIFY(wv);
    QTRY_COMPARE(wv->property("progress").toDouble(), 1.0);

    QVERIFY(wv->property("renderingEnabled").toBool());
    QSignalSpy renderingEnabledSpy(wv, SIGNAL(renderingEnabledChanged()));

    wv->setProperty("renderingEnabled", false);
    QVERIFY(!wv->property("renderingEnabled").toBool());
    QCOMPARE(renderingEnabledSpy.count(), 1);

    wv->setProperty("renderingEnabled", false);
    QCOMPARE(renderingEnabledSpy.count(), 1);

    wv->setProperty("renderingEnabled", true);
    QCOMPARE(renderingEnabledSpy.count(), 2);
}

void tst_QDeclarativeWebView::setHtml()
{
    QDeclarativeEngine engine;
    QDeclarativeComponent component(&engine, QUrl("qrc:///resources/sethtml.qml"));
    checkNoErrors(component);
    QObject* wv = component.create();
    QVERIFY(wv);
    QCOMPARE(wv->property("html").toString(), QLatin1String("<html><head></head><body><p>This is a <b>string</b> set on the WebView</p></body></html>"));

    QSignalSpy spy(wv, SIGNAL(htmlChanged()));
    wv->setProperty("html", QLatin1String("<html><head><title>Basic</title></head><body><p>text</p></body></html>"));
    QCOMPARE(spy.count(), 1);
}

void tst_QDeclarativeWebView::settings()
{
    QDeclarativeEngine engine;
    QDeclarativeComponent component(&engine, QUrl("qrc:///resources/basic.qml"));
    checkNoErrors(component);
    QObject* wv = component.create();
    QVERIFY(wv);
    QTRY_COMPARE(wv->property("progress").toDouble(), 1.0);

    QObject* s = QDeclarativeProperty(wv, "settings").object();
    QVERIFY(s);

    QStringList settingsList;
    settingsList << QString::fromAscii("autoLoadImages")
                 << QString::fromAscii("developerExtrasEnabled")
                 << QString::fromAscii("javaEnabled")
                 << QString::fromAscii("javascriptCanAccessClipboard")
                 << QString::fromAscii("javascriptCanOpenWindows")
                 << QString::fromAscii("javascriptEnabled")
                 << QString::fromAscii("linksIncludedInFocusChain")
                 << QString::fromAscii("localContentCanAccessRemoteUrls")
                 << QString::fromAscii("localStorageDatabaseEnabled")
                 << QString::fromAscii("offlineStorageDatabaseEnabled")
                 << QString::fromAscii("offlineWebApplicationCacheEnabled")
                 << QString::fromAscii("pluginsEnabled")
                 << QString::fromAscii("printElementBackgrounds")
                 << QString::fromAscii("privateBrowsingEnabled")
                 << QString::fromAscii("zoomTextOnly");

    // Merely tests that setting gets stored (in QWebSettings), behavioural tests are in WebKit.
    for (int b = 0; b <= 1; b++) {
        bool value = !!b;
        foreach (const QString& name, settingsList)
            s->setProperty(name.toAscii().data(), value);
        for (int i = 0; i < 2; i++) {
            foreach (const QString& name, settingsList)
                QCOMPARE(s->property(name.toAscii().data()).toBool(), value);
        }
    }
}

#if QT_VERSION >= 0x040704
void tst_QDeclarativeWebView::backgroundColor()
{
    // We test here the rendering of the background.
    QDeclarativeEngine engine;
    QDeclarativeComponent component(&engine, QUrl("qrc:///resources/webviewbackgroundcolor.qml"));
    checkNoErrors(component);
    QObject* wv = component.create();
    QVERIFY(wv);
    QCOMPARE(wv->property("backgroundColor").value<QColor>(), QColor(Qt::red));
    QDeclarativeView view;
    view.setSource(QUrl("qrc:///resources/webviewbackgroundcolor.qml"));
    view.show();
    QTest::qWaitForWindowShown(&view);
    QPixmap result(view.width(), view.height());
    QPainter painter(&result);
    view.render(&painter);
    QPixmap reference(view.width(), view.height());
    reference.fill(Qt::red);
    QCOMPARE(reference, result);

    // We test the emission of the backgroundColorChanged signal.
    QSignalSpy spyColorChanged(wv, SIGNAL(backgroundColorChanged()));
    wv->setProperty("backgroundColor", Qt::red);
    QCOMPARE(spyColorChanged.count(), 0);
    wv->setProperty("backgroundColor", Qt::green);
    QCOMPARE(spyColorChanged.count(), 1);
}
#endif

void tst_QDeclarativeWebView::checkNoErrors(const QDeclarativeComponent& component)
{
    // Wait until the component is ready
    QTRY_VERIFY(component.isReady() || component.isError());
    if (component.isError()) {
        QList<QDeclarativeError> errors = component.errors();
        for (int ii = 0; ii < errors.count(); ++ii) {
            const QDeclarativeError &error = errors.at(ii);
            QByteArray errorStr = QByteArray::number(error.line()) + ":" +
                                  QByteArray::number(error.column()) + ":" +
                                  error.description().toUtf8();
            qWarning() << errorStr;
        }
    }
    QVERIFY(!component.isError());
}

QTEST_MAIN(tst_QDeclarativeWebView)
#include "tst_qdeclarativewebview.moc"
