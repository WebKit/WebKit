/*
    Copyright (C) 2008,2009 Nokia Corporation and/or its subsidiary(-ies)

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
#include <qwebelement.h>
#include <qwidget.h>
#include <qwebview.h>
#include <qwebframe.h>
#include <qwebhistory.h>
#include <QAbstractItemView>
#include <QApplication>
#include <QComboBox>
#include <QPicture>
#include <QRegExp>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTextCodec>
#ifndef QT_NO_OPENSSL
#include <qsslerror.h>
#endif
#include "../util.h"

struct CustomType {
    QString string;
};
Q_DECLARE_METATYPE(CustomType)

Q_DECLARE_METATYPE(QBrush*)
Q_DECLARE_METATYPE(QObjectList)
Q_DECLARE_METATYPE(QList<int>)
Q_DECLARE_METATYPE(Qt::BrushStyle)
Q_DECLARE_METATYPE(QVariantList)
Q_DECLARE_METATYPE(QVariantMap)

class MyQObject : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int intProperty READ intProperty WRITE setIntProperty)
    Q_PROPERTY(QVariant variantProperty READ variantProperty WRITE setVariantProperty)
    Q_PROPERTY(QVariantList variantListProperty READ variantListProperty WRITE setVariantListProperty)
    Q_PROPERTY(QVariantMap variantMapProperty READ variantMapProperty WRITE setVariantMapProperty)
    Q_PROPERTY(QString stringProperty READ stringProperty WRITE setStringProperty)
    Q_PROPERTY(QStringList stringListProperty READ stringListProperty WRITE setStringListProperty)
    Q_PROPERTY(QByteArray byteArrayProperty READ byteArrayProperty WRITE setByteArrayProperty)
    Q_PROPERTY(QBrush brushProperty READ brushProperty WRITE setBrushProperty)
    Q_PROPERTY(double hiddenProperty READ hiddenProperty WRITE setHiddenProperty SCRIPTABLE false)
    Q_PROPERTY(int writeOnlyProperty WRITE setWriteOnlyProperty)
    Q_PROPERTY(int readOnlyProperty READ readOnlyProperty)
    Q_PROPERTY(QKeySequence shortcut READ shortcut WRITE setShortcut)
    Q_PROPERTY(CustomType propWithCustomType READ propWithCustomType WRITE setPropWithCustomType)
    Q_PROPERTY(QWebElement webElementProperty READ webElementProperty WRITE setWebElementProperty)
    Q_PROPERTY(QObject* objectStarProperty READ objectStarProperty WRITE setObjectStarProperty)
    Q_ENUMS(Policy Strategy)
    Q_FLAGS(Ability)

public:
    enum Policy {
        FooPolicy = 0,
        BarPolicy,
        BazPolicy
    };

    enum Strategy {
        FooStrategy = 10,
        BarStrategy,
        BazStrategy
    };

    enum AbilityFlag {
        NoAbility  = 0x000,
        FooAbility = 0x001,
        BarAbility = 0x080,
        BazAbility = 0x200,
        AllAbility = FooAbility | BarAbility | BazAbility
    };

    Q_DECLARE_FLAGS(Ability, AbilityFlag)

    MyQObject(QObject* parent = 0)
        : QObject(parent),
            m_intValue(123),
            m_variantValue(QLatin1String("foo")),
            m_variantListValue(QVariantList() << QVariant(123) << QVariant(QLatin1String("foo"))),
            m_stringValue(QLatin1String("bar")),
            m_stringListValue(QStringList() << QLatin1String("zig") << QLatin1String("zag")),
            m_brushValue(QColor(10, 20, 30, 40)),
            m_hiddenValue(456.0),
            m_writeOnlyValue(789),
            m_readOnlyValue(987),
            m_objectStar(0),
            m_qtFunctionInvoked(-1)
    {
        m_variantMapValue.insert("a", QVariant(123));
        m_variantMapValue.insert("b", QVariant(QLatin1String("foo")));
        m_variantMapValue.insert("c", QVariant::fromValue<QObject*>(this));
    }

    ~MyQObject() { }

    int intProperty() const {
        return m_intValue;
    }
    void setIntProperty(int value) {
        m_intValue = value;
    }

    QVariant variantProperty() const {
        return m_variantValue;
    }
    void setVariantProperty(const QVariant &value) {
        m_variantValue = value;
    }

    QVariantList variantListProperty() const {
        return m_variantListValue;
    }
    void setVariantListProperty(const QVariantList &value) {
        m_variantListValue = value;
    }

    QVariantMap variantMapProperty() const {
        return m_variantMapValue;
    }
    void setVariantMapProperty(const QVariantMap &value) {
        m_variantMapValue = value;
    }

    QString stringProperty() const {
        return m_stringValue;
    }
    void setStringProperty(const QString &value) {
        m_stringValue = value;
    }

    QStringList stringListProperty() const {
        return m_stringListValue;
    }
    void setStringListProperty(const QStringList &value) {
        m_stringListValue = value;
    }

    QByteArray byteArrayProperty() const {
        return m_byteArrayValue;
    }
    void setByteArrayProperty(const QByteArray &value) {
        m_byteArrayValue = value;
    }

    QBrush brushProperty() const {
        return m_brushValue;
    }
    Q_INVOKABLE void setBrushProperty(const QBrush &value) {
        m_brushValue = value;
    }

    double hiddenProperty() const {
        return m_hiddenValue;
    }
    void setHiddenProperty(double value) {
        m_hiddenValue = value;
    }

    int writeOnlyProperty() const {
        return m_writeOnlyValue;
    }
    void setWriteOnlyProperty(int value) {
        m_writeOnlyValue = value;
    }

    int readOnlyProperty() const {
        return m_readOnlyValue;
    }

    QKeySequence shortcut() const {
        return m_shortcut;
    }
    void setShortcut(const QKeySequence &seq) {
        m_shortcut = seq;
    }

    QWebElement webElementProperty() const {
        return m_webElement;
    }

    void setWebElementProperty(const QWebElement& element) {
        m_webElement = element;
    }

    CustomType propWithCustomType() const {
        return m_customType;
    }
    void setPropWithCustomType(const CustomType &c) {
        m_customType = c;
    }

    QObject* objectStarProperty() const {
        return m_objectStar;
    }

    void setObjectStarProperty(QObject* object) {
        m_objectStar = object;
    }


    int qtFunctionInvoked() const {
        return m_qtFunctionInvoked;
    }

    QVariantList qtFunctionActuals() const {
        return m_actuals;
    }

    void resetQtFunctionInvoked() {
        m_qtFunctionInvoked = -1;
        m_actuals.clear();
    }

    Q_INVOKABLE void myInvokable() {
        m_qtFunctionInvoked = 0;
    }
    Q_INVOKABLE void myInvokableWithIntArg(int arg) {
        m_qtFunctionInvoked = 1;
        m_actuals << arg;
    }
    Q_INVOKABLE void myInvokableWithLonglongArg(qlonglong arg) {
        m_qtFunctionInvoked = 2;
        m_actuals << arg;
    }
    Q_INVOKABLE void myInvokableWithFloatArg(float arg) {
        m_qtFunctionInvoked = 3;
        m_actuals << arg;
    }
    Q_INVOKABLE void myInvokableWithDoubleArg(double arg) {
        m_qtFunctionInvoked = 4;
        m_actuals << arg;
    }
    Q_INVOKABLE void myInvokableWithStringArg(const QString &arg) {
        m_qtFunctionInvoked = 5;
        m_actuals << arg;
    }
    Q_INVOKABLE void myInvokableWithIntArgs(int arg1, int arg2) {
        m_qtFunctionInvoked = 6;
        m_actuals << arg1 << arg2;
    }
    Q_INVOKABLE int myInvokableReturningInt() {
        m_qtFunctionInvoked = 7;
        return 123;
    }
    Q_INVOKABLE qlonglong myInvokableReturningLongLong() {
        m_qtFunctionInvoked = 39;
        return 456;
    }
    Q_INVOKABLE QString myInvokableReturningString() {
        m_qtFunctionInvoked = 8;
        return QLatin1String("ciao");
    }
    Q_INVOKABLE void myInvokableWithIntArg(int arg1, int arg2) { // overload
        m_qtFunctionInvoked = 9;
        m_actuals << arg1 << arg2;
    }
    Q_INVOKABLE void myInvokableWithEnumArg(Policy policy) {
        m_qtFunctionInvoked = 10;
        m_actuals << policy;
    }
    Q_INVOKABLE void myInvokableWithQualifiedEnumArg(MyQObject::Policy policy) {
        m_qtFunctionInvoked = 36;
        m_actuals << policy;
    }
    Q_INVOKABLE Policy myInvokableReturningEnum() {
        m_qtFunctionInvoked = 37;
        return BazPolicy;
    }
    Q_INVOKABLE MyQObject::Policy myInvokableReturningQualifiedEnum() {
        m_qtFunctionInvoked = 38;
        return BazPolicy;
    }
    Q_INVOKABLE QVector<int> myInvokableReturningVectorOfInt() {
        m_qtFunctionInvoked = 11;
        return QVector<int>();
    }
    Q_INVOKABLE void myInvokableWithVectorOfIntArg(const QVector<int> &) {
        m_qtFunctionInvoked = 12;
    }
    Q_INVOKABLE QObject* myInvokableReturningQObjectStar() {
        m_qtFunctionInvoked = 13;
        return this;
    }
    Q_INVOKABLE QObjectList myInvokableWithQObjectListArg(const QObjectList &lst) {
        m_qtFunctionInvoked = 14;
        m_actuals << qVariantFromValue(lst);
        return lst;
    }
    Q_INVOKABLE QVariant myInvokableWithVariantArg(const QVariant &v) {
        m_qtFunctionInvoked = 15;
        m_actuals << v;
        return v;
    }
    Q_INVOKABLE QVariantMap myInvokableWithVariantMapArg(const QVariantMap &vm) {
        m_qtFunctionInvoked = 16;
        m_actuals << vm;
        return vm;
    }
    Q_INVOKABLE QList<int> myInvokableWithListOfIntArg(const QList<int> &lst) {
        m_qtFunctionInvoked = 17;
        m_actuals << qVariantFromValue(lst);
        return lst;
    }
    Q_INVOKABLE QObject* myInvokableWithQObjectStarArg(QObject* obj) {
        m_qtFunctionInvoked = 18;
        m_actuals << qVariantFromValue(obj);
        return obj;
    }
    Q_INVOKABLE QBrush myInvokableWithQBrushArg(const QBrush &brush) {
        m_qtFunctionInvoked = 19;
        m_actuals << qVariantFromValue(brush);
        return brush;
    }
    Q_INVOKABLE void myInvokableWithBrushStyleArg(Qt::BrushStyle style) {
        m_qtFunctionInvoked = 43;
        m_actuals << qVariantFromValue(style);
    }
    Q_INVOKABLE void myInvokableWithVoidStarArg(void* arg) {
        m_qtFunctionInvoked = 44;
        m_actuals << qVariantFromValue(arg);
    }
    Q_INVOKABLE void myInvokableWithAmbiguousArg(int arg) {
        m_qtFunctionInvoked = 45;
        m_actuals << qVariantFromValue(arg);
    }
    Q_INVOKABLE void myInvokableWithAmbiguousArg(uint arg) {
        m_qtFunctionInvoked = 46;
        m_actuals << qVariantFromValue(arg);
    }
    Q_INVOKABLE void myInvokableWithDefaultArgs(int arg1, const QString &arg2 = "") {
        m_qtFunctionInvoked = 47;
        m_actuals << qVariantFromValue(arg1) << qVariantFromValue(arg2);
    }
    Q_INVOKABLE QObject& myInvokableReturningRef() {
        m_qtFunctionInvoked = 48;
        return *this;
    }
    Q_INVOKABLE const QObject& myInvokableReturningConstRef() const {
        const_cast<MyQObject*>(this)->m_qtFunctionInvoked = 49;
        return *this;
    }
    Q_INVOKABLE void myInvokableWithPointArg(const QPoint &arg) {
        const_cast<MyQObject*>(this)->m_qtFunctionInvoked = 50;
        m_actuals << qVariantFromValue(arg);
    }
    Q_INVOKABLE void myInvokableWithPointArg(const QPointF &arg) {
        const_cast<MyQObject*>(this)->m_qtFunctionInvoked = 51;
        m_actuals << qVariantFromValue(arg);
    }
    Q_INVOKABLE void myInvokableWithBoolArg(bool arg) {
        m_qtFunctionInvoked = 52;
        m_actuals << arg;
    }

    void emitMySignal() {
        emit mySignal();
    }
    void emitMySignalWithIntArg(int arg) {
        emit mySignalWithIntArg(arg);
    }
    void emitMySignal2(bool arg) {
        emit mySignal2(arg);
    }
    void emitMySignal2() {
        emit mySignal2();
    }
    void emitMySignalWithDateTimeArg(QDateTime dt) {
        emit mySignalWithDateTimeArg(dt);
    }
    void emitMySignalWithRegexArg(QRegExp r) {
        emit mySignalWithRegexArg(r);
    }

public Q_SLOTS:
    void mySlot() {
        m_qtFunctionInvoked = 20;
    }
    void mySlotWithIntArg(int arg) {
        m_qtFunctionInvoked = 21;
        m_actuals << arg;
    }
    void mySlotWithDoubleArg(double arg) {
        m_qtFunctionInvoked = 22;
        m_actuals << arg;
    }
    void mySlotWithStringArg(const QString &arg) {
        m_qtFunctionInvoked = 23;
        m_actuals << arg;
    }

    void myOverloadedSlot() {
        m_qtFunctionInvoked = 24;
    }
    void myOverloadedSlot(QObject* arg) {
        m_qtFunctionInvoked = 41;
        m_actuals << qVariantFromValue(arg);
    }
    void myOverloadedSlot(bool arg) {
        m_qtFunctionInvoked = 25;
        m_actuals << arg;
    }
    void myOverloadedSlot(const QStringList &arg) {
        m_qtFunctionInvoked = 42;
        m_actuals << arg;
    }
    void myOverloadedSlot(double arg) {
        m_qtFunctionInvoked = 26;
        m_actuals << arg;
    }
    void myOverloadedSlot(float arg) {
        m_qtFunctionInvoked = 27;
        m_actuals << arg;
    }
    void myOverloadedSlot(int arg) {
        m_qtFunctionInvoked = 28;
        m_actuals << arg;
    }
    void myOverloadedSlot(const QString &arg) {
        m_qtFunctionInvoked = 29;
        m_actuals << arg;
    }
    void myOverloadedSlot(const QColor &arg) {
        m_qtFunctionInvoked = 30;
        m_actuals << arg;
    }
    void myOverloadedSlot(const QBrush &arg) {
        m_qtFunctionInvoked = 31;
        m_actuals << arg;
    }
    void myOverloadedSlot(const QDateTime &arg) {
        m_qtFunctionInvoked = 32;
        m_actuals << arg;
    }
    void myOverloadedSlot(const QDate &arg) {
        m_qtFunctionInvoked = 33;
        m_actuals << arg;
    }
    void myOverloadedSlot(const QRegExp &arg) {
        m_qtFunctionInvoked = 34;
        m_actuals << arg;
    }
    void myOverloadedSlot(const QVariant &arg) {
        m_qtFunctionInvoked = 35;
        m_actuals << arg;
    }
    void myOverloadedSlot(const QWebElement &arg) {
        m_qtFunctionInvoked = 36;
        m_actuals << QVariant::fromValue<QWebElement>(arg);
    }

    void qscript_call(int arg) {
        m_qtFunctionInvoked = 40;
        m_actuals << arg;
    }

protected Q_SLOTS:
    void myProtectedSlot() {
        m_qtFunctionInvoked = 36;
    }

private Q_SLOTS:
    void myPrivateSlot() { }

Q_SIGNALS:
    void mySignal();
    void mySignalWithIntArg(int arg);
    void mySignalWithDoubleArg(double arg);
    void mySignal2(bool arg = false);
    void mySignalWithDateTimeArg(QDateTime dt);
    void mySignalWithRegexArg(QRegExp r);

private:
    int m_intValue;
    QVariant m_variantValue;
    QVariantList m_variantListValue;
    QVariantMap m_variantMapValue;
    QString m_stringValue;
    QStringList m_stringListValue;
    QByteArray m_byteArrayValue;
    QBrush m_brushValue;
    double m_hiddenValue;
    int m_writeOnlyValue;
    int m_readOnlyValue;
    QKeySequence m_shortcut;
    QWebElement m_webElement;
    CustomType m_customType;
    QObject* m_objectStar;
    int m_qtFunctionInvoked;
    QVariantList m_actuals;
};

class MyOtherQObject : public MyQObject
{
public:
    MyOtherQObject(QObject* parent = 0)
        : MyQObject(parent) { }
};

class MyEnumTestQObject : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString p1 READ p1)
    Q_PROPERTY(QString p2 READ p2)
    Q_PROPERTY(QString p3 READ p3 SCRIPTABLE false)
    Q_PROPERTY(QString p4 READ p4)
    Q_PROPERTY(QString p5 READ p5 SCRIPTABLE false)
    Q_PROPERTY(QString p6 READ p6)
public:
    MyEnumTestQObject(QObject* parent = 0)
        : QObject(parent) { }
    QString p1() const {
        return QLatin1String("p1");
    }
    QString p2() const {
        return QLatin1String("p2");
    }
    QString p3() const {
        return QLatin1String("p3");
    }
    QString p4() const {
        return QLatin1String("p4");
    }
    QString p5() const {
        return QLatin1String("p5");
    }
    QString p6() const {
        return QLatin1String("p5");
    }
public Q_SLOTS:
    void mySlot() { }
    void myOtherSlot() { }
Q_SIGNALS:
    void mySignal();
};

class tst_QWebFrame : public QObject
{
    Q_OBJECT

public:
    tst_QWebFrame();
    virtual ~tst_QWebFrame();
    bool eventFilter(QObject* watched, QEvent* event);

public slots:
    void init();
    void cleanup();

private slots:
    void getSetStaticProperty();
    void getSetDynamicProperty();
    void getSetChildren();
    void callQtInvokable();
    void connectAndDisconnect();
    void classEnums();
    void classConstructor();
    void overrideInvokable();
    void transferInvokable();
    void findChild();
    void findChildren();
    void overloadedSlots();
    void enumerate_data();
    void enumerate();
    void objectDeleted();
    void typeConversion();
    void arrayObjectEnumerable();
    void symmetricUrl();
    void progressSignal();
    void urlChange();
    void domCycles();
    void requestedUrl();
    void javaScriptWindowObjectCleared_data();
    void javaScriptWindowObjectCleared();
    void javaScriptWindowObjectClearedOnEvaluate();
    void setHtml();
    void setHtmlWithResource();
    void setHtmlWithBaseURL();
    void setHtmlWithJSAlert();
    void ipv6HostEncoding();
    void metaData();
#if !defined(Q_WS_MAEMO_5) && !defined(Q_OS_SYMBIAN)
    // as maemo 5 && symbian do not use QComboBoxes to implement the popups
    // this test does not make sense for it.
    void popupFocus();
#endif
    void inputFieldFocus();
    void hitTestContent();
    void jsByteArray();
    void ownership();
    void nullValue();
    void baseUrl_data();
    void baseUrl();
    void hasSetFocus();
    void render();
    void scrollPosition();
    void scrollToAnchor();
    void scrollbarsOff();
    void evaluateWillCauseRepaint();
    void qObjectWrapperWithSameIdentity();
    void introspectQtMethods_data();
    void introspectQtMethods();
    void setContent_data();
    void setContent();

private:
    QString  evalJS(const QString&s) {
        // Convert an undefined return variant to the string "undefined"
        QVariant ret = evalJSV(s);
        if (ret.userType() == QMetaType::Void)
            return "undefined";
        else
            return ret.toString();
    }
    QVariant evalJSV(const QString &s) {
        return m_page->mainFrame()->evaluateJavaScript(s);
    }

    QString  evalJS(const QString&s, QString& type) {
        return evalJSV(s, type).toString();
    }
    QVariant evalJSV(const QString &s, QString& type) {
        // As a special measure, if we get an exception we set the type to 'error'
        // (in ecma, an Error object has typeof object, but qtscript has a convenience function)
        // Similarly, an array is an object, but we'd prefer to have a type of 'array'
        // Also, consider a QMetaType::Void QVariant to be undefined
        QString escaped = s;
        escaped.replace('\'', "\\'"); // Don't preescape your single quotes!
        evalJS("var retvalue;\
               var typevalue; \
               try {\
               retvalue = eval('" + escaped + "'); \
               typevalue = typeof retvalue; \
               if (retvalue instanceof Array) \
               typevalue = 'array'; \
           } \
               catch(e) {\
               retvalue = e.name + ': ' + e.message;\
               typevalue = 'error';\
           }");
        QVariant ret = evalJSV("retvalue");
        if (ret.userType() != QMetaType::Void)
            type = evalJS("typevalue");
        else {
            ret = QString("undefined");
            type = sUndefined;
        }
        evalJS("delete retvalue; delete typevalue");
        return ret;
    }
    QObject* firstChildByClassName(QObject* parent, const char* className) {
        const QObjectList & children = parent->children();
        foreach (QObject* child, children) {
            if (!strcmp(child->metaObject()->className(), className)) {
                return child;
            }
        }
        return 0;
    }

    const QString sTrue;
    const QString sFalse;
    const QString sUndefined;
    const QString sArray;
    const QString sFunction;
    const QString sError;
    const QString sString;
    const QString sObject;
    const QString sNumber;

private:
    QWebView* m_view;
    QWebPage* m_page;
    MyQObject* m_myObject;
    QWebView* m_inputFieldsTestView;
    int m_inputFieldTestPaintCount;
};

tst_QWebFrame::tst_QWebFrame()
    : sTrue("true"), sFalse("false"), sUndefined("undefined"), sArray("array"), sFunction("function"), sError("error"),
        sString("string"), sObject("object"), sNumber("number"), m_inputFieldsTestView(0), m_inputFieldTestPaintCount(0)
{
}

tst_QWebFrame::~tst_QWebFrame()
{
}

bool tst_QWebFrame::eventFilter(QObject* watched, QEvent* event)
{
    // used on the inputFieldFocus test
    if (watched == m_inputFieldsTestView) {
        if (event->type() == QEvent::Paint)
            m_inputFieldTestPaintCount++;
    }
    return QObject::eventFilter(watched, event);
}

void tst_QWebFrame::init()
{
    m_view = new QWebView();
    m_page = m_view->page();
    m_myObject = new MyQObject();
    m_page->mainFrame()->addToJavaScriptWindowObject("myObject", m_myObject);
}

void tst_QWebFrame::cleanup()
{
    delete m_view;
    delete m_myObject;
}

void tst_QWebFrame::getSetStaticProperty()
{
    m_page->mainFrame()->setHtml("<html><head><body></body></html>");
    QCOMPARE(evalJS("typeof myObject.noSuchProperty"), sUndefined);

    // initial value (set in MyQObject constructor)
    {
        QString type;
        QVariant ret = evalJSV("myObject.intProperty", type);
        QCOMPARE(type, sNumber);
        QCOMPARE(ret.type(), QVariant::Double);
        QCOMPARE(ret.toInt(), 123);
    }
    QCOMPARE(evalJS("myObject.intProperty === 123.0"), sTrue);

    {
        QString type;
        QVariant ret = evalJSV("myObject.variantProperty", type);
        QCOMPARE(type, sString);
        QCOMPARE(ret.type(), QVariant::String);
        QCOMPARE(ret.toString(), QLatin1String("foo"));
    }
    QCOMPARE(evalJS("myObject.variantProperty == 'foo'"), sTrue);

    {
        QString type;
        QVariant ret = evalJSV("myObject.stringProperty", type);
        QCOMPARE(type, sString);
        QCOMPARE(ret.type(), QVariant::String);
        QCOMPARE(ret.toString(), QLatin1String("bar"));
    }
    QCOMPARE(evalJS("myObject.stringProperty === 'bar'"), sTrue);

    {
        QString type;
        QVariant ret = evalJSV("myObject.variantListProperty", type);
        QCOMPARE(type, sArray);
        QCOMPARE(ret.type(), QVariant::List);
        QVariantList vl = ret.value<QVariantList>();
        QCOMPARE(vl.size(), 2);
        QCOMPARE(vl.at(0).toInt(), 123);
        QCOMPARE(vl.at(1).toString(), QLatin1String("foo"));
    }
    QCOMPARE(evalJS("myObject.variantListProperty.length === 2"), sTrue);
    QCOMPARE(evalJS("myObject.variantListProperty[0] === 123"), sTrue);
    QCOMPARE(evalJS("myObject.variantListProperty[1] === 'foo'"), sTrue);

    {
        QString type;
        QVariant ret = evalJSV("myObject.variantMapProperty", type);
        QCOMPARE(type, sObject);
        QCOMPARE(ret.type(), QVariant::Map);
        QVariantMap vm = ret.value<QVariantMap>();
        QCOMPARE(vm.size(), 3);
        QCOMPARE(vm.value("a").toInt(), 123);
        QCOMPARE(vm.value("b").toString(), QLatin1String("foo"));
        QCOMPARE(vm.value("c").value<QObject*>(), static_cast<QObject*>(m_myObject));
    }
    QCOMPARE(evalJS("myObject.variantMapProperty.a === 123"), sTrue);
    QCOMPARE(evalJS("myObject.variantMapProperty.b === 'foo'"), sTrue);
    QCOMPARE(evalJS("myObject.variantMapProperty.c.variantMapProperty.b === 'foo'"), sTrue);

    {
        QString type;
        QVariant ret = evalJSV("myObject.stringListProperty", type);
        QCOMPARE(type, sArray);
        QCOMPARE(ret.type(), QVariant::List);
        QVariantList vl = ret.value<QVariantList>();
        QCOMPARE(vl.size(), 2);
        QCOMPARE(vl.at(0).toString(), QLatin1String("zig"));
        QCOMPARE(vl.at(1).toString(), QLatin1String("zag"));
    }
    QCOMPARE(evalJS("myObject.stringListProperty.length === 2"), sTrue);
    QCOMPARE(evalJS("typeof myObject.stringListProperty[0]"), sString);
    QCOMPARE(evalJS("myObject.stringListProperty[0]"), QLatin1String("zig"));
    QCOMPARE(evalJS("typeof myObject.stringListProperty[1]"), sString);
    QCOMPARE(evalJS("myObject.stringListProperty[1]"), QLatin1String("zag"));

    // property change in C++ should be reflected in script
    m_myObject->setIntProperty(456);
    QCOMPARE(evalJS("myObject.intProperty == 456"), sTrue);
    m_myObject->setIntProperty(789);
    QCOMPARE(evalJS("myObject.intProperty == 789"), sTrue);

    m_myObject->setVariantProperty(QLatin1String("bar"));
    QCOMPARE(evalJS("myObject.variantProperty === 'bar'"), sTrue);
    m_myObject->setVariantProperty(42);
    QCOMPARE(evalJS("myObject.variantProperty === 42"), sTrue);
    m_myObject->setVariantProperty(qVariantFromValue(QBrush()));
//XFAIL
//  QCOMPARE(evalJS("typeof myObject.variantProperty"), sVariant);

    m_myObject->setStringProperty(QLatin1String("baz"));
    QCOMPARE(evalJS("myObject.stringProperty === 'baz'"), sTrue);
    m_myObject->setStringProperty(QLatin1String("zab"));
    QCOMPARE(evalJS("myObject.stringProperty === 'zab'"), sTrue);

    // property change in script should be reflected in C++
    QCOMPARE(evalJS("myObject.intProperty = 123"), QLatin1String("123"));
    QCOMPARE(evalJS("myObject.intProperty == 123"), sTrue);
    QCOMPARE(m_myObject->intProperty(), 123);
    QCOMPARE(evalJS("myObject.intProperty = 'ciao!';"
                    "myObject.intProperty == 0"), sTrue);
    QCOMPARE(m_myObject->intProperty(), 0);
    QCOMPARE(evalJS("myObject.intProperty = '123';"
                    "myObject.intProperty == 123"), sTrue);
    QCOMPARE(m_myObject->intProperty(), 123);

    QCOMPARE(evalJS("myObject.stringProperty = 'ciao'"), QLatin1String("ciao"));
    QCOMPARE(evalJS("myObject.stringProperty"), QLatin1String("ciao"));
    QCOMPARE(m_myObject->stringProperty(), QLatin1String("ciao"));
    QCOMPARE(evalJS("myObject.stringProperty = 123;"
                    "myObject.stringProperty"), QLatin1String("123"));
    QCOMPARE(m_myObject->stringProperty(), QLatin1String("123"));
    QCOMPARE(evalJS("myObject.stringProperty = null"), QString());
    QCOMPARE(evalJS("myObject.stringProperty"), QString());
    QCOMPARE(m_myObject->stringProperty(), QString());
    QCOMPARE(evalJS("myObject.stringProperty = undefined"), sUndefined);
    QCOMPARE(evalJS("myObject.stringProperty"), QString());
    QCOMPARE(m_myObject->stringProperty(), QString());

    QCOMPARE(evalJS("myObject.variantProperty = new Number(1234);"
                    "myObject.variantProperty").toDouble(), 1234.0);
    QCOMPARE(m_myObject->variantProperty().toDouble(), 1234.0);

    QCOMPARE(evalJS("myObject.variantProperty = new Boolean(1234);"
                    "myObject.variantProperty"), sTrue);
    QCOMPARE(m_myObject->variantProperty().toBool(), true);

    QCOMPARE(evalJS("myObject.variantProperty = null;"
                    "myObject.variantProperty.valueOf()"), sUndefined);
    QCOMPARE(m_myObject->variantProperty(), QVariant());
    QCOMPARE(evalJS("myObject.variantProperty = undefined;"
                    "myObject.variantProperty.valueOf()"), sUndefined);
    QCOMPARE(m_myObject->variantProperty(), QVariant());

    QCOMPARE(evalJS("myObject.variantProperty = 'foo';"
                    "myObject.variantProperty.valueOf()"), QLatin1String("foo"));
    QCOMPARE(m_myObject->variantProperty(), QVariant(QLatin1String("foo")));
    QCOMPARE(evalJS("myObject.variantProperty = 42;"
                    "myObject.variantProperty").toDouble(), 42.0);
    QCOMPARE(m_myObject->variantProperty().toDouble(), 42.0);

    QCOMPARE(evalJS("myObject.variantListProperty = [1, 'two', true];"
                    "myObject.variantListProperty.length == 3"), sTrue);
    QCOMPARE(evalJS("myObject.variantListProperty[0] === 1"), sTrue);
    QCOMPARE(evalJS("myObject.variantListProperty[1]"), QLatin1String("two"));
    QCOMPARE(evalJS("myObject.variantListProperty[2] === true"), sTrue);

    QCOMPARE(evalJS("myObject.stringListProperty = [1, 'two', true];"
                    "myObject.stringListProperty.length == 3"), sTrue);
    QCOMPARE(evalJS("typeof myObject.stringListProperty[0]"), sString);
    QCOMPARE(evalJS("myObject.stringListProperty[0] == '1'"), sTrue);
    QCOMPARE(evalJS("typeof myObject.stringListProperty[1]"), sString);
    QCOMPARE(evalJS("myObject.stringListProperty[1]"), QLatin1String("two"));
    QCOMPARE(evalJS("typeof myObject.stringListProperty[2]"), sString);
    QCOMPARE(evalJS("myObject.stringListProperty[2]"), QLatin1String("true"));
    evalJS("myObject.webElementProperty=document.body;");
    QCOMPARE(evalJS("myObject.webElementProperty.tagName"), QLatin1String("BODY"));

    // try to delete
    QCOMPARE(evalJS("delete myObject.intProperty"), sFalse);
    QCOMPARE(evalJS("myObject.intProperty == 123"), sTrue);

    QCOMPARE(evalJS("delete myObject.variantProperty"), sFalse);
    QCOMPARE(evalJS("myObject.variantProperty").toDouble(), 42.0);

    // custom property
    QCOMPARE(evalJS("myObject.customProperty"), sUndefined);
    QCOMPARE(evalJS("myObject.customProperty = 123;"
                    "myObject.customProperty == 123"), sTrue);
    QVariant v = m_page->mainFrame()->evaluateJavaScript("myObject.customProperty");
    QCOMPARE(v.type(), QVariant::Double);
    QCOMPARE(v.toInt(), 123);

    // non-scriptable property
    QCOMPARE(m_myObject->hiddenProperty(), 456.0);
    QCOMPARE(evalJS("myObject.hiddenProperty"), sUndefined);
    QCOMPARE(evalJS("myObject.hiddenProperty = 123;"
                    "myObject.hiddenProperty == 123"), sTrue);
    QCOMPARE(m_myObject->hiddenProperty(), 456.0);

    // write-only property
    QCOMPARE(m_myObject->writeOnlyProperty(), 789);
    QCOMPARE(evalJS("typeof myObject.writeOnlyProperty"), sUndefined);
    QCOMPARE(evalJS("myObject.writeOnlyProperty = 123;"
                    "typeof myObject.writeOnlyProperty"), sUndefined);
    QCOMPARE(m_myObject->writeOnlyProperty(), 123);

    // read-only property
    QCOMPARE(m_myObject->readOnlyProperty(), 987);
    QCOMPARE(evalJS("myObject.readOnlyProperty == 987"), sTrue);
    QCOMPARE(evalJS("myObject.readOnlyProperty = 654;"
                    "myObject.readOnlyProperty == 987"), sTrue);
    QCOMPARE(m_myObject->readOnlyProperty(), 987);

    // QObject* property
    m_myObject->setObjectStarProperty(0);
    QCOMPARE(m_myObject->objectStarProperty(), (QObject*)0);
    QCOMPARE(evalJS("myObject.objectStarProperty == null"), sTrue);
    QCOMPARE(evalJS("typeof myObject.objectStarProperty"), sObject);
    QCOMPARE(evalJS("Boolean(myObject.objectStarProperty)"), sFalse);
    QCOMPARE(evalJS("String(myObject.objectStarProperty) == 'null'"), sTrue);
    QCOMPARE(evalJS("myObject.objectStarProperty.objectStarProperty"),
        sUndefined);
    m_myObject->setObjectStarProperty(this);
    QCOMPARE(evalJS("myObject.objectStarProperty != null"), sTrue);
    QCOMPARE(evalJS("typeof myObject.objectStarProperty"), sObject);
    QCOMPARE(evalJS("Boolean(myObject.objectStarProperty)"), sTrue);
    QCOMPARE(evalJS("String(myObject.objectStarProperty) != 'null'"), sTrue);
}

void tst_QWebFrame::getSetDynamicProperty()
{
    // initially the object does not have the property
    // In WebKit, RuntimeObjects do not inherit Object, so don't have hasOwnProperty

    //QCOMPARE(evalJS("myObject.hasOwnProperty('dynamicProperty')"), sFalse);
    QCOMPARE(evalJS("typeof myObject.dynamicProperty"), sUndefined);

    // add a dynamic property in C++
    QCOMPARE(m_myObject->setProperty("dynamicProperty", 123), false);
    //QCOMPARE(evalJS("myObject.hasOwnProperty('dynamicProperty')"), sTrue);
    QCOMPARE(evalJS("typeof myObject.dynamicProperty != 'undefined'"), sTrue);
    QCOMPARE(evalJS("myObject.dynamicProperty == 123"), sTrue);

    // property change in script should be reflected in C++
    QCOMPARE(evalJS("myObject.dynamicProperty = 'foo';"
                    "myObject.dynamicProperty"), QLatin1String("foo"));
    QCOMPARE(m_myObject->property("dynamicProperty").toString(), QLatin1String("foo"));

    // delete the property (XFAIL - can't delete properties)
    QEXPECT_FAIL("", "can't delete properties", Continue);
    QCOMPARE(evalJS("delete myObject.dynamicProperty"), sTrue);
    /*
    QCOMPARE(m_myObject->property("dynamicProperty").isValid(), false);
    QCOMPARE(evalJS("typeof myObject.dynamicProperty"), sUndefined);
    //    QCOMPARE(evalJS("myObject.hasOwnProperty('dynamicProperty')"), sFalse);
    QCOMPARE(evalJS("typeof myObject.dynamicProperty"), sUndefined);
    */
}

