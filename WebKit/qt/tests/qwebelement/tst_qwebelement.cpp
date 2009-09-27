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
    void foreachManipulation();
    void evaluateJavaScript();
    void documentElement();
    void frame();
    void style();
    void computedStyle();
    void appendAndPrepend();
    void insertBeforeAndAfter();
    void remove();
    void clear();
    void replaceWith();
    void encloseWith();
    void encloseContentsWith();
    void nullSelect();
    void firstChildNextSibling();
    void lastChildPreviousSibling();
    void hasSetFocus();

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
    QString html = "<head></head><body><p>test</p></body>";
    m_mainFrame->setHtml(html);
    QWebElement body = m_mainFrame->documentElement();
    QVERIFY(!body.isNull());

    QCOMPARE(body.toPlainText(), QString("test"));
    QCOMPARE(body.toPlainText(), m_mainFrame->toPlainText());

    QCOMPARE(body.toInnerXml(), html);
}

void tst_QWebElement::simpleCollection()
{
    QString html = "<body><p>first para</p><p>second para</p></body>";
    m_mainFrame->setHtml(html);
    QWebElement body = m_mainFrame->documentElement();

    QList<QWebElement> list = body.findAll("p");
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
    QCOMPARE(body.namespaceUri(), QLatin1String("http://www.w3.org/1999/xhtml"));

    QWebElement svg = body.findAll("*#foobar").at(0);
    QCOMPARE(svg.prefix(), QLatin1String("svg"));
    QCOMPARE(svg.localName(), QLatin1String("svg"));
    QCOMPARE(svg.tagName(), QLatin1String("svg:svg"));
    QCOMPARE(svg.namespaceUri(), QLatin1String("http://www.w3.org/2000/svg"));

}

void tst_QWebElement::foreachManipulation()
{
    QString html = "<body><p>first para</p><p>second para</p></body>";
    m_mainFrame->setHtml(html);
    QWebElement body = m_mainFrame->documentElement();

    foreach(QWebElement p, body.findAll("p")) {
        p.setInnerXml("<div>foo</div><div>bar</div>");
    }

    QCOMPARE(body.findAll("div").count(), 4);
}

