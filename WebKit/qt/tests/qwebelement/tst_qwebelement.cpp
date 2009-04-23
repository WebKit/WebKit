/*
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)

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


#include <QtTest/QtTest>

#include <qwebpage.h>
#include <qwidget.h>
#include <qwebview.h>
#include <qwebframe.h>
#include <qwebelement.h>
//TESTED_CLASS=
//TESTED_FILES=

/**
 * Starts an event loop that runs until the given signal is received.
 Optionally the event loop
 * can return earlier on a timeout.
 *
 * \return \p true if the requested signal was received
 *         \p false on timeout
 */
static bool waitForSignal(QObject* obj, const char* signal, int timeout = 0)
{
    QEventLoop loop;
    QObject::connect(obj, signal, &loop, SLOT(quit()));
    QTimer timer;
    QSignalSpy timeoutSpy(&timer, SIGNAL(timeout()));
    if (timeout > 0) {
        QObject::connect(&timer, SIGNAL(timeout()), &loop, SLOT(quit()));
        timer.setSingleShot(true);
        timer.start(timeout);
    }
    loop.exec();
    return timeoutSpy.isEmpty();
}

class tst_QWebElement : public QObject
{
    Q_OBJECT

public:
    tst_QWebElement();
    virtual ~tst_QWebElement();

public slots:
    void init();
    void cleanup();

private slots:
    void textHtml();
    void simpleCollection();
    void attributes();
    void attributesNS();
    void classes();
    void namespaceURI();
    void iteration();
    void foreachManipulation();
    void callFunction();
    void callFunctionSubmitForm();
    void functionNames();
    void documentElement();
    void frame();
    void emptyCollection();
    void style();
    void computedStyle();
    void appendCollection();
    void properties();
    void appendAndPrepend();
    void insertBeforeAndAfter();
    void remove();
    void clear();
    void replaceWith();
    void wrap();
    void nullSelect();
    void firstChildNextSibling();
    void firstChildNextSiblingWithTag();
    void lastChildPreviousSibling();
    void lastChildPreviousSiblingWithTag();

private:
    QWebView* m_view;
    QWebPage* m_page;
    QWebFrame* m_mainFrame;
};

tst_QWebElement::tst_QWebElement()
{
}

tst_QWebElement::~tst_QWebElement()
{
}

void tst_QWebElement::init()
{
    m_view = new QWebView();
    m_page = m_view->page();
    m_mainFrame = m_page->mainFrame();
}

void tst_QWebElement::cleanup()
{
    delete m_view;
}

void tst_QWebElement::textHtml()
{
    QString html = "<body><p>test</p></body>";
    m_mainFrame->setHtml(html);
    QWebElement body = m_mainFrame->documentElement();
    QVERIFY(!body.isNull());

    QCOMPARE(body.toPlainText(), QString("test"));
    QCOMPARE(body.toPlainText(), m_mainFrame->toPlainText());

    QCOMPARE(body.toXml(QWebElement::InnerXml), html);
}

void tst_QWebElement::simpleCollection()
{
    QString html = "<body><p>first para</p><p>second para</p></body>";
    m_mainFrame->setHtml(html);
    QWebElement body = m_mainFrame->documentElement();

    QWebElementCollection paras = body.findAll("p");
    QCOMPARE(paras.count(), 2);

    QList<QWebElement> list = paras.toList();
    QCOMPARE(list.count(), 2);
    QCOMPARE(list.at(0).toPlainText(), QString("first para"));
    QCOMPARE(list.at(1).toPlainText(), QString("second para"));
}

void tst_QWebElement::attributes()
{
    m_mainFrame->setHtml("<body><p>Test");
    QWebElement body = m_mainFrame->documentElement();

    QVERIFY(!body.hasAttribute("title"));
    QVERIFY(!body.hasAttributes());

    body.setAttribute("title", "test title");

    QVERIFY(body.hasAttributes());
    QVERIFY(body.hasAttribute("title"));

    QCOMPARE(body.attribute("title"), QString("test title"));

    body.removeAttribute("title");

    QVERIFY(!body.hasAttribute("title"));
    QVERIFY(!body.hasAttributes());

    QCOMPARE(body.attribute("does-not-exist", "testvalue"), QString("testvalue"));
}