void tst_QWebFrame::getSetChildren()
{
    // initially the object does not have the child
    // (again, no hasOwnProperty)

    //QCOMPARE(evalJS("myObject.hasOwnProperty('child')"), sFalse);
    QCOMPARE(evalJS("typeof myObject.child"), sUndefined);

    // add a child
    MyQObject* child = new MyQObject(m_myObject);
    child->setObjectName("child");
//  QCOMPARE(evalJS("myObject.hasOwnProperty('child')"), sTrue);
    QCOMPARE(evalJS("typeof myObject.child != 'undefined'"), sTrue);

    // add a grandchild
    MyQObject* grandChild = new MyQObject(child);
    grandChild->setObjectName("grandChild");
//  QCOMPARE(evalJS("myObject.child.hasOwnProperty('grandChild')"), sTrue);
    QCOMPARE(evalJS("typeof myObject.child.grandChild != 'undefined'"), sTrue);

    // delete grandchild
    delete grandChild;
//  QCOMPARE(evalJS("myObject.child.hasOwnProperty('grandChild')"), sFalse);
    QCOMPARE(evalJS("typeof myObject.child.grandChild == 'undefined'"), sTrue);

    // delete child
    delete child;
//  QCOMPARE(evalJS("myObject.hasOwnProperty('child')"), sFalse);
    QCOMPARE(evalJS("typeof myObject.child == 'undefined'"), sTrue);
}

