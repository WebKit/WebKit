#include "../util.h"
#include <QDebug>
#include <QDeclarativeComponent>
#include <QDeclarativeEngine>
#include <QDeclarativeProperty>
#include <QDeclarativeView>
#include <QDir>
#include <QGraphicsWebView>
#include <QTest>
#include <QWebFrame>

QT_BEGIN_NAMESPACE

class tst_QDeclarativeWebView : public QObject {
    Q_OBJECT

public:
    tst_QDeclarativeWebView();

private slots:
    void preferredWidthTest();
    void preferredHeightTest();
    void preferredWidthDefaultTest();
    void preferredHeightDefaultTest();

private:
    void checkNoErrors(const QDeclarativeComponent&);
};

tst_QDeclarativeWebView::tst_QDeclarativeWebView()
{
    Q_UNUSED(waitForSignal)
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

void tst_QDeclarativeWebView::checkNoErrors(const QDeclarativeComponent& component)
{
    // Wait until the component is ready
    QTRY_VERIFY(component.isReady() || component.isError());
    QVERIFY(!component.isError());
}

QTEST_MAIN(tst_QDeclarativeWebView)
#include "tst_qdeclarativewebview.moc"

QT_END_NAMESPACE