void tst_QWebElement::attributesNS()
{
    QString content = "<html xmlns=\"http://www.w3.org/1999/xhtml\" "
                      "xmlns:svg=\"http://www.w3.org/2000/svg\">"
                      "<body><svg:svg id=\"foobar\" width=\"400px\" height=\"300px\">"
                      "</svg:svg></body></html>";

    m_mainFrame->setContent(content.toUtf8(), "application/xhtml+xml");

    QWebElement svg = m_mainFrame->findFirstElement("svg");
    QVERIFY(!svg.isNull());

    QVERIFY(!svg.hasAttributeNS("http://www.w3.org/2000/svg", "foobar"));
    QCOMPARE(svg.attributeNS("http://www.w3.org/2000/svg", "foobar", "defaultblah"), QString("defaultblah"));
    svg.setAttributeNS("http://www.w3.org/2000/svg", "svg:foobar", "true");
    QVERIFY(svg.hasAttributeNS("http://www.w3.org/2000/svg", "foobar"));
    QCOMPARE(svg.attributeNS("http://www.w3.org/2000/svg", "foobar", "defaultblah"), QString("true"));
}

void tst_QWebElement::classes()
{
    m_mainFrame->setHtml("<body><p class=\"a b c d a c\">Test");

    QWebElement body = m_mainFrame->documentElement();
    QCOMPARE(body.classes().count(), 0);

    QWebElement p = m_mainFrame->documentElement().findAll("p").at(0);
    QStringList classes = p.classes();
    QCOMPARE(classes.count(), 4);
    QCOMPARE(classes[0], QLatin1String("a"));
    QCOMPARE(classes[1], QLatin1String("b"));
    QCOMPARE(classes[2], QLatin1String("c"));
    QCOMPARE(classes[3], QLatin1String("d"));
    QVERIFY(p.hasClass("a"));
    QVERIFY(p.hasClass("b"));
    QVERIFY(p.hasClass("c"));
    QVERIFY(p.hasClass("d"));
    QVERIFY(!p.hasClass("e"));

    p.addClass("f");
    QVERIFY(p.hasClass("f"));
    p.addClass("a");
    QCOMPARE(p.classes().count(), 5);
    QVERIFY(p.hasClass("a"));
    QVERIFY(p.hasClass("b"));
    QVERIFY(p.hasClass("c"));
    QVERIFY(p.hasClass("d"));

    p.toggleClass("a");
    QVERIFY(!p.hasClass("a"));
    QVERIFY(p.hasClass("b"));
    QVERIFY(p.hasClass("c"));
    QVERIFY(p.hasClass("d"));
    QVERIFY(p.hasClass("f"));
    QCOMPARE(p.classes().count(), 4);
    p.toggleClass("f");
    QVERIFY(!p.hasClass("f"));
    QCOMPARE(p.classes().count(), 3);
    p.toggleClass("a");
    p.toggleClass("f");
    QVERIFY(p.hasClass("a"));
    QVERIFY(p.hasClass("f"));
    QCOMPARE(p.classes().count(), 5);

    p.toggleClass("a", true);
    QCOMPARE(p.classes().count(), 5);
    p.toggleClass("a", false);
    QCOMPARE(p.classes().count(), 4);
    p.toggleClass("z", false);
    QVERIFY(!p.hasClass("z"));
    QCOMPARE(p.classes().count(), 4);
    p.toggleClass("z", true);
    QVERIFY(p.hasClass("z"));
    QCOMPARE(p.classes().count(), 5);
    p.toggleClass("a", true);
    p.toggleClass("z", false);
    QCOMPARE(p.classes().count(), 5);

    p.removeClass("f");
    QVERIFY(!p.hasClass("f"));
    QCOMPARE(p.classes().count(), 4);
    p.removeClass("d");
    QVERIFY(!p.hasClass("d"));
    QCOMPARE(p.classes().count(), 3);
    p.removeClass("not-exist");
    QCOMPARE(p.classes().count(), 3);
    p.removeClass("c");
    QVERIFY(!p.hasClass("c"));
    QCOMPARE(p.classes().count(), 2);
    p.removeClass("b");
    QVERIFY(!p.hasClass("b"));
    QCOMPARE(p.classes().count(), 1);
    p.removeClass("a");
    QVERIFY(!p.hasClass("a"));
    QCOMPARE(p.classes().count(), 0);
    p.removeClass("foobar");
    QCOMPARE(p.classes().count(), 0);
}