Q_DECLARE_METATYPE(QVector<int>)
Q_DECLARE_METATYPE(QVector<double>)
Q_DECLARE_METATYPE(QVector<QString>)

void tst_QWebFrame::callQtInvokable()
{
    qRegisterMetaType<QObjectList>();

    m_myObject->resetQtFunctionInvoked();
    QCOMPARE(evalJS("typeof myObject.myInvokable()"), sUndefined);
    QCOMPARE(m_myObject->qtFunctionInvoked(), 0);
    QCOMPARE(m_myObject->qtFunctionActuals(), QVariantList());

    // extra arguments should silently be ignored
    m_myObject->resetQtFunctionInvoked();
    QCOMPARE(evalJS("typeof myObject.myInvokable(10, 20, 30)"), sUndefined);
    QCOMPARE(m_myObject->qtFunctionInvoked(), 0);
    QCOMPARE(m_myObject->qtFunctionActuals(), QVariantList());

    m_myObject->resetQtFunctionInvoked();
    QCOMPARE(evalJS("typeof myObject.myInvokableWithIntArg(123)"), sUndefined);
    QCOMPARE(m_myObject->qtFunctionInvoked(), 1);
    QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
    QCOMPARE(m_myObject->qtFunctionActuals().at(0).toInt(), 123);

    m_myObject->resetQtFunctionInvoked();
    QCOMPARE(evalJS("typeof myObject.myInvokableWithIntArg('123')"), sUndefined);
    QCOMPARE(m_myObject->qtFunctionInvoked(), 1);
    QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
    QCOMPARE(m_myObject->qtFunctionActuals().at(0).toInt(), 123);

    m_myObject->resetQtFunctionInvoked();
    QCOMPARE(evalJS("typeof myObject.myInvokableWithLonglongArg(123)"), sUndefined);
    QCOMPARE(m_myObject->qtFunctionInvoked(), 2);
    QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
    QCOMPARE(m_myObject->qtFunctionActuals().at(0).toLongLong(), qlonglong(123));

    m_myObject->resetQtFunctionInvoked();
    QCOMPARE(evalJS("typeof myObject.myInvokableWithFloatArg(123.5)"), sUndefined);
    QCOMPARE(m_myObject->qtFunctionInvoked(), 3);
    QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
    QCOMPARE(m_myObject->qtFunctionActuals().at(0).toDouble(), 123.5);

    m_myObject->resetQtFunctionInvoked();
    QCOMPARE(evalJS("typeof myObject.myInvokableWithDoubleArg(123.5)"), sUndefined);
    QCOMPARE(m_myObject->qtFunctionInvoked(), 4);
    QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
    QCOMPARE(m_myObject->qtFunctionActuals().at(0).toDouble(), 123.5);

    m_myObject->resetQtFunctionInvoked();
    QCOMPARE(evalJS("typeof myObject.myInvokableWithDoubleArg(new Number(1234.5))"), sUndefined);
    QCOMPARE(m_myObject->qtFunctionInvoked(), 4);
    QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
    QCOMPARE(m_myObject->qtFunctionActuals().at(0).toDouble(), 1234.5);

    m_myObject->resetQtFunctionInvoked();
    QCOMPARE(evalJS("typeof myObject.myInvokableWithBoolArg(new Boolean(true))"), sUndefined);
    QCOMPARE(m_myObject->qtFunctionInvoked(), 52);
    QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
    QCOMPARE(m_myObject->qtFunctionActuals().at(0).toBool(), true);

    m_myObject->resetQtFunctionInvoked();
    QCOMPARE(evalJS("typeof myObject.myInvokableWithStringArg('ciao')"), sUndefined);
    QCOMPARE(m_myObject->qtFunctionInvoked(), 5);
    QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
    QCOMPARE(m_myObject->qtFunctionActuals().at(0).toString(), QLatin1String("ciao"));

    m_myObject->resetQtFunctionInvoked();
    QCOMPARE(evalJS("typeof myObject.myInvokableWithStringArg(123)"), sUndefined);
    QCOMPARE(m_myObject->qtFunctionInvoked(), 5);
    QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
    QCOMPARE(m_myObject->qtFunctionActuals().at(0).toString(), QLatin1String("123"));

    m_myObject->resetQtFunctionInvoked();
    QCOMPARE(evalJS("typeof myObject.myInvokableWithStringArg(null)"), sUndefined);
    QCOMPARE(m_myObject->qtFunctionInvoked(), 5);
    QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
    QCOMPARE(m_myObject->qtFunctionActuals().at(0).toString(), QString());
    QVERIFY(m_myObject->qtFunctionActuals().at(0).toString().isEmpty());

    m_myObject->resetQtFunctionInvoked();
    QCOMPARE(evalJS("typeof myObject.myInvokableWithStringArg(undefined)"), sUndefined);
    QCOMPARE(m_myObject->qtFunctionInvoked(), 5);
    QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
    QCOMPARE(m_myObject->qtFunctionActuals().at(0).toString(), QString());
    QVERIFY(m_myObject->qtFunctionActuals().at(0).toString().isEmpty());

    m_myObject->resetQtFunctionInvoked();
    QCOMPARE(evalJS("typeof myObject.myInvokableWithIntArgs(123, 456)"), sUndefined);
    QCOMPARE(m_myObject->qtFunctionInvoked(), 6);
    QCOMPARE(m_myObject->qtFunctionActuals().size(), 2);
    QCOMPARE(m_myObject->qtFunctionActuals().at(0).toInt(), 123);
    QCOMPARE(m_myObject->qtFunctionActuals().at(1).toInt(), 456);

    m_myObject->resetQtFunctionInvoked();
    QCOMPARE(evalJS("myObject.myInvokableReturningInt()"), QLatin1String("123"));
    QCOMPARE(m_myObject->qtFunctionInvoked(), 7);
    QCOMPARE(m_myObject->qtFunctionActuals(), QVariantList());

    m_myObject->resetQtFunctionInvoked();
    QCOMPARE(evalJS("myObject.myInvokableReturningLongLong()"), QLatin1String("456"));
    QCOMPARE(m_myObject->qtFunctionInvoked(), 39);
    QCOMPARE(m_myObject->qtFunctionActuals(), QVariantList());

    m_myObject->resetQtFunctionInvoked();
    QCOMPARE(evalJS("myObject.myInvokableReturningString()"), QLatin1String("ciao"));
    QCOMPARE(m_myObject->qtFunctionInvoked(), 8);
    QCOMPARE(m_myObject->qtFunctionActuals(), QVariantList());

    m_myObject->resetQtFunctionInvoked();
    QCOMPARE(evalJS("typeof myObject.myInvokableWithIntArg(123, 456)"), sUndefined);
    QCOMPARE(m_myObject->qtFunctionInvoked(), 9);
    QCOMPARE(m_myObject->qtFunctionActuals().size(), 2);
    QCOMPARE(m_myObject->qtFunctionActuals().at(0).toInt(), 123);
    QCOMPARE(m_myObject->qtFunctionActuals().at(1).toInt(), 456);

    m_myObject->resetQtFunctionInvoked();
    QCOMPARE(evalJS("typeof myObject.myInvokableWithVoidStarArg(null)"), sUndefined);
    QCOMPARE(m_myObject->qtFunctionInvoked(), 44);
    m_myObject->resetQtFunctionInvoked();
    {
        QString type;
        QString ret = evalJS("myObject.myInvokableWithVoidStarArg(123)", type);
        QCOMPARE(type, sError);
        QCOMPARE(ret, QLatin1String("TypeError: incompatible type of argument(s) in call to myInvokableWithVoidStarArg(); candidates were\n    myInvokableWithVoidStarArg(void*)"));
        QCOMPARE(m_myObject->qtFunctionInvoked(), -1);
    }

    m_myObject->resetQtFunctionInvoked();
    {
        QString type;
        QString ret = evalJS("myObject.myInvokableWithAmbiguousArg(123)", type);
        QCOMPARE(type, sError);
        QCOMPARE(ret, QLatin1String("TypeError: ambiguous call of overloaded function myInvokableWithAmbiguousArg(); candidates were\n    myInvokableWithAmbiguousArg(int)\n    myInvokableWithAmbiguousArg(uint)"));
    }

    m_myObject->resetQtFunctionInvoked();
    {
        QString type;
        QString ret = evalJS("myObject.myInvokableWithDefaultArgs(123, 'hello')", type);
        QCOMPARE(type, sUndefined);
        QCOMPARE(m_myObject->qtFunctionInvoked(), 47);
        QCOMPARE(m_myObject->qtFunctionActuals().size(), 2);
        QCOMPARE(m_myObject->qtFunctionActuals().at(0).toInt(), 123);
        QCOMPARE(m_myObject->qtFunctionActuals().at(1).toString(), QLatin1String("hello"));
    }

    m_myObject->resetQtFunctionInvoked();
    {
        QString type;
        QString ret = evalJS("myObject.myInvokableWithDefaultArgs(456)", type);
        QCOMPARE(type, sUndefined);
        QCOMPARE(m_myObject->qtFunctionInvoked(), 47);
        QCOMPARE(m_myObject->qtFunctionActuals().size(), 2);
        QCOMPARE(m_myObject->qtFunctionActuals().at(0).toInt(), 456);
        QCOMPARE(m_myObject->qtFunctionActuals().at(1).toString(), QString());
    }

    // calling function that returns (const)ref
    m_myObject->resetQtFunctionInvoked();
    {
        QString type;
        QString ret = evalJS("typeof myObject.myInvokableReturningRef()");
        QCOMPARE(ret, sUndefined);
        //QVERIFY(!m_engine->hasUncaughtException());
        QCOMPARE(m_myObject->qtFunctionInvoked(), 48);
    }

    m_myObject->resetQtFunctionInvoked();
    {
        QString type;
        QString ret = evalJS("typeof myObject.myInvokableReturningConstRef()");
        QCOMPARE(ret, sUndefined);
        //QVERIFY(!m_engine->hasUncaughtException());
        QCOMPARE(m_myObject->qtFunctionInvoked(), 49);
    }

    m_myObject->resetQtFunctionInvoked();
    {
        QString type;
        QVariant ret = evalJSV("myObject.myInvokableReturningQObjectStar()", type);
        QCOMPARE(m_myObject->qtFunctionInvoked(), 13);
        QCOMPARE(m_myObject->qtFunctionActuals().size(), 0);
        QCOMPARE(type, sObject);
        QCOMPARE(ret.userType(), int(QMetaType::QObjectStar));
    }

    m_myObject->resetQtFunctionInvoked();
    {
        QString type;
        QVariant ret = evalJSV("myObject.myInvokableWithQObjectListArg([myObject])", type);
        QCOMPARE(m_myObject->qtFunctionInvoked(), 14);
        QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
        QCOMPARE(type, sArray);
        QCOMPARE(ret.userType(), int(QVariant::List)); // All lists get downgraded to QVariantList
        QVariantList vl = qvariant_cast<QVariantList>(ret);
        QCOMPARE(vl.count(), 1);
    }

    m_myObject->resetQtFunctionInvoked();
    {
        QString type;
        m_myObject->setVariantProperty(QVariant(123));
        QVariant ret = evalJSV("myObject.myInvokableWithVariantArg(myObject.variantProperty)", type);
        QCOMPARE(type, sNumber);
        QCOMPARE(m_myObject->qtFunctionInvoked(), 15);
        QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
        QCOMPARE(m_myObject->qtFunctionActuals().at(0), m_myObject->variantProperty());
        QCOMPARE(ret.userType(), int(QMetaType::Double)); // all JS numbers are doubles, even though this started as an int
        QCOMPARE(ret.toInt(),123);
    }

    m_myObject->resetQtFunctionInvoked();
    {
        QString type;
        QVariant ret = evalJSV("myObject.myInvokableWithVariantArg(null)", type);
        QCOMPARE(type, sObject);
        QCOMPARE(m_myObject->qtFunctionInvoked(), 15);
        QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
        QCOMPARE(m_myObject->qtFunctionActuals().at(0), QVariant());
        QVERIFY(!m_myObject->qtFunctionActuals().at(0).isValid());
    }

    m_myObject->resetQtFunctionInvoked();
    {
        QString type;
        QVariant ret = evalJSV("myObject.myInvokableWithVariantArg(undefined)", type);
        QCOMPARE(type, sObject);
        QCOMPARE(m_myObject->qtFunctionInvoked(), 15);
        QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
        QCOMPARE(m_myObject->qtFunctionActuals().at(0), QVariant());
        QVERIFY(!m_myObject->qtFunctionActuals().at(0).isValid());
    }

    /* XFAIL - variant support
    m_myObject->resetQtFunctionInvoked();
    {
        m_myObject->setVariantProperty(qVariantFromValue(QBrush()));
        QVariant ret = evalJS("myObject.myInvokableWithVariantArg(myObject.variantProperty)");
        QVERIFY(ret.isVariant());
        QCOMPARE(m_myObject->qtFunctionInvoked(), 15);
        QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
        QCOMPARE(ret.toVariant(), m_myObject->qtFunctionActuals().at(0));
        QCOMPARE(ret.toVariant(), m_myObject->variantProperty());
    }
    */

    m_myObject->resetQtFunctionInvoked();
    {
        QString type;
        QVariant ret = evalJSV("myObject.myInvokableWithVariantArg(123)", type);
        QCOMPARE(type, sNumber);
        QCOMPARE(m_myObject->qtFunctionInvoked(), 15);
        QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
        QCOMPARE(m_myObject->qtFunctionActuals().at(0), QVariant(123));
        QCOMPARE(ret.userType(), int(QMetaType::Double));
        QCOMPARE(ret.toInt(),123);
    }

    m_myObject->resetQtFunctionInvoked();
    {
        QString type;
        QVariant ret = evalJSV("myObject.myInvokableWithVariantMapArg({ a:123, b:'ciao' })", type);
        QCOMPARE(m_myObject->qtFunctionInvoked(), 16);
        QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);

        QVariant v = m_myObject->qtFunctionActuals().at(0);
        QCOMPARE(v.userType(), int(QMetaType::QVariantMap));

        QVariantMap vmap = qvariant_cast<QVariantMap>(v);
        QCOMPARE(vmap.keys().size(), 2);
        QCOMPARE(vmap.keys().at(0), QLatin1String("a"));
        QCOMPARE(vmap.value("a"), QVariant(123));
        QCOMPARE(vmap.keys().at(1), QLatin1String("b"));
        QCOMPARE(vmap.value("b"), QVariant("ciao"));

        QCOMPARE(type, sObject);

        QCOMPARE(ret.userType(), int(QMetaType::QVariantMap));
        vmap = qvariant_cast<QVariantMap>(ret);
        QCOMPARE(vmap.keys().size(), 2);
        QCOMPARE(vmap.keys().at(0), QLatin1String("a"));
        QCOMPARE(vmap.value("a"), QVariant(123));
        QCOMPARE(vmap.keys().at(1), QLatin1String("b"));
        QCOMPARE(vmap.value("b"), QVariant("ciao"));
    }

    m_myObject->resetQtFunctionInvoked();
    {
        QString type;
        QVariant ret = evalJSV("myObject.myInvokableWithListOfIntArg([1, 5])", type);
        QCOMPARE(m_myObject->qtFunctionInvoked(), 17);
        QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
        QVariant v = m_myObject->qtFunctionActuals().at(0);
        QCOMPARE(v.userType(), qMetaTypeId<QList<int> >());
        QList<int> ilst = qvariant_cast<QList<int> >(v);
        QCOMPARE(ilst.size(), 2);
        QCOMPARE(ilst.at(0), 1);
        QCOMPARE(ilst.at(1), 5);

        QCOMPARE(type, sArray);
        QCOMPARE(ret.userType(), int(QMetaType::QVariantList)); // ints get converted to doubles, so this is a qvariantlist
        QVariantList vlst = qvariant_cast<QVariantList>(ret);
        QCOMPARE(vlst.size(), 2);
        QCOMPARE(vlst.at(0).toInt(), 1);
        QCOMPARE(vlst.at(1).toInt(), 5);
    }

    m_myObject->resetQtFunctionInvoked();
    {
        QString type;
        QVariant ret = evalJSV("myObject.myInvokableWithQObjectStarArg(myObject)", type);
        QCOMPARE(m_myObject->qtFunctionInvoked(), 18);
        QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
        QVariant v = m_myObject->qtFunctionActuals().at(0);
        QCOMPARE(v.userType(), int(QMetaType::QObjectStar));
        QCOMPARE(qvariant_cast<QObject*>(v), (QObject*)m_myObject);

        QCOMPARE(ret.userType(), int(QMetaType::QObjectStar));
        QCOMPARE(qvariant_cast<QObject*>(ret), (QObject*)m_myObject);

        QCOMPARE(type, sObject);
    }

    m_myObject->resetQtFunctionInvoked();
    {
        // no implicit conversion from integer to QObject*
        QString type;
        evalJS("myObject.myInvokableWithQObjectStarArg(123)", type);
        QCOMPARE(type, sError);
    }

    /*
    m_myObject->resetQtFunctionInvoked();
    {
        QString fun = evalJS("myObject.myInvokableWithQBrushArg");
        Q_ASSERT(fun.isFunction());
        QColor color(10, 20, 30, 40);
        // QColor should be converted to a QBrush
        QVariant ret = fun.call(QString(), QStringList()
                                    << qScriptValueFromValue(m_engine, color));
        QCOMPARE(m_myObject->qtFunctionInvoked(), 19);
        QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
        QVariant v = m_myObject->qtFunctionActuals().at(0);
        QCOMPARE(v.userType(), int(QMetaType::QBrush));
        QCOMPARE(qvariant_cast<QColor>(v), color);

        QCOMPARE(qscriptvalue_cast<QColor>(ret), color);
    }
    */

    // private slots should not be part of the QObject binding
    QCOMPARE(evalJS("typeof myObject.myPrivateSlot"), sUndefined);

    // protected slots should be fine
    m_myObject->resetQtFunctionInvoked();
    evalJS("myObject.myProtectedSlot()");
    QCOMPARE(m_myObject->qtFunctionInvoked(), 36);

    // call with too few arguments
    {
        QString type;
        QString ret = evalJS("myObject.myInvokableWithIntArg()", type);
        QCOMPARE(type, sError);
        QCOMPARE(ret, QLatin1String("SyntaxError: too few arguments in call to myInvokableWithIntArg(); candidates are\n    myInvokableWithIntArg(int,int)\n    myInvokableWithIntArg(int)"));
    }

    // call function where not all types have been registered
    m_myObject->resetQtFunctionInvoked();
    {
        QString type;
        QString ret = evalJS("myObject.myInvokableWithBrushStyleArg(0)", type);
        QCOMPARE(type, sError);
        QCOMPARE(ret, QLatin1String("TypeError: cannot call myInvokableWithBrushStyleArg(): unknown type `Qt::BrushStyle'"));
        QCOMPARE(m_myObject->qtFunctionInvoked(), -1);
    }

    // call function with incompatible argument type
    m_myObject->resetQtFunctionInvoked();
    {
        QString type;
        QString ret = evalJS("myObject.myInvokableWithQBrushArg(null)", type);
        QCOMPARE(type, sError);
        QCOMPARE(ret, QLatin1String("TypeError: incompatible type of argument(s) in call to myInvokableWithQBrushArg(); candidates were\n    myInvokableWithQBrushArg(QBrush)"));
        QCOMPARE(m_myObject->qtFunctionInvoked(), -1);
    }
}

