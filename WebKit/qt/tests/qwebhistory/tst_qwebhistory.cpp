/*
    Copyright (C) 2008 Holger Hans Peter Freyther

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

#include "qwebpage.h"
#include "qwebview.h"
#include "qwebframe.h"
#include "qwebhistory.h"
#include "qdebug.h"

class tst_QWebHistory : public QObject
{
    Q_OBJECT

public:
    tst_QWebHistory();
    virtual ~tst_QWebHistory();

protected :
    void loadPage(int nr)
    {
        frame->load(QUrl("qrc:/data/page"+QString::number(nr)+".html"));
        waitForLoadFinished.exec();
    }

public slots:
    void init();
    void cleanup();

private slots:    
    void title();
    void count();
    void back();
    void forward();
    void itemAt();
    void goToItem();
    void items();
    /*
    void serialize_1(); //QWebHistory countity
    void serialize_2(); //QWebHistory index
    void serialize_3(); //QWebHistoryItem
    */


private:
    QWebPage* page;
    QWebFrame* frame;
    QWebHistory* hist;
    QEventLoop waitForLoadFinished;  //operation on history are asynchronous!
    int histsize;
};

tst_QWebHistory::tst_QWebHistory()
{
}

tst_QWebHistory::~tst_QWebHistory()
{
}

void tst_QWebHistory::init()
{
    page = new QWebPage(this);
    frame = page->mainFrame();
    connect(page,SIGNAL(loadFinished(bool)),&waitForLoadFinished,SLOT(quit()));

    for(int i=1;i<6;i++){
        loadPage(i);
    }
    hist = page->history();
    histsize=5;
}

void tst_QWebHistory::cleanup()
{
    delete page;
}

/**
  * Check QWebHistoryItem::title() method
  */
void tst_QWebHistory::title()
{
    QCOMPARE(hist->currentItem().title(),QString("page5"));
}

/**
  * Check QWebHistory::count() method
  */
void tst_QWebHistory::count()
{
    QCOMPARE(hist->count(),histsize);
}

/**
  * Check QWebHistory::back() method
  */
void tst_QWebHistory::back()
{
    for(int i=histsize;i>1;i--){
        QCOMPARE(page->mainFrame()->toPlainText(),QString("page")+QString::number(i));
        hist->back();
        waitForLoadFinished.exec();
    }
}

/**
  * Check QWebHistory::forward() method
  */
void tst_QWebHistory::forward()
{
    //rewind history :-)
    while(hist->canGoBack()){
        hist->back();
        waitForLoadFinished.exec();
    }

    for(int i=1;i<histsize;i++){
        QCOMPARE(page->mainFrame()->toPlainText(),QString("page")+QString::number(i));
        hist->forward();
        waitForLoadFinished.exec();
    }
}

/**
  * Check QWebHistory::itemAt() method
  */
void tst_QWebHistory::itemAt()
{
    for(int i=1;i<histsize;i++) {
        QCOMPARE(hist->itemAt(i-1).title(),QString("page")+QString::number(i));
    }
}

/**
  * Check QWebHistory::goToItem() method
  */
void tst_QWebHistory::goToItem()
{
    QWebHistoryItem current=hist->currentItem();
    hist->back();
    waitForLoadFinished.exec();
    hist->back();
    waitForLoadFinished.exec();
    QVERIFY(hist->currentItem().title()!=current.title());
    hist->goToItem(current);
    waitForLoadFinished.exec();
    QCOMPARE(hist->currentItem().title(),current.title());
}

/**
  * Check QWebHistory::items() method
  */
void tst_QWebHistory::items()
{
    QList<QWebHistoryItem> items=hist->items();
    //check count
    QCOMPARE(histsize,items.count());

    //check order
    for(int i=1;i<=histsize;i++){
        QCOMPARE(items.at(i-1).title(),QString("page")+QString::number(i));
    }
}

/**
  * Check history state after serialization (pickle, persistent..) method
  * Checks history size, history order
void tst_QWebHistory::serialize_1()
{
    QByteArray tmp;  //buffer
    QDataStream save(&tmp,QIODevice::WriteOnly); //here data will be saved
    QDataStream load(&tmp,QIODevice::ReadOnly);  //from here data will be loaded

    hist->operator<<(save);
    QCOMPARE(hist->count(),histsize);

    //check size of history
    //load next page to find differences
    loadPage(6);
    QCOMPARE(hist->count(),histsize+1);
    hist->operator>>(load);
    QCOMPARE(hist->count(),histsize);

    //check order of historyItems
    QList<QWebHistoryItem> items = hist->items();
    for(int i=1;i<=histsize;i++){
        QCOMPARE(items.at(i-1).title(),QString("page")+QString::number(i));
    }
}

/**
  * Check history state after serialization (pickle, persistent..) method
  * Checks history currentIndex value
void tst_QWebHistory::serialize_2()
{
    QByteArray tmp;  //buffer
    QDataStream save(&tmp,QIODevice::WriteOnly); //here data will be saved
    QDataStream load(&tmp,QIODevice::ReadOnly);  //from here data will be loaded

    int oldCurrentIndex=hist->currentItemIndex();

    hist->back();
    waitForLoadFinished.exec();
    hist->back();
    waitForLoadFinished.exec();
    //check if current index was changed (make sure that it is not last item)
    QVERIFY(hist->currentItemIndex()!=oldCurrentIndex);
    //save current index
    oldCurrentIndex=hist->currentItemIndex();

    hist->operator<<(save);
    hist->operator>>(load);

    //check current index
    QCOMPARE(hist->currentItemIndex(),oldCurrentIndex);
}

/**
  * Check history state after serialization (pickle, persistent..) method
  * Checks QWebHistoryItem public property after serialization
void tst_QWebHistory::serialize_3()
{
    QByteArray tmp;  //buffer
    QDataStream save(&tmp,QIODevice::WriteOnly); //here data will be saved
    QDataStream load(&tmp,QIODevice::ReadOnly);  //from here data will be loaded

    //prepare two different history items
    QWebHistoryItem a=hist->currentItem();
    hist->back();
    waitForLoadFinished.exec();
    QWebHistoryItem b=hist->currentItem();
    a.setUserData("A - user data");
    b.setUserData("B - user data");

    //check properties BEFORE serialization
    QVERIFY(a.title()!=b.title());
    //there is no way to set lastVisited without visiting :-) (from public interface)
    QVERIFY(a.originalUrl()!=b.originalUrl());
    QVERIFY(a.url()!=b.url());
    QVERIFY(a.userData()!=b.userData());

    a.operator<<(save);
    QVERIFY(save.status()==QDataStream::Ok);
    QVERIFY(!load.atEnd());
    b.operator>>(load);
    QVERIFY(load.status()==QDataStream::Ok);

    //check properties AFTER serialization
    QCOMPARE(a.title(),b.title());
    QCOMPARE(a.lastVisited(),b.lastVisited());
    QCOMPARE(a.originalUrl(),b.originalUrl());
    QCOMPARE(a.url(),b.url());
    QCOMPARE(a.userData(),b.userData());

    //Check if all data was read
    QVERIFY(load.atEnd());
}
*/


QTEST_MAIN(tst_QWebHistory)
#include "tst_qwebhistory.moc"