void tst_QWebElement::namespaceURI()
{
    QString content = "<html xmlns=\"http://www.w3.org/1999/xhtml\" "
                      "xmlns:svg=\"http://www.w3.org/2000/svg\">"
                      "<body><svg:svg id=\"foobar\" width=\"400px\" height=\"300px\">"
                      "</svg:svg></body></html>";

    m_mainFrame->setContent(content.toUtf8(), "application/xhtml+xml");
    QWebElement body = m_mainFrame->documentElement();
    QCOMPARE(body.namespaceURI(), QLatin1String("http://www.w3.org/1999/xhtml"));

    QWebElement svg = body.findAll("*#foobar").toList().at(0);
    QCOMPARE(svg.prefix(), QLatin1String("svg"));
    QCOMPARE(svg.localName(), QLatin1String("svg"));
    QCOMPARE(svg.tagName(), QLatin1String("svg:svg"));
    QCOMPARE(svg.namespaceURI(), QLatin1String("http://www.w3.org/2000/svg"));

}

void tst_QWebElement::iteration()
{
    QString html = "<body><p>first para</p><p>second para</p></body>";
    m_mainFrame->setHtml(html);
    QWebElement body = m_mainFrame->documentElement();

    QWebElementCollection paras = body.findAll("p");
    QList<QWebElement> referenceList = paras.toList();

    QList<QWebElement> foreachList;
    foreach(QWebElement p, paras) {
        foreachList.append(p);
    }
    QVERIFY(foreachList.count() == 2);
    QCOMPARE(foreachList.count(), referenceList.count());
    QCOMPARE(foreachList.at(0), referenceList.at(0));
    QCOMPARE(foreachList.at(1), referenceList.at(1));

    QList<QWebElement> forLoopList;
    for (int i = 0; i < paras.count(); ++i) {
        forLoopList.append(paras.at(i));
    }
    QVERIFY(foreachList.count() == 2);
    QCOMPARE(foreachList.count(), referenceList.count());
    QCOMPARE(foreachList.at(0), referenceList.at(0));
    QCOMPARE(foreachList.at(1), referenceList.at(1));

    for (int i = 0; i < paras.count(); ++i) {
        QCOMPARE(paras.at(i), paras[i]);
    }

    QCOMPARE(paras.at(0), paras.first());
    QCOMPARE(paras.at(1), paras.last());
}

void tst_QWebElement::foreachManipulation()
{
    QString html = "<body><p>first para</p><p>second para</p></body>";
    m_mainFrame->setHtml(html);
    QWebElement body = m_mainFrame->documentElement();

    foreach(QWebElement p, body.findAll("p")) {
        p.setXml(QWebElement::InnerXml, "<div>foo</div><div>bar</div>");
    }

    QCOMPARE(body.findAll("div").count(), 4);
}

void tst_QWebElement::callFunction()
{
    m_mainFrame->setHtml("<body><p>test");
    QWebElement body = m_mainFrame->documentElement();
    QVERIFY(body.scriptFunctions().contains("hasChildNodes"));
    QVariant result = body.callScriptFunction("hasChildNodes");
    QVERIFY(result.isValid());
    QVERIFY(result.type() == QVariant::Bool);
    QVERIFY(result.toBool());

    body.callScriptFunction("setAttribute", QVariantList() << "foo" << "bar");
    QCOMPARE(body.attribute("foo"), QString("bar"));
}