void tst_QWebFrame::connectAndDisconnect()
{
    // connect(function)
    QCOMPARE(evalJS("typeof myObject.mySignal"), sFunction);
    QCOMPARE(evalJS("typeof myObject.mySignal.connect"), sFunction);
    QCOMPARE(evalJS("typeof myObject.mySignal.disconnect"), sFunction);

    {
        QString type;
        evalJS("myObject.mySignal.connect(123)", type);
        QCOMPARE(type, sError);
    }

    evalJS("myHandler = function() { window.gotSignal = true; window.signalArgs = arguments; window.slotThisObject = this; window.signalSender = __qt_sender__; }");

    QCOMPARE(evalJS("myObject.mySignal.connect(myHandler)"), sUndefined);

    evalJS("gotSignal = false");
    evalJS("myObject.mySignal()");
    QCOMPARE(evalJS("gotSignal"), sTrue);
    QCOMPARE(evalJS("signalArgs.length == 0"), sTrue);
    QCOMPARE(evalJS("signalSender"),evalJS("myObject"));
    QCOMPARE(evalJS("slotThisObject == window"), sTrue);

    evalJS("gotSignal = false");
    m_myObject->emitMySignal();
    QCOMPARE(evalJS("gotSignal"), sTrue);
    QCOMPARE(evalJS("signalArgs.length == 0"), sTrue);

    QCOMPARE(evalJS("myObject.mySignalWithIntArg.connect(myHandler)"), sUndefined);

    evalJS("gotSignal = false");
    m_myObject->emitMySignalWithIntArg(123);
    QCOMPARE(evalJS("gotSignal"), sTrue);
    QCOMPARE(evalJS("signalArgs.length == 1"), sTrue);
    QCOMPARE(evalJS("signalArgs[0] == 123.0"), sTrue);

    QCOMPARE(evalJS("myObject.mySignal.disconnect(myHandler)"), sUndefined);
    {
        QString type;
        evalJS("myObject.mySignal.disconnect(myHandler)", type);
        QCOMPARE(type, sError);
    }

    evalJS("gotSignal = false");
    QCOMPARE(evalJS("myObject.mySignal2.connect(myHandler)"), sUndefined);
    m_myObject->emitMySignal2(true);
    QCOMPARE(evalJS("gotSignal"), sTrue);
    QCOMPARE(evalJS("signalArgs.length == 1"), sTrue);
    QCOMPARE(evalJS("signalArgs[0]"), sTrue);

    QCOMPARE(evalJS("myObject.mySignal2.disconnect(myHandler)"), sUndefined);

    QCOMPARE(evalJS("typeof myObject['mySignal2()']"), sFunction);
    QCOMPARE(evalJS("typeof myObject['mySignal2()'].connect"), sFunction);
    QCOMPARE(evalJS("typeof myObject['mySignal2()'].disconnect"), sFunction);

    QCOMPARE(evalJS("myObject['mySignal2()'].connect(myHandler)"), sUndefined);

    evalJS("gotSignal = false");
    m_myObject->emitMySignal2();
    QCOMPARE(evalJS("gotSignal"), sTrue);

    QCOMPARE(evalJS("myObject['mySignal2()'].disconnect(myHandler)"), sUndefined);

    // connect(object, function)
    evalJS("otherObject = { name:'foo' }");
    QCOMPARE(evalJS("myObject.mySignal.connect(otherObject, myHandler)"), sUndefined);
    QCOMPARE(evalJS("myObject.mySignal.disconnect(otherObject, myHandler)"), sUndefined);
    evalJS("gotSignal = false");
    m_myObject->emitMySignal();
    QCOMPARE(evalJS("gotSignal"), sFalse);

    {
        QString type;
        evalJS("myObject.mySignal.disconnect(otherObject, myHandler)", type);
        QCOMPARE(type, sError);
    }

    QCOMPARE(evalJS("myObject.mySignal.connect(otherObject, myHandler)"), sUndefined);
    evalJS("gotSignal = false");
    m_myObject->emitMySignal();
    QCOMPARE(evalJS("gotSignal"), sTrue);
    QCOMPARE(evalJS("signalArgs.length == 0"), sTrue);
    QCOMPARE(evalJS("slotThisObject"),evalJS("otherObject"));
    QCOMPARE(evalJS("signalSender"),evalJS("myObject"));
    QCOMPARE(evalJS("slotThisObject.name"), QLatin1String("foo"));
    QCOMPARE(evalJS("myObject.mySignal.disconnect(otherObject, myHandler)"), sUndefined);

    evalJS("yetAnotherObject = { name:'bar', func : function() { } }");
    QCOMPARE(evalJS("myObject.mySignal2.connect(yetAnotherObject, myHandler)"), sUndefined);
    evalJS("gotSignal = false");
    m_myObject->emitMySignal2(true);
    QCOMPARE(evalJS("gotSignal"), sTrue);
    QCOMPARE(evalJS("signalArgs.length == 1"), sTrue);
    QCOMPARE(evalJS("slotThisObject == yetAnotherObject"), sTrue);
    QCOMPARE(evalJS("signalSender == myObject"), sTrue);
    QCOMPARE(evalJS("slotThisObject.name"), QLatin1String("bar"));
    QCOMPARE(evalJS("myObject.mySignal2.disconnect(yetAnotherObject, myHandler)"), sUndefined);

    QCOMPARE(evalJS("myObject.mySignal2.connect(myObject, myHandler)"), sUndefined);
    evalJS("gotSignal = false");
    m_myObject->emitMySignal2(true);
    QCOMPARE(evalJS("gotSignal"), sTrue);
    QCOMPARE(evalJS("signalArgs.length == 1"), sTrue);
    QCOMPARE(evalJS("slotThisObject == myObject"), sTrue);
    QCOMPARE(evalJS("signalSender == myObject"), sTrue);
    QCOMPARE(evalJS("myObject.mySignal2.disconnect(myObject, myHandler)"), sUndefined);

    // connect(obj, string)
    QCOMPARE(evalJS("myObject.mySignal.connect(yetAnotherObject, 'func')"), sUndefined);
    QCOMPARE(evalJS("myObject.mySignal.connect(myObject, 'mySlot')"), sUndefined);
    QCOMPARE(evalJS("myObject.mySignal.disconnect(yetAnotherObject, 'func')"), sUndefined);
    QCOMPARE(evalJS("myObject.mySignal.disconnect(myObject, 'mySlot')"), sUndefined);

    // check that emitting signals from script works

    // no arguments
    QCOMPARE(evalJS("myObject.mySignal.connect(myObject.mySlot)"), sUndefined);
    m_myObject->resetQtFunctionInvoked();
    QCOMPARE(evalJS("myObject.mySignal()"), sUndefined);
    QCOMPARE(m_myObject->qtFunctionInvoked(), 20);
    QCOMPARE(evalJS("myObject.mySignal.disconnect(myObject.mySlot)"), sUndefined);

    // one argument
    QCOMPARE(evalJS("myObject.mySignalWithIntArg.connect(myObject.mySlotWithIntArg)"), sUndefined);
    m_myObject->resetQtFunctionInvoked();
    QCOMPARE(evalJS("myObject.mySignalWithIntArg(123)"), sUndefined);
    QCOMPARE(m_myObject->qtFunctionInvoked(), 21);
    QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
    QCOMPARE(m_myObject->qtFunctionActuals().at(0).toInt(), 123);
    QCOMPARE(evalJS("myObject.mySignalWithIntArg.disconnect(myObject.mySlotWithIntArg)"), sUndefined);

    QCOMPARE(evalJS("myObject.mySignalWithIntArg.connect(myObject.mySlotWithDoubleArg)"), sUndefined);
    m_myObject->resetQtFunctionInvoked();
    QCOMPARE(evalJS("myObject.mySignalWithIntArg(123)"), sUndefined);
    QCOMPARE(m_myObject->qtFunctionInvoked(), 22);
    QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
    QCOMPARE(m_myObject->qtFunctionActuals().at(0).toDouble(), 123.0);
    QCOMPARE(evalJS("myObject.mySignalWithIntArg.disconnect(myObject.mySlotWithDoubleArg)"), sUndefined);

    QCOMPARE(evalJS("myObject.mySignalWithIntArg.connect(myObject.mySlotWithStringArg)"), sUndefined);
    m_myObject->resetQtFunctionInvoked();
    QCOMPARE(evalJS("myObject.mySignalWithIntArg(123)"), sUndefined);
    QCOMPARE(m_myObject->qtFunctionInvoked(), 23);
    QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
    QCOMPARE(m_myObject->qtFunctionActuals().at(0).toString(), QLatin1String("123"));
    QCOMPARE(evalJS("myObject.mySignalWithIntArg.disconnect(myObject.mySlotWithStringArg)"), sUndefined);

    // connecting to overloaded slot
    QCOMPARE(evalJS("myObject.mySignalWithIntArg.connect(myObject.myOverloadedSlot)"), sUndefined);
    m_myObject->resetQtFunctionInvoked();
    QCOMPARE(evalJS("myObject.mySignalWithIntArg(123)"), sUndefined);
    QCOMPARE(m_myObject->qtFunctionInvoked(), 26); // double overload
    QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
    QCOMPARE(m_myObject->qtFunctionActuals().at(0).toInt(), 123);
    QCOMPARE(evalJS("myObject.mySignalWithIntArg.disconnect(myObject.myOverloadedSlot)"), sUndefined);

    QCOMPARE(evalJS("myObject.mySignalWithIntArg.connect(myObject['myOverloadedSlot(int)'])"), sUndefined);
    m_myObject->resetQtFunctionInvoked();
    QCOMPARE(evalJS("myObject.mySignalWithIntArg(456)"), sUndefined);
    QCOMPARE(m_myObject->qtFunctionInvoked(), 28); // int overload
    QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
    QCOMPARE(m_myObject->qtFunctionActuals().at(0).toInt(), 456);
    QCOMPARE(evalJS("myObject.mySignalWithIntArg.disconnect(myObject['myOverloadedSlot(int)'])"), sUndefined);

    // erroneous input
    {
        // ### QtScript adds .connect to all functions, WebKit does only to signals/slots
        QString type;
        QString ret = evalJS("(function() { }).connect()", type);
        QCOMPARE(type, sError);
        QCOMPARE(ret, QLatin1String("TypeError: 'undefined' is not a function"));
    }
    {
        QString type;
        QString ret = evalJS("var o = { }; o.connect = Function.prototype.connect;  o.connect()", type);
        QCOMPARE(type, sError);
        QCOMPARE(ret, QLatin1String("TypeError: 'undefined' is not a function"));
    }

    {
        QString type;
        QString ret = evalJS("(function() { }).connect(123)", type);
        QCOMPARE(type, sError);
        QCOMPARE(ret, QLatin1String("TypeError: 'undefined' is not a function"));
    }
    {
        QString type;
        QString ret = evalJS("var o = { }; o.connect = Function.prototype.connect;  o.connect(123)", type);
        QCOMPARE(type, sError);
        QCOMPARE(ret, QLatin1String("TypeError: 'undefined' is not a function"));
    }

    {
        QString type;
        QString ret = evalJS("myObject.myInvokable.connect(123)", type);
        QCOMPARE(type, sError);
        QCOMPARE(ret, QLatin1String("TypeError: QtMetaMethod.connect: MyQObject::myInvokable() is not a signal"));
    }
    {
        QString type;
        QString ret = evalJS("myObject.myInvokable.connect(function() { })", type);
        QCOMPARE(type, sError);
        QCOMPARE(ret, QLatin1String("TypeError: QtMetaMethod.connect: MyQObject::myInvokable() is not a signal"));
    }

    {
        QString type;
        QString ret = evalJS("myObject.mySignal.connect(123)", type);
        QCOMPARE(type, sError);
        QCOMPARE(ret, QLatin1String("TypeError: QtMetaMethod.connect: target is not a function"));
    }

    {
        QString type;
        QString ret = evalJS("myObject.mySignal.disconnect()", type);
        QCOMPARE(type, sError);
        QCOMPARE(ret, QLatin1String("Error: QtMetaMethod.disconnect: no arguments given"));
    }
    {
        QString type;
        QString ret = evalJS("var o = { }; o.disconnect = myObject.mySignal.disconnect;  o.disconnect()", type);
        QCOMPARE(type, sError);
        QCOMPARE(ret, QLatin1String("Error: QtMetaMethod.disconnect: no arguments given"));
    }

    /* XFAIL - Function.prototype doesn't get connect/disconnect, just signals/slots
    {
        QString type;
        QString ret = evalJS("(function() { }).disconnect(123)", type);
        QCOMPARE(type, sError);
        QCOMPARE(ret, QLatin1String("TypeError: QtMetaMethod.disconnect: this object is not a signal"));
    }
    */

    {
        QString type;
        QString ret = evalJS("var o = { }; o.disconnect = myObject.myInvokable.disconnect; o.disconnect(123)", type);
        QCOMPARE(type, sError);
        QCOMPARE(ret, QLatin1String("TypeError: QtMetaMethod.disconnect: MyQObject::myInvokable() is not a signal"));
    }

    {
        QString type;
        QString ret = evalJS("myObject.myInvokable.disconnect(123)", type);
        QCOMPARE(type, sError);
        QCOMPARE(ret, QLatin1String("TypeError: QtMetaMethod.disconnect: MyQObject::myInvokable() is not a signal"));
    }
    {
        QString type;
        QString ret = evalJS("myObject.myInvokable.disconnect(function() { })", type);
        QCOMPARE(type, sError);
        QCOMPARE(ret, QLatin1String("TypeError: QtMetaMethod.disconnect: MyQObject::myInvokable() is not a signal"));
    }

    {
        QString type;
        QString ret = evalJS("myObject.mySignal.disconnect(123)", type);
        QCOMPARE(type, sError);
        QCOMPARE(ret, QLatin1String("TypeError: QtMetaMethod.disconnect: target is not a function"));
    }

    {
        QString type;
        QString ret = evalJS("myObject.mySignal.disconnect(function() { })", type);
        QCOMPARE(type, sError);
        QCOMPARE(ret, QLatin1String("Error: QtMetaMethod.disconnect: failed to disconnect from MyQObject::mySignal()"));
    }

    // when the wrapper dies, the connection stays alive
    QCOMPARE(evalJS("myObject.mySignal.connect(myObject.mySlot)"), sUndefined);
    m_myObject->resetQtFunctionInvoked();
    m_myObject->emitMySignal();
    QCOMPARE(m_myObject->qtFunctionInvoked(), 20);
    evalJS("myObject = null");
    evalJS("gc()");
    m_myObject->resetQtFunctionInvoked();
    m_myObject->emitMySignal();
    QCOMPARE(m_myObject->qtFunctionInvoked(), 20);
}

