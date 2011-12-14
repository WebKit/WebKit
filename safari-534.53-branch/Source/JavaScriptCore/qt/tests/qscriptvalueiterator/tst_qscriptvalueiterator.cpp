/*
    Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)

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

#ifndef tst_qscriptvalueiterator_h
#define tst_qscriptvalueiterator_h

#include "qscriptengine.h"
#include "qscriptvalue.h"
#include "qscriptvalueiterator.h"
#include <QtCore/qhash.h>
#include <QtTest/QtTest>

class tst_QScriptValueIterator : public QObject {
    Q_OBJECT

public:
    tst_QScriptValueIterator();
    virtual ~tst_QScriptValueIterator();

private slots:
    void iterateForward_data();
    void iterateForward();
    void iterateBackward_data();
    void iterateBackward();
    void iterateArray_data();
    void iterateArray();
    void iterateBackAndForth();
    void setValue();
    void remove();
    void removeMixed();
    void removeUndeletable();
    void iterateString();
    void assignObjectToIterator();
};

tst_QScriptValueIterator::tst_QScriptValueIterator()
{
}

tst_QScriptValueIterator::~tst_QScriptValueIterator()
{
}

void tst_QScriptValueIterator::iterateForward_data()
{
    QTest::addColumn<QStringList>("propertyNames");
    QTest::addColumn<QStringList>("propertyValues");

    QTest::newRow("no properties")
        << QStringList() << QStringList();
    QTest::newRow("foo=bar")
        << (QStringList() << "foo")
        << (QStringList() << "bar");
    QTest::newRow("foo=bar, baz=123")
        << (QStringList() << "foo" << "baz")
        << (QStringList() << "bar" << "123");
    QTest::newRow("foo=bar, baz=123, rab=oof")
        << (QStringList() << "foo" << "baz" << "rab")
        << (QStringList() << "bar" << "123" << "oof");
}

void tst_QScriptValueIterator::iterateForward()
{
    QFETCH(QStringList, propertyNames);
    QFETCH(QStringList, propertyValues);
    QMap<QString, QString> pmap;
    Q_ASSERT(propertyNames.size() == propertyValues.size());

    QScriptEngine engine;
    QScriptValue object = engine.newObject();
    for (int i = 0; i < propertyNames.size(); ++i) {
        QString name = propertyNames.at(i);
        QString value = propertyValues.at(i);
        pmap.insert(name, value);
        object.setProperty(name, QScriptValue(&engine, value));
    }
    QScriptValue otherObject = engine.newObject();
    otherObject.setProperty("foo", QScriptValue(&engine, 123456));
    otherObject.setProperty("protoProperty", QScriptValue(&engine, 654321));
    object.setPrototype(otherObject); // should not affect iterator

    QStringList lst;
    QScriptValueIterator it(object);
    while (!pmap.isEmpty()) {
        QCOMPARE(it.hasNext(), true);
        QCOMPARE(it.hasNext(), true);
        it.next();
        QString name = it.name();
        QCOMPARE(pmap.contains(name), true);
        QCOMPARE(it.name(), name);
        QCOMPARE(it.flags(), object.propertyFlags(name));
        QCOMPARE(it.value().strictlyEquals(QScriptValue(&engine, pmap.value(name))), true);
        QCOMPARE(it.scriptName(), engine.toStringHandle(name));
        pmap.remove(name);
        lst.append(name);
    }

    QCOMPARE(it.hasNext(), false);
    QCOMPARE(it.hasNext(), false);

    it.toFront();
    for (int i = 0; i < lst.count(); ++i) {
        QCOMPARE(it.hasNext(), true);
        it.next();
        QCOMPARE(it.name(), lst.at(i));
    }

    for (int i = 0; i < lst.count(); ++i) {
        QCOMPARE(it.hasPrevious(), true);
        it.previous();
        QCOMPARE(it.name(), lst.at(lst.count()-1-i));
    }
    QCOMPARE(it.hasPrevious(), false);
}

void tst_QScriptValueIterator::iterateBackward_data()
{
    iterateForward_data();
}

void tst_QScriptValueIterator::iterateBackward()
{
    QFETCH(QStringList, propertyNames);
    QFETCH(QStringList, propertyValues);
    QMap<QString, QString> pmap;
    Q_ASSERT(propertyNames.size() == propertyValues.size());

    QScriptEngine engine;
    QScriptValue object = engine.newObject();
    for (int i = 0; i < propertyNames.size(); ++i) {
        QString name = propertyNames.at(i);
        QString value = propertyValues.at(i);
        pmap.insert(name, value);
        object.setProperty(name, QScriptValue(&engine, value));
    }

    QStringList lst;
    QScriptValueIterator it(object);
    it.toBack();
    while (!pmap.isEmpty()) {
        QCOMPARE(it.hasPrevious(), true);
        QCOMPARE(it.hasPrevious(), true);
        it.previous();
        QString name = it.name();
        QCOMPARE(pmap.contains(name), true);
        QCOMPARE(it.name(), name);
        QCOMPARE(it.flags(), object.propertyFlags(name));
        QCOMPARE(it.value().strictlyEquals(QScriptValue(&engine, pmap.value(name))), true);
        pmap.remove(name);
        lst.append(name);
    }

    QCOMPARE(it.hasPrevious(), false);
    QCOMPARE(it.hasPrevious(), false);

    it.toBack();
    for (int i = 0; i < lst.count(); ++i) {
        QCOMPARE(it.hasPrevious(), true);
        it.previous();
        QCOMPARE(it.name(), lst.at(i));
    }

    for (int i = 0; i < lst.count(); ++i) {
        QCOMPARE(it.hasNext(), true);
        it.next();
        QCOMPARE(it.name(), lst.at(lst.count()-1-i));
    }
    QCOMPARE(it.hasNext(), false);
}

void tst_QScriptValueIterator::iterateArray_data()
{
    QTest::addColumn<QStringList>("inputPropertyNames");
    QTest::addColumn<QStringList>("inputPropertyValues");
    QTest::addColumn<QStringList>("propertyNames");
    QTest::addColumn<QStringList>("propertyValues");
    QTest::newRow("no elements") << QStringList() << QStringList() << QStringList() << QStringList();

    QTest::newRow("0=foo, 1=barr")
        << (QStringList() << "0" << "1")
        << (QStringList() << "foo" << "bar")
        << (QStringList() << "0" << "1")
        << (QStringList() << "foo" << "bar");

    QTest::newRow("0=foo, 3=barr")
        << (QStringList() << "0" << "1" << "2" << "3")
        << (QStringList() << "foo" << "" << "" << "bar")
        << (QStringList() << "0" << "1" << "2" << "3")
        << (QStringList() << "foo" << "" << "" << "bar");
}

void tst_QScriptValueIterator::iterateArray()
{
    QFETCH(QStringList, inputPropertyNames);
    QFETCH(QStringList, inputPropertyValues);
    QFETCH(QStringList, propertyNames);
    QFETCH(QStringList, propertyValues);

    QScriptEngine engine;
    QScriptValue array = engine.newArray();
    for (int i = 0; i < inputPropertyNames.size(); ++i)
        array.setProperty(inputPropertyNames.at(i), inputPropertyValues.at(i));

    int length = array.property("length").toInt32();
    QCOMPARE(length, propertyNames.size());
    QScriptValueIterator it(array);
    for (int i = 0; i < length; ++i) {
        QCOMPARE(it.hasNext(), true);
        it.next();
        QCOMPARE(it.name(), propertyNames.at(i));
        QCOMPARE(it.flags(), array.propertyFlags(propertyNames.at(i)));
        QVERIFY(it.value().strictlyEquals(array.property(propertyNames.at(i))));
        QCOMPARE(it.value().toString(), propertyValues.at(i));
    }
    QVERIFY(it.hasNext());
    it.next();
    QCOMPARE(it.name(), QString::fromLatin1("length"));
    QVERIFY(it.value().isNumber());
    QCOMPARE(it.value().toInt32(), length);
    QCOMPARE(it.flags(), QScriptValue::PropertyFlags(QScriptValue::SkipInEnumeration | QScriptValue::Undeletable));

    it.previous();
    QCOMPARE(it.hasPrevious(), length > 0);
    for (int i = length - 1; i >= 0; --i) {
        it.previous();
        QCOMPARE(it.name(), propertyNames.at(i));
        QCOMPARE(it.flags(), array.propertyFlags(propertyNames.at(i)));
        QVERIFY(it.value().strictlyEquals(array.property(propertyNames.at(i))));
        QCOMPARE(it.value().toString(), propertyValues.at(i));
        QCOMPARE(it.hasPrevious(), i > 0);
    }
    QCOMPARE(it.hasPrevious(), false);

    // hasNext() and hasPrevious() cache their result; verify that the result is in sync
    if (length > 1) {
        QVERIFY(it.hasNext());
        it.next();
        QCOMPARE(it.name(), QString::fromLatin1("0"));
        QVERIFY(it.hasNext());
        it.previous();
        QCOMPARE(it.name(), QString::fromLatin1("0"));
        QVERIFY(!it.hasPrevious());
        it.next();
        QCOMPARE(it.name(), QString::fromLatin1("0"));
        QVERIFY(it.hasPrevious());
        it.next();
        QCOMPARE(it.name(), QString::fromLatin1("1"));
    }
    {
        // same test as object:
        QScriptValue originalArray = engine.newArray();
        for (int i = 0; i < inputPropertyNames.size(); ++i)
            originalArray.setProperty(inputPropertyNames.at(i), inputPropertyValues.at(i));

        QScriptValue array = originalArray.toObject();
        int length = array.property("length").toInt32();
        QCOMPARE(length, propertyNames.size());
        QScriptValueIterator it(array);
        for (int i = 0; i < length; ++i) {
            QCOMPARE(it.hasNext(), true);
            it.next();
            QCOMPARE(it.name(), propertyNames.at(i));
            QCOMPARE(it.flags(), array.propertyFlags(propertyNames.at(i)));
            QVERIFY(it.value().strictlyEquals(array.property(propertyNames.at(i))));
            QCOMPARE(it.value().toString(), propertyValues.at(i));
        }
        QCOMPARE(it.hasNext(), true);
        it.next();
        QCOMPARE(it.name(), QString::fromLatin1("length"));
    }
}

void tst_QScriptValueIterator::iterateBackAndForth()
{
    QScriptEngine engine;
    {
        QScriptValue object = engine.newObject();
        object.setProperty("foo", QScriptValue(&engine, "bar"));
        object.setProperty("rab", QScriptValue(&engine, "oof"),
                           QScriptValue::SkipInEnumeration); // should not affect iterator
        QScriptValueIterator it(object);
        QVERIFY(it.hasNext());
        it.next();
        QCOMPARE(it.name(), QLatin1String("foo"));
        QVERIFY(it.hasPrevious());
        it.previous();
        QCOMPARE(it.name(), QLatin1String("foo"));
        QVERIFY(it.hasNext());
        it.next();
        QCOMPARE(it.name(), QLatin1String("foo"));
        QVERIFY(it.hasPrevious());
        it.previous();
        QCOMPARE(it.name(), QLatin1String("foo"));
        QVERIFY(it.hasNext());
        it.next();
        QCOMPARE(it.name(), QLatin1String("foo"));
        QVERIFY(it.hasNext());
        it.next();
        QCOMPARE(it.name(), QLatin1String("rab"));
        QVERIFY(it.hasPrevious());
        it.previous();
        QCOMPARE(it.name(), QLatin1String("rab"));
        QVERIFY(it.hasNext());
        it.next();
        QCOMPARE(it.name(), QLatin1String("rab"));
        QVERIFY(it.hasPrevious());
        it.previous();
        QCOMPARE(it.name(), QLatin1String("rab"));
    }
    {
        // hasNext() and hasPrevious() cache their result; verify that the result is in sync
        QScriptValue object = engine.newObject();
        object.setProperty("foo", QScriptValue(&engine, "bar"));
        object.setProperty("rab", QScriptValue(&engine, "oof"));
        QScriptValueIterator it(object);
        QVERIFY(it.hasNext());
        it.next();
        QCOMPARE(it.name(), QString::fromLatin1("foo"));
        QVERIFY(it.hasNext());
        it.previous();
        QCOMPARE(it.name(), QString::fromLatin1("foo"));
        QVERIFY(!it.hasPrevious());
        it.next();
        QCOMPARE(it.name(), QString::fromLatin1("foo"));
        QVERIFY(it.hasPrevious());
        it.next();
        QCOMPARE(it.name(), QString::fromLatin1("rab"));
    }
}

void tst_QScriptValueIterator::setValue()
{
    QScriptEngine engine;
    QScriptValue object = engine.newObject();
    object.setProperty("foo", QScriptValue(&engine, "bar"));
    QScriptValueIterator it(object);
    it.next();
    QCOMPARE(it.name(), QLatin1String("foo"));
    it.setValue(QScriptValue(&engine, "baz"));
    QCOMPARE(it.value().strictlyEquals(QScriptValue(&engine, QLatin1String("baz"))), true);
    QCOMPARE(object.property("foo").toString(), QLatin1String("baz"));
    it.setValue(QScriptValue(&engine, "zab"));
    QCOMPARE(it.value().strictlyEquals(QScriptValue(&engine, QLatin1String("zab"))), true);
    QCOMPARE(object.property("foo").toString(), QLatin1String("zab"));
}

void tst_QScriptValueIterator::remove()
{
    QScriptEngine engine;
    QScriptValue object = engine.newObject();
    object.setProperty("foo", QScriptValue(&engine, "bar"),
                       QScriptValue::SkipInEnumeration); // should not affect iterator
    object.setProperty("rab", QScriptValue(&engine, "oof"));
    QScriptValueIterator it(object);
    it.next();
    QCOMPARE(it.name(), QLatin1String("foo"));
    it.remove();
    QCOMPARE(it.hasPrevious(), false);
    QCOMPARE(object.property("foo").isValid(), false);
    QCOMPARE(object.property("rab").toString(), QLatin1String("oof"));
    it.next();
    QCOMPARE(it.name(), QLatin1String("rab"));
    QCOMPARE(it.value().toString(), QLatin1String("oof"));
    QCOMPARE(it.hasNext(), false);
    it.remove();
    QCOMPARE(object.property("rab").isValid(), false);
    QCOMPARE(it.hasPrevious(), false);
    QCOMPARE(it.hasNext(), false);
}

void tst_QScriptValueIterator::removeMixed()
{
    // This test checks if QScriptValueIterator behaives correctly if an object's property got deleted
    // in different way.
    QScriptEngine engine;
    QScriptValue object = engine.evaluate("o = new Object; o");
    object.setProperty("a", QScriptValue(124), QScriptValue::SkipInEnumeration);
    object.setProperty("b", QScriptValue(816));
    object.setProperty("c", QScriptValue(3264));
    QScriptValueIterator it(object);
    it.next();
    it.next();
    QCOMPARE(it.name(), QLatin1String("b"));
    QCOMPARE(it.hasPrevious(), true);
    QCOMPARE(it.hasNext(), true);
    // Remove 'a'
    object.setProperty("a", QScriptValue());
    QEXPECT_FAIL("", "That would be a significant behavioral and performance change, new QtScript API should be developed (QTBUG-12087)", Abort);
    QCOMPARE(it.hasPrevious(), false);
    QCOMPARE(it.hasNext(), true);
    // Remove 'c'
    engine.evaluate("delete o.c");
    QCOMPARE(it.hasPrevious(), false);
    QCOMPARE(it.hasNext(), false);
    // Remove 'b'
    object.setProperty("b", QScriptValue());
    QCOMPARE(it.hasPrevious(), false);
    QCOMPARE(it.hasNext(), false);
    QCOMPARE(it.name(), QString());
    QCOMPARE(it.value().toString(), QString());

    // Try to remove a removed property.
    it.remove();
    QCOMPARE(it.hasPrevious(), false);
    QCOMPARE(it.hasNext(), false);
    QCOMPARE(it.name(), QString());
    QCOMPARE(it.value().toString(), QString());

    for (int i = 0; i < 2; ++i) {
        it.next();
        QCOMPARE(it.hasPrevious(), false);
        QCOMPARE(it.hasNext(), false);
        QCOMPARE(it.name(), QString());
        QCOMPARE(it.value().toString(), QString());
    }

    for (int i = 0; i < 2; ++i) {
        it.previous();
        QCOMPARE(it.hasPrevious(), false);
        QCOMPARE(it.hasNext(), false);
        QCOMPARE(it.name(), QString());
        QCOMPARE(it.value().toString(), QString());
    }
}

void tst_QScriptValueIterator::removeUndeletable()
{
    // Undeletable property can't be deleted via iterator.
    QScriptEngine engine;
    QScriptValue object = engine.evaluate("o = new Object; o");
    object.setProperty("a", QScriptValue(&engine, 124));
    object.setProperty("b", QScriptValue(&engine, 816), QScriptValue::Undeletable);
    QVERIFY(object.property("b").isValid());
    QScriptValueIterator it(object);
    it.next();
    it.next();
    it.remove();
    it.toFront();
    QVERIFY(it.hasNext());
    QVERIFY(object.property("b").isValid());
}

void tst_QScriptValueIterator::iterateString()
{
    QScriptEngine engine;
    QScriptValue str = QScriptValue(&engine, QString::fromLatin1("ciao"));
    QVERIFY(str.isString());
    QScriptValue obj = str.toObject();
    int length = obj.property("length").toInt32();
    QCOMPARE(length, 4);
    QScriptValueIterator it(obj);
    for (int i = 0; i < length; ++i) {
        QCOMPARE(it.hasNext(), true);
        QString indexStr = QScriptValue(&engine, i).toString();
        it.next();
        QCOMPARE(it.name(), indexStr);
        QCOMPARE(it.flags(), obj.propertyFlags(indexStr));
        QCOMPARE(it.value().strictlyEquals(obj.property(indexStr)), true);
    }
    QVERIFY(it.hasNext());
    it.next();
    QCOMPARE(it.name(), QString::fromLatin1("length"));
    QVERIFY(it.value().isNumber());
    QCOMPARE(it.value().toInt32(), length);
    QCOMPARE(it.flags(), QScriptValue::PropertyFlags(QScriptValue::ReadOnly | QScriptValue::SkipInEnumeration | QScriptValue::Undeletable));

    it.previous();
    QCOMPARE(it.hasPrevious(), length > 0);
    for (int i = length - 1; i >= 0; --i) {
        it.previous();
        QString indexStr = QScriptValue(&engine, i).toString();
        QCOMPARE(it.name(), indexStr);
        QCOMPARE(it.flags(), obj.propertyFlags(indexStr));
        QCOMPARE(it.value().strictlyEquals(obj.property(indexStr)), true);
        QCOMPARE(it.hasPrevious(), i > 0);
    }
    QCOMPARE(it.hasPrevious(), false);
}

void tst_QScriptValueIterator::assignObjectToIterator()
{
    QScriptEngine eng;
    QScriptValue obj1 = eng.newObject();
    obj1.setProperty("foo", 123);
    QScriptValue obj2 = eng.newObject();
    obj2.setProperty("bar", 456);

    QScriptValueIterator it(obj1);
    QVERIFY(it.hasNext());
    it.next();
    it = obj2;
    QVERIFY(it.hasNext());
    it.next();
    QCOMPARE(it.name(), QString::fromLatin1("bar"));

    it = obj1;
    QVERIFY(it.hasNext());
    it.next();
    QCOMPARE(it.name(), QString::fromLatin1("foo"));

    it = obj2;
    QVERIFY(it.hasNext());
    it.next();
    QCOMPARE(it.name(), QString::fromLatin1("bar"));

    it = obj2;
    QVERIFY(it.hasNext());
    it.next();
    QCOMPARE(it.name(), QString::fromLatin1("bar"));
}

QTEST_MAIN(tst_QScriptValueIterator)
#include "tst_qscriptvalueiterator.moc"

#endif // tst_qscriptvalueiterator_h