void tst_QWebElement::callFunctionSubmitForm()
{
    m_mainFrame->setHtml(QString("<html><body><form name='tstform' action='data:text/html,foo'method='get'>"
                                 "<input type='text'><input type='submit'></form></body></html>"), QUrl());

    QWebElement form = m_mainFrame->documentElement().findAll("form").at(0);
    QVERIFY(form.scriptFunctions().contains("submit"));
    QVERIFY(!form.isNull());
    form.callScriptFunction("submit");

    waitForSignal(m_page, SIGNAL(loadFinished(bool)));
    QCOMPARE(m_mainFrame->url().toString(), QString("data:text/html,foo?"));
}

void tst_QWebElement::functionNames()
{
    m_mainFrame->setHtml("<body><p>Test");

    QWebElement body = m_mainFrame->documentElement();

    QVERIFY(body.scriptFunctions().contains("setAttribute"));
}

void tst_QWebElement::documentElement()
{
    m_mainFrame->setHtml("<body><p>Test");

    QWebElement para = m_mainFrame->documentElement().findAll("p").at(0);
    QVERIFY(para.parent().parent() == m_mainFrame->documentElement());
    QVERIFY(para.document() == m_mainFrame->documentElement());
}

void tst_QWebElement::frame()
{
    m_mainFrame->setHtml("<body><p>test");

    QWebElement doc = m_mainFrame->documentElement();
    QVERIFY(doc.webFrame() == m_mainFrame);

    m_view->setHtml(QString("data:text/html,<frameset cols=\"25%,75%\"><frame src=\"data:text/html,"
                            "<p>frame1\">"
                            "<frame src=\"data:text/html,<p>frame2\"></frameset>"), QUrl());

    waitForSignal(m_page, SIGNAL(loadFinished(bool)));

    QCOMPARE(m_mainFrame->childFrames().count(), 2);

    QWebFrame* firstFrame = m_mainFrame->childFrames().at(0);
    QWebFrame* secondFrame = m_mainFrame->childFrames().at(1);

    QCOMPARE(firstFrame->toPlainText(), QString("frame1"));
    QCOMPARE(secondFrame->toPlainText(), QString("frame2"));

    QWebElement firstPara = firstFrame->documentElement().findAll("p").at(0);
    QWebElement secondPara = secondFrame->documentElement().findAll("p").at(0);

    QVERIFY(firstPara.webFrame() == firstFrame);
    QVERIFY(secondPara.webFrame() == secondFrame);
}

void tst_QWebElement::emptyCollection()
{
    QWebElementCollection emptyCollection;
    QCOMPARE(emptyCollection.count(), 0);
}

void tst_QWebElement::style()
{
    QString html = "<body><p style=\"color: blue;\">some text</p></body>";
    m_mainFrame->setHtml(html);

    QWebElement p = m_mainFrame->documentElement().findAll("p").at(0);
    QCOMPARE(p.styleProperty("color"), QLatin1String("blue"));
    QVERIFY(p.styleProperty("cursor").isEmpty());

    p.setStyleProperty("color", "red");
    p.setStyleProperty("cursor", "auto");

    QCOMPARE(p.styleProperty("color"), QLatin1String("red"));
    QCOMPARE(p.styleProperty("cursor"), QLatin1String("auto"));
}

void tst_QWebElement::computedStyle()
{
    QString html = "<body><p>some text</p></body>";
    m_mainFrame->setHtml(html);

    QWebElement p = m_mainFrame->documentElement().findAll("p").at(0);
    QCOMPARE(p.computedStyleProperty("cursor"), QLatin1String("auto"));
    QVERIFY(!p.computedStyleProperty("cursor").isEmpty());
    QVERIFY(p.styleProperty("cursor").isEmpty());

    p.setStyleProperty("cursor", "text");
    p.setStyleProperty("color", "red");

    QCOMPARE(p.computedStyleProperty("cursor"), QLatin1String("text"));
    QCOMPARE(p.computedStyleProperty("color"), QLatin1String("rgb(255, 0, 0)"));
    QCOMPARE(p.styleProperty("color"), QLatin1String("red"));
}