void tst_QWebFrame::classEnums()
{
    // We don't do the meta thing currently, unfortunately!!!
    /*
    QString myClass = m_engine->newQMetaObject(m_myObject->metaObject(), m_engine->undefinedValue());
    m_engine->globalObject().setProperty("MyQObject", myClass);

    QCOMPARE(static_cast<MyQObject::Policy>(evalJS("MyQObject.FooPolicy").toInt()),
             MyQObject::FooPolicy);
    QCOMPARE(static_cast<MyQObject::Policy>(evalJS("MyQObject.BarPolicy").toInt()),
             MyQObject::BarPolicy);
    QCOMPARE(static_cast<MyQObject::Policy>(evalJS("MyQObject.BazPolicy").toInt()),
             MyQObject::BazPolicy);

    QCOMPARE(static_cast<MyQObject::Strategy>(evalJS("MyQObject.FooStrategy").toInt()),
             MyQObject::FooStrategy);
    QCOMPARE(static_cast<MyQObject::Strategy>(evalJS("MyQObject.BarStrategy").toInt()),
             MyQObject::BarStrategy);
    QCOMPARE(static_cast<MyQObject::Strategy>(evalJS("MyQObject.BazStrategy").toInt()),
             MyQObject::BazStrategy);

    QCOMPARE(MyQObject::Ability(evalJS("MyQObject.NoAbility").toInt()),
             MyQObject::NoAbility);
    QCOMPARE(MyQObject::Ability(evalJS("MyQObject.FooAbility").toInt()),
             MyQObject::FooAbility);
    QCOMPARE(MyQObject::Ability(evalJS("MyQObject.BarAbility").toInt()),
             MyQObject::BarAbility);
    QCOMPARE(MyQObject::Ability(evalJS("MyQObject.BazAbility").toInt()),
             MyQObject::BazAbility);
    QCOMPARE(MyQObject::Ability(evalJS("MyQObject.AllAbility").toInt()),
             MyQObject::AllAbility);

    // enums from Qt are inherited through prototype
    QCOMPARE(static_cast<Qt::FocusPolicy>(evalJS("MyQObject.StrongFocus").toInt()),
             Qt::StrongFocus);
    QCOMPARE(static_cast<Qt::Key>(evalJS("MyQObject.Key_Left").toInt()),
             Qt::Key_Left);

    QCOMPARE(evalJS("MyQObject.className()"), QLatin1String("MyQObject"));

    qRegisterMetaType<MyQObject::Policy>("Policy");

    m_myObject->resetQtFunctionInvoked();
    QCOMPARE(evalJS("myObject.myInvokableWithEnumArg(MyQObject.BazPolicy)"), sUndefined);
    QCOMPARE(m_myObject->qtFunctionInvoked(), 10);
    QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
    QCOMPARE(m_myObject->qtFunctionActuals().at(0).toInt(), int(MyQObject::BazPolicy));

    m_myObject->resetQtFunctionInvoked();
    QCOMPARE(evalJS("myObject.myInvokableWithQualifiedEnumArg(MyQObject.BazPolicy)"), sUndefined);
    QCOMPARE(m_myObject->qtFunctionInvoked(), 36);
    QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
    QCOMPARE(m_myObject->qtFunctionActuals().at(0).toInt(), int(MyQObject::BazPolicy));

    m_myObject->resetQtFunctionInvoked();
    {
        QVariant ret = evalJS("myObject.myInvokableReturningEnum()");
        QCOMPARE(m_myObject->qtFunctionInvoked(), 37);
        QCOMPARE(m_myObject->qtFunctionActuals().size(), 0);
        QCOMPARE(ret.isVariant());
    }
    m_myObject->resetQtFunctionInvoked();
    {
        QVariant ret = evalJS("myObject.myInvokableReturningQualifiedEnum()");
        QCOMPARE(m_myObject->qtFunctionInvoked(), 38);
        QCOMPARE(m_myObject->qtFunctionActuals().size(), 0);
        QCOMPARE(ret.isNumber());
    }
    */
}

void tst_QWebFrame::classConstructor()
{
    /*
    QString myClass = qScriptValueFromQMetaObject<MyQObject>(m_engine);
    m_engine->globalObject().setProperty("MyQObject", myClass);

    QString myObj = evalJS("myObj = MyQObject()");
    QObject* qobj = myObj.toQObject();
    QVERIFY(qobj != 0);
    QCOMPARE(qobj->metaObject()->className(), "MyQObject");
    QCOMPARE(qobj->parent(), (QObject*)0);

    QString qobjectClass = qScriptValueFromQMetaObject<QObject>(m_engine);
    m_engine->globalObject().setProperty("QObject", qobjectClass);

    QString otherObj = evalJS("otherObj = QObject(myObj)");
    QObject* qqobj = otherObj.toQObject();
    QVERIFY(qqobj != 0);
    QCOMPARE(qqobj->metaObject()->className(), "QObject");
    QCOMPARE(qqobj->parent(), qobj);

    delete qobj;
    */
}

void tst_QWebFrame::overrideInvokable()
{
    m_myObject->resetQtFunctionInvoked();
    QCOMPARE(evalJS("myObject.myInvokable()"), sUndefined);
    QCOMPARE(m_myObject->qtFunctionInvoked(), 0);

    /* XFAIL - can't write to functions with RuntimeObject
    m_myObject->resetQtFunctionInvoked();
    evalJS("myObject.myInvokable = function() { window.a = 123; }");
    evalJS("myObject.myInvokable()");
    QCOMPARE(m_myObject->qtFunctionInvoked(), -1);
    QCOMPARE(evalJS("window.a").toDouble(), 123.0);

    evalJS("myObject.myInvokable = function() { window.a = 456; }");
    evalJS("myObject.myInvokable()");
    QCOMPARE(m_myObject->qtFunctionInvoked(), -1);
    QCOMPARE(evalJS("window.a").toDouble(), 456.0);
    */

    evalJS("delete myObject.myInvokable");
    evalJS("myObject.myInvokable()");
    QCOMPARE(m_myObject->qtFunctionInvoked(), 0);

    /* XFAIL - ditto
    m_myObject->resetQtFunctionInvoked();
    evalJS("myObject.myInvokable = myObject.myInvokableWithIntArg");
    evalJS("myObject.myInvokable(123)");
    QCOMPARE(m_myObject->qtFunctionInvoked(), 1);
    */

    evalJS("delete myObject.myInvokable");
    m_myObject->resetQtFunctionInvoked();
    // this form (with the '()') is read-only
    evalJS("myObject['myInvokable()'] = function() { window.a = 123; }");
    evalJS("myObject.myInvokable()");
    QCOMPARE(m_myObject->qtFunctionInvoked(), 0);
}

void tst_QWebFrame::transferInvokable()
{
    /* XFAIL - can't put to functions with RuntimeObject
    m_myObject->resetQtFunctionInvoked();
    evalJS("myObject.foozball = myObject.myInvokable");
    evalJS("myObject.foozball()");
    QCOMPARE(m_myObject->qtFunctionInvoked(), 0);
    m_myObject->resetQtFunctionInvoked();
    evalJS("myObject.foozball = myObject.myInvokableWithIntArg");
    evalJS("myObject.foozball(123)");
    QCOMPARE(m_myObject->qtFunctionInvoked(), 1);
    m_myObject->resetQtFunctionInvoked();
    evalJS("myObject.myInvokable = myObject.myInvokableWithIntArg");
    evalJS("myObject.myInvokable(123)");
    QCOMPARE(m_myObject->qtFunctionInvoked(), 1);

    MyOtherQObject other;
    m_page->mainFrame()->addToJSWindowObject("myOtherObject", &other);
    evalJS("myOtherObject.foo = myObject.foozball");
    other.resetQtFunctionInvoked();
    evalJS("myOtherObject.foo(456)");
    QCOMPARE(other.qtFunctionInvoked(), 1);
    */
}