void tst_QWebElement::evaluateJavaScript()
{
    QVariant result;
    m_mainFrame->setHtml("<body><p>test");
    QWebElement para = m_mainFrame->findFirstElement("p");

    result = para.evaluateJavaScript("this.tagName");
    QVERIFY(result.isValid());
    QVERIFY(result.type() == QVariant::String);
    QCOMPARE(result.toString(), QLatin1String("P"));

    result = para.evaluateJavaScript("this.hasAttributes()");
    QVERIFY(result.isValid());
    QVERIFY(result.type() == QVariant::Bool);
    QVERIFY(!result.toBool());

    para.evaluateJavaScript("this.setAttribute('align', 'left');");
    QCOMPARE(para.attribute("align"), QLatin1String("left"));

    result = para.evaluateJavaScript("this.hasAttributes()");
    QVERIFY(result.isValid());
    QVERIFY(result.type() == QVariant::Bool);
    QVERIFY(result.toBool());
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

void tst_QWebElement::style()
{
    QString html = "<head>"
        "<style type='text/css'>"
            "p { color: green !important }"
            "#idP { color: red }"
            ".classP { color : yellow ! important }"
        "</style>"
    "</head>"
    "<body>"
        "<p id='idP' class='classP' style='color: blue;'>some text</p>"
    "</body>";

    m_mainFrame->setHtml(html);

    QWebElement p = m_mainFrame->documentElement().findAll("p").at(0);
    QCOMPARE(p.styleProperty("color", QWebElement::InlineStyle), QLatin1String("blue"));
    QVERIFY(p.styleProperty("cursor", QWebElement::InlineStyle).isEmpty());

    p.setStyleProperty("color", "red");
    p.setStyleProperty("cursor", "auto");

    QCOMPARE(p.styleProperty("color", QWebElement::InlineStyle), QLatin1String("red"));
    QCOMPARE(p.styleProperty("color", QWebElement::CascadedStyle), QLatin1String("yellow"));
    QCOMPARE(p.styleProperty("cursor", QWebElement::InlineStyle), QLatin1String("auto"));

    p.setStyleProperty("color", "green !important");
    QCOMPARE(p.styleProperty("color", QWebElement::InlineStyle), QLatin1String("green"));
    QCOMPARE(p.styleProperty("color", QWebElement::CascadedStyle), QLatin1String("green"));

    p.setStyleProperty("color", "blue");
    QCOMPARE(p.styleProperty("color", QWebElement::InlineStyle), QLatin1String("green"));
    QCOMPARE(p.styleProperty("color", QWebElement::CascadedStyle), QLatin1String("green"));

    p.setStyleProperty("color", "blue !important");
    QCOMPARE(p.styleProperty("color", QWebElement::InlineStyle), QLatin1String("blue"));
    QCOMPARE(p.styleProperty("color", QWebElement::CascadedStyle), QLatin1String("blue"));

    QString html2 = "<head>"
        "<style type='text/css'>"
            "p { color: green }"
            "#idP { color: red }"
            ".classP { color: yellow }"
        "</style>"
    "</head>"
    "<body>"
        "<p id='idP' class='classP' style='color: blue;'>some text</p>"
    "</body>";

    m_mainFrame->setHtml(html2);
    p = m_mainFrame->documentElement().findAll("p").at(0);

    QCOMPARE(p.styleProperty("color", QWebElement::InlineStyle), QLatin1String("blue"));
    QCOMPARE(p.styleProperty("color", QWebElement::CascadedStyle), QLatin1String("blue"));

    QString html3 = "<head>"
        "<style type='text/css'>"
            "p { color: green !important }"
            "#idP { color: red !important}"
            ".classP { color: yellow !important}"
        "</style>"
    "</head>"
    "<body>"
        "<p id='idP' class='classP' style='color: blue !important;'>some text</p>"
    "</body>";

    m_mainFrame->setHtml(html3);
    p = m_mainFrame->documentElement().findAll("p").at(0);

    QCOMPARE(p.styleProperty("color", QWebElement::InlineStyle), QLatin1String("blue"));
    QCOMPARE(p.styleProperty("color", QWebElement::CascadedStyle), QLatin1String("blue"));

    QString html5 = "<head>"
        "<style type='text/css'>"
            "p { color: green }"
            "#idP { color: red }"
            ".classP { color: yellow }"
        "</style>"
    "</head>"
    "<body>"
        "<p id='idP' class='classP'>some text</p>"
    "</body>";

    m_mainFrame->setHtml(html5);
    p = m_mainFrame->documentElement().findAll("p").at(0);

    QCOMPARE(p.styleProperty("color", QWebElement::InlineStyle), QLatin1String(""));
    QCOMPARE(p.styleProperty("color", QWebElement::CascadedStyle), QLatin1String("red"));

    QString html6 = "<head>"
        "<link rel='stylesheet' href='qrc:/style.css' type='text/css' />"
        "<style type='text/css'>"
            "p { color: green }"
            "#idP { color: red }"
            ".classP { color: yellow ! important}"
        "</style>"
    "</head>"
    "<body>"
        "<p id='idP' class='classP' style='color: blue;'>some text</p>"
    "</body>";

    // in few seconds, the CSS should be completey loaded
    QSignalSpy spy(m_page, SIGNAL(loadFinished(bool)));
    m_mainFrame->setHtml(html6);
    QTest::qWait(200);

    p = m_mainFrame->documentElement().findAll("p").at(0);
    QCOMPARE(p.styleProperty("color", QWebElement::InlineStyle), QLatin1String("blue"));
    QCOMPARE(p.styleProperty("color", QWebElement::CascadedStyle), QLatin1String("black"));

    QString html7 = "<head>"
        "<style type='text/css'>"
            "@import url(qrc:/style2.css);"
        "</style>"
        "<link rel='stylesheet' href='qrc:/style.css' type='text/css' />"
    "</head>"
    "<body>"
        "<p id='idP' style='color: blue;'>some text</p>"
    "</body>";

    // in few seconds, the style should be completey loaded
    m_mainFrame->setHtml(html7);
    QTest::qWait(200);

    p = m_mainFrame->documentElement().findAll("p").at(0);
    QCOMPARE(p.styleProperty("color", QWebElement::CascadedStyle), QLatin1String("black"));

    QString html8 = "<body><p>some text</p></body>";

    m_mainFrame->setHtml(html8);
    p = m_mainFrame->documentElement().findAll("p").at(0);

    QCOMPARE(p.styleProperty("color", QWebElement::InlineStyle), QLatin1String(""));
    QCOMPARE(p.styleProperty("color", QWebElement::CascadedStyle), QLatin1String(""));
}

void tst_QWebElement::computedStyle()
{
    QString html = "<body><p>some text</p></body>";
    m_mainFrame->setHtml(html);

    QWebElement p = m_mainFrame->documentElement().findAll("p").at(0);
    QCOMPARE(p.styleProperty("cursor", QWebElement::ComputedStyle), QLatin1String("auto"));
    QVERIFY(!p.styleProperty("cursor", QWebElement::ComputedStyle).isEmpty());
    QVERIFY(p.styleProperty("cursor", QWebElement::InlineStyle).isEmpty());

    p.setStyleProperty("cursor", "text");
    p.setStyleProperty("color", "red");

    QCOMPARE(p.styleProperty("cursor", QWebElement::ComputedStyle), QLatin1String("text"));
    QCOMPARE(p.styleProperty("color", QWebElement::ComputedStyle), QLatin1String("rgb(255, 0, 0)"));
    QCOMPARE(p.styleProperty("color", QWebElement::InlineStyle), QLatin1String("red"));
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
    body.appendInside(body.findFirst("p"));
    QCOMPARE(body.findAll("p").count(), 2);
    QCOMPARE(body.findFirst("p").toPlainText(), QString("bar"));
    QCOMPARE(body.findAll("p").last().toPlainText(), QString("foo"));

    body.appendInside(body.findFirst("p").clone());
    QCOMPARE(body.findAll("p").count(), 3);
    QCOMPARE(body.findFirst("p").toPlainText(), QString("bar"));
    QCOMPARE(body.findAll("p").last().toPlainText(), QString("bar"));

    body.prependInside(body.findAll("p").at(1).clone());
    QCOMPARE(body.findAll("p").count(), 4);
    QCOMPARE(body.findFirst("p").toPlainText(), QString("foo"));

    body.findFirst("p").appendInside("<div>booyakasha</div>");
    QCOMPARE(body.findAll("p div").count(), 1);
    QCOMPARE(body.findFirst("p div").toPlainText(), QString("booyakasha"));

    body.findFirst("div").prependInside("<code>yepp</code>");
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

    div.prependOutside(body.findAll("p").last().clone());
    QCOMPARE(body.findAll("p").count(), 3);
    QCOMPARE(body.findAll("p").at(0).toPlainText(), QString("foo"));
    QCOMPARE(body.findAll("p").at(1).toPlainText(), QString("bar"));
    QCOMPARE(body.findAll("p").at(2).toPlainText(), QString("bar"));

    div.appendOutside(body.findFirst("p").clone());
    QCOMPARE(body.findAll("p").count(), 4);
    QCOMPARE(body.findAll("p").at(0).toPlainText(), QString("foo"));
    QCOMPARE(body.findAll("p").at(1).toPlainText(), QString("bar"));
    QCOMPARE(body.findAll("p").at(2).toPlainText(), QString("foo"));
    QCOMPARE(body.findAll("p").at(3).toPlainText(), QString("bar"));

    div.prependOutside("<span>hey</span>");
    QCOMPARE(body.findAll("span").count(), 1);

    div.appendOutside("<span>there</span>");
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
    div.takeFromDocument();

    QCOMPARE(div.isNull(), false);
    QCOMPARE(body.findAll("div").count(), 0);
    QCOMPARE(body.findAll("p").count(), 2);

    body.appendInside(div);

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
    body.findFirst("div").removeChildren();
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
    body.findFirst("div").replace(body.findFirst("span").clone());
    QCOMPARE(body.findAll("div").count(), 0);
    QCOMPARE(body.findAll("span").count(), 2);
    QCOMPARE(body.findAll("p").count(), 2);

    body.findFirst("span").replace("<p><code>wow</code></p>");
    QCOMPARE(body.findAll("p").count(), 3);
    QCOMPARE(body.findAll("p code").count(), 1);
    QCOMPARE(body.findFirst("p code").toPlainText(), QString("wow"));
}

void tst_QWebElement::encloseContentsWith()
{
    QString html = "<body>"
        "<div>"
            "<i>"
                "yeah"
            "</i>"
            "<i>"
                "hello"
            "</i>"
        "</div>"
        "<p>"
            "<span>foo</span>"
            "<span>bar</span>"
        "</p>"
        "<u></u>"
        "<b></b>"
        "<em>hey</em>"
    "</body>";

    m_mainFrame->setHtml(html);
    QWebElement body = m_mainFrame->documentElement().findFirst("body");

    body.findFirst("p").encloseContentsWith(body.findFirst("b"));
    QCOMPARE(body.findAll("p b span").count(), 2);
    QCOMPARE(body.findFirst("p b span").toPlainText(), QString("foo"));

    body.findFirst("u").encloseContentsWith("<i></i>");
    QCOMPARE(body.findAll("u i").count(), 1);
    QCOMPARE(body.findFirst("u i").toPlainText(), QString());

    body.findFirst("div").encloseContentsWith("<span></span>");
    QCOMPARE(body.findAll("div span i").count(), 2);
    QCOMPARE(body.findFirst("div span i").toPlainText(), QString("yeah"));

    QString snippet = ""
        "<table>"
            "<tbody>"
                "<tr>"
                    "<td></td>"
                    "<td></td>"
                "</tr>"
                "<tr>"
                    "<td></td>"
                    "<td></td>"
                "<tr>"
            "</tbody>"
        "</table>";

    body.findFirst("em").encloseContentsWith(snippet);
    QCOMPARE(body.findFirst("em table tbody tr td").toPlainText(), QString("hey"));
}

void tst_QWebElement::encloseWith()
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
        "<em>hey</em>"
        "<h1>hello</h1>"
    "</body>";

    m_mainFrame->setHtml(html);
    QWebElement body = m_mainFrame->documentElement().findFirst("body");

    body.findFirst("p").encloseWith("<br>");
    QCOMPARE(body.findAll("br").count(), 0);

    QCOMPARE(body.findAll("div").count(), 1);
    body.findFirst("div").encloseWith(body.findFirst("span").clone());
    QCOMPARE(body.findAll("div").count(), 1);
    QCOMPARE(body.findAll("span").count(), 2);
    QCOMPARE(body.findAll("p").count(), 2);

    body.findFirst("div").encloseWith("<code></code>");
    QCOMPARE(body.findAll("code").count(), 1);
    QCOMPARE(body.findAll("code div").count(), 1);
    QCOMPARE(body.findFirst("code div").toPlainText(), QString("yeah"));

    QString snippet = ""
        "<table>"
            "<tbody>"
                "<tr>"
                    "<td></td>"
                    "<td></td>"
                "</tr>"
                "<tr>"
                    "<td></td>"
                    "<td></td>"
                "<tr>"
            "</tbody>"
        "</table>";

    body.findFirst("em").encloseWith(snippet);
    QCOMPARE(body.findFirst("table tbody tr td em").toPlainText(), QString("hey"));
}

void tst_QWebElement::nullSelect()
{
    m_mainFrame->setHtml("<body><p>Test");

    QList<QWebElement> collection = m_mainFrame->findAllElements("invalid{syn(tax;;%#$f223e>>");
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

void tst_QWebElement::hasSetFocus()
{
    m_mainFrame->setHtml("<html><body>" \
                            "<input type='text' id='input1'/>" \
                            "<br>"\
                            "<input type='text' id='input2'/>" \
                            "</body></html>");

    QList<QWebElement> inputs = m_mainFrame->documentElement().findAll("input");
    QWebElement input1 = inputs.at(0);
    input1.setFocus();
    QVERIFY(input1.hasFocus());

    QWebElement input2 = inputs.at(1);
    input2.setFocus();
    QVERIFY(!input1.hasFocus());
    QVERIFY(input2.hasFocus());
}

QTEST_MAIN(tst_QWebElement)
#include "tst_qwebelement.moc"