void tst_QWebElement::appendCollection()
{
    QString html = "<body><span class='a'>aaa</span><p>first para</p><div>foo</div>"
        "<span class='b'>bbb</span><p>second para</p><div>bar</div></body>";
    m_mainFrame->setHtml(html);
    QWebElement body = m_mainFrame->documentElement();

    QWebElementCollection collection = body.findAll("p");
    QCOMPARE(collection.count(), 2);

    collection.append(body.findAll("div"));
    QCOMPARE(collection.count(), 4);

    collection += body.findAll("span.a");
    QCOMPARE(collection.count(), 5);

    QWebElementCollection all = collection + body.findAll("span.b");
    QCOMPARE(all.count(), 6);
    QCOMPARE(collection.count(), 5);

    all += collection;
    QCOMPARE(all.count(), 11);

    QCOMPARE(collection.count(), 5);
    QWebElementCollection test;
    test.append(collection);
    QCOMPARE(test.count(), 5);
    test.append(QWebElementCollection());
    QCOMPARE(test.count(), 5);
}

void tst_QWebElement::properties()
{
    m_mainFrame->setHtml("<body><form><input type=checkbox id=ourcheckbox checked=true>");

    QWebElement checkBox = m_mainFrame->findFirstElement("#ourcheckbox");
    QVERIFY(!checkBox.isNull());

    QVERIFY(checkBox.scriptProperties().contains("checked"));

    QCOMPARE(checkBox.scriptProperty("checked"), QVariant(true));
    checkBox.setScriptProperty("checked", false);
    QCOMPARE(checkBox.scriptProperty("checked"), QVariant(false));

    QVERIFY(!checkBox.scriptProperties().contains("non_existant"));
    QCOMPARE(checkBox.scriptProperty("non_existant"), QVariant());

    checkBox.setScriptProperty("non_existant", "test");

    QCOMPARE(checkBox.scriptProperty("non_existant"), QVariant("test"));
    QVERIFY(checkBox.scriptProperties().contains("non_existant"));

    // removing scriptProperties is currently not supported. We should look into this
    // and consider the option of just allowing through the QtScript API only.
#if 0
    checkBox.setScriptProperty("non_existant", QVariant());

    QCOMPARE(checkBox.scriptProperty("non_existant"), QVariant());
    QVERIFY(!checkBox.scriptProperties().contains("non_existant"));
#endif
}

void tst_QWebElement::appendAndPrepend()
{
    QString html = "<body>"
        "<p>"
            "foo"
        "</p>"
        "<p>"
            "bar"
        "</p>"
    "</body>";

    m_mainFrame->setHtml(html);
    QWebElement body = m_mainFrame->documentElement().findFirst("body");

    QCOMPARE(body.findAll("p").count(), 2);
    body.append(body.findFirst("p"));
    QCOMPARE(body.findAll("p").count(), 2);
    QCOMPARE(body.findFirst("p").toPlainText(), QString("bar"));
    QCOMPARE(body.findAll("p").last().toPlainText(), QString("foo"));

    body.append(body.findFirst("p").clone());
    QCOMPARE(body.findAll("p").count(), 3);
    QCOMPARE(body.findFirst("p").toPlainText(), QString("bar"));
    QCOMPARE(body.findAll("p").last().toPlainText(), QString("bar"));

    body.prepend(body.findAll("p").at(1).clone());
    QCOMPARE(body.findAll("p").count(), 4);
    QCOMPARE(body.findFirst("p").toPlainText(), QString("foo"));

    body.findFirst("p").append("<div>booyakasha</div>");
    QCOMPARE(body.findAll("p div").count(), 1);
    QCOMPARE(body.findFirst("p div").toPlainText(), QString("booyakasha"));

    body.findFirst("div").prepend("<code>yepp</code>");
    QCOMPARE(body.findAll("p div code").count(), 1);
    QCOMPARE(body.findFirst("p div code").toPlainText(), QString("yepp"));
}