void tst_QWebFrame::findChild()
{
    /*
    QObject* child = new QObject(m_myObject);
    child->setObjectName(QLatin1String("myChildObject"));

    {
        QString result = evalJS("myObject.findChild('noSuchChild')");
        QCOMPARE(result.isNull());
    }

    {
        QString result = evalJS("myObject.findChild('myChildObject')");
        QCOMPARE(result.isQObject());
        QCOMPARE(result.toQObject(), child);
    }

    delete child;
    */
}

void tst_QWebFrame::findChildren()
{
    /*
    QObject* child = new QObject(m_myObject);
    child->setObjectName(QLatin1String("myChildObject"));

    {
        QString result = evalJS("myObject.findChildren('noSuchChild')");
        QCOMPARE(result.isArray());
        QCOMPARE(result.property(QLatin1String("length")).toDouble(), 0.0);
    }

    {
        QString result = evalJS("myObject.findChildren('myChildObject')");
        QCOMPARE(result.isArray());
        QCOMPARE(result.property(QLatin1String("length")).toDouble(), 1.0);
        QCOMPARE(result.property(QLatin1String("0")).toQObject(), child);
    }

    QObject* namelessChild = new QObject(m_myObject);

    {
        QString result = evalJS("myObject.findChildren('myChildObject')");
        QCOMPARE(result.isArray());
        QCOMPARE(result.property(QLatin1String("length")).toDouble(), 1.0);
        QCOMPARE(result.property(QLatin1String("0")).toQObject(), child);
    }

    QObject* anotherChild = new QObject(m_myObject);
    anotherChild->setObjectName(QLatin1String("anotherChildObject"));

    {
        QString result = evalJS("myObject.findChildren('anotherChildObject')");
        QCOMPARE(result.isArray());
        QCOMPARE(result.property(QLatin1String("length")).toDouble(), 1.0);
        QCOMPARE(result.property(QLatin1String("0")).toQObject(), anotherChild);
    }

    anotherChild->setObjectName(QLatin1String("myChildObject"));
    {
        QString result = evalJS("myObject.findChildren('myChildObject')");
        QCOMPARE(result.isArray());
        QCOMPARE(result.property(QLatin1String("length")).toDouble(), 2.0);
        QObject* o1 = result.property(QLatin1String("0")).toQObject();
        QObject* o2 = result.property(QLatin1String("1")).toQObject();
        if (o1 != child) {
            QCOMPARE(o1, anotherChild);
            QCOMPARE(o2, child);
        } else {
            QCOMPARE(o1, child);
            QCOMPARE(o2, anotherChild);
        }
    }

    // find all
    {
        QString result = evalJS("myObject.findChildren()");
        QVERIFY(result.isArray());
        int count = 3;
        QCOMPARE(result.property("length"), QLatin1String(count);
        for (int i = 0; i < 3; ++i) {
            QObject* o = result.property(i).toQObject();
            if (o == namelessChild || o == child || o == anotherChild)
                --count;
        }
        QVERIFY(count == 0);
    }

    delete anotherChild;
    delete namelessChild;
    delete child;
    */
}

void tst_QWebFrame::overloadedSlots()
{
    // should pick myOverloadedSlot(double)
    m_myObject->resetQtFunctionInvoked();
    evalJS("myObject.myOverloadedSlot(10)");
    QCOMPARE(m_myObject->qtFunctionInvoked(), 26);

    // should pick myOverloadedSlot(double)
    m_myObject->resetQtFunctionInvoked();
    evalJS("myObject.myOverloadedSlot(10.0)");
    QCOMPARE(m_myObject->qtFunctionInvoked(), 26);

    // should pick myOverloadedSlot(QString)
    m_myObject->resetQtFunctionInvoked();
    evalJS("myObject.myOverloadedSlot('10')");
    QCOMPARE(m_myObject->qtFunctionInvoked(), 29);

    // should pick myOverloadedSlot(bool)
    m_myObject->resetQtFunctionInvoked();
    evalJS("myObject.myOverloadedSlot(true)");
    QCOMPARE(m_myObject->qtFunctionInvoked(), 25);

    // should pick myOverloadedSlot(QDateTime)
    m_myObject->resetQtFunctionInvoked();
    evalJS("myObject.myOverloadedSlot(new Date())");
    QCOMPARE(m_myObject->qtFunctionInvoked(), 32);

    // should pick myOverloadedSlot(QRegExp)
    m_myObject->resetQtFunctionInvoked();
    evalJS("myObject.myOverloadedSlot(new RegExp())");
    QCOMPARE(m_myObject->qtFunctionInvoked(), 34);

    // should pick myOverloadedSlot(QVariant)
    /* XFAIL
    m_myObject->resetQtFunctionInvoked();
    QString f = evalJS("myObject.myOverloadedSlot");
    f.call(QString(), QStringList() << m_engine->newVariant(QVariant("ciao")));
    QCOMPARE(m_myObject->qtFunctionInvoked(), 35);
    */

    // should pick myOverloadedSlot(QRegExp)
    m_myObject->resetQtFunctionInvoked();
    evalJS("myObject.myOverloadedSlot(document.body)");
    QEXPECT_FAIL("", "https://bugs.webkit.org/show_bug.cgi?id=37319", Continue);
    QCOMPARE(m_myObject->qtFunctionInvoked(), 36);

    // should pick myOverloadedSlot(QObject*)
    m_myObject->resetQtFunctionInvoked();
    evalJS("myObject.myOverloadedSlot(myObject)");
    QCOMPARE(m_myObject->qtFunctionInvoked(), 41);

    // should pick myOverloadedSlot(QObject*)
    m_myObject->resetQtFunctionInvoked();
    evalJS("myObject.myOverloadedSlot(null)");
    QCOMPARE(m_myObject->qtFunctionInvoked(), 41);

    // should pick myOverloadedSlot(QStringList)
    m_myObject->resetQtFunctionInvoked();
    evalJS("myObject.myOverloadedSlot(['hello'])");
    QCOMPARE(m_myObject->qtFunctionInvoked(), 42);
}

void tst_QWebFrame::enumerate_data()
{
    QTest::addColumn<QStringList>("expectedNames");

    QTest::newRow("enumerate all")
    << (QStringList()
        // meta-object-defined properties:
        //   inherited
        << "objectName"
        //   non-inherited
        << "p1" << "p2" << "p4" << "p6"
        // dynamic properties
        << "dp1" << "dp2" << "dp3"
        // inherited slots
        << "destroyed(QObject*)" << "destroyed()"
        << "deleteLater()"
        // not included because it's private:
        // << "_q_reregisterTimers(void*)"
        // signals
        << "mySignal()"
        // slots
        << "mySlot()" << "myOtherSlot()");
}

void tst_QWebFrame::enumerate()
{
    QFETCH(QStringList, expectedNames);

    MyEnumTestQObject enumQObject;
    // give it some dynamic properties
    enumQObject.setProperty("dp1", "dp1");
    enumQObject.setProperty("dp2", "dp2");
    enumQObject.setProperty("dp3", "dp3");
    m_page->mainFrame()->addToJavaScriptWindowObject("myEnumObject", &enumQObject);

    // enumerate in script
    {
        evalJS("var enumeratedProperties = []");
        evalJS("for (var p in myEnumObject) { enumeratedProperties.push(p); }");
        QStringList result = evalJSV("enumeratedProperties").toStringList();
        QCOMPARE(result.size(), expectedNames.size());
        for (int i = 0; i < expectedNames.size(); ++i)
            QCOMPARE(result.at(i), expectedNames.at(i));
    }
}

void tst_QWebFrame::objectDeleted()
{
    MyQObject* qobj = new MyQObject();
    m_page->mainFrame()->addToJavaScriptWindowObject("bar", qobj);
    evalJS("bar.objectName = 'foo';");
    QCOMPARE(qobj->objectName(), QLatin1String("foo"));
    evalJS("bar.intProperty = 123;");
    QCOMPARE(qobj->intProperty(), 123);
    qobj->resetQtFunctionInvoked();
    evalJS("bar.myInvokable.call(bar);");
    QCOMPARE(qobj->qtFunctionInvoked(), 0);

    // do this, to ensure that we cache that it implements call
    evalJS("bar()");

    // now delete the object
    delete qobj;

    QCOMPARE(evalJS("typeof bar"), sObject);

    // any attempt to access properties of the object should result in an exception
    {
        QString type;
        QString ret = evalJS("bar.objectName", type);
        QCOMPARE(type, sError);
        QCOMPARE(ret, QLatin1String("Error: cannot access member `objectName' of deleted QObject"));
    }
    {
        QString type;
        QString ret = evalJS("bar.objectName = 'foo'", type);
        QCOMPARE(type, sError);
        QCOMPARE(ret, QLatin1String("Error: cannot access member `objectName' of deleted QObject"));
    }

    // myInvokable is stored in member table (since we've accessed it before deletion)
    {
        QString type;
        evalJS("bar.myInvokable", type);
        QCOMPARE(type, sFunction);
    }

    {
        QString type;
        QString ret = evalJS("bar.myInvokable.call(bar);", type);
        ret = evalJS("bar.myInvokable(bar)", type);
        QCOMPARE(type, sError);
        QCOMPARE(ret, QLatin1String("Error: cannot call function of deleted QObject"));
    }
    // myInvokableWithIntArg is not stored in member table (since we've not accessed it)
    {
        QString type;
        QString ret = evalJS("bar.myInvokableWithIntArg", type);
        QCOMPARE(type, sError);
        QCOMPARE(ret, QLatin1String("Error: cannot access member `myInvokableWithIntArg' of deleted QObject"));
    }

    // access from script
    evalJS("window.o = bar;");
    {
        QString type;
        QString ret = evalJS("o.objectName", type);
        QCOMPARE(type, sError);
        QCOMPARE(ret, QLatin1String("Error: cannot access member `objectName' of deleted QObject"));
    }
    {
        QString type;
        QString ret = evalJS("o.myInvokable()", type);
        QCOMPARE(type, sError);
        QCOMPARE(ret, QLatin1String("Error: cannot call function of deleted QObject"));
    }
    {
        QString type;
        QString ret = evalJS("o.myInvokableWithIntArg(10)", type);
        QCOMPARE(type, sError);
        QCOMPARE(ret, QLatin1String("Error: cannot access member `myInvokableWithIntArg' of deleted QObject"));
    }
}

void tst_QWebFrame::typeConversion()
{
    m_myObject->resetQtFunctionInvoked();

    QDateTime localdt(QDate(2008,1,18), QTime(12,31,0));
    QDateTime utclocaldt = localdt.toUTC();
    QDateTime utcdt(QDate(2008,1,18), QTime(12,31,0), Qt::UTC);

    // Dates in JS (default to local)
    evalJS("myObject.myOverloadedSlot(new Date(2008,0,18,12,31,0))");
    QCOMPARE(m_myObject->qtFunctionInvoked(), 32);
    QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
    QCOMPARE(m_myObject->qtFunctionActuals().at(0).toDateTime().toUTC(), utclocaldt);

    m_myObject->resetQtFunctionInvoked();
    evalJS("myObject.myOverloadedSlot(new Date(Date.UTC(2008,0,18,12,31,0)))");
    QCOMPARE(m_myObject->qtFunctionInvoked(), 32);
    QCOMPARE(m_myObject->qtFunctionActuals().size(), 1);
    QCOMPARE(m_myObject->qtFunctionActuals().at(0).toDateTime().toUTC(), utcdt);

    // Pushing QDateTimes into JS
    // Local
    evalJS("function checkDate(d) {window.__date_equals = (d.toString() == new Date(2008,0,18,12,31,0))?true:false;}");
    evalJS("myObject.mySignalWithDateTimeArg.connect(checkDate)");
    m_myObject->emitMySignalWithDateTimeArg(localdt);
    QCOMPARE(evalJS("window.__date_equals"), sTrue);
    evalJS("delete window.__date_equals");
    m_myObject->emitMySignalWithDateTimeArg(utclocaldt);
    QCOMPARE(evalJS("window.__date_equals"), sTrue);
    evalJS("delete window.__date_equals");
    evalJS("myObject.mySignalWithDateTimeArg.disconnect(checkDate); delete checkDate;");

    // UTC
    evalJS("function checkDate(d) {window.__date_equals = (d.toString() == new Date(Date.UTC(2008,0,18,12,31,0)))?true:false; }");
    evalJS("myObject.mySignalWithDateTimeArg.connect(checkDate)");
    m_myObject->emitMySignalWithDateTimeArg(utcdt);
    QCOMPARE(evalJS("window.__date_equals"), sTrue);
    evalJS("delete window.__date_equals");
    evalJS("myObject.mySignalWithDateTimeArg.disconnect(checkDate); delete checkDate;");

    // ### RegExps
}

class StringListTestObject : public QObject {
    Q_OBJECT
public Q_SLOTS:
    QVariant stringList()
    {
        return QStringList() << "Q" << "t";
    };
};

void tst_QWebFrame::arrayObjectEnumerable()
{
    QWebPage page;
    QWebFrame* frame = page.mainFrame();
    QObject* qobject = new StringListTestObject();
    frame->addToJavaScriptWindowObject("test", qobject, QScriptEngine::ScriptOwnership);

    const QString script("var stringArray = test.stringList();"
                         "var result = '';"
                         "for (var i in stringArray) {"
                         "    result += stringArray[i];"
                         "}"
                         "result;");
    QCOMPARE(frame->evaluateJavaScript(script).toString(), QString::fromLatin1("Qt"));
}

void tst_QWebFrame::symmetricUrl()
{
    QVERIFY(m_view->url().isEmpty());

    QCOMPARE(m_view->history()->count(), 0);

    QUrl dataUrl("data:text/html,<h1>Test");

    m_view->setUrl(dataUrl);
    QCOMPARE(m_view->url(), dataUrl);
    QCOMPARE(m_view->history()->count(), 0);

    // loading is _not_ immediate, so the text isn't set just yet.
    QVERIFY(m_view->page()->mainFrame()->toPlainText().isEmpty());

    ::waitForSignal(m_view, SIGNAL(loadFinished(bool)));

    QCOMPARE(m_view->history()->count(), 1);
    QCOMPARE(m_view->page()->mainFrame()->toPlainText(), QString("Test"));

    QUrl dataUrl2("data:text/html,<h1>Test2");
    QUrl dataUrl3("data:text/html,<h1>Test3");

    m_view->setUrl(dataUrl2);
    m_view->setUrl(dataUrl3);

    QCOMPARE(m_view->url(), dataUrl3);

    ::waitForSignal(m_view, SIGNAL(loadFinished(bool)));

    QCOMPARE(m_view->history()->count(), 2);

    QCOMPARE(m_view->page()->mainFrame()->toPlainText(), QString("Test3"));
}

void tst_QWebFrame::progressSignal()
{
    QSignalSpy progressSpy(m_view, SIGNAL(loadProgress(int)));

    QUrl dataUrl("data:text/html,<h1>Test");
    m_view->setUrl(dataUrl);

    ::waitForSignal(m_view, SIGNAL(loadFinished(bool)));

    QVERIFY(progressSpy.size() >= 2);

    // WebKit defines initialProgressValue as 10%, not 0%
    QCOMPARE(progressSpy.first().first().toInt(), 10);

    // But we always end at 100%
    QCOMPARE(progressSpy.last().first().toInt(), 100);
}

void tst_QWebFrame::urlChange()
{
    QSignalSpy urlSpy(m_page->mainFrame(), SIGNAL(urlChanged(QUrl)));

    QUrl dataUrl("data:text/html,<h1>Test");
    m_view->setUrl(dataUrl);

    ::waitForSignal(m_page->mainFrame(), SIGNAL(urlChanged(QUrl)));

    QCOMPARE(urlSpy.size(), 1);

    QUrl dataUrl2("data:text/html,<html><head><title>title</title></head><body><h1>Test</body></html>");
    m_view->setUrl(dataUrl2);

    ::waitForSignal(m_page->mainFrame(), SIGNAL(urlChanged(QUrl)));

    QCOMPARE(urlSpy.size(), 2);
}