void tst_QWebElement::insertBeforeAndAfter()
{
    QString html = "<body>"
        "<p>"
            "foo"
        "</p>"
        "<div>"
            "yeah"
        "</div>"
        "<p>"
            "bar"
        "</p>"
    "</body>";

    m_mainFrame->setHtml(html);
    QWebElement body = m_mainFrame->documentElement().findFirst("body");
    QWebElement div = body.findFirst("div");

    QCOMPARE(body.findAll("p").count(), 2);
    QCOMPARE(body.findAll("div").count(), 1);

    div.insertBefore(body.findAll("p").last().clone());
    QCOMPARE(body.findAll("p").count(), 3);
    QCOMPARE(body.findAll("p").at(0).toPlainText(), QString("foo"));
    QCOMPARE(body.findAll("p").at(1).toPlainText(), QString("bar"));
    QCOMPARE(body.findAll("p").at(2).toPlainText(), QString("bar"));

    div.insertAfter(body.findFirst("p").clone());
    QCOMPARE(body.findAll("p").count(), 4);
    QCOMPARE(body.findAll("p").at(0).toPlainText(), QString("foo"));
    QCOMPARE(body.findAll("p").at(1).toPlainText(), QString("bar"));
    QCOMPARE(body.findAll("p").at(2).toPlainText(), QString("foo"));
    QCOMPARE(body.findAll("p").at(3).toPlainText(), QString("bar"));

    div.insertBefore("<span>hey</span>");
    QCOMPARE(body.findAll("span").count(), 1);

    div.insertAfter("<span>there</span>");
    QCOMPARE(body.findAll("span").count(), 2);
    QCOMPARE(body.findAll("span").at(0).toPlainText(), QString("hey"));
    QCOMPARE(body.findAll("span").at(1).toPlainText(), QString("there"));
}

void tst_QWebElement::remove()
{
    QString html = "<body>"
        "<p>"
            "foo"
        "</p>"
        "<div>"
            "<p>yeah</p>"
        "</div>"
        "<p>"
            "bar"
        "</p>"
    "</body>";

    m_mainFrame->setHtml(html);
    QWebElement body = m_mainFrame->documentElement().findFirst("body");

    QCOMPARE(body.findAll("div").count(), 1);
    QCOMPARE(body.findAll("p").count(), 3);

    QWebElement div = body.findFirst("div");
    div.remove();

    QCOMPARE(div.isNull(), false);
    QCOMPARE(body.findAll("div").count(), 0);
    QCOMPARE(body.findAll("p").count(), 2);

    body.append(div);

    QCOMPARE(body.findAll("div").count(), 1);
    QCOMPARE(body.findAll("p").count(), 3);
}

void tst_QWebElement::clear()
{
    QString html = "<body>"
        "<p>"
            "foo"
        "</p>"
        "<div>"
            "<p>yeah</p>"
        "</div>"
        "<p>"
            "bar"
        "</p>"
    "</body>";

    m_mainFrame->setHtml(html);
    QWebElement body = m_mainFrame->documentElement().findFirst("body");

    QCOMPARE(body.findAll("div").count(), 1);
    QCOMPARE(body.findAll("p").count(), 3);
    body.findFirst("div").clear();
    QCOMPARE(body.findAll("div").count(), 1);
    QCOMPARE(body.findAll("p").count(), 2);
}


void tst_QWebElement::replaceWith()
{
    QString html = "<body>"
        "<p>"
            "foo"
        "</p>"
        "<div>"
            "yeah"
        "</div>"
        "<p>"
            "<span>haba</span>"
        "</p>"
    "</body>";

    m_mainFrame->setHtml(html);
    QWebElement body = m_mainFrame->documentElement().findFirst("body");

    QCOMPARE(body.findAll("div").count(), 1);
    QCOMPARE(body.findAll("span").count(), 1);
    body.findFirst("div").replaceWith(body.findFirst("span").clone());
    QCOMPARE(body.findAll("div").count(), 0);
    QCOMPARE(body.findAll("span").count(), 2);
    QCOMPARE(body.findAll("p").count(), 2);

    body.findFirst("span").replaceWith("<p><code>wow</code></p>");
    QCOMPARE(body.findAll("p").count(), 3);
    QCOMPARE(body.findAll("p code").count(), 1);
    QCOMPARE(body.findFirst("p code").toPlainText(), QString("wow"));
}