void tst_QWebFrame::domCycles()
{
    m_view->setHtml("<html><body>");
    QVariant v = m_page->mainFrame()->evaluateJavaScript("document");
    QVERIFY(v.type() == QVariant::Map);
}

class FakeReply : public QNetworkReply {
    Q_OBJECT

public:
    FakeReply(const QNetworkRequest& request, QObject* parent = 0)
        : QNetworkReply(parent)
    {
        setOperation(QNetworkAccessManager::GetOperation);
        setRequest(request);
        if (request.url() == QUrl("qrc:/test1.html")) {
            setHeader(QNetworkRequest::LocationHeader, QString("qrc:/test2.html"));
            setAttribute(QNetworkRequest::RedirectionTargetAttribute, QUrl("qrc:/test2.html"));
        }
#ifndef QT_NO_OPENSSL
        else if (request.url() == QUrl("qrc:/fake-ssl-error.html"))
            setError(QNetworkReply::SslHandshakeFailedError, tr("Fake error !")); // force a ssl error
#endif
        else if (request.url() == QUrl("http://abcdef.abcdef/"))
            setError(QNetworkReply::HostNotFoundError, tr("Invalid URL"));

        open(QIODevice::ReadOnly);
        QTimer::singleShot(0, this, SLOT(timeout()));
    }
    ~FakeReply()
    {
        close();
    }
    virtual void abort() {}
    virtual void close() {}

protected:
    qint64 readData(char*, qint64)
    {
        return 0;
    }

private slots:
    void timeout()
    {
        if (request().url() == QUrl("qrc://test1.html"))
            emit error(this->error());
        else if (request().url() == QUrl("http://abcdef.abcdef/"))
            emit metaDataChanged();
#ifndef QT_NO_OPENSSL
        else if (request().url() == QUrl("qrc:/fake-ssl-error.html"))
            return;
#endif

        emit readyRead();
        emit finished();
    }
};

class FakeNetworkManager : public QNetworkAccessManager {
    Q_OBJECT

public:
    FakeNetworkManager(QObject* parent) : QNetworkAccessManager(parent) { }

protected:
    virtual QNetworkReply* createRequest(Operation op, const QNetworkRequest& request, QIODevice* outgoingData)
    {
        QString url = request.url().toString();
        if (op == QNetworkAccessManager::GetOperation) {
            if (url == "qrc:/test1.html" ||  url == "http://abcdef.abcdef/")
                return new FakeReply(request, this);
#ifndef QT_NO_OPENSSL
            else if (url == "qrc:/fake-ssl-error.html") {
                FakeReply* reply = new FakeReply(request, this);
                QList<QSslError> errors;
                emit sslErrors(reply, errors << QSslError(QSslError::UnspecifiedError));
                return reply;
            }
#endif
       }

        return QNetworkAccessManager::createRequest(op, request, outgoingData);
    }
};

void tst_QWebFrame::requestedUrl()
{
    QWebPage page;
    QWebFrame* frame = page.mainFrame();

    // in few seconds, the image should be completely loaded
    QSignalSpy spy(&page, SIGNAL(loadFinished(bool)));
    FakeNetworkManager* networkManager = new FakeNetworkManager(&page);
    page.setNetworkAccessManager(networkManager);

    frame->setUrl(QUrl("qrc:/test1.html"));
    waitForSignal(frame, SIGNAL(loadFinished(bool)), 200);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(frame->requestedUrl(), QUrl("qrc:/test1.html"));
    QCOMPARE(frame->url(), QUrl("qrc:/test2.html"));

    frame->setUrl(QUrl("qrc:/non-existent.html"));
    waitForSignal(frame, SIGNAL(loadFinished(bool)), 200);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(frame->requestedUrl(), QUrl("qrc:/non-existent.html"));
    QCOMPARE(frame->url(), QUrl("qrc:/non-existent.html"));

    frame->setUrl(QUrl("http://abcdef.abcdef"));
    waitForSignal(frame, SIGNAL(loadFinished(bool)), 200);
    QCOMPARE(spy.count(), 3);
    QCOMPARE(frame->requestedUrl(), QUrl("http://abcdef.abcdef/"));
    QCOMPARE(frame->url(), QUrl("http://abcdef.abcdef/"));

#ifndef QT_NO_OPENSSL
    qRegisterMetaType<QList<QSslError> >("QList<QSslError>");
    qRegisterMetaType<QNetworkReply* >("QNetworkReply*");

    QSignalSpy spy2(page.networkAccessManager(), SIGNAL(sslErrors(QNetworkReply*,QList<QSslError>)));
    frame->setUrl(QUrl("qrc:/fake-ssl-error.html"));
    waitForSignal(frame, SIGNAL(loadFinished(bool)), 200);
    QCOMPARE(spy2.count(), 1);
    QCOMPARE(frame->requestedUrl(), QUrl("qrc:/fake-ssl-error.html"));
    QCOMPARE(frame->url(), QUrl("qrc:/fake-ssl-error.html"));
#endif
}

void tst_QWebFrame::javaScriptWindowObjectCleared_data()
{
    QTest::addColumn<QString>("html");
    QTest::addColumn<int>("signalCount");
    QTest::newRow("with <script>") << "<html><body><script>i=0</script><p>hello world</p></body></html>" << 1;
    // NOTE: Empty scripts no longer cause this signal to be emitted.
    QTest::newRow("with empty <script>") << "<html><body><script></script><p>hello world</p></body></html>" << 0;
    QTest::newRow("without <script>") << "<html><body><p>hello world</p></body></html>" << 0;
}

void tst_QWebFrame::javaScriptWindowObjectCleared()
{
    QWebPage page;
    QWebFrame* frame = page.mainFrame();
    QSignalSpy spy(frame, SIGNAL(javaScriptWindowObjectCleared()));
    QFETCH(QString, html);
    frame->setHtml(html);

    QFETCH(int, signalCount);
    QCOMPARE(spy.count(), signalCount);
}

void tst_QWebFrame::javaScriptWindowObjectClearedOnEvaluate()
{
    QWebPage page;
    QWebFrame* frame = page.mainFrame();
    QSignalSpy spy(frame, SIGNAL(javaScriptWindowObjectCleared()));
    frame->setHtml("<html></html>");
    QCOMPARE(spy.count(), 0);
    frame->evaluateJavaScript("var a = 'a';");
    QCOMPARE(spy.count(), 1);
    // no new clear for a new script:
    frame->evaluateJavaScript("var a = 1;");
    QCOMPARE(spy.count(), 1);
}

void tst_QWebFrame::setHtml()
{
    QString html("<html><head></head><body><p>hello world</p></body></html>");
    m_view->page()->mainFrame()->setHtml(html);
    QCOMPARE(m_view->page()->mainFrame()->toHtml(), html);
}

void tst_QWebFrame::setHtmlWithResource()
{
    QString html("<html><body><p>hello world</p><img src='qrc:/image.png'/></body></html>");

    QWebPage page;
    QWebFrame* frame = page.mainFrame();

    // in few seconds, the image should be completey loaded
    QSignalSpy spy(&page, SIGNAL(loadFinished(bool)));
    frame->setHtml(html);
    waitForSignal(frame, SIGNAL(loadFinished(bool)), 200);
    QCOMPARE(spy.count(), 1);

    QCOMPARE(frame->evaluateJavaScript("document.images.length").toInt(), 1);
    QCOMPARE(frame->evaluateJavaScript("document.images[0].width").toInt(), 128);
    QCOMPARE(frame->evaluateJavaScript("document.images[0].height").toInt(), 128);

    QString html2 =
        "<html>"
            "<head>"
                "<link rel='stylesheet' href='qrc:/style.css' type='text/css' />"
            "</head>"
            "<body>"
                "<p id='idP'>some text</p>"
            "</body>"
        "</html>";

    // in few seconds, the CSS should be completey loaded
    frame->setHtml(html2);
    waitForSignal(frame, SIGNAL(loadFinished(bool)), 200);
    QCOMPARE(spy.size(), 2);

    QWebElement p = frame->documentElement().findAll("p").at(0);
    QCOMPARE(p.styleProperty("color", QWebElement::CascadedStyle), QLatin1String("red"));
}

void tst_QWebFrame::setHtmlWithBaseURL()
{
    if (!QDir(TESTS_SOURCE_DIR).exists())
        QSKIP(QString("This test requires access to resources found in '%1'").arg(TESTS_SOURCE_DIR).toLatin1().constData(), SkipAll);

    QDir::setCurrent(TESTS_SOURCE_DIR);

    QString html("<html><body><p>hello world</p><img src='resources/image2.png'/></body></html>");

    QWebPage page;
    QWebFrame* frame = page.mainFrame();

    // in few seconds, the image should be completey loaded
    QSignalSpy spy(&page, SIGNAL(loadFinished(bool)));

    frame->setHtml(html, QUrl::fromLocalFile(TESTS_SOURCE_DIR));
    waitForSignal(frame, SIGNAL(loadFinished(bool)), 200);
    QCOMPARE(spy.count(), 1);

    QCOMPARE(frame->evaluateJavaScript("document.images.length").toInt(), 1);
    QCOMPARE(frame->evaluateJavaScript("document.images[0].width").toInt(), 128);
    QCOMPARE(frame->evaluateJavaScript("document.images[0].height").toInt(), 128);

    // no history item has to be added.
    QCOMPARE(m_view->page()->history()->count(), 0);
}

class MyPage : public QWebPage
{
public:
    MyPage() :  QWebPage(), alerts(0) {}
    int alerts;

protected:
    virtual void javaScriptAlert(QWebFrame*, const QString& msg)
    {
        alerts++;
        QCOMPARE(msg, QString("foo"));
        // Should not be enough to trigger deferred loading, since we've upped the HTML
        // tokenizer delay in the Qt frameloader. See HTMLTokenizer::continueProcessing()
        QTest::qWait(1000);
    }
};

void tst_QWebFrame::setHtmlWithJSAlert()
{
    QString html("<html><head></head><body><script>alert('foo');</script><p>hello world</p></body></html>");
    MyPage page;
    m_view->setPage(&page);
    page.mainFrame()->setHtml(html);
    QCOMPARE(page.alerts, 1);
    QCOMPARE(m_view->page()->mainFrame()->toHtml(), html);
}

class TestNetworkManager : public QNetworkAccessManager
{
public:
    TestNetworkManager(QObject* parent) : QNetworkAccessManager(parent) {}

    QList<QUrl> requestedUrls;

protected:
    virtual QNetworkReply* createRequest(Operation op, const QNetworkRequest &request, QIODevice* outgoingData) {
        requestedUrls.append(request.url());
        QNetworkRequest redirectedRequest = request;
        redirectedRequest.setUrl(QUrl("data:text/html,<p>hello"));
        return QNetworkAccessManager::createRequest(op, redirectedRequest, outgoingData);
    }
};

void tst_QWebFrame::ipv6HostEncoding()
{
    TestNetworkManager* networkManager = new TestNetworkManager(m_page);
    m_page->setNetworkAccessManager(networkManager);
    networkManager->requestedUrls.clear();

    QUrl baseUrl = QUrl::fromEncoded("http://[::1]/index.html");
    m_view->setHtml("<p>Hi", baseUrl);
    m_view->page()->mainFrame()->evaluateJavaScript("var r = new XMLHttpRequest();"
            "r.open('GET', 'http://[::1]/test.xml', false);"
            "r.send(null);"
            );
    QCOMPARE(networkManager->requestedUrls.count(), 1);
    QCOMPARE(networkManager->requestedUrls.at(0), QUrl::fromEncoded("http://[::1]/test.xml"));
}

void tst_QWebFrame::metaData()
{
    m_view->setHtml("<html>"
                    "    <head>"
                    "        <meta name=\"description\" content=\"Test description\">"
                    "        <meta name=\"keywords\" content=\"HTML, JavaScript, Css\">"
                    "    </head>"
                    "</html>");

    QMultiMap<QString, QString> metaData = m_view->page()->mainFrame()->metaData();

    QCOMPARE(metaData.count(), 2);

    QCOMPARE(metaData.value("description"), QString("Test description"));
    QCOMPARE(metaData.value("keywords"), QString("HTML, JavaScript, Css"));
    QCOMPARE(metaData.value("nonexistant"), QString());

    m_view->setHtml("<html>"
                    "    <head>"
                    "        <meta name=\"samekey\" content=\"FirstValue\">"
                    "        <meta name=\"samekey\" content=\"SecondValue\">"
                    "    </head>"
                    "</html>");

    metaData = m_view->page()->mainFrame()->metaData();

    QCOMPARE(metaData.count(), 2);

    QStringList values = metaData.values("samekey");
    QCOMPARE(values.count(), 2);

    QVERIFY(values.contains("FirstValue"));
    QVERIFY(values.contains("SecondValue"));

    QCOMPARE(metaData.value("nonexistant"), QString());
}

#if !defined(Q_WS_MAEMO_5) && !defined(Q_OS_SYMBIAN)
void tst_QWebFrame::popupFocus()
{
    QWebView view;
    view.setHtml("<html>"
                 "    <body>"
                 "        <select name=\"select\">"
                 "            <option>1</option>"
                 "            <option>2</option>"
                 "        </select>"
                 "        <input type=\"text\"> </input>"
                 "        <textarea name=\"text_area\" rows=\"3\" cols=\"40\">"
                 "This test checks whether showing and hiding a popup"
                 "takes the focus away from the webpage."
                 "        </textarea>"
                 "    </body>"
                 "</html>");
    view.resize(400, 100);
    // Call setFocus before show to work around http://bugreports.qt.nokia.com/browse/QTBUG-14762
    view.setFocus();
    view.show();
    QTest::qWaitForWindowShown(&view);
    view.activateWindow();
    QTRY_VERIFY(view.hasFocus());

    // open the popup by clicking. check if focus is on the popup
    const QWebElement webCombo = view.page()->mainFrame()->documentElement().findFirst(QLatin1String("select[name=select]"));
    QTest::mouseClick(&view, Qt::LeftButton, 0, webCombo.geometry().center());
    QObject* webpopup = firstChildByClassName(&view, "QComboBox");
    QComboBox* combo = qobject_cast<QComboBox*>(webpopup);
    QVERIFY(combo != 0);
    QTRY_VERIFY(!view.hasFocus() && combo->view()->hasFocus()); // Focus should be on the popup

    // hide the popup and check if focus is on the page
    combo->hidePopup();
    QTRY_VERIFY(view.hasFocus()); // Focus should be back on the WebView
}
#endif

void tst_QWebFrame::inputFieldFocus()
{
    QWebView view;
    view.setHtml("<html><body><input type=\"text\"></input></body></html>");
    view.resize(400, 100);
    view.show();
    QTest::qWaitForWindowShown(&view);
    view.activateWindow();
    view.setFocus();
    QTRY_VERIFY(view.hasFocus());

    // double the flashing time, should at least blink once already
    int delay = qApp->cursorFlashTime() * 2;

    // focus the lineedit and check if it blinks
    const QWebElement inputElement = view.page()->mainFrame()->documentElement().findFirst(QLatin1String("input[type=text]"));
    QTest::mouseClick(&view, Qt::LeftButton, 0, inputElement.geometry().center());
    m_inputFieldsTestView = &view;
    view.installEventFilter( this );
    QTest::qWait(delay);
    QVERIFY2(m_inputFieldTestPaintCount >= 3,
             "The input field should have a blinking caret");
}

void tst_QWebFrame::hitTestContent()
{
    QString html("<html><body><p>A paragraph</p><br/><br/><br/><a href=\"about:blank\" target=\"_foo\" id=\"link\">link text</a></body></html>");

    QWebPage page;
    QWebFrame* frame = page.mainFrame();
    frame->setHtml(html);
    page.setViewportSize(QSize(200, 0)); //no height so link is not visible
    const QWebElement linkElement = frame->documentElement().findFirst(QLatin1String("a#link"));
    QWebHitTestResult result = frame->hitTestContent(linkElement.geometry().center());
    QCOMPARE(result.linkText(), QString("link text"));
    QWebElement link = result.linkElement();
    QCOMPARE(link.attribute("target"), QString("_foo"));
}

void tst_QWebFrame::jsByteArray()
{
    QByteArray ba("hello world");
    m_myObject->setByteArrayProperty(ba);

    // read-only property
    QCOMPARE(m_myObject->byteArrayProperty(), ba);
    QString type;
    QVariant v = evalJSV("myObject.byteArrayProperty");
    QCOMPARE(int(v.type()), int(QVariant::ByteArray));

    QCOMPARE(v.toByteArray(), ba);
}

void tst_QWebFrame::ownership()
{
    // test ownership
    {
        QPointer<QObject> ptr = new QObject();
        QVERIFY(ptr != 0);
        {
            QWebPage page;
            QWebFrame* frame = page.mainFrame();
            frame->addToJavaScriptWindowObject("test", ptr, QScriptEngine::ScriptOwnership);
        }
        QVERIFY(ptr == 0);
    }
    {
        QPointer<QObject> ptr = new QObject();
        QVERIFY(ptr != 0);
        QObject* before = ptr;
        {
            QWebPage page;
            QWebFrame* frame = page.mainFrame();
            frame->addToJavaScriptWindowObject("test", ptr, QScriptEngine::QtOwnership);
        }
        QVERIFY(ptr == before);
        delete ptr;
    }
    {
        QObject* parent = new QObject();
        QObject* child = new QObject(parent);
        QWebPage page;
        QWebFrame* frame = page.mainFrame();
        frame->addToJavaScriptWindowObject("test", child, QScriptEngine::QtOwnership);
        QVariant v = frame->evaluateJavaScript("test");
        QCOMPARE(qvariant_cast<QObject*>(v), child);
        delete parent;
        v = frame->evaluateJavaScript("test");
        QCOMPARE(qvariant_cast<QObject*>(v), (QObject *)0);
    }
    {
        QPointer<QObject> ptr = new QObject();
        QVERIFY(ptr != 0);
        {
            QWebPage page;
            QWebFrame* frame = page.mainFrame();
            frame->addToJavaScriptWindowObject("test", ptr, QScriptEngine::AutoOwnership);
        }
        // no parent, so it should be like ScriptOwnership
        QVERIFY(ptr == 0);
    }
    {
        QObject* parent = new QObject();
        QPointer<QObject> child = new QObject(parent);
        QVERIFY(child != 0);
        {
            QWebPage page;
            QWebFrame* frame = page.mainFrame();
            frame->addToJavaScriptWindowObject("test", child, QScriptEngine::AutoOwnership);
        }
        // has parent, so it should be like QtOwnership
        QVERIFY(child != 0);
        delete parent;
    }
}

void tst_QWebFrame::nullValue()
{
    QVariant v = m_view->page()->mainFrame()->evaluateJavaScript("null");
    QVERIFY(v.isNull());
}

void tst_QWebFrame::baseUrl_data()
{
    QTest::addColumn<QString>("html");
    QTest::addColumn<QUrl>("loadUrl");
    QTest::addColumn<QUrl>("url");
    QTest::addColumn<QUrl>("baseUrl");

    QTest::newRow("null") << QString() << QUrl()
                          << QUrl("about:blank") << QUrl("about:blank");

    QTest::newRow("foo") << QString() << QUrl("http://foobar.baz/")
                         << QUrl("http://foobar.baz/") << QUrl("http://foobar.baz/");

    QString html = "<html>"
        "<head>"
            "<base href=\"http://foobaz.bar/\" />"
        "</head>"
    "</html>";
    QTest::newRow("customBaseUrl") << html << QUrl("http://foobar.baz/")
                                   << QUrl("http://foobar.baz/") << QUrl("http://foobaz.bar/");
}

void tst_QWebFrame::baseUrl()
{
    QFETCH(QString, html);
    QFETCH(QUrl, loadUrl);
    QFETCH(QUrl, url);
    QFETCH(QUrl, baseUrl);

    m_page->mainFrame()->setHtml(html, loadUrl);
    QCOMPARE(m_page->mainFrame()->url(), url);
    QCOMPARE(m_page->mainFrame()->baseUrl(), baseUrl);
}

void tst_QWebFrame::hasSetFocus()
{
    QString html("<html><body><p>top</p>" \
                    "<iframe width='80%' height='30%'/>" \
                 "</body></html>");

    QSignalSpy loadSpy(m_page, SIGNAL(loadFinished(bool)));
    m_page->mainFrame()->setHtml(html);

    waitForSignal(m_page->mainFrame(), SIGNAL(loadFinished(bool)), 200);
    QCOMPARE(loadSpy.size(), 1);

    QList<QWebFrame*> children = m_page->mainFrame()->childFrames();
    QWebFrame* frame = children.at(0);
    QString innerHtml("<html><body><p>another iframe</p>" \
                        "<iframe width='80%' height='30%'/>" \
                      "</body></html>");
    frame->setHtml(innerHtml);

    waitForSignal(frame, SIGNAL(loadFinished(bool)), 200);
    QCOMPARE(loadSpy.size(), 2);

    m_page->mainFrame()->setFocus();
    QTRY_VERIFY(m_page->mainFrame()->hasFocus());

    for (int i = 0; i < children.size(); ++i) {
        children.at(i)->setFocus();
        QTRY_VERIFY(children.at(i)->hasFocus());
        QVERIFY(!m_page->mainFrame()->hasFocus());
    }

    m_page->mainFrame()->setFocus();
    QTRY_VERIFY(m_page->mainFrame()->hasFocus());
}

void tst_QWebFrame::render()
{
    QString html("<html>" \
                    "<head><style>" \
                       "body, iframe { margin: 0px; border: none; }" \
                    "</style></head>" \
                    "<body><iframe width='100px' height='100px'/></body>" \
                 "</html>");

    QWebPage page;
    page.mainFrame()->setHtml(html);

    QList<QWebFrame*> frames = page.mainFrame()->childFrames();
    QWebFrame *frame = frames.at(0);
    QString innerHtml("<body style='margin: 0px;'><img src='qrc:/image.png'/></body>");
    frame->setHtml(innerHtml);

    QPicture picture;

    QSize size = page.mainFrame()->contentsSize();
    page.setViewportSize(size);

    // render contents layer only (the iframe is smaller than the image, so it will have scrollbars)
    QPainter painter1(&picture);
    frame->render(&painter1, QWebFrame::ContentsLayer);
    painter1.end();

    QCOMPARE(size.width(), picture.boundingRect().width() + frame->scrollBarGeometry(Qt::Vertical).width());
    QCOMPARE(size.height(), picture.boundingRect().height() + frame->scrollBarGeometry(Qt::Horizontal).height());

    // render everything, should be the size of the iframe
    QPainter painter2(&picture);
    frame->render(&painter2, QWebFrame::AllLayers);
    painter2.end();

    QCOMPARE(size.width(), picture.boundingRect().width());   // width: 100px
    QCOMPARE(size.height(), picture.boundingRect().height()); // height: 100px
}

void tst_QWebFrame::scrollPosition()
{
    // enlarged image in a small viewport, to provoke the scrollbars to appear
    QString html("<html><body><img src='qrc:/image.png' height=500 width=500/></body></html>");

    QWebPage page;
    page.setViewportSize(QSize(200, 200));

    QWebFrame* frame = page.mainFrame();
    frame->setHtml(html);
    frame->setScrollBarPolicy(Qt::Vertical, Qt::ScrollBarAlwaysOff);
    frame->setScrollBarPolicy(Qt::Horizontal, Qt::ScrollBarAlwaysOff);

    // try to set the scroll offset programmatically
    frame->setScrollPosition(QPoint(23, 29));
    QCOMPARE(frame->scrollPosition().x(), 23);
    QCOMPARE(frame->scrollPosition().y(), 29);

    int x = frame->evaluateJavaScript("window.scrollX").toInt();
    int y = frame->evaluateJavaScript("window.scrollY").toInt();
    QCOMPARE(x, 23);
    QCOMPARE(y, 29);
}

void tst_QWebFrame::scrollToAnchor()
{
    QWebPage page;
    page.setViewportSize(QSize(480, 800));
    QWebFrame* frame = page.mainFrame();

    QString html("<html><body><p style=\"margin-bottom: 1500px;\">Hello.</p>"
                 "<p><a id=\"foo\">This</a> is an anchor</p>"
                 "<p style=\"margin-bottom: 1500px;\"><a id=\"bar\">This</a> is another anchor</p>"
                 "</body></html>");
    frame->setHtml(html);
    frame->setScrollPosition(QPoint(0, 0));
    QCOMPARE(frame->scrollPosition().x(), 0);
    QCOMPARE(frame->scrollPosition().y(), 0);

    QWebElement fooAnchor = frame->findFirstElement("a[id=foo]");

    frame->scrollToAnchor("foo");
    QCOMPARE(frame->scrollPosition().y(), fooAnchor.geometry().top());

    frame->scrollToAnchor("bar");
    frame->scrollToAnchor("foo");
    QCOMPARE(frame->scrollPosition().y(), fooAnchor.geometry().top());

    frame->scrollToAnchor("top");
    QCOMPARE(frame->scrollPosition().y(), 0);

    frame->scrollToAnchor("bar");
    frame->scrollToAnchor("notexist");
    QVERIFY(frame->scrollPosition().y() != 0);
}


void tst_QWebFrame::scrollbarsOff()
{
    QWebView view;
    QWebFrame* mainFrame = view.page()->mainFrame();

    mainFrame->setScrollBarPolicy(Qt::Vertical, Qt::ScrollBarAlwaysOff);
    mainFrame->setScrollBarPolicy(Qt::Horizontal, Qt::ScrollBarAlwaysOff);

    QString html("<script>" \
                 "   function checkScrollbar() {" \
                 "       if (innerWidth === document.documentElement.offsetWidth)" \
                 "           document.getElementById('span1').innerText = 'SUCCESS';" \
                 "       else" \
                 "           document.getElementById('span1').innerText = 'FAIL';" \
                 "   }" \
                 "</script>" \
                 "<body>" \
                 "   <div style='margin-top:1000px ; margin-left:1000px'>" \
                 "       <a id='offscreen' href='a'>End</a>" \
                 "   </div>" \
                 "<span id='span1'></span>" \
                 "</body>");


    view.setHtml(html);
    ::waitForSignal(&view, SIGNAL(loadFinished(bool)));

    mainFrame->evaluateJavaScript("checkScrollbar();");
    QCOMPARE(mainFrame->documentElement().findAll("span").at(0).toPlainText(), QString("SUCCESS"));
}

void tst_QWebFrame::evaluateWillCauseRepaint()
{
    QWebView view;
    QString html("<html><body>top<div id=\"junk\" style=\"display: block;\">"
                    "junk</div>bottom</body></html>");
    view.setHtml(html);
    view.show();

    QTest::qWaitForWindowShown(&view);
    view.page()->mainFrame()->evaluateJavaScript(
        "document.getElementById('junk').style.display = 'none';");

    ::waitForSignal(view.page(), SIGNAL(repaintRequested(QRect)));
}

class TestFactory : public QObject
{
    Q_OBJECT
public:
    TestFactory()
        : obj(0), counter(0)
    {}

    Q_INVOKABLE QObject* getNewObject()
    {
        delete obj;
        obj = new QObject(this);
        obj->setObjectName(QLatin1String("test") + QString::number(++counter));
        return obj;

    }

    QObject* obj;
    int counter;
};

void tst_QWebFrame::qObjectWrapperWithSameIdentity()
{
    m_view->setHtml("<script>function triggerBug() { document.getElementById('span1').innerText = test.getNewObject().objectName; }</script>"
                    "<body><span id='span1'>test</span></body>");

    QWebFrame* mainFrame = m_view->page()->mainFrame();
    QCOMPARE(mainFrame->toPlainText(), QString("test"));

    mainFrame->addToJavaScriptWindowObject("test", new TestFactory, QScriptEngine::ScriptOwnership);

    mainFrame->evaluateJavaScript("triggerBug();");
    QCOMPARE(mainFrame->toPlainText(), QString("test1"));

    mainFrame->evaluateJavaScript("triggerBug();");
    QCOMPARE(mainFrame->toPlainText(), QString("test2"));
}

void tst_QWebFrame::introspectQtMethods_data()
{
    QTest::addColumn<QString>("objectExpression");
    QTest::addColumn<QString>("methodName");
    QTest::addColumn<QStringList>("expectedPropertyNames");

    QTest::newRow("myObject.mySignal")
        << "myObject" << "mySignal" << (QStringList() << "connect" << "disconnect" << "length" << "name");
    QTest::newRow("myObject.mySlot")
        << "myObject" << "mySlot" << (QStringList() << "connect" << "disconnect" << "length" << "name");
    QTest::newRow("myObject.myInvokable")
        << "myObject" << "myInvokable" << (QStringList() << "connect" << "disconnect" << "length" << "name");
    QTest::newRow("myObject.mySignal.connect")
        << "myObject.mySignal" << "connect" << (QStringList() << "length" << "name");
    QTest::newRow("myObject.mySignal.disconnect")
        << "myObject.mySignal" << "disconnect" << (QStringList() << "length" << "name");
}

void tst_QWebFrame::introspectQtMethods()
{
    QFETCH(QString, objectExpression);
    QFETCH(QString, methodName);
    QFETCH(QStringList, expectedPropertyNames);

    QString methodLookup = QString::fromLatin1("%0['%1']").arg(objectExpression).arg(methodName);
    QCOMPARE(evalJSV(QString::fromLatin1("Object.getOwnPropertyNames(%0).sort()").arg(methodLookup)).toStringList(), expectedPropertyNames);

    for (int i = 0; i < expectedPropertyNames.size(); ++i) {
        QString name = expectedPropertyNames.at(i);
        QCOMPARE(evalJS(QString::fromLatin1("%0.hasOwnProperty('%1')").arg(methodLookup).arg(name)), sTrue);
        evalJS(QString::fromLatin1("var descriptor = Object.getOwnPropertyDescriptor(%0, '%1')").arg(methodLookup).arg(name));
        QCOMPARE(evalJS("typeof descriptor"), QString::fromLatin1("object"));
        QCOMPARE(evalJS("descriptor.get"), sUndefined);
        QCOMPARE(evalJS("descriptor.set"), sUndefined);
        QCOMPARE(evalJS(QString::fromLatin1("descriptor.value === %0['%1']").arg(methodLookup).arg(name)), sTrue);
        QCOMPARE(evalJS(QString::fromLatin1("descriptor.enumerable")), sFalse);
        QCOMPARE(evalJS(QString::fromLatin1("descriptor.configurable")), sFalse);
    }

    QVERIFY(evalJSV("var props=[]; for (var p in myObject.deleteLater) {props.push(p);}; props.sort()").toStringList().isEmpty());
}

void tst_QWebFrame::setContent_data()
{
    QTest::addColumn<QString>("mimeType");
    QTest::addColumn<QByteArray>("testContents");
    QTest::addColumn<QString>("expected");

    QString str = QString::fromUtf8("ὕαλον ϕαγεῖν δύναμαι· τοῦτο οὔ με βλάπτει");
    QTest::newRow("UTF-8 plain text") << "text/plain; charset=utf-8" << str.toUtf8() << str;

    QTextCodec *utf16 = QTextCodec::codecForName("UTF-16");
    if (utf16)
        QTest::newRow("UTF-16 plain text") << "text/plain; charset=utf-16" << utf16->fromUnicode(str) << str;

    str = QString::fromUtf8("Une chaîne de caractères à sa façon.");
    QTest::newRow("latin-1 plain text") << "text/plain; charset=iso-8859-1" << str.toLatin1() << str;


}

void tst_QWebFrame::setContent()
{
    QFETCH(QString, mimeType);
    QFETCH(QByteArray, testContents);
    QFETCH(QString, expected);
    m_view->setContent(testContents, mimeType);
    QWebFrame* mainFrame = m_view->page()->mainFrame();
    QCOMPARE(expected , mainFrame->toPlainText());
}

QTEST_MAIN(tst_QWebFrame)
#include "tst_qwebframe.moc"