void tst_QWebElement::wrap()
{
    QString html = "<body>"
        "<p>"
            "foo"
        "</p>"
        "<div>"
            "yeah"
        "</div>"
        "<p>"
            "<span>bar</span>"
        "</p>"
    "</body>";

    m_mainFrame->setHtml(html);
    QWebElement body = m_mainFrame->documentElement().findFirst("body");

    QCOMPARE(body.findAll("div").count(), 1);
    body.findFirst("div").wrap(body.findFirst("span").clone());
    QCOMPARE(body.findAll("div").count(), 1);
    QCOMPARE(body.findAll("span").count(), 2);
    QCOMPARE(body.findAll("p").count(), 2);

    body.findFirst("div").wrap("<code></code>");
    QCOMPARE(body.findAll("code").count(), 1);
    QCOMPARE(body.findAll("code div").count(), 1);
    QCOMPARE(body.findFirst("code div").toPlainText(), QString("yeah"));
}

void tst_QWebElement::nullSelect()
{
    m_mainFrame->setHtml("<body><p>Test");

    QWebElementCollection collection = m_mainFrame->findAllElements("invalid{syn(tax;;%#$f223e>>");
    QVERIFY(collection.count() == 0);
}

void tst_QWebElement::firstChildNextSibling()
{
    m_mainFrame->setHtml("<body><!--comment--><p>Test</p><!--another commend><table>");

    QWebElement body = m_mainFrame->findFirstElement("body");
    QVERIFY(!body.isNull());
    QWebElement p = body.firstChild();
    QVERIFY(!p.isNull());
    QCOMPARE(p.tagName(), QString("P"));
    QWebElement table = p.nextSibling();
    QVERIFY(!table.isNull());
    QCOMPARE(table.tagName(), QString("TABLE"));
    QVERIFY(table.nextSibling().isNull());
}

void tst_QWebElement::firstChildNextSiblingWithTag()
{
    m_mainFrame->setHtml("<body><!--comment--><p><span>Test</span></p><div>test</div><p><span>test2</span></p><div>last</div>");

    QWebElement body = m_mainFrame->findFirstElement("body");
    QVERIFY(!body.isNull());
    QWebElement div = body.firstChild("div");
    QVERIFY(!div.isNull());
    QCOMPARE(div.tagName(), QString("DIV"));
    QCOMPARE(div.toPlainText(), QString("test"));
    QCOMPARE(div.nextSibling().tagName(), QString("P"));
    div = div.nextSibling("div");
    QVERIFY(!div.isNull());
    QCOMPARE(div.tagName(), QString("DIV"));
    QCOMPARE(div.toPlainText(), QString("last"));
}

void tst_QWebElement::lastChildPreviousSibling()
{
    m_mainFrame->setHtml("<body><!--comment--><p>Test</p><!--another commend><table>");

    QWebElement body = m_mainFrame->findFirstElement("body");
    QVERIFY(!body.isNull());
    QWebElement table = body.lastChild();
    QVERIFY(!table.isNull());
    QCOMPARE(table.tagName(), QString("TABLE"));
    QWebElement p = table.previousSibling();
    QVERIFY(!p.isNull());
    QCOMPARE(p.tagName(), QString("P"));
    QVERIFY(p.previousSibling().isNull());
}

void tst_QWebElement::lastChildPreviousSiblingWithTag()
{
    m_mainFrame->setHtml("<body><!--comment--><p><span>Test</span></p><div>test</div><p><span>test2</span></p><div>last</div>");

    QWebElement body = m_mainFrame->findFirstElement("body");
    QVERIFY(!body.isNull());
    QWebElement div = body.lastChild("div");
    QVERIFY(!div.isNull());
    QCOMPARE(div.tagName(), QString("DIV"));
    QCOMPARE(div.toPlainText(), QString("last"));
    QCOMPARE(div.previousSibling().tagName(), QString("P"));
    div = div.previousSibling("div");
    QVERIFY(!div.isNull());
    QCOMPARE(div.tagName(), QString("DIV"));
    QCOMPARE(div.toPlainText(), QString("test"));
}


QTEST_MAIN(tst_QWebElement)
#include "tst_qwebelement.moc"
